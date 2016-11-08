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

/*-----------------------------------------------------------------------------
  GUIDs
-----------------------------------------------------------------------------*/

// c9ea094d-b304-4f7c-bfc6-608b0ecc5447
DEFINE_GUID(
    SHAPER_OUTBOUND_CALLOUT,
    0xc9ea094d,
    0xb304,
    0x4f7c,
    0xbf, 0xc6, 0x60, 0x8b, 0x0e, 0xcc, 0x54, 0x47
);
// 4fbb02a9-0e93-471e-a04c-d1439b5d402e
DEFINE_GUID(
    SHAPER_INBOUND_CALLOUT,
    0x4fbb02a9,
    0x0e93,
    0x471e,
    0xa0, 0x4c, 0xd1, 0x43, 0x9b, 0x5d, 0x40, 0x2e
);
// ac63c367-b14f-483a-89fc-196c98eec1ad
DEFINE_GUID(
    SHAPER_SUBLAYER,
    0xac63c367,
    0xb14f,
    0x483a,
    0x89, 0xfc, 0x19, 0x6c, 0x98, 0xee, 0xc1, 0xad
);

/*-----------------------------------------------------------------------------
  Globals
-----------------------------------------------------------------------------*/
DEVICE_OBJECT* wdm_device = NULL;
BOOLEAN driver_unloading = FALSE;
HANDLE engine_handle = NULL;
UINT32 outbound_callout_id = 0, inbound_callout_id = 0;

// packet injection handles for inbound/outbound IPv4/IPv6/Unspecified
HANDLE ih_out_ipv4 = NULL;
HANDLE ih_out_ipv6 = NULL;
HANDLE ih_out_unspecified = NULL;
HANDLE ih_in_ipv4 = NULL;
HANDLE ih_in_ipv6 = NULL;
HANDLE ih_in_unspecified = NULL;

/*-----------------------------------------------------------------------------
  Forward declarations
-----------------------------------------------------------------------------*/
NTSTATUS ShaperInitDriverObjects(
    _Inout_ DRIVER_OBJECT* driverObject,
    _In_ const UNICODE_STRING* registryPath,
    _Out_ WDFDRIVER* pDriver,
    _Out_ WDFDEVICE* pDevice);
NTSTATUS RegisterCallouts(_Inout_ void* deviceObject);
void Cleanup(void);

/******************************************************************************
******************************************************************************/

/*-----------------------------------------------------------------------------
  Main driver entry point
-----------------------------------------------------------------------------*/
NTSTATUS DriverEntry(DRIVER_OBJECT* driverObject, UNICODE_STRING* registryPath) {
  NTSTATUS status;
  WDFDRIVER driver;
  WDFDEVICE device;

  // Request NX Non-Paged Pool when available
  ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

  status = ShaperInitDriverObjects(driverObject, registryPath, &driver, &device);
  if (NT_SUCCESS(status)) {
    FwpsInjectionHandleCreate(AF_INET, FWPS_INJECTION_TYPE_L2, &ih_out_ipv4);
    FwpsInjectionHandleCreate(AF_INET6, FWPS_INJECTION_TYPE_L2, &ih_out_ipv6);
    FwpsInjectionHandleCreate(AF_UNSPEC, FWPS_INJECTION_TYPE_L2, &ih_out_unspecified);
    FwpsInjectionHandleCreate(AF_INET, FWPS_INJECTION_TYPE_L2, &ih_in_ipv4);
    FwpsInjectionHandleCreate(AF_INET6, FWPS_INJECTION_TYPE_L2, &ih_in_ipv6);
    FwpsInjectionHandleCreate(AF_UNSPEC, FWPS_INJECTION_TYPE_L2, &ih_in_unspecified);
  }

  if (NT_SUCCESS(status))
    status = InitializePacketQueues(device);

  if (NT_SUCCESS(status))
    status = RegisterCallouts(wdm_device);

  if (!NT_SUCCESS(status))
    Cleanup();

  return status;
};

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void Cleanup(void) {
  DestroyPacketQueues();

  if (engine_handle) {
    FwpmEngineClose(engine_handle);
    engine_handle = NULL;
  }

  if (outbound_callout_id) {
    FwpsCalloutUnregisterById(outbound_callout_id);
    outbound_callout_id = 0;
  }
  if (inbound_callout_id) {
    FwpsCalloutUnregisterById(inbound_callout_id);
    inbound_callout_id = 0;
  }
  if (ih_out_ipv4) {
    FwpsInjectionHandleDestroy(ih_out_ipv4);
    ih_out_ipv4 = NULL;
  }
  if (ih_out_ipv6) {
    FwpsInjectionHandleDestroy(ih_out_ipv6);
    ih_out_ipv6 = NULL;
  }
  if (ih_out_unspecified) {
    FwpsInjectionHandleDestroy(ih_out_unspecified);
    ih_out_unspecified = NULL;
  }
  if (ih_in_ipv4) {
    FwpsInjectionHandleDestroy(ih_in_ipv4);
    ih_in_ipv4 = NULL;
  }
  if (ih_in_ipv6) {
    FwpsInjectionHandleDestroy(ih_in_ipv6);
    ih_in_ipv6 = NULL;
  }
  if (ih_in_unspecified) {
    FwpsInjectionHandleDestroy(ih_in_unspecified);
    ih_in_unspecified = NULL;
  }
}

/*-----------------------------------------------------------------------------
  Called when the driver is unloading
-----------------------------------------------------------------------------*/
_Function_class_(EVT_WDF_DRIVER_UNLOAD)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
void ShaperEvtDriverUnload(_In_ WDFDRIVER driverObject) {
  UNREFERENCED_PARAMETER(driverObject);
  driver_unloading = TRUE;

  Cleanup();
}


/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
VOID EvtDeviceIOCtl(_In_ WDFQUEUE Queue,
                    _In_ WDFREQUEST Request,
                    _In_ size_t OutputBufferLength,
                    _In_ size_t InputBufferLength,
                    _In_ ULONG IoControlCode) {
  UNREFERENCED_PARAMETER(Queue);
  UNREFERENCED_PARAMETER(Request);
  UNREFERENCED_PARAMETER(OutputBufferLength);
  UNREFERENCED_PARAMETER(InputBufferLength);
  NTSTATUS status = STATUS_SUCCESS;
  switch (IoControlCode) {
    case SHAPER_IOCTL_DISABLE: {
      if (!ShaperDisable())
        status = STATUS_INVALID_PARAMETER;
      break;
    }
    case SHAPER_IOCTL_ENABLE: {
      if (InputBufferLength >= sizeof(SHAPER_PARAMS)) {
        WDFMEMORY pMemory;
        SHAPER_PARAMS* settings;
        status = WdfRequestRetrieveInputMemory(Request, &pMemory);
        if (NT_SUCCESS(status)) {
          settings = (SHAPER_PARAMS *)WdfMemoryGetBuffer(pMemory, NULL);
          if (settings) {
            if (!ShaperEnable(settings->plr,
                              settings->inBps,
                              settings->outBps,
                              settings->inLatency,
                              settings->outLatency,
                              settings->inBufferBytes,
                              settings->outBufferBytes)) {
              status = STATUS_INVALID_PARAMETER;
            }
          } else {
            status = STATUS_INVALID_PARAMETER;
          }
        }
      } else {
        status = STATUS_INVALID_PARAMETER;
      }
      break;
    }
    default: {
      DbgPrint("[shaper] IOCTL unknown command - %lu\n", IoControlCode);
      status = STATUS_INVALID_PARAMETER;
    }
  }
  WdfRequestComplete(Request, status);
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
NTSTATUS ShaperInitDriverObjects(
    _Inout_ DRIVER_OBJECT* driverObject,
    _In_ const UNICODE_STRING* registryPath,
    _Out_ WDFDRIVER* pDriver,
    _Out_ WDFDEVICE* pDevice) {
  NTSTATUS status;
  WDF_DRIVER_CONFIG config;
  PWDFDEVICE_INIT pInit = NULL;
  DECLARE_CONST_UNICODE_STRING(ntDeviceName, SHAPER_DEVICE_NAME);
  DECLARE_CONST_UNICODE_STRING(symbolicName, SHAPER_SYMBOLIC_NAME);

  WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);

  config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
  config.EvtDriverUnload = ShaperEvtDriverUnload;

  status = WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES,
                           &config, pDriver);

  if (NT_SUCCESS(status)) {
    pInit = WdfControlDeviceInitAllocate(*pDriver, &SDDL_DEVOBJ_KERNEL_ONLY);
    if (pInit) {
      WdfDeviceInitSetDeviceType(pInit, FILE_DEVICE_NETWORK);
      WdfDeviceInitSetCharacteristics(pInit, FILE_DEVICE_SECURE_OPEN, FALSE);

      // Set up the interface for CreateFile/WriteFile from user mode
      status = WdfDeviceInitAssignName(pInit, &ntDeviceName);
      if (NT_SUCCESS(status))
        WdfDeviceInitAssignSDDLString(pInit, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
      if (NT_SUCCESS(status))
        status = WdfDeviceCreate(&pInit, WDF_NO_OBJECT_ATTRIBUTES, pDevice);
      if (NT_SUCCESS(status))
        status = WdfDeviceCreateSymbolicLink(*pDevice, &symbolicName);
      if (NT_SUCCESS(status)) {
        WDF_IO_QUEUE_CONFIG queueConfig;
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
        queueConfig.EvtIoDeviceControl = EvtDeviceIOCtl;
        status = WdfIoQueueCreate(*pDevice, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
      }
      if (NT_SUCCESS(status))
        wdm_device = WdfDeviceWdmGetDeviceObject(*pDevice);

      if (NT_SUCCESS(status))
        WdfControlFinishInitializing(*pDevice);
      else
        WdfDeviceInitFree(pInit);
    } else {
      status = STATUS_INSUFFICIENT_RESOURCES;
    }
  }

  return status;
}

NTSTATUS AddFilter(
    _In_ const wchar_t* filterName,
    _In_ const wchar_t* filterDesc,
    _In_ UINT64 context,
    _In_ const GUID* layerKey,
    _In_ const GUID* calloutKey) {
  NTSTATUS status = STATUS_SUCCESS;

  FWPM_FILTER filter = {0};
  FWPM_FILTER_CONDITION filterConditions[3] = {0}; 
  UINT conditionIndex;

  filter.layerKey = *layerKey;
  filter.displayData.name = (wchar_t*)filterName;
  filter.displayData.description = (wchar_t*)filterDesc;

  filter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
  filter.action.calloutKey = *calloutKey;
  filter.filterCondition = filterConditions;
  filter.subLayerKey = SHAPER_SUBLAYER;
  filter.weight.type = FWP_EMPTY; // auto-weight.
  filter.rawContext = context;

  conditionIndex = 0;

  filter.numFilterConditions = conditionIndex;

  status = FwpmFilterAdd(engine_handle, &filter, NULL, NULL);

  return status;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
NTSTATUS RegisterCallout(
    _In_ const GUID* layerKey,
    _In_ const GUID* calloutKey,
    _Inout_ void* deviceObject,
    _Out_ UINT32* calloutId,
    _In_ BOOLEAN is_outbound) {
  NTSTATUS status = STATUS_SUCCESS;

  FWPS_CALLOUT sCallout = {0};
  FWPM_CALLOUT mCallout = {0};

  FWPM_DISPLAY_DATA displayData = {0};

  sCallout.calloutKey = *calloutKey;
  sCallout.classifyFn = ShaperClassify;
  sCallout.notifyFn = ShaperNotify;

  status = FwpsCalloutRegister(deviceObject, &sCallout, calloutId);
  if (NT_SUCCESS(status)) {
    displayData.name = is_outbound ? L"Shaper Outbound Callout" : L"Shaper Inbound Callout";
    displayData.description = is_outbound ? L"Traffic-shape outbound traffic" : L"Traffic-shape inbound traffic";

    mCallout.calloutKey = *calloutKey;
    mCallout.displayData = displayData;
    mCallout.applicableLayer = *layerKey;

    status = FwpmCalloutAdd(engine_handle, &mCallout, NULL, NULL);
    if (NT_SUCCESS(status)) {
      status = AddFilter(
                  is_outbound ? L"Traffic Shaper Filter (Outbound)" : L"Traffic Shaper Filter (Inbound)",
                  is_outbound ? L"Traffic-shape outbound traffic" : L"Traffic-shape inbound traffic",
                  0, layerKey, calloutKey);
    }

    if (!NT_SUCCESS(status)) {
      FwpsCalloutUnregisterById(*calloutId);
      *calloutId = 0;
    }
  }

  return status;
}

/*-----------------------------------------------------------------------------
  Register the callouts and filters for intercepting MAC layer traffic
-----------------------------------------------------------------------------*/
NTSTATUS RegisterCallouts(_Inout_ void* deviceObject) {
  NTSTATUS status = STATUS_SUCCESS;
  FWPM_SUBLAYER shaper_sublayer;

  FWPM_SESSION session = {0};
  session.flags = FWPM_SESSION_FLAG_DYNAMIC;

  status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, &session, &engine_handle);
  if (NT_SUCCESS(status)) {
    status = FwpmTransactionBegin(engine_handle, 0);
    if (NT_SUCCESS(status)) {
      RtlZeroMemory(&shaper_sublayer, sizeof(FWPM_SUBLAYER)); 
      shaper_sublayer.subLayerKey = SHAPER_SUBLAYER;
      shaper_sublayer.displayData.name = L"Traffic Shaper Sub-Layer";
      shaper_sublayer.displayData.description = L"Sub-Layer for use by Traffic Shaper callouts";
      shaper_sublayer.flags = 0;
      shaper_sublayer.weight = 0;

      status = FwpmSubLayerAdd(engine_handle, &shaper_sublayer, NULL);
      if (NT_SUCCESS(status)) {
        status = RegisterCallout(&FWPM_LAYER_OUTBOUND_MAC_FRAME_ETHERNET,
                    &SHAPER_OUTBOUND_CALLOUT, deviceObject, &outbound_callout_id, TRUE);
        if (NT_SUCCESS(status)) {
          status = RegisterCallout(&FWPM_LAYER_INBOUND_MAC_FRAME_ETHERNET,
                      &SHAPER_INBOUND_CALLOUT, deviceObject, &inbound_callout_id, FALSE);
          if (NT_SUCCESS(status)) {
            status = FwpmTransactionCommit(engine_handle);
          }
        }
      }

      // Cleanup the failed transaction
      if (!NT_SUCCESS(status)) {
        FwpmTransactionAbort(engine_handle);
        _Analysis_assume_lock_not_held_(engine_handle); // Potential leak if "FwpmTransactionAbort" fails
      }
    }

    // Cleanup the engine handle if something went wrong
    if (!NT_SUCCESS(status)) {
      FwpmEngineClose(engine_handle);
      engine_handle = NULL;
    }
  }

  return status;
}
