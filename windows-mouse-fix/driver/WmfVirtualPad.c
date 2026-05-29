/*
 * WmfVirtualPad.c
 * UMDF2 virtual HID Precision Touchpad driver.
 *
 * Architecture:
 *   - Creates a virtual HID device that Windows recognizes as a PTP touchpad
 *   - Accepts IOCTL_WMF_SUBMIT_REPORT from user-mode app
 *   - Forwards raw HID report bytes to hidclass.sys / PrecisionTouchPad.sys
 *
 * Based on the VHidMini2 UMDF2 sample from the Windows Driver Kit.
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

    // Mark as filter — required for HID miniport
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

    // Create device interface for user-mode app access
    // The app will use SetupDi to find this interface
    {
        // GUID_DEVINTERFACE_WMF_VIRTUAL_PAD = {B5A2C4D1-3E7F-4A8B-9C6D-1F2E3A4B5C6D}
        static const GUID GUID_DEVINTERFACE_WMF = 
            { 0xB5A2C4D1, 0x3E7F, 0x4A8B, { 0x9C, 0x6D, 0x1F, 0x2E, 0x3A, 0x4B, 0x5C, 0x6D } };
        
        status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_WMF, NULL);
        // Non-fatal
        if (!NT_SUCCESS(status)) {
            status = STATUS_SUCCESS;
        }
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
        // Accept but ignore
        status = STATUS_SUCCESS;
        break;

    // UMDF-specific IOCTLs (mshidumdf.sys translates HID_XFER_PACKET)
    case IOCTL_UMDF_HID_GET_FEATURE:
        status = WmfGetFeature(queueContext, Request);
        break;

    case IOCTL_UMDF_HID_SET_FEATURE:
        status = WmfSetFeature(queueContext, Request);
        break;

    case IOCTL_UMDF_HID_GET_INPUT_REPORT:
        // Return last report if available
        status = STATUS_NOT_IMPLEMENTED;
        break;

    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT:
        status = STATUS_SUCCESS;
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
// IOCTL_HID_READ_REPORT — pend to manual queue
// ---------------------------------------------------------------------------

NTSTATUS
WmfReadReport(
    _In_  PQUEUE_CONTEXT QueueContext,
    _In_  WDFREQUEST     Request,
    _Out_ BOOLEAN*       CompleteRequest
)
{
    NTSTATUS status;

    status = WdfRequestForwardToIoQueue(
        Request,
        QueueContext->DeviceContext->ManualQueue);

    if (!NT_SUCCESS(status)) {
        *CompleteRequest = TRUE;
    } else {
        *CompleteRequest = FALSE;
    }

    return status;
}

// ---------------------------------------------------------------------------
// IOCTL_UMDF_HID_GET_FEATURE
// ---------------------------------------------------------------------------

NTSTATUS
WmfGetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    NTSTATUS        status;
    HID_XFER_PACKET packet;
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;

    status = RequestGetHidXferPacket_ToReadFromDevice(Request, &packet);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    switch (packet.reportId) {

    case 0x02: // Contact Count Maximum
        if (packet.reportBufferLen < 1) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        packet.reportBuffer[0] = 5; // max 5 contacts
        WdfRequestSetInformation(Request, 1 + sizeof(packet.reportId));
        break;

    case 0x03: // Input Mode
        if (packet.reportBufferLen < 1) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        packet.reportBuffer[0] = devCtx->InputMode;
        WdfRequestSetInformation(Request, 1 + sizeof(packet.reportId));
        break;

    case 0x04: // Selective Reporting
        if (packet.reportBufferLen < 1) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        // Surface switch = 1, Button switch = 1
        packet.reportBuffer[0] = 0x03;
        WdfRequestSetInformation(Request, 1 + sizeof(packet.reportId));
        break;

    case 0x05: // PTPHQA Certification Blob
        if (packet.reportBufferLen < WMF_FEATURE_PTPHQA_SIZE) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        // All zeros — Windows checks presence, not content
        RtlZeroMemory(packet.reportBuffer, WMF_FEATURE_PTPHQA_SIZE);
        WdfRequestSetInformation(Request, WMF_FEATURE_PTPHQA_SIZE + sizeof(packet.reportId));
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// IOCTL_UMDF_HID_SET_FEATURE
// ---------------------------------------------------------------------------

NTSTATUS
WmfSetFeature(
    _In_ PQUEUE_CONTEXT QueueContext,
    _In_ WDFREQUEST     Request
)
{
    NTSTATUS        status;
    HID_XFER_PACKET packet;
    PDEVICE_CONTEXT devCtx = QueueContext->DeviceContext;

    status = RequestGetHidXferPacket_ToWriteToDevice(Request, &packet);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    switch (packet.reportId) {

    case 0x03: // Input Mode — Windows writes 0x03 to enable PTP mode
        if (packet.reportBufferLen >= 1) {
            devCtx->InputMode = packet.reportBuffer[0];
        }
        WdfRequestSetInformation(Request, packet.reportBufferLen + sizeof(packet.reportId));
        break;

    case 0x04: // Selective Reporting — accept and ignore
        WdfRequestSetInformation(Request, packet.reportBufferLen + sizeof(packet.reportId));
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

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
    WDFMEMORY   memory;
    size_t      bufferLength;
    PVOID       buffer;

    static const WCHAR manufacturer[] = L"Windows Mouse Fix";
    static const WCHAR product[]      = L"Virtual Precision Touchpad";

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    buffer = WdfMemoryGetBuffer(memory, &bufferLength);
    if (bufferLength < sizeof(product)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Return product string by default
    RtlCopyMemory(buffer, product, sizeof(product));
    WdfRequestSetInformation(Request, sizeof(product));
    return STATUS_SUCCESS;
}

NTSTATUS
WmfGetIndexedString(
    _In_ WDFREQUEST Request
)
{
    // Same as GetString — return product name
    return WmfGetString(Request);
}

// ---------------------------------------------------------------------------
// IOCTL_WMF_SUBMIT_REPORT — user-mode app submits a PTP report
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
    // Send report data WITHOUT the report_id byte (HID class adds it)
    WmfCompleteReadRequest(DevCtx,
        (PUCHAR)report + 1,  // skip report_id byte
        WMF_HID_INPUT_REPORT_SIZE);

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
    if (outputSize < ReportSize) {
        WdfRequestComplete(pendingRequest, STATUS_BUFFER_TOO_SMALL);
        return;
    }

    status = WdfMemoryCopyFromBuffer(memory, 0, ReportBuffer, ReportSize);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(pendingRequest, status);
        return;
    }

    WdfRequestCompleteWithInformation(pendingRequest, STATUS_SUCCESS, ReportSize);
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

// ---------------------------------------------------------------------------
// UMDF HID Xfer Packet helpers
// ---------------------------------------------------------------------------
// In UMDF, mshidumdf.sys converts HID_XFER_PACKET-based IOCTLs into
// separate buffers:
//   Input buffer  = report data (for SET operations)
//   Output buffer = report data (for GET operations)
//   The report ID is passed as a separate 1-byte input/output buffer
//
// For IOCTL_UMDF_HID_GET_FEATURE (reading from device):
//   Output buffer = where to write feature report data
//   Input buffer  = 1 byte containing the report ID
//
// For IOCTL_UMDF_HID_SET_FEATURE (writing to device):
//   Input buffer  = report data (first byte is report ID)

NTSTATUS
RequestGetHidXferPacket_ToReadFromDevice(
    _In_  WDFREQUEST     Request,
    _Out_ HID_XFER_PACKET* Packet
)
{
    NTSTATUS    status;
    WDFMEMORY   inputMemory;
    WDFMEMORY   outputMemory;
    size_t      inputBufferLength;
    size_t      outputBufferLength;
    PVOID       inputBuffer;
    PVOID       outputBuffer;

    // Input buffer contains the report ID
    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < sizeof(UCHAR)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    // Output buffer is where we write the feature report data
    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    outputBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);

    Packet->reportId = *(PUCHAR)inputBuffer;
    Packet->reportBuffer = (PUCHAR)outputBuffer;
    Packet->reportBufferLen = (ULONG)outputBufferLength;

    return STATUS_SUCCESS;
}

NTSTATUS
RequestGetHidXferPacket_ToWriteToDevice(
    _In_  WDFREQUEST     Request,
    _Out_ HID_XFER_PACKET* Packet
)
{
    NTSTATUS    status;
    WDFMEMORY   inputMemory;
    size_t      inputBufferLength;
    PVOID       inputBuffer;

    // Input buffer contains: [reportId (1 byte)] [report data...]
    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < 1) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    Packet->reportId = *(PUCHAR)inputBuffer;
    Packet->reportBuffer = (PUCHAR)inputBuffer + 1;
    Packet->reportBufferLen = (ULONG)(inputBufferLength - 1);

    return STATUS_SUCCESS;
}
