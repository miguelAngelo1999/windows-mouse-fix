#pragma once
/*
 * WmfVirtualPad.h
 * UMDF2 virtual HID Precision Touchpad driver.
 *
 * Based on the VHidMini2 UMDF2 sample from the Windows Driver Kit.
 */

#include <windows.h>
#include <wdf.h>
#include <hidport.h>
#include <initguid.h>

#include "../shared/WmfIoctl.h"
#include "HidReportDescriptor.h"

// ---- Device context ----
typedef struct _DEVICE_CONTEXT {
    WDFDEVICE           Device;
    WDFQUEUE            DefaultQueue;
    WDFQUEUE            ManualQueue;      // pending HID read requests
    WDFSPINLOCK         ReportLock;
    WMF_PTP_REPORT      LastReport;
    BOOLEAN             HasReport;
    UCHAR               InputMode;        // 0x00=mouse, 0x03=touchpad
    HID_DESCRIPTOR      HidDescriptor;
    HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

// ---- Queue context ----
typedef struct _QUEUE_CONTEXT {
    WDFQUEUE            Queue;
    PDEVICE_CONTEXT     DeviceContext;
} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, GetQueueContext)

// ---- VID/PID ----
#define WMF_VID     0x045E   // Microsoft
#define WMF_PID     0x07A5   // Virtual Precision Touchpad
#define WMF_VERSION 0x0100

// ---- Driver callbacks ----
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD WmfEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL WmfEvtIoDeviceControl;

// ---- Internal helpers ----
NTSTATUS WmfCreateDefaultQueue(_In_ WDFDEVICE Device, _Out_ WDFQUEUE* Queue);
NTSTATUS WmfCreateManualQueue(_In_ WDFDEVICE Device, _Out_ WDFQUEUE* Queue);

NTSTATUS WmfGetHidDescriptor(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request);
NTSTATUS WmfGetReportDescriptor(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request);
NTSTATUS WmfGetDeviceAttributes(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request);
NTSTATUS WmfReadReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request, _Out_ BOOLEAN* CompleteRequest);
NTSTATUS WmfGetFeature(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request);
NTSTATUS WmfSetFeature(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request);
NTSTATUS WmfGetInputReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request);
NTSTATUS WmfWriteReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request);
NTSTATUS WmfGetString(_In_ WDFREQUEST Request);
NTSTATUS WmfGetIndexedString(_In_ WDFREQUEST Request);

NTSTATUS WmfHandleSubmitReport(_In_ PDEVICE_CONTEXT DevCtx, _In_ WDFREQUEST Request);
VOID     WmfCompleteReadRequest(_In_ PDEVICE_CONTEXT DevCtx, _In_ PUCHAR ReportBuffer, _In_ ULONG ReportSize);

NTSTATUS RequestCopyFromBuffer(
    _In_ WDFREQUEST Request,
    _In_ PVOID SourceBuffer,
    _In_ size_t NumBytesToCopyFrom);
