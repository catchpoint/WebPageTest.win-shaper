/*
Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "common.h"
#define QUEUED_PACKET_POOL_TAG 'kppD'

// timer
WDFTIMER timer_handle = NULL;
KSPIN_LOCK timer_spinlock = 0;
BOOLEAN timer_pending = FALSE;

// QUEUED_PACKET is the object type we used to store all information
// needed for out-of-band packet re-injection. This type
// also points back to the flow context the packet belongs to.
typedef struct {
   LIST_ENTRY     listEntry;
   HANDLE         injection_handle;
   BOOLEAN        outbound;
   unsigned long  packet_length;  // Size of the packet (in bytes)
   LARGE_INTEGER  latency_start;  // time it was placed in the latency queue

   // Data fields for packet re-injection.
   UINT16 layerId;
   IF_INDEX interfaceIndex;
   NDIS_PORT_NUMBER NdisPortNumber;
   NET_BUFFER_LIST *netBufferList;
} QUEUED_PACKET;

// packet queues
// All packets are first buffered into the bandwidth queue and released
// at the appropriate rate for the configured bandwidth into the latency queue.
// When they are added to the latency queue they are timestamped when they
// entered and they are released when the appropriate latency has expired.
// Only the bandwidth queue is affected by the queue buffer size.  The latency
// queue has no limit.
typedef struct {
  LIST_ENTRY bandwidth_queue;
  LIST_ENTRY latency_queue;

  unsigned short   plr;         // packet loss in 1/100% (0-10000)
  unsigned __int64 bps;         // bandwidth in bits per second
  unsigned long    latency;     // latency in microseconds
  unsigned __int64 bufferBytes; // size of packet buffer in bytes

  unsigned __int64 queued_bytes;    // accumulated size of queued packets
  unsigned __int64 available_bytes; // accumulated bytes available for sending
  LARGE_INTEGER last_tick;          // performance counter timestamp of the last time the queue was checked
  KSPIN_LOCK lock;
} PACKET_QUEUE;

static BOOLEAN queues_initialized = FALSE;
static KSPIN_LOCK queue_lock = 0;
PACKET_QUEUE inbound_queue;
PACKET_QUEUE outbound_queue;

static BOOLEAN traffic_shaping_enabled = FALSE;

// Forward declarations
VOID TimerEvt(_In_ WDFTIMER Timer);
VOID StartPacketTimerIfNecessary();
void ProcessQueue(PACKET_QUEUE *queue);

#define INITIAL_TOKEN_COUNT 1500

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
NTSTATUS InitializePacketQueues(WDFDEVICE timer_parent) {
  NTSTATUS status = STATUS_SUCCESS;

  KeInitializeSpinLock(&queue_lock);   

  queues_initialized = TRUE;

  KeInitializeSpinLock(&timer_spinlock);   
  KeInitializeSpinLock(&inbound_queue.lock);
  InitializeListHead(&inbound_queue.bandwidth_queue);
  InitializeListHead(&inbound_queue.latency_queue);
  KeInitializeSpinLock(&outbound_queue.lock);   
  InitializeListHead(&outbound_queue.bandwidth_queue);
  InitializeListHead(&outbound_queue.latency_queue);

  // Create the timer that will be used to process the queues
  WDF_TIMER_CONFIG timer_config;
  WDF_OBJECT_ATTRIBUTES timer_attributes;
  WDF_TIMER_CONFIG_INIT(&timer_config, TimerEvt);
  timer_config.Period = 0;
  timer_config.TolerableDelay = 0;
  timer_config.AutomaticSerialization = FALSE;
  timer_config.UseHighResolutionTimer = WdfTrue;
  WDF_OBJECT_ATTRIBUTES_INIT(&timer_attributes);
  timer_attributes.ParentObject = timer_parent;
  timer_pending = FALSE;
  status = WdfTimerCreate(&timer_config, &timer_attributes, &timer_handle);

  return status;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void FreeQueuedPacket(_Inout_ __drv_freesMem(Mem) QUEUED_PACKET* packet) {
  if (packet->netBufferList != NULL)
    FwpsFreeCloneNetBufferList(packet->netBufferList, 0);
  ExFreePoolWithTag(packet, QUEUED_PACKET_POOL_TAG);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void DropQueue(PACKET_QUEUE *queue) {
  KLOCK_QUEUE_HANDLE lock;
  KeAcquireInStackQueuedSpinLock(&queue->lock, &lock);
  while (!IsListEmpty(&queue->latency_queue)) {
    LIST_ENTRY * listEntry = RemoveHeadList(&queue->latency_queue);
    QUEUED_PACKET *packet = CONTAINING_RECORD(listEntry, QUEUED_PACKET, listEntry);
    FreeQueuedPacket(packet);
  }
  while (!IsListEmpty(&queue->bandwidth_queue)) {
    LIST_ENTRY * listEntry = RemoveHeadList(&queue->bandwidth_queue);
    QUEUED_PACKET *packet = CONTAINING_RECORD(listEntry, QUEUED_PACKET, listEntry);
    FreeQueuedPacket(packet);
  }
  KeReleaseInStackQueuedSpinLock(&lock);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
BOOLEAN ShaperEnable(_In_ unsigned short plr,
                     _In_ unsigned __int64 inBps,
                     _In_ unsigned __int64 outBps,
                     _In_ unsigned long inLatency,
                     _In_ unsigned long outLatency,
                     _In_ unsigned __int64 inBufferBytes,
                     _In_ unsigned __int64 outBufferBytes) {
  BOOLEAN ret = TRUE;
  KLOCK_QUEUE_HANDLE lock_handle;

  KeAcquireInStackQueuedSpinLock(&inbound_queue.lock, &lock_handle);
  inbound_queue.plr = plr;
  inbound_queue.bps = inBps;
  inbound_queue.latency = inLatency;
  inbound_queue.bufferBytes = inBufferBytes;
  inbound_queue.queued_bytes = 0;
  inbound_queue.available_bytes = INITIAL_TOKEN_COUNT;
  inbound_queue.last_tick = KeQueryPerformanceCounter(NULL);
  KeReleaseInStackQueuedSpinLock(&lock_handle);

  KeAcquireInStackQueuedSpinLock(&outbound_queue.lock, &lock_handle);
  outbound_queue.plr = plr;
  outbound_queue.bps = outBps;
  outbound_queue.latency = outLatency;
  outbound_queue.bufferBytes = outBufferBytes;
  outbound_queue.queued_bytes = 0;
  outbound_queue.available_bytes = INITIAL_TOKEN_COUNT;
  outbound_queue.last_tick = inbound_queue.last_tick;
  KeReleaseInStackQueuedSpinLock(&lock_handle);

  KeAcquireInStackQueuedSpinLock(&queue_lock, &lock_handle);
  traffic_shaping_enabled = TRUE;
  KeReleaseInStackQueuedSpinLock(&lock_handle);
  return ret;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
BOOLEAN ShaperDisable() {
  BOOLEAN ret = TRUE;
  KLOCK_QUEUE_HANDLE lock_handle;
  KeAcquireInStackQueuedSpinLock(&queue_lock, &lock_handle);
  traffic_shaping_enabled = FALSE;
  KeReleaseInStackQueuedSpinLock(&lock_handle);
  ProcessQueue(&outbound_queue);
  ProcessQueue(&inbound_queue);
  return ret;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void DestroyPacketQueues() {
  if (queues_initialized) {
    KLOCK_QUEUE_HANDLE lock_handle;
    KeAcquireInStackQueuedSpinLock(&queue_lock, &lock_handle);
    if (queues_initialized) {
      DropQueue(&inbound_queue);
      DropQueue(&outbound_queue);
    }
    queues_initialized = FALSE;

    if (timer_handle) {
      WdfTimerStop(timer_handle, FALSE);
      timer_handle = NULL;
      timer_pending = FALSE;
    }
    KeReleaseInStackQueuedSpinLock(&lock_handle);
  }
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void PacketInjectionComplete(_Inout_ void* context,
    _Inout_ NET_BUFFER_LIST* netBufferList,
    _In_ BOOLEAN dispatchLevel) {
  QUEUED_PACKET *packet = context;
  UNREFERENCED_PARAMETER(netBufferList);  
  UNREFERENCED_PARAMETER(dispatchLevel);  

  FreeQueuedPacket(packet);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void InjectPacket(QUEUED_PACKET *packet) {
  if (packet) {
    NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
    if (packet->outbound) {
      status = FwpsInjectMacSendAsync(packet->injection_handle,
                              NULL,
                              0,
                              packet->layerId,
                              packet->interfaceIndex,
                              packet->NdisPortNumber,
                              packet->netBufferList,
                              PacketInjectionComplete,
                              packet);
    } else {
      status = FwpsInjectMacReceiveAsync(packet->injection_handle,
                                NULL,
                                0,
                                packet->layerId,
                                packet->interfaceIndex,
                                packet->NdisPortNumber,
                                packet->netBufferList,
                                PacketInjectionComplete,
                                packet);
    }
    if (!NT_SUCCESS(status))
      FreeQueuedPacket(packet);
  }
}

/*-----------------------------------------------------------------------------
  Inject any packets that are due (for now, all of them)
-----------------------------------------------------------------------------*/
void ProcessQueue(PACKET_QUEUE *queue) {
  QUEUED_PACKET* packet = NULL;
  KLOCK_QUEUE_HANDLE lock;

  // process the bandwidth queue

  // Increment the available bytes if there is something in the queue
  KeAcquireInStackQueuedSpinLock(&queue->lock, &lock);
  LARGE_INTEGER frequency;
  LARGE_INTEGER now = KeQueryPerformanceCounter(&frequency);
  unsigned __int64 accumulated = ((now.QuadPart - queue->last_tick.QuadPart) * queue->bps) / (8 * frequency.QuadPart);
  queue->available_bytes += accumulated;
  if (IsListEmpty(&queue->bandwidth_queue) && queue->available_bytes > INITIAL_TOKEN_COUNT)
    queue->available_bytes = INITIAL_TOKEN_COUNT;

  // Move as many packets to the latency queue as the accumulated available bytes will allow
  do {
    packet = NULL;
    if (!IsListEmpty(&queue->bandwidth_queue)) {
      LIST_ENTRY * listEntry = RemoveHeadList(&queue->bandwidth_queue);
      packet = CONTAINING_RECORD(listEntry, QUEUED_PACKET, listEntry);
      if (!traffic_shaping_enabled || packet->packet_length <= queue->available_bytes) {
        queue->available_bytes -= packet->packet_length;
        queue->queued_bytes -= packet->packet_length;
        packet->latency_start = KeQueryPerformanceCounter(NULL);
        InsertTailList(&queue->latency_queue, &packet->listEntry);
      } else {
        InsertHeadList(&queue->bandwidth_queue, &packet->listEntry);
        packet = NULL;
      }
    }
  } while (packet);

  // Reset the byte accumulation if the bandwidth queue is empty
  queue->last_tick = KeQueryPerformanceCounter(NULL);
  KeReleaseInStackQueuedSpinLock(&lock);

  // process the latency queue
  do {
    packet = NULL;
    KeAcquireInStackQueuedSpinLock(&queue->lock, &lock);
    if (!IsListEmpty(&queue->latency_queue)) {
      now = KeQueryPerformanceCounter(&frequency);
      LIST_ENTRY * listEntry = RemoveHeadList(&queue->latency_queue);
      packet = CONTAINING_RECORD(listEntry, QUEUED_PACKET, listEntry);
      if (traffic_shaping_enabled) {
        // round to the closest ms instead of truncating by adding 1/2 of a ms to the elapsed ticks
        unsigned __int64 round = frequency.QuadPart / 2000LL;
        unsigned long elapsed_ms = (unsigned long)(((now.QuadPart - packet->latency_start.QuadPart) * 1000 + round) / frequency.QuadPart);
        if (elapsed_ms < queue->latency) {
          InsertHeadList(&queue->latency_queue, &packet->listEntry);
          packet = NULL;
        }
      }
    }
    KeReleaseInStackQueuedSpinLock(&lock);
    if (packet) {
      InjectPacket(packet);
    }
  } while (packet);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
VOID TimerEvt(_In_ WDFTIMER Timer) {
  UNREFERENCED_PARAMETER(Timer);
 
  // Reset the timer state since it is a one-shot timer
  KLOCK_QUEUE_HANDLE timer_spinlock_handle;
  KeAcquireInStackQueuedSpinLock(&timer_spinlock, &timer_spinlock_handle);
  timer_pending = FALSE;
  KeReleaseInStackQueuedSpinLock(&timer_spinlock_handle);

  ProcessQueue(&outbound_queue);
  ProcessQueue(&inbound_queue);

  // Queue up the timer to handle any pending packets
  StartPacketTimerIfNecessary();
}

/*-----------------------------------------------------------------------------
  If any packets are pending in a queue, set the timer to callback when the
  first one is due to be sent.
-----------------------------------------------------------------------------*/
VOID StartPacketTimerIfNecessary() {
  KLOCK_QUEUE_HANDLE timer_spinlock_handle, outbound_lock, inbound_lock;
  KeAcquireInStackQueuedSpinLock(&timer_spinlock, &timer_spinlock_handle);
  KeAcquireInStackQueuedSpinLock(&outbound_queue.lock, &outbound_lock);
  KeAcquireInStackQueuedSpinLock(&inbound_queue.lock, &inbound_lock);
  if (!timer_pending &&
      (!IsListEmpty(&inbound_queue.bandwidth_queue) ||
       !IsListEmpty(&outbound_queue.bandwidth_queue) ||
       !IsListEmpty(&inbound_queue.latency_queue) ||
       !IsListEmpty(&outbound_queue.latency_queue)) &&
      timer_handle) {
    // Set a 1ms tick rate as long as there are packets in one of the queues
    timer_pending = TRUE;
    WdfTimerStart(timer_handle, WDF_REL_TIMEOUT_IN_MS(1));
  }
  KeReleaseInStackQueuedSpinLock(&inbound_lock);
  KeReleaseInStackQueuedSpinLock(&outbound_lock);
  KeReleaseInStackQueuedSpinLock(&timer_spinlock_handle);
}

/*-----------------------------------------------------------------------------
  Decide if the packet should be queued and if so, place it in the appropriate
  queue and start the processing timer.
-----------------------------------------------------------------------------*/
BOOLEAN ShaperQueuePacket(_In_ const FWPS_INCOMING_VALUES* inFixedValues,
                          _In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
                          _Inout_opt_ void* layerData,
                          BOOLEAN outbound,
                          _In_ HANDLE injection_handle) {
  BOOLEAN queued = FALSE;

  PACKET_QUEUE * queue = outbound ? &outbound_queue: &inbound_queue;
  LARGE_INTEGER now = KeQueryPerformanceCounter(NULL);

  // see if we need to drop the packet because of plr
  if (traffic_shaping_enabled && queue->plr > 0) {
    // plr range is 0-10000, grab a random number capped to that range and compare
    ULONG random = RtlRandomEx(&now.LowPart) % 10000;
    if (random < queue->plr)
      return TRUE;
  }

  // see if we need to drop the packet because the buffer is "full"
  unsigned long packet_length = NET_BUFFER_DATA_LENGTH(NET_BUFFER_LIST_FIRST_NB((NET_BUFFER_LIST*)layerData));
  if (traffic_shaping_enabled && queue->bufferBytes > 0 && queue->queued_bytes + packet_length > queue->bufferBytes)
    return TRUE;

  // clone the packet and add it to the appropriate queue
  QUEUED_PACKET* packet = NULL;
  if (traffic_shaping_enabled && layerData) {
    packet = ExAllocatePoolWithTag(
                          NonPagedPool,
                          sizeof(QUEUED_PACKET),
                          QUEUED_PACKET_POOL_TAG);
    if (packet) {
      RtlZeroMemory(packet, sizeof(QUEUED_PACKET));

      // Keep track of the meta-data necessary for re-injection
      packet->injection_handle = injection_handle;
      packet->outbound = outbound;
      packet->layerId = inFixedValues->layerId;
      UINT ifIndex = outbound ? FWPS_FIELD_OUTBOUND_MAC_FRAME_ETHERNET_INTERFACE_INDEX : FWPS_FIELD_INBOUND_MAC_FRAME_ETHERNET_INTERFACE_INDEX;
      packet->interfaceIndex = inFixedValues->incomingValue[ifIndex].value.uint32;
      UINT ndisPort = outbound ? FWPS_FIELD_OUTBOUND_MAC_FRAME_ETHERNET_NDIS_PORT : FWPS_FIELD_INBOUND_MAC_FRAME_ETHERNET_NDIS_PORT;
      packet->NdisPortNumber = inFixedValues->incomingValue[ndisPort].value.uint32;

      UINT32 bytesRetreated = 0;
      if (!outbound) {
        UINT32 ethernetHeaderSize = 0;
        if(FWPS_IS_L2_METADATA_FIELD_PRESENT(inMetaValues, FWPS_L2_METADATA_FIELD_ETHERNET_MAC_HEADER_SIZE))
          ethernetHeaderSize = inMetaValues->ethernetMacHeaderSize;
        bytesRetreated = ethernetHeaderSize;
      }

      NTSTATUS status = STATUS_SUCCESS;

      // Clone the buffer
      packet->packet_length = packet_length;
      if (bytesRetreated)
        status = NdisRetreatNetBufferDataStart(NET_BUFFER_LIST_FIRST_NB((NET_BUFFER_LIST*)layerData), bytesRetreated, 0, 0);
      if (NT_SUCCESS(status))
        status = FwpsAllocateCloneNetBufferList((NET_BUFFER_LIST*)layerData, NULL, NULL, 0, &packet->netBufferList);
      if (bytesRetreated)
        NdisAdvanceNetBufferDataStart(NET_BUFFER_LIST_FIRST_NB((NET_BUFFER_LIST*)layerData), bytesRetreated, FALSE, 0);

      if (NT_SUCCESS(status)) {
        KLOCK_QUEUE_HANDLE lock;
        KeAcquireInStackQueuedSpinLock(&queue->lock, &lock);
        queue->queued_bytes += packet_length;
        InsertTailList(&queue->bandwidth_queue, &packet->listEntry);
        KeReleaseInStackQueuedSpinLock(&lock);
        packet = NULL;
        queued = TRUE;

        // Start the timer to process the packet queue if it isn't already running
        ProcessQueue(queue);
        StartPacketTimerIfNecessary();
      }
    }
  }

  if (!queued && packet)
    FreeQueuedPacket(packet);
  
  return queued;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void ShaperGetStatus(SHAPER_STATUS *status) {
  // Don't bother locking to read the values out
  status->enabled = traffic_shaping_enabled;
  status->params.inBps = inbound_queue.bps;
  status->params.outBps = outbound_queue.bps;
  status->params.inLatency = inbound_queue.latency;
  status->params.outLatency = outbound_queue.latency;
  status->params.plr = inbound_queue.plr;
  status->params.inBufferBytes = inbound_queue.bufferBytes;
  status->params.outBufferBytes = outbound_queue.bufferBytes;
  status->inQueuedBytes = inbound_queue.queued_bytes;
  status->outQueuedBytes = outbound_queue.queued_bytes;
}