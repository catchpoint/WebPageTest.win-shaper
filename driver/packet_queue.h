#pragma once
#include "interface.h"

NTSTATUS InitializePacketQueues(WDFDEVICE timer_parent);
void DestroyPacketQueues();

BOOLEAN ShaperQueuePacket(_In_ const FWPS_INCOMING_VALUES* inFixedValues,
                          _In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
                          _Inout_opt_ void* layerData,
                          BOOLEAN outbound,
                          _In_ HANDLE injection_handle);

BOOLEAN ShaperEnable(_In_ unsigned short plr,
                     _In_ unsigned __int64 inBps,
                     _In_ unsigned __int64 outBps,
                     _In_ unsigned long inLatency,
                     _In_ unsigned long outLatency,
                     _In_ unsigned __int64 inBufferBytes,
                     _In_ unsigned __int64 outBufferBytes);

BOOLEAN ShaperDisable();
void ShaperGetStatus(SHAPER_STATUS *status);
void ShaperGetStats(SHAPER_STATS *stats);
