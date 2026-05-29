#pragma once
/*
 * HidReportDescriptor.h
 * Windows Precision Touchpad HID report descriptor.
 *
 * Conforms to the Microsoft Precision Touchpad specification:
 * https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/touchpad-protocol-implementation
 *
 * Reports:
 *   0x01 - Input: 5 contacts + contact count + scan time
 *   0x02 - Feature: Contact Count Maximum (5)
 *   0x03 - Feature: Input Mode (mouse=0x00, touchpad=0x03)
 *   0x04 - Feature: Selective Reporting (surface+button switches)
 *   0x05 - Feature: PTPHQA certification blob (256 bytes)
 *
 * Input report layout (report ID 0x01):
 *   Byte 0:    Report ID (0x01)
 *   Per contact (6 bytes each, 5 contacts = 30 bytes):
 *     Byte 0:  Confidence(1) | TipSwitch(1) | Padding(6)
 *     Byte 1:  Contact ID
 *     Byte 2-3: X (16-bit, 0-4095)
 *     Byte 4-5: Y (16-bit, 0-4095)
 *   Byte 31:   Contact Count
 *   Byte 32-33: Scan Time (16-bit, 100us units)
 *   Total: 1 + 30 + 1 + 2 = 34 bytes
 */

static const unsigned char kWmfHidReportDescriptor[] = {
    // ======== Top-level collection: Touch Pad ========
    0x05, 0x0D,              // Usage Page (Digitizer)
    0x09, 0x05,              // Usage (Touch Pad)
    0xA1, 0x01,              // Collection (Application)

    // ---- Input Report (Report ID 0x01) ----
    0x85, 0x01,              //   Report ID (1)

    // ======== Contact 0 ========
    0x05, 0x0D,              //   Usage Page (Digitizer)
    0x09, 0x22,              //   Usage (Finger)
    0xA1, 0x02,              //   Collection (Logical)
    0x09, 0x47,              //     Usage (Confidence)
    0x09, 0x42,              //     Usage (Tip Switch)
    0x15, 0x00,              //     Logical Minimum (0)
    0x25, 0x01,              //     Logical Maximum (1)
    0x75, 0x01,              //     Report Size (1)
    0x95, 0x02,              //     Report Count (2)
    0x81, 0x02,              //     Input (Data, Variable, Absolute)
    0x95, 0x06,              //     Report Count (6)
    0x81, 0x03,              //     Input (Constant)
    0x09, 0x51,              //     Usage (Contact Identifier)
    0x75, 0x08,              //     Report Size (8)
    0x95, 0x01,              //     Report Count (1)
    0x25, 0x0A,              //     Logical Maximum (10)
    0x81, 0x02,              //     Input (Data, Variable, Absolute)
    0x05, 0x01,              //     Usage Page (Generic Desktop)
    0x09, 0x30,              //     Usage (X)
    0x75, 0x10,              //     Report Size (16)
    0x55, 0x0E,              //     Unit Exponent (-2)
    0x65, 0x13,              //     Unit (Inch, English Linear)
    0x35, 0x00,              //     Physical Minimum (0)
    0x46, 0x90, 0x01,        //     Physical Maximum (400)
    0x26, 0xFF, 0x0F,        //     Logical Maximum (4095)
    0x81, 0x02,              //     Input (Data, Variable, Absolute)
    0x09, 0x31,              //     Usage (Y)
    0x46, 0x13, 0x01,        //     Physical Maximum (275)
    0x26, 0xFF, 0x0F,        //     Logical Maximum (4095)
    0x81, 0x02,              //     Input (Data, Variable, Absolute)
    0xC0,                    //   End Collection (Contact 0)

    // ======== Contact 1 ========
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x47,
    0x09, 0x42,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x95, 0x06,
    0x81, 0x03,
    0x09, 0x51,
    0x75, 0x08,
    0x95, 0x01,
    0x25, 0x0A,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x75, 0x10,
    0x55, 0x0E,
    0x65, 0x13,
    0x35, 0x00,
    0x46, 0x90, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0x09, 0x31,
    0x46, 0x13, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0xC0,

    // ======== Contact 2 ========
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x47,
    0x09, 0x42,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x95, 0x06,
    0x81, 0x03,
    0x09, 0x51,
    0x75, 0x08,
    0x95, 0x01,
    0x25, 0x0A,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x75, 0x10,
    0x55, 0x0E,
    0x65, 0x13,
    0x35, 0x00,
    0x46, 0x90, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0x09, 0x31,
    0x46, 0x13, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0xC0,

    // ======== Contact 3 ========
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x47,
    0x09, 0x42,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x95, 0x06,
    0x81, 0x03,
    0x09, 0x51,
    0x75, 0x08,
    0x95, 0x01,
    0x25, 0x0A,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x75, 0x10,
    0x55, 0x0E,
    0x65, 0x13,
    0x35, 0x00,
    0x46, 0x90, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0x09, 0x31,
    0x46, 0x13, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0xC0,

    // ======== Contact 4 ========
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x47,
    0x09, 0x42,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x95, 0x06,
    0x81, 0x03,
    0x09, 0x51,
    0x75, 0x08,
    0x95, 0x01,
    0x25, 0x0A,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x75, 0x10,
    0x55, 0x0E,
    0x65, 0x13,
    0x35, 0x00,
    0x46, 0x90, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0x09, 0x31,
    0x46, 0x13, 0x01,
    0x26, 0xFF, 0x0F,
    0x81, 0x02,
    0xC0,

    // ======== Contact Count (8 bits) ========
    0x05, 0x0D,              //   Usage Page (Digitizer)
    0x09, 0x54,              //   Usage (Contact Count)
    0x25, 0x05,              //   Logical Maximum (5)
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x01,              //   Report Count (1)
    0x81, 0x02,              //   Input (Data, Variable, Absolute)

    // ======== Scan Time (16 bits, 100us units) ========
    0x55, 0x0C,              //   Unit Exponent (-4)
    0x66, 0x01, 0x10,        //   Unit (Seconds, SI Linear)
    0x47, 0xFF, 0xFF, 0x00, 0x00,  // Physical Maximum (65535)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  // Logical Maximum (65535)
    0x75, 0x10,              //   Report Size (16)
    0x95, 0x01,              //   Report Count (1)
    0x09, 0x56,              //   Usage (Scan Time)
    0x81, 0x02,              //   Input (Data, Variable, Absolute)

    // ======== Feature Report: Contact Count Maximum (Report ID 0x02) ========
    0x85, 0x02,              //   Report ID (2)
    0x09, 0x55,              //   Usage (Contact Count Maximum)
    0x25, 0x05,              //   Logical Maximum (5)
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x01,              //   Report Count (1)
    0xB1, 0x02,              //   Feature (Data, Variable, Absolute)

    // ======== Feature Report: Input Mode (Report ID 0x03) ========
    // Windows writes 0x03 (Touchpad) to this to switch from mouse to PTP mode
    0x85, 0x03,              //   Report ID (3)
    0x09, 0x52,              //   Usage (Input Mode)
    0x15, 0x00,              //   Logical Minimum (0)
    0x25, 0x0A,              //   Logical Maximum (10)
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x01,              //   Report Count (1)
    0xB1, 0x02,              //   Feature (Data, Variable, Absolute)

    // ======== Feature Report: Selective Reporting (Report ID 0x04) ========
    0x85, 0x04,              //   Report ID (4)
    0x05, 0x0D,              //   Usage Page (Digitizer)
    0x09, 0x22,              //   Usage (Finger)
    0xA1, 0x02,              //   Collection (Logical)
    0x09, 0x57,              //     Usage (Surface Switch)
    0x09, 0x58,              //     Usage (Button Switch)
    0x15, 0x00,              //     Logical Minimum (0)
    0x25, 0x01,              //     Logical Maximum (1)
    0x75, 0x01,              //     Report Size (1)
    0x95, 0x02,              //     Report Count (2)
    0xB1, 0x02,              //     Feature (Data, Variable, Absolute)
    0x95, 0x06,              //     Report Count (6)
    0xB1, 0x03,              //     Feature (Constant)
    0xC0,                    //   End Collection

    // ======== Feature Report: PTPHQA Certification Blob (Report ID 0x05) ========
    // 256 bytes of zeros — Windows checks for presence, not content
    0x06, 0x00, 0xFF,        //   Usage Page (Vendor Defined)
    0x85, 0x05,              //   Report ID (5)
    0x09, 0xC5,              //   Usage (Vendor Usage 0xC5) - PTPHQA
    0x15, 0x00,              //   Logical Minimum (0)
    0x26, 0xFF, 0x00,        //   Logical Maximum (255)
    0x75, 0x08,              //   Report Size (8)
    0x96, 0x00, 0x01,        //   Report Count (256)
    0xB1, 0x02,              //   Feature (Data, Variable, Absolute)

    0xC0,                    // End Collection (Touch Pad)
};

#define WMF_HID_REPORT_DESCRIPTOR_SIZE  sizeof(kWmfHidReportDescriptor)

// Input report size (without report ID byte for HID read):
// 5 contacts * 6 bytes + 1 (count) + 2 (scan_time) = 33 bytes
// With report ID: 34 bytes
#define WMF_HID_INPUT_REPORT_SIZE   33

// Feature report sizes (without report ID):
#define WMF_FEATURE_CONTACT_COUNT_MAX_SIZE  1   // Report ID 0x02: 1 byte
#define WMF_FEATURE_INPUT_MODE_SIZE         1   // Report ID 0x03: 1 byte
#define WMF_FEATURE_SELECTIVE_SIZE          1   // Report ID 0x04: 1 byte
#define WMF_FEATURE_PTPHQA_SIZE           256   // Report ID 0x05: 256 bytes
