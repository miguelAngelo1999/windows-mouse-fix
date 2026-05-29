#pragma once
/*
 * WmfIoctl.h
 * Shared between the WmfVirtualPad UMDF2 driver and the WindowsMouseFix user-mode app.
 * Defines the IOCTL control code and the PTP input report structure.
 */

#ifdef _KERNEL_MODE
#include <wdm.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

#include <stdint.h>

// ---------------------------------------------------------------------------
// IOCTL control code
// ---------------------------------------------------------------------------

#define WMF_DEVICE_TYPE  0x8000

// User-mode app sends a WMF_PTP_REPORT to the driver via this IOCTL.
#define IOCTL_WMF_SUBMIT_REPORT \
    CTL_CODE(WMF_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)

// Symbolic link the driver exposes; user-mode opens this path.
#define WMF_DEVICE_SYMLINK        L"\\\\.\\WmfVirtualPad"
#define WMF_DEVICE_SYMLINK_KERNEL L"\\DosDevices\\WmfVirtualPad"

// ---------------------------------------------------------------------------
// PTP report structures  (packed — must match HID descriptor layout exactly)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)

// Per-contact data — 5 bytes each.
// Byte 0 bit layout: [confidence:1][tip_switch:1][padding:6]
// Byte 1: contact_id
// Byte 2-3: X (16-bit LE)
// Byte 4-5: Y (16-bit LE)
typedef struct _WMF_CONTACT {
    uint8_t  flags;            // bit0=confidence, bit1=tip_switch, bits2-7=0
    uint8_t  contact_id;       // unique contact identifier: 0-4
    uint16_t x;                // logical X position, 0-4095
    uint16_t y;                // logical Y position, 0-4095
} WMF_CONTACT;

// Helper macros for setting contact flags
#define WMF_CONTACT_FLAG_CONFIDENCE  0x01
#define WMF_CONTACT_FLAG_TIP_SWITCH  0x02
#define WMF_CONTACT_FLAGS_DOWN       (WMF_CONTACT_FLAG_CONFIDENCE | WMF_CONTACT_FLAG_TIP_SWITCH)
#define WMF_CONTACT_FLAGS_UP         0x00

// Full PTP input report — report ID 0x01.
// Total size: 1 + 5*6 + 1 + 2 = 34 bytes
typedef struct _WMF_PTP_REPORT {
    uint8_t     report_id;      // must be 0x01
    WMF_CONTACT contacts[5];    // up to 5 contacts; unused ones zeroed
    uint8_t     contact_count;  // number of active contacts (0 or 2 for scroll)
    uint16_t    scan_time;      // timestamp in 100-microsecond units, wraps at 65535
} WMF_PTP_REPORT;

#pragma pack(pop)

// Compile-time size check: must be exactly 34 bytes
typedef char _WMF_PTP_REPORT_SIZE_CHECK[(sizeof(WMF_PTP_REPORT) == 34) ? 1 : -1];

#define WMF_PTP_REPORT_SIZE     sizeof(WMF_PTP_REPORT)

// Feature report (report ID 0x02) — returned on GET_FEATURE.
typedef struct _WMF_FEATURE_REPORT {
    uint8_t report_id;          // 0x02
    uint8_t contact_count_max;  // 5
} WMF_FEATURE_REPORT;

#define WMF_FEATURE_REPORT_SIZE sizeof(WMF_FEATURE_REPORT)
