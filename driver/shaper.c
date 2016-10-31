// Based on the wfp inspect sample

#include <ntddk.h>

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union
#include <fwpsk.h>
#pragma warning(pop)

#include <fwpmk.h>

#include "shaper.h"
#include "utils.h"

void ShaperClassifyOutbound(
    _In_ const FWPS_INCOMING_VALUES* inFixedValues,
    _In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
    _Inout_opt_ void* layerData,
    _In_opt_ const void* classifyContext,
    _In_ const FWPS_FILTER* filter,
    _In_ UINT64 flowContext,
    _Inout_ FWPS_CLASSIFY_OUT* classifyOut) {
  UNREFERENCED_PARAMETER(inFixedValues);
  UNREFERENCED_PARAMETER(inMetaValues);
  UNREFERENCED_PARAMETER(layerData);
  UNREFERENCED_PARAMETER(classifyContext);
  UNREFERENCED_PARAMETER(flowContext);
  DbgPrint("[shaper] ShaperClassifyOutbound\n");
  NT_ASSERT(filter);
  NT_ASSERT(classifyOut);
  classifyOut->actionType = FWP_ACTION_PERMIT;
  if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
    classifyOut->rights ^= FWPS_RIGHT_ACTION_WRITE;
  return;
}

void ShaperClassifyInbound(
    _In_ const FWPS_INCOMING_VALUES* inFixedValues,
    _In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
    _Inout_opt_ void* layerData,
    _In_opt_ const void* classifyContext,
    _In_ const FWPS_FILTER* filter,
    _In_ UINT64 flowContext,
    _Inout_ FWPS_CLASSIFY_OUT* classifyOut) {
  UNREFERENCED_PARAMETER(inFixedValues);
  UNREFERENCED_PARAMETER(inMetaValues);
  UNREFERENCED_PARAMETER(layerData);
  UNREFERENCED_PARAMETER(classifyContext);
  UNREFERENCED_PARAMETER(flowContext);
  DbgPrint("[shaper] ShaperClassifyInbound\n");
  NT_ASSERT(filter);
  NT_ASSERT(classifyOut);
  classifyOut->actionType = FWP_ACTION_PERMIT;
  if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
    classifyOut->rights ^= FWPS_RIGHT_ACTION_WRITE;
  return;
}

NTSTATUS ShaperNotifyOutbound(
    _In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
    _In_ const GUID* filterKey,
    _Inout_ const FWPS_FILTER* filter) {
  UNREFERENCED_PARAMETER(notifyType);
  UNREFERENCED_PARAMETER(filterKey);
  UNREFERENCED_PARAMETER(filter);
  DbgPrint("[shaper] ShaperNotifyOutbound\n");
  return STATUS_SUCCESS;
}

NTSTATUS ShaperNotifyInbound(
    _In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
    _In_ const GUID* filterKey,
    _Inout_ const FWPS_FILTER* filter) {
  UNREFERENCED_PARAMETER(notifyType);
  UNREFERENCED_PARAMETER(filterKey);
  UNREFERENCED_PARAMETER(filter);
  DbgPrint("[shaper] ShaperNotifyInbound\n");
  return STATUS_SUCCESS;
}
