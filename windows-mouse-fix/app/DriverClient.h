#pragma once

//
// DriverClient.h
// Communicates with the WmfVirtualPad UMDF2 driver via DeviceIoControl.
//

#include <windows.h>
#include "../shared/WmfIoctl.h"

class DriverClient {
public:
    DriverClient();
    ~DriverClient();

    // Open the driver device. Returns true on success.
    // Returns false if the driver is not installed — app continues without it.
    bool open();

    // Close the device handle.
    void close();

    // Submit a PTP report to the driver.
    // Returns false if the IOCTL fails (e.g. driver was unloaded).
    // Automatically attempts to reopen on failure.
    bool submitReport(const WMF_PTP_REPORT& report);

    bool isOpen() const { return m_hDevice != INVALID_HANDLE_VALUE; }

private:
    HANDLE m_hDevice;
    DWORD  m_lastError;

    // Attempt to reopen after a failed submit
    bool tryReopen();
};
