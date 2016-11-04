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

// packet queues
static BOOLEAN queues_initialized = FALSE;
static KSPIN_LOCK queue_lock = 0;
PACKET_QUEUE inbound_queue;
PACKET_QUEUE outbound_queue;

//
// QUEUED_PACKET is the object type we used to store all information
// needed for out-of-band packet re-injection. This type
// also points back to the flow context the packet belongs to.
typedef struct {
   LIST_ENTRY listEntry;
   HANDLE injection_handle;
   BOOLEAN outbound;

   // Data fields for packet re-injection.
   UINT16 layerId;
   IF_INDEX interfaceIndex;
   NDIS_PORT_NUMBER NdisPortNumber;
   NET_BUFFER_LIST *netBufferList;
} QUEUED_PACKET;

// Forward declarations
VOID TimerEvt(_In_ WDFTIMER Timer);
VOID StartPacketTimerIfNecessary();

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
NTSTATUS InitializePacketQueues(WDFDEVICE timer_parent) {
  NTSTATUS status = STATUS_SUCCESS;

  KeInitializeSpinLock(&queue_lock);   

  queues_initialized = TRUE;

  KeInitializeSpinLock(&timer_spinlock);   
  KeInitializeSpinLock(&inbound_queue.lock);
  InitializeListHead(&inbound_queue.packets);
  KeInitializeSpinLock(&outbound_queue.lock);   
  InitializeListHead(&outbound_queue.packets);

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
void DrainQueue(PACKET_QUEUE *queue) {
  KLOCK_QUEUE_HANDLE lock;
  KeAcquireInStackQueuedSpinLock(&queue->lock, &lock);
  while (!IsListEmpty(&queue->packets)) {
    LIST_ENTRY * listEntry = RemoveHeadList(&queue->packets);
    QUEUED_PACKET *packet = CONTAINING_RECORD(listEntry, QUEUED_PACKET, listEntry);
    FreeQueuedPacket(packet);
  }
  KeReleaseInStackQueuedSpinLock(&lock);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void DestroyPacketQueues() {
  if (queues_initialized) {
    KLOCK_QUEUE_HANDLE lock_handle;
    KeAcquireInStackQueuedSpinLock(&queue_lock, &lock_handle);
    if (queues_initialized) {
      DrainQueue(&inbound_queue);
      DrainQueue(&outbound_queue);
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
  Inject any packets that are due (for now, all of them)
-----------------------------------------------------------------------------*/
void ProcessQueue(PACKET_QUEUE *queue) {
  QUEUED_PACKET* packet = NULL;
  KLOCK_QUEUE_HANDLE lock;
  DbgPrint("[shaper] Processing Queue\n");
  do {
    packet = NULL;

    KeAcquireInStackQueuedSpinLock(&queue->lock, &lock);
    if (!IsListEmpty(&queue->packets)) {
      LIST_ENTRY * listEntry = RemoveHeadList(&queue->packets);
      packet = CONTAINING_RECORD(listEntry, QUEUED_PACKET, listEntry);
    } else {
      DbgPrint("[shaper] queue is empty\n");
    }
    KeReleaseInStackQueuedSpinLock(&lock);

    if (packet) {
      NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
      if (packet->outbound) {
        DbgPrint("[shaper] Injecting outbound packet 0x%p\n", packet);
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
        DbgPrint("[shaper] Injecting inbound packet 0x%p\n", packet);
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
  } while (packet);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
VOID TimerEvt(_In_ WDFTIMER Timer) {
  UNREFERENCED_PARAMETER(Timer);
  DbgPrint("[shaper] ShaperTimerEvt\n");
 
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
      (!IsListEmpty(&inbound_queue.packets) || !IsListEmpty(&outbound_queue.packets)) &&
      timer_handle) {
    // for now always set it on a 100ms interval until the configuration is actually hooked up
    timer_pending = TRUE;
    WdfTimerStart(timer_handle, WDF_REL_TIMEOUT_IN_MS(100));
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

  // clone the packet and add it to the appropriate queue
  QUEUED_PACKET* packet = NULL;
  if (layerData) {
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
      if (bytesRetreated)
        status = NdisRetreatNetBufferDataStart(NET_BUFFER_LIST_FIRST_NB((NET_BUFFER_LIST*)layerData), bytesRetreated, 0, 0);
      if (NT_SUCCESS(status))
        status = FwpsAllocateCloneNetBufferList((NET_BUFFER_LIST*)layerData, NULL, NULL, 0, &packet->netBufferList);
      if (bytesRetreated)
        NdisAdvanceNetBufferDataStart(NET_BUFFER_LIST_FIRST_NB((NET_BUFFER_LIST*)layerData), bytesRetreated, FALSE, 0);

      if (NT_SUCCESS(status)) {
        if (outbound)
          DbgPrint("[shaper] Queuing outbound packet 0x%p\n", packet);
        else
          DbgPrint("[shaper] Queuing inbound packet 0x%p\n", packet);
        PACKET_QUEUE * queue = outbound ? &outbound_queue: &inbound_queue;
        KLOCK_QUEUE_HANDLE lock;
        KeAcquireInStackQueuedSpinLock(&queue->lock, &lock);
        InsertTailList(&queue->packets, &packet->listEntry);
        packet = NULL;
        KeReleaseInStackQueuedSpinLock(&lock);
        queued = TRUE;

        // Start the timer to process the packet queue if it isn't already running
        StartPacketTimerIfNecessary();
      }
    }
  }

  if (!queued && packet)
    FreeQueuedPacket(packet);
  
  return queued;
}

