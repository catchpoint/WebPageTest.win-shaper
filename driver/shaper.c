// Based on the wfp inspect sample

#include <ntddk.h>
#include <wdf.h>

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union
#include <fwpsk.h>
#pragma warning(pop)

#include <fwpmk.h>

#include "shaper.h"
#include "utils.h"

// Globals
extern HANDLE injection_handle;
extern KSPIN_LOCK outbound_list_spinlock;
extern KSPIN_LOCK inbound_list_spinlock;
extern WDFTIMER timer_handle;
extern KSPIN_LOCK timer_spinlock;
extern BOOLEAN timer_pending;

// Forward declarations
VOID StartPacketTimerIfNecessary();

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
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

  UNREFERENCED_PARAMETER(inMetaValues);
  UNREFERENCED_PARAMETER(classifyContext);
  UNREFERENCED_PARAMETER(flowContext);

  // Only bother if we can actually act on the packet
  if ((classifyOut->rights & FWPS_RIGHT_ACTION_WRITE) != 0) {
    NT_ASSERT(layerData != NULL);
    _Analysis_assume_(layerData != NULL);

    BOOLEAN intercept_packet = FALSE;
    BOOLEAN outbound = inFixedValues->layerId == FWPS_LAYER_OUTBOUND_MAC_FRAME_ETHERNET;

    // skip packets that we re-injected
    FWPS_PACKET_INJECTION_STATE packet_state = FwpsQueryPacketInjectionState(injection_handle, layerData, NULL);
    if ((packet_state != FWPS_PACKET_INJECTED_BY_SELF) && 
        (packet_state != FWPS_PACKET_PREVIOUSLY_INJECTED_BY_SELF)) {
      //PNET_BUFFER_LIST rawData = (PNET_BUFFER_LIST)layerData;
      if (outbound)
        DbgPrint("[shaper] ==>\n");
      else
        DbgPrint("[shaper] <==\n");
      StartPacketTimerIfNecessary();
    }

    if (intercept_packet) {
    } else {
      classifyOut->actionType = FWP_ACTION_PERMIT;
      if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
        classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
    }
  }

  return;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
VOID ShaperTimerEvt(_In_ WDFTIMER Timer) {
  UNREFERENCED_PARAMETER(Timer);
  DbgPrint("[shaper] ShaperTimerEvt\n");
 
  // Reset the timer state since it is a one-shot timer
  KLOCK_QUEUE_HANDLE timer_spinlock_handle;
  KeAcquireInStackQueuedSpinLock(&timer_spinlock, &timer_spinlock_handle);
  timer_pending = FALSE;
  KeReleaseInStackQueuedSpinLock(&timer_spinlock_handle);

  // TODO: add packet queue processing

  // Queue up the timer to handle any pending packets
  StartPacketTimerIfNecessary();
}

/*-----------------------------------------------------------------------------
  If any packets are pending in a queue, set the timer to callback when the
  first one is due to be sent.
-----------------------------------------------------------------------------*/
VOID StartPacketTimerIfNecessary() {
  KLOCK_QUEUE_HANDLE timer_spinlock_handle;
  KeAcquireInStackQueuedSpinLock(&timer_spinlock, &timer_spinlock_handle);
  if (!timer_pending && timer_handle) {
    // for now always set it on a 1-second interval until the queues are actually hooked up
    timer_pending = TRUE;
    WdfTimerStart(timer_handle, WDF_REL_TIMEOUT_IN_MS(1000));
  }
  KeReleaseInStackQueuedSpinLock(&timer_spinlock_handle);
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
