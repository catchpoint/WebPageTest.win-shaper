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

// Globals
extern BOOLEAN driver_unloading;
extern HANDLE ih_out_ipv4;
extern HANDLE ih_out_ipv6;
extern HANDLE ih_out_unspecified;
extern HANDLE ih_in_ipv4;
extern HANDLE ih_in_ipv6;
extern HANDLE ih_in_unspecified;


/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
void ShaperClassify(
    _In_ const FWPS_INCOMING_VALUES* inFixedValues,
    _In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
    _Inout_opt_ void* layerData,
    _In_opt_ const void* classifyContext,
    _In_ const FWPS_FILTER* filter,
    _In_ UINT64 flowContext,
    _Inout_ FWPS_CLASSIFY_OUT* classifyOut) {

  NT_ASSERT(filter);
  NT_ASSERT(classifyOut);

  UNREFERENCED_PARAMETER(classifyContext);
  UNREFERENCED_PARAMETER(flowContext);

  // Only bother if we can actually act on the packet
  if ((classifyOut->rights & FWPS_RIGHT_ACTION_WRITE) != 0) {
    NT_ASSERT(layerData != NULL);
    _Analysis_assume_(layerData != NULL);

    BOOLEAN packet_queued = FALSE;

    // Use the correct type of injection handle depending on the protocol
    HANDLE injection_handle = NULL;
    BOOLEAN outbound = inFixedValues->layerId == FWPS_LAYER_OUTBOUND_MAC_FRAME_ETHERNET;
    UINT typeIndex = outbound ? FWPS_FIELD_OUTBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE : FWPS_FIELD_INBOUND_MAC_FRAME_ETHERNET_ETHER_TYPE;
    UINT16 etherType = inFixedValues->incomingValue[typeIndex].value.uint16;
    if (etherType == 0x86DD) {
      injection_handle = outbound ? ih_out_ipv6 : ih_in_ipv6;
    } else if (etherType == 0x8600) {
      injection_handle = outbound ? ih_out_ipv4 : ih_in_ipv4;
    } else {
      injection_handle = outbound ? ih_out_unspecified : ih_in_unspecified;
    }

    // skip packets that we re-injected, that are already being dropped or if we are quitting
    FWPS_PACKET_INJECTION_STATE packet_state = FwpsQueryPacketInjectionState(injection_handle, layerData, NULL);
    if (!driver_unloading &&
        classifyOut->actionType != FWP_ACTION_BLOCK &&
        (packet_state != FWPS_PACKET_INJECTED_BY_SELF) && 
        (packet_state != FWPS_PACKET_PREVIOUSLY_INJECTED_BY_SELF)) {
      packet_queued = ShaperQueuePacket(inFixedValues, inMetaValues, layerData, outbound, injection_handle);
    }

    if (packet_queued) {
      classifyOut->actionType = FWP_ACTION_BLOCK;
      classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
      classifyOut->flags |= FWPS_CLASSIFY_OUT_FLAG_ABSORB;
    } else {
      classifyOut->actionType = FWP_ACTION_PERMIT;
      if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
        classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
    }
  }

  return;
}

/*-----------------------------------------------------------------------------
  Empty notification function (we don't need to do anything here)
-----------------------------------------------------------------------------*/
NTSTATUS ShaperNotify(
    _In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
    _In_ const GUID* filterKey,
    _Inout_ const FWPS_FILTER* filter) {
  UNREFERENCED_PARAMETER(notifyType);
  UNREFERENCED_PARAMETER(filterKey);
  UNREFERENCED_PARAMETER(filter);
  return STATUS_SUCCESS;
}
