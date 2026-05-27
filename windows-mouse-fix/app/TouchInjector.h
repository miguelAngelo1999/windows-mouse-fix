#pragma once
//
// TouchInjector.h
// Fallback touch injector using Windows InjectTouchInput API.
// Used when the WmfVirtualPad driver is not installed.
//
// IMPORTANT: InjectTouchInput does NOT go through PrecisionTouchPad.sys,
// so rubber-band and OS momentum won't work with this path.
// This is purely for testing the pipeline end-to-end without the driver.
// The real driver path (DriverClient -> WmfVirtualPad) is required for
// full functionality.
//
#include <windows.h>
#include "../shared/WmfIoctl.h"

class TouchInjector {
public:
    TouchInjector();
    ~TouchInjector();

    bool init();   // calls InitializeTouchInjection
    bool submitReport(const WMF_PTP_REPORT& report);
    bool isAvailable() const { return m_available; }

private:
    bool            m_available = false;
    POINTER_TOUCH_INFO m_contacts[2] = {};

    // Function pointers (loaded dynamically — not available on all Windows versions)
    typedef BOOL (WINAPI* PFN_InitializeTouchInjection)(UINT32, DWORD);
    typedef BOOL (WINAPI* PFN_InjectTouchInput)(UINT32, const POINTER_TOUCH_INFO*);

    PFN_InitializeTouchInjection m_pfnInit   = nullptr;
    PFN_InjectTouchInput         m_pfnInject = nullptr;
    HMODULE                      m_hUser32   = nullptr;
};
