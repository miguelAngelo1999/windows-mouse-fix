/*
 * WmfVirtualPad.c
 * UMDF2 virtual HID Precision Touchpad driver.
 *
 * Architecture:
 *   - Creates a virtual HID device that Windows recognizes as a PTP touchpad
 *   - Exposes \\.\WmfVirtualPad for user-mode app to submit PTP reports via IOCTL
 *   - Forwards raw HID report bytes to hidclass.sys / PrecisionTouchPad.sys
 *
 * Build requirements: WDK + Visual Studio, UMDF2 target
 */

#include "WmfVirtualPad.h"

// ---------------------------------------------------------------------------
// DriverEntry
// ---------------------------------------------------------------------------

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, WmfEvtDeviceAdd);

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );

    return status;
}

// ---------------------------------------------------------------------------
// WmfEvtDeviceAdd — called when PnP manager adds our device
// ---------------------------------------------------------------------------

NTSTATUS
WmfEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS            status;
    WDFDEVICE           device;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_CONTEXT     deviceContext;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE            queue;
    UNICODE_STRING      symLink;

    UNREFERENCED_PARAMETER(Driver);

    // Mark this as a HID device
    // The HID class driver will call our callbacks for descriptor/attribute requests
    status = WdfFdoInitSetFilter(DeviceInit);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Set device attributes — security descriptor allows user-mode access
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Get device context
    deviceContext = DeviceGetContext(device);
    deviceContext->Device = device;
    deviceContext->HasReport = FALSE;
    RtlZeroMemory(&deviceContext->LastReport, sizeof(WMF_PTP_REPORT));

    // Create spinlock for report access
    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &deviceContext->ReportLock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Create default I/O queue (sequential, handles IOCTL from user-mode app)
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = WmfEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    deviceContext->IoQueue = queue;

    // Create manual queue for pending HID read requests from hidclass.sys
    // These are pended here and completed when user-mode submits a report via IOCTL
    {
        WDF_IO_QUEUE_CONFIG manualQueueConfig;
        WDF_IO_QUEUE_CONFIG_INIT(&manualQueueConfig, WdfIoQueueDispatchManual);
        status = WdfIoQueueCreate(device, &manualQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &g_ReadQueue);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    // Create symbolic link so user-mode can open \\.\WmfVirtualPad
    RtlInitUnicodeString(&symLink, WMF_DEVICE_SYMLINK_KERNEL);
    status = WdfDeviceCreateSymbolicLink(device, &symLink);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Expose device interface for user-mode discovery
    // (alternative to symbolic link — both are provided for compatibility)
    status = WdfDeviceCreateDeviceInterface(
        device,
        (LPGUID)&GUID_DEVINTERFACE_HID,
        NULL
    );
    // Non-fatal if this fails — symbolic link is sufficient

    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// WmfEvtIoDeviceControl — handles IOCTLs from user-mode app and HID class
// ---------------------------------------------------------------------------

VOID
WmfEvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    NTSTATUS        status = STATUS_SUCCESS;
    WDFDEVICE       device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT ctx    = DeviceGetContext(device);

    UNREFERENCED_PARAMETER(OutputBufferLength);

    switch (IoControlCode) {

    // ---- User-mode app submits a PTP report ----
    case IOCTL_WMF_SUBMIT_REPORT:
    {
        WMF_PTP_REPORT* inputReport = NULL;
        size_t          inputSize   = 0;

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(WMF_PTP_REPORT),
            (PVOID*)&inputReport,
            &inputSize
        );

        if (!NT_SUCCESS(status) || inputSize < sizeof(WMF_PTP_REPORT)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (inputReport->report_id != 0x01) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        // Store the report under spinlock
        WdfSpinLockAcquire(ctx->ReportLock);
        RtlCopyMemory(&ctx->LastReport, inputReport, sizeof(WMF_PTP_REPORT));
        ctx->HasReport = TRUE;
        WdfSpinLockRelease(ctx->ReportLock);

        // Complete any pending HID read request with this report
        // (HID class driver queues a read; we complete it with our data)
        WmfCompleteReadRequest(device, inputReport);

        status = STATUS_SUCCESS;
        break;
    }

    // ---- HID class driver requests the report descriptor ----
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        status = WmfGetReportDescriptor(device, Request);
        return; // WmfGetReportDescriptor completes the request

    // ---- HID class driver requests device attributes ----
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        status = WmfGetDeviceAttributes(device, Request);
        return;

    // ---- HID class driver requests a feature report ----
    case IOCTL_HID_GET_FEATURE:
        status = WmfGetFeatureReport(device, Request);
        return;

    // ---- HID class driver issues a read (waiting for input) ----
    case IOCTL_HID_READ_REPORT:
        status = WmfReadReport(device, Request);
        return;

    // ---- HID class driver sets a feature (we accept but ignore) ----
    case IOCTL_HID_SET_FEATURE:
        status = STATUS_SUCCESS;
        break;

    // ---- HID class driver writes a report (we accept but ignore) ----
    case IOCTL_HID_WRITE_REPORT:
        status = STATUS_SUCCESS;
        break;

    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    WdfRequestComplete(Request, status);
}

// ---------------------------------------------------------------------------
// WmfGetReportDescriptor — returns our PTP HID descriptor to hidclass.sys
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetReportDescriptor(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request
)
{
    NTSTATUS  status;
    PVOID     outputBuffer;
    size_t    outputSize;

    UNREFERENCED_PARAMETER(Device);

    status = WdfRequestRetrieveOutputBuffer(
        Request,
        WMF_HID_REPORT_DESCRIPTOR_SIZE,
        &outputBuffer,
        &outputSize
    );

    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return status;
    }

    if (outputSize < WMF_HID_REPORT_DESCRIPTOR_SIZE) {
        WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(outputBuffer, kWmfHidReportDescriptor, WMF_HID_REPORT_DESCRIPTOR_SIZE);
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, WMF_HID_REPORT_DESCRIPTOR_SIZE);
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// WmfGetDeviceAttributes — returns VID/PID/version to hidclass.sys
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetDeviceAttributes(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request
)
{
    NTSTATUS              status;
    PHID_DEVICE_ATTRIBUTES attrs;
    size_t                outputSize;

    UNREFERENCED_PARAMETER(Device);

    status = WdfRequestRetrieveOutputBuffer(
        Request,
        sizeof(HID_DEVICE_ATTRIBUTES),
        (PVOID*)&attrs,
        &outputSize
    );

    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return status;
    }

    attrs->Size          = sizeof(HID_DEVICE_ATTRIBUTES);
    attrs->VendorID      = 0x045E;  // Microsoft
    attrs->ProductID     = 0x07A5;  // Virtual Precision Touchpad
    attrs->VersionNumber = 0x0100;  // v1.0

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(HID_DEVICE_ATTRIBUTES));
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// WmfGetFeatureReport — returns contact count maximum
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetFeatureReport(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request
)
{
    NTSTATUS            status;
    PHID_XFER_PACKET    xferPacket;
    size_t              outputSize;
    WMF_FEATURE_REPORT* featureReport;

    UNREFERENCED_PARAMETER(Device);

    status = WdfRequestRetrieveOutputBuffer(
        Request,
        sizeof(HID_XFER_PACKET),
        (PVOID*)&xferPacket,
        &outputSize
    );

    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(Request, status);
        return status;
    }

    if (xferPacket->reportBufferLen < WMF_HID_FEATURE_REPORT_SIZE) {
        WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
        return STATUS_BUFFER_TOO_SMALL;
    }

    featureReport = (WMF_FEATURE_REPORT*)xferPacket->reportBuffer;

    if (xferPacket->reportId == 0x02) {
        featureReport->report_id        = 0x02;
        featureReport->contact_count_max = 5;
    } else if (xferPacket->reportId == 0x03) {
        // Pad type: 0x08 = precision touchpad
        featureReport->report_id        = 0x03;
        featureReport->contact_count_max = 0x08;
    } else {
        WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, WMF_HID_FEATURE_REPORT_SIZE);
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// WmfReadReport — pends or immediately completes a HID read request
// ---------------------------------------------------------------------------

// We keep a queue of pending read requests from hidclass.sys.
// When user-mode submits a report via IOCTL, we complete the oldest pending read.

static WDFQUEUE g_ReadQueue = NULL;

NTSTATUS
WmfReadReport(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request
)
{
    NTSTATUS        status;
    PDEVICE_CONTEXT ctx = DeviceGetContext(Device);

    // If we have a report ready, complete immediately
    WdfSpinLockAcquire(ctx->ReportLock);
    if (ctx->HasReport) {
        PVOID  outputBuffer;
        size_t outputSize;

        status = WdfRequestRetrieveOutputBuffer(
            Request,
            WMF_HID_INPUT_REPORT_SIZE,
            &outputBuffer,
            &outputSize
        );

        if (NT_SUCCESS(status) && outputSize >= WMF_HID_INPUT_REPORT_SIZE) {
            RtlCopyMemory(outputBuffer, &ctx->LastReport, WMF_HID_INPUT_REPORT_SIZE);
            ctx->HasReport = FALSE;
            WdfSpinLockRelease(ctx->ReportLock);
            WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, WMF_HID_INPUT_REPORT_SIZE);
            return STATUS_SUCCESS;
        }
    }
    WdfSpinLockRelease(ctx->ReportLock);

    // No report ready — forward to manual queue to pend
    if (g_ReadQueue != NULL) {
        status = WdfRequestForwardToIoQueue(Request, g_ReadQueue);
        if (NT_SUCCESS(status)) {
            return STATUS_PENDING;
        }
    }

    // Fallback: complete with no data
    WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
    return STATUS_UNSUCCESSFUL;
}

// ---------------------------------------------------------------------------
// WmfCompleteReadRequest — called from IOCTL handler to wake a pending read
// ---------------------------------------------------------------------------

VOID
WmfCompleteReadRequest(
    _In_ WDFDEVICE         Device,
    _In_ WMF_PTP_REPORT*   Report
)
{
    WDFREQUEST  pendingRequest;
    NTSTATUS    status;
    PVOID       outputBuffer;
    size_t      outputSize;

    UNREFERENCED_PARAMETER(Device);

    if (g_ReadQueue == NULL) return;

    status = WdfIoQueueRetrieveNextRequest(g_ReadQueue, &pendingRequest);
    if (!NT_SUCCESS(status)) {
        // No pending read — the report was already stored in LastReport
        return;
    }

    status = WdfRequestRetrieveOutputBuffer(
        pendingRequest,
        WMF_HID_INPUT_REPORT_SIZE,
        &outputBuffer,
        &outputSize
    );

    if (NT_SUCCESS(status) && outputSize >= WMF_HID_INPUT_REPORT_SIZE) {
        RtlCopyMemory(outputBuffer, Report, WMF_HID_INPUT_REPORT_SIZE);
        WdfRequestCompleteWithInformation(pendingRequest, STATUS_SUCCESS, WMF_HID_INPUT_REPORT_SIZE);
    } else {
        WdfRequestComplete(pendingRequest, STATUS_BUFFER_TOO_SMALL);
    }
}
