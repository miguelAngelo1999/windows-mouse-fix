#pragma once

//
// WmfVirtualPad.h
// UMDF2 virtual HID Precision Touchpad driver.
//

#include <windows.h>
#include <wdf.h>
#include <wdmsec.h>
#include <hidport.h>

#include "../shared/WmfIoctl.h"
#include "HidReportDescriptor.h"

// Device context — stored per WDF device object.
typedef struct _DEVICE_CONTEXT {
    WDFDEVICE       Device;
    WDFQUEUE        IoQueue;        // default I/O queue for IOCTL requests
    WDFSPINLOCK     ReportLock;     // protects LastReport
    WMF_PTP_REPORT  LastReport;     // most recent report submitted by user-mode
    BOOLEAN         HasReport;      // TRUE once at least one report has arrived
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

// Driver callbacks
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD WmfEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL WmfEvtIoDeviceControl;

// HID mini-driver callbacks
NTSTATUS WmfGetReportDescriptor(IN WDFDEVICE Device, IN WDFREQUEST Request);
NTSTATUS WmfGetDeviceAttributes(IN WDFDEVICE Device, IN WDFREQUEST Request);
NTSTATUS WmfGetFeatureReport(IN WDFDEVICE Device, IN WDFREQUEST Request);
NTSTATUS WmfReadReport(IN WDFDEVICE Device, IN WDFREQUEST Request);

// Called from IOCTL handler to complete a pending HID read with new report data
VOID WmfCompleteReadRequest(IN WDFDEVICE Device, IN WMF_PTP_REPORT* Report);

// Manual queue for pending HID read requests (extern so WmfEvtDeviceAdd can init it)
extern WDFQUEUE g_ReadQueue;
