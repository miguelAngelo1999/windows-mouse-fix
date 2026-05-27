#pragma once

//
// DriverClient.h
// Communicates with the WmfVirtualPad UMDF2 driver via DeviceIoControl.
// Falls back to TouchInjector (InjectTouchInput) when driver is not installed.
//

#include <windows.h>
#include "../shared/WmfIoctl.h"
#include "TouchInjector.h"

enum class DriverMode {
    NotConnected,   // neither driver nor touch injection available
    TouchInject,    // using InjectTouchInput fallback (no rubber-band)
    VirtualDriver   // using WmfVirtualPad driver (full PTP, rubber-band works)
};

class DriverClient {
public:
    DriverClient();
    ~DriverClient();

    // Try to open the driver first; fall back to TouchInjector.
    // Returns the mode that was activated.
    DriverMode open();

    void close();

    bool submitReport(const WMF_PTP_REPORT& report);

    bool      isOpen()   const { return m_mode != DriverMode::NotConnected; }
    DriverMode mode()    const { return m_mode; }

    // Human-readable status for tray tooltip
    const wchar_t* statusText() const;

private:
    HANDLE       m_hDevice  = INVALID_HANDLE_VALUE;
    DriverMode   m_mode     = DriverMode::NotConnected;
    DWORD        m_lastError = 0;
    TouchInjector m_touchInjector;

    bool tryOpenDriver();
    bool tryReopen();
};
