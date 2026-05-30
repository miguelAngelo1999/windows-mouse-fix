#pragma once
//
// TouchInjector.h
// Touch injector using Windows InjectTouchInput API.
// Produces smooth scroll with momentum and rubber-band in Windows 11
// when used with proper DOWN/UPDATE/UP pointer flag sequencing.
// Used as the primary input delivery path (PrecisionTouchPad.sys not required).
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
    bool testInject();  // test if InjectTouchInput actually works

private:
    bool            m_available = false;
    POINTER_TOUCH_INFO m_contacts[2] = {};
    bool            m_contactDown[2] = { false, false };  // track DOWN/UPDATE/UP state

    // Function pointers (loaded dynamically — not available on all Windows versions)
    typedef BOOL (WINAPI* PFN_InitializeTouchInjection)(UINT32, DWORD);
    typedef BOOL (WINAPI* PFN_InjectTouchInput)(UINT32, const POINTER_TOUCH_INFO*);

    PFN_InitializeTouchInjection m_pfnInit   = nullptr;
    PFN_InjectTouchInput         m_pfnInject = nullptr;
    HMODULE                      m_hUser32   = nullptr;
};
