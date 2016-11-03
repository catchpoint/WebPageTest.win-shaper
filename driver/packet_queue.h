#pragma once

typedef struct {
  LIST_ENTRY packets;
  KSPIN_LOCK lock;
} PACKET_QUEUE;

extern PACKET_QUEUE inbound_queue;
extern PACKET_QUEUE outbound_queue;

NTSTATUS InitializePacketQueues(WDFDEVICE timer_parent);
void DestroyPacketQueues();

BOOLEAN ShaperQueuePacket(_In_ const FWPS_INCOMING_VALUES* inFixedValues,
                          _In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
                          _Inout_opt_ void* layerData,
                          BOOLEAN outbound,
                          _In_ HANDLE injection_handle);
