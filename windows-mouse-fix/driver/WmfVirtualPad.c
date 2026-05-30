/*
 * WmfVirtualPad.c
 * UMDF2 virtual HID Precision Touchpad driver.
 *
 * Architecture:
 *   - Creates a virtual HID device that Windows recognizes as a PTP touchpad
 *   - Accepts HidD_SetOutputReport (report ID 0x06) from user-mode app
 *   - Forwards raw HID report bytes to hidclass.sys via mshidumdf.sys
 *
 * Based on the VHidMini2 UMDF2 sample from the Windows Driver Kit.
 *
 * UMDF HID_XFER_PACKET note:
 *   mshidumdf.sys converts HID_XFER_PACKET (which has an embedded pointer)
 *   into two separate buffers:
 *
 *   For IOCTL_UMDF_HID_GET_FEATURE (read from device):
 *     Output buffer = [reportId (1 byte)] [report data ...]
 *     Input buffer  = not used / may be absent
 *
 *   For IOCTL_UMDF_HID_SET_FEATURE (write to device):
 *     Input buffer  = [reportId (1 byte)] [report data ...]
 *     Output buffer = not used
 *
 *   For IOCTL_UMDF_HID_GET_INPUT_REPORT (read from device):
 *     Output buffer = [reportId (1 byte)] [report data ...]
 *
 *   For IOCTL_UMDF_HID_SET_OUTPUT_REPORT (write to device):
 *     Input buffer  = [reportId (1 byte)] [report data ...]
 */

#include "WmfVirtualPad.h"

// ---------------------------------------------------------------------------
// HID Descriptor (returned for IOCTL_HID_GET_DEVICE_DESCRIPTOR)
// ---------------------------------------------------------------------------

static HID_DESCRIPTOR G_WmfHidDescriptor = {
    0x09,   // bLength
    0x21,   // bDescriptorType (HID)
    0x0100, // bcdHID (1.00)
    0x00,   // bCountryCode
    0x01,   // bNumDescriptors
    {
        {
            0x22,                                   // bDescriptorType (Report)
            (USHORT)WMF_HID_REPORT_DESCRIPTOR_SIZE  // wDescriptorLength
        }
    }
};

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
// WmfEvtDeviceAdd
// ---------------------------------------------------------------------------

NTSTATUS
WmfEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS                status;
    WDFDEVICE               device;
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PDEVICE_CONTEXT         deviceContext;

    UNREFERENCED_PARAMETER(Driver);

    // Mark as filter — required for HID miniport under MsHidUmdf
    WdfFdoInitSetFilter(DeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceContext = GetDeviceContext(device);
    deviceContext->Device = device;
    deviceContext->HasReport = FALSE;
    deviceContext->InputMode = 0x03; // Start in touchpad mode

    // HID device attributes
    deviceContext->HidDeviceAttributes.Size = sizeof(HID_DEVICE_ATTRIBUTES);
    deviceContext->HidDeviceAttributes.VendorID = WMF_VID;
    deviceContext->HidDeviceAttributes.ProductID = WMF_PID;
    deviceContext->HidDeviceAttributes.VersionNumber = WMF_VERSION;

    // HID descriptor
    deviceContext->HidDescriptor = G_WmfHidDescriptor;

    // Create spinlock
    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &deviceContext->ReportLock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Create default I/O queue (parallel dispatch for HID IOCTLs)
    status = WmfCreateDefaultQueue(device, &deviceContext->DefaultQueue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Create manual queue for pending HID read requests
    status = WmfCreateManualQueue(device, &deviceContext->ManualQueue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Create device interface for user-mode app to find and open this device
    {
        static const GUID GUID_DEVINTERFACE_WMF =
            { 0xB5A2C4D1, 0x3E7F, 0x4A8B, { 0x9C, 0x6D, 0x1F, 0x2E, 0x3A, 0x4B, 0x5C, 0x6D } };

        status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_WMF, NULL);
        if (!NT_SUCCESS(status)) {
            // Non-fatal
            status = STATUS_SUCCESS;
        }
    }

    // Symbolic link for user-mode app
    {
        UNICODE_STRING symLink;
        RtlInitUnicodeString(&symLink, L"\\DosDevices\\WmfVirtualPad");
        WdfDeviceCreateSymbolicLink(device, &symLink);
        // Non-fatal if fails
    }

    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// Queue creation
// ---------------------------------------------------------------------------

NTSTATUS
WmfCreateDefaultQueue(
    _In_  WDFDEVICE Device,
    _Out_ WDFQUEUE* Queue
)
{
    NTSTATUS                status;
    WDF_IO_QUEUE_CONFIG     queueConfig;
    WDF_OBJECT_ATTRIBUTES   queueAttributes;
    WDFQUEUE                queue;
    PQUEUE_CONTEXT          queueContext;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = WmfEvtIoDeviceControl;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes, QUEUE_CONTEXT);

    status = WdfIoQueueCreate(Device, &queueConfig, &queueAttributes, &queue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    queueContext = GetQueueContext(queue);
    queueContext->Queue = queue;
    queueContext->DeviceContext = GetDeviceContext(Device);

    *Queue = queue;
    return status;
}

NTSTATUS
WmfCreateManualQueue(
    _In_  WDFDEVICE Device,
    _Out_ WDFQUEUE* Queue
)
{
    NTSTATUS            status;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE            queue;

    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *Queue = queue;
    return status;
}

// ---------------------------------------------------------------------------
// I/O Device Control handler
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
    NTSTATUS        status;
    BOOLEAN         completeRequest = TRUE;
    PQUEUE_CONTEXT  queueContext = GetQueueContext(Queue);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {

    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        status = WmfGetHidDescriptor(queueContext, Request);
        break;

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        status = WmfGetDeviceAttributes(queueContext, Request);
        break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        status = WmfGetReportDescriptor(queueContext, Request);
        break;

    case IOCTL_HID_READ_REPORT:
        status = WmfReadReport(queueContext, Request, &completeRequest);
        break;

    case IOCTL_HID_WRITE_REPORT:
        status = WmfWriteReport(queueContext, Request);
        break;

    // UMDF-specific IOCTLs
    case IOCTL_UMDF_HID_GET_FEATURE:
        status = WmfGetFeature(queueContext, Request);
        break;

    case IOCTL_UMDF_HID_SET_FEATURE:
        status = WmfSetFeature(queueContext, Request);
        break;

    case IOCTL_UMDF_HID_GET_INPUT_REPORT:
        status = WmfGetInputReport(queueContext, Request);
        break;

    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT:
        status = WmfWriteReport(queueContext, Request);
        break;

    case IOCTL_HID_GET_STRING:
        status = WmfGetString(Request);
        break;

    case IOCTL_HID_GET_INDEXED_STRING:
        status = WmfGetIndexedString(Request);
        break;

    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
    case IOCTL_HID_ACTIVATE_DEVICE:
    case IOCTL_HID_DEACTIVATE_DEVICE:
    case IOCTL_GET_PHYSICAL_DESCRIPTOR:
        status = STATUS_NOT_IMPLEMENTED;
        break;

    // ---- Our custom IOCTL from user-mode app ----
    case IOCTL_WMF_SUBMIT_REPORT:
        status = WmfHandleSubmitReport(queueContext->DeviceContext, Request);
        break;

    default:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    if (completeRequest) {
        WdfRequestComplete(Request, status);
    }
}

// ---------------------------------------------------------------------------
// IOCTL_HID_GET_DEVICE_DESCRIPTOR
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetHidDescriptor(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;
    return RequestCopyFromBuffer(Request,
        &devCtx->HidDescriptor,
        devCtx->HidDescriptor.bLength);
}

// ---------------------------------------------------------------------------
// IOCTL_HID_GET_REPORT_DESCRIPTOR
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetReportDescriptor(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    UNREFERENCED_PARAMETER(QueueContext);
    return RequestCopyFromBuffer(Request,
        (PVOID)kWmfHidReportDescriptor,
        WMF_HID_REPORT_DESCRIPTOR_SIZE);
}

// ---------------------------------------------------------------------------
// IOCTL_HID_GET_DEVICE_ATTRIBUTES
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetDeviceAttributes(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    return RequestCopyFromBuffer(Request,
        &QueueContext->DeviceContext->HidDeviceAttributes,
        sizeof(HID_DEVICE_ATTRIBUTES));
}

// ---------------------------------------------------------------------------
// IOCTL_HID_READ_REPORT — complete immediately if report available, else pend
// ---------------------------------------------------------------------------

NTSTATUS
WmfReadReport(
    _In_  PQUEUE_CONTEXT QueueContext,
    _In_  WDFREQUEST     Request,
    _Out_ BOOLEAN*       CompleteRequest
)
{
    NTSTATUS status;
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;

    // If we have a stored report, complete immediately
    WdfSpinLockAcquire(devCtx->ReportLock);
    BOOLEAN hasReport = devCtx->HasReport;
    WMF_PTP_REPORT lastReport = devCtx->LastReport;
    if (hasReport) {
        devCtx->HasReport = FALSE; // consume it
    }
    WdfSpinLockRelease(devCtx->ReportLock);

    if (hasReport) {
        // Complete immediately with stored report
        WDFMEMORY memory;
        status = WdfRequestRetrieveOutputMemory(Request, &memory);
        if (NT_SUCCESS(status)) {
            size_t outputSize;
            WdfMemoryGetBuffer(memory, &outputSize);
            ULONG bytesToCopy = sizeof(WMF_PTP_REPORT);
            if (outputSize < bytesToCopy) bytesToCopy = (ULONG)outputSize;
            status = WdfMemoryCopyFromBuffer(memory, 0, &lastReport, bytesToCopy);
            if (NT_SUCCESS(status)) {
                WdfRequestSetInformation(Request, bytesToCopy);
            }
        }
        *CompleteRequest = TRUE;
        return status;
    }

    // No report available — forward to manual queue
    status = WdfRequestForwardToIoQueue(
        Request,
        devCtx->ManualQueue);

    if (!NT_SUCCESS(status)) {
        *CompleteRequest = TRUE;
    } else {
        *CompleteRequest = FALSE;
    }

    return status;
}

// ---------------------------------------------------------------------------
// IOCTL_UMDF_HID_GET_FEATURE
//
// In UMDF, mshidumdf.sys passes:
//   Output buffer = [reportId (1 byte)] [feature data ...]
//   Input buffer  = may be absent
//
// We read the report ID from output buffer[0], then fill output buffer[1..].
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    NTSTATUS        status;
    WDFMEMORY       outputMemory;
    size_t          outputBufferLength;
    PUCHAR          outputBuffer;
    UCHAR           reportId;
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;

    // Get output buffer — report ID is in byte 0, data goes in bytes 1+
    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    outputBuffer = (PUCHAR)WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    if (outputBufferLength < 1) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    reportId = outputBuffer[0];

    switch (reportId) {

    case 0x02: // Contact Count Maximum
        if (outputBufferLength < 2) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        outputBuffer[1] = 5; // max 5 contacts
        WdfRequestSetInformation(Request, 2);
        break;

    case 0x03: // Input Mode
        if (outputBufferLength < 2) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        outputBuffer[1] = devCtx->InputMode;
        WdfRequestSetInformation(Request, 2);
        break;

    case 0x04: // Selective Reporting
        if (outputBufferLength < 2) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        outputBuffer[1] = 0x03; // surface + button switches enabled
        WdfRequestSetInformation(Request, 2);
        break;

    case 0x05: // PTPHQA Certification Blob
        if (outputBufferLength < WMF_FEATURE_PTPHQA_SIZE + 1) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        // All zeros — Windows checks presence, not content
        RtlZeroMemory(outputBuffer + 1, WMF_FEATURE_PTPHQA_SIZE);
        WdfRequestSetInformation(Request, WMF_FEATURE_PTPHQA_SIZE + 1);
        break;

    default:
        // Unknown feature report — return STATUS_INVALID_PARAMETER
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// IOCTL_UMDF_HID_SET_FEATURE
//
// In UMDF, mshidumdf.sys passes:
//   Input buffer = [reportId (1 byte)] [feature data ...]
// ---------------------------------------------------------------------------

NTSTATUS
WmfSetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    NTSTATUS        status;
    WDFMEMORY       inputMemory;
    size_t          inputBufferLength;
    PUCHAR          inputBuffer;
    UCHAR           reportId;
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;

    // Get input buffer — report ID is in byte 0, data in bytes 1+
    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    inputBuffer = (PUCHAR)WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < 1) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    reportId = inputBuffer[0];

    switch (reportId) {

    case 0x03: // Input Mode — Windows writes 0x03 to enable PTP mode
        if (inputBufferLength >= 2) {
            devCtx->InputMode = inputBuffer[1];
        }
        WdfRequestSetInformation(Request, inputBufferLength);
        break;

    case 0x04: // Selective Reporting — accept and ignore
        WdfRequestSetInformation(Request, inputBufferLength);
        break;

    default:
        // Unknown feature — accept silently to avoid crashing
        WdfRequestSetInformation(Request, inputBufferLength);
        break;
    }

    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// IOCTL_UMDF_HID_GET_INPUT_REPORT
//
// In UMDF, mshidumdf.sys passes:
//   Output buffer = [reportId (1 byte)] [input data ...]
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetInputReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    NTSTATUS        status;
    WDFMEMORY       outputMemory;
    size_t          outputBufferLength;
    PUCHAR          outputBuffer;
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;

    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    outputBuffer = (PUCHAR)WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    if (outputBufferLength < sizeof(WMF_PTP_REPORT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Return last report if available, otherwise return empty report
    WdfSpinLockAcquire(devCtx->ReportLock);
    if (devCtx->HasReport) {
        RtlCopyMemory(outputBuffer, &devCtx->LastReport, sizeof(WMF_PTP_REPORT));
    } else {
        RtlZeroMemory(outputBuffer, sizeof(WMF_PTP_REPORT));
        outputBuffer[0] = 0x01; // report ID
    }
    WdfSpinLockRelease(devCtx->ReportLock);

    WdfRequestSetInformation(Request, sizeof(WMF_PTP_REPORT));
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// IOCTL_HID_WRITE_REPORT / IOCTL_UMDF_HID_SET_OUTPUT_REPORT
//
// In UMDF, mshidumdf.sys passes:
//   Input buffer = [reportId (1 byte)] [report data ...]
//
// App sends PTP data via HidD_SetOutputReport with report ID 0x06.
// We store it and complete any pending HID read request.
// ---------------------------------------------------------------------------

NTSTATUS
WmfWriteReport(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    NTSTATUS        status;
    WDFMEMORY       inputMemory;
    size_t          inputBufferLength;
    PUCHAR          inputBuffer;
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;

    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    inputBuffer = (PUCHAR)WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < 1) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    // Report ID 0x06 = user-mode PTP data channel (output report)
    if (inputBuffer[0] == 0x06 && inputBufferLength >= WMF_HID_INPUT_REPORT_SIZE + 1) {
        // The output report data IS the input report data (without report_id)
        WdfSpinLockAcquire(devCtx->ReportLock);
        devCtx->LastReport.report_id = 0x01;
        RtlCopyMemory(((PUCHAR)&devCtx->LastReport) + 1, inputBuffer + 1, WMF_HID_INPUT_REPORT_SIZE);
        devCtx->HasReport = TRUE;
        WdfSpinLockRelease(devCtx->ReportLock);

        // Complete pending read with FULL report including report_id
        WmfCompleteReadRequest(devCtx, (PUCHAR)&devCtx->LastReport, sizeof(WMF_PTP_REPORT));

        WdfRequestSetInformation(Request, inputBufferLength);
        return STATUS_SUCCESS;
    }

    // Unknown output report — accept silently
    WdfRequestSetInformation(Request, inputBufferLength);
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// IOCTL_HID_GET_STRING / IOCTL_HID_GET_INDEXED_STRING
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetString(
    _In_ WDFREQUEST Request
)
{
    NTSTATUS    status;
    WDFMEMORY   inputMemory;
    size_t      inputBufferLength;
    PVOID       inputBuffer;
    ULONG       stringId;

    static const WCHAR manufacturer[] = L"Windows Mouse Fix";
    static const WCHAR product[]      = L"Virtual Precision Touchpad";
    static const WCHAR serial[]       = L"0001";

    // In UMDF, mshidumdf.sys passes the string ID through the input buffer
    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        // If no input buffer, return product string by default
        return RequestCopyFromBuffer(Request, (PVOID)product, sizeof(product));
    }

    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < sizeof(ULONG)) {
        return RequestCopyFromBuffer(Request, (PVOID)product, sizeof(product));
    }

    stringId = (*(PULONG)inputBuffer) & 0xFFFF;

    switch (stringId) {
    case HID_STRING_ID_IMANUFACTURER:
        return RequestCopyFromBuffer(Request, (PVOID)manufacturer, sizeof(manufacturer));
    case HID_STRING_ID_IPRODUCT:
        return RequestCopyFromBuffer(Request, (PVOID)product, sizeof(product));
    case HID_STRING_ID_ISERIALNUMBER:
        return RequestCopyFromBuffer(Request, (PVOID)serial, sizeof(serial));
    default:
        return RequestCopyFromBuffer(Request, (PVOID)product, sizeof(product));
    }
}

NTSTATUS
WmfGetIndexedString(
    _In_ WDFREQUEST Request
)
{
    return WmfGetString(Request);
}

// ---------------------------------------------------------------------------
// IOCTL_WMF_SUBMIT_REPORT — user-mode app submits a PTP report directly
// ---------------------------------------------------------------------------

NTSTATUS
WmfHandleSubmitReport(
    _In_ PDEVICE_CONTEXT DevCtx,
    _In_ WDFREQUEST      Request
)
{
    NTSTATUS    status;
    PVOID       inputBuffer = NULL;
    size_t      inputSize = 0;
    WMF_PTP_REPORT* report;

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(WMF_PTP_REPORT),
        &inputBuffer,
        &inputSize);

    if (!NT_SUCCESS(status) || inputSize < sizeof(WMF_PTP_REPORT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    report = (WMF_PTP_REPORT*)inputBuffer;

    if (report->report_id != 0x01) {
        return STATUS_INVALID_PARAMETER;
    }

    // Store under spinlock
    WdfSpinLockAcquire(DevCtx->ReportLock);
    RtlCopyMemory(&DevCtx->LastReport, report, sizeof(WMF_PTP_REPORT));
    DevCtx->HasReport = TRUE;
    WdfSpinLockRelease(DevCtx->ReportLock);

    // Complete any pending HID read request with this report
    WmfCompleteReadRequest(DevCtx,
        (PUCHAR)report,
        sizeof(WMF_PTP_REPORT));

    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// Complete a pending HID read request with report data
// ---------------------------------------------------------------------------

VOID
WmfCompleteReadRequest(
    _In_ PDEVICE_CONTEXT DevCtx,
    _In_ PUCHAR          ReportBuffer,
    _In_ ULONG           ReportSize
)
{
    WDFREQUEST  pendingRequest;
    NTSTATUS    status;
    WDFMEMORY   memory;
    size_t      outputSize;
    ULONG       bytesToCopy;

    status = WdfIoQueueRetrieveNextRequest(DevCtx->ManualQueue, &pendingRequest);
    if (!NT_SUCCESS(status)) {
        // No pending read — report is stored in LastReport for next read
        return;
    }

    status = WdfRequestRetrieveOutputMemory(pendingRequest, &memory);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(pendingRequest, status);
        return;
    }

    WdfMemoryGetBuffer(memory, &outputSize);

    // Use the smaller of what we have and what was requested
    bytesToCopy = ReportSize;
    if (outputSize < ReportSize) {
        bytesToCopy = (ULONG)outputSize;
    }

    status = WdfMemoryCopyFromBuffer(memory, 0, ReportBuffer, bytesToCopy);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(pendingRequest, status);
        return;
    }

    WdfRequestCompleteWithInformation(pendingRequest, STATUS_SUCCESS, bytesToCopy);
}

// ---------------------------------------------------------------------------
// Helper: Copy from buffer to request output memory
// ---------------------------------------------------------------------------

NTSTATUS
RequestCopyFromBuffer(
    _In_ WDFREQUEST Request,
    _In_ PVOID      SourceBuffer,
    _In_ size_t     NumBytesToCopyFrom
)
{
    NTSTATUS    status;
    WDFMEMORY   memory;
    size_t      outputBufferLength;

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WdfMemoryGetBuffer(memory, &outputBufferLength);
    if (outputBufferLength < NumBytesToCopyFrom) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    status = WdfMemoryCopyFromBuffer(memory, 0, SourceBuffer, NumBytesToCopyFrom);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WdfRequestSetInformation(Request, NumBytesToCopyFrom);
    return status;
}
