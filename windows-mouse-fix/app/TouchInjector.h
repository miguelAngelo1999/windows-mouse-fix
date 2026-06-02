#pragma once
//
// TouchInjector.h
// Injects touchpad (PT_TOUCHPAD) or touch (PT_TOUCH) events for scroll.
// PT_TOUCHPAD with 2 fingers = scroll without clicking (like real trackpad).
//
#include <windows.h>
#include "../shared/WmfIoctl.h"

class TouchInjector {
public:
    TouchInjector();
    ~TouchInjector();

    bool init();
    bool isAvailable() const { return m_available; }
    bool testInject();

    void injectDown(long x, long y);
    bool injectMove(long x, long y);
    void injectUp();
    bool isDown() const { return m_isDown; }

    // Legacy (unused)
    bool submitReport(const WMF_PTP_REPORT& report);

private:
    bool injectTouchpadEvent(DWORD flags, long x, long y);

    bool    m_available = false;
    bool    m_useSynthetic = false;
    bool    m_isDown = false;
    long    m_currentX = 0;
    long    m_currentY = 0;
    int64_t m_lastInjectTime = 0;
    int64_t m_qpcFreq = 0;

    // Synthetic touchpad device (no hardware needed)
    void*   m_synDevice = nullptr;
    void*   m_pfnSynInject = nullptr;

    // Legacy InjectTouchInput
    typedef BOOL (WINAPI* PFN_InitializeTouchInjection)(UINT32, DWORD);
    typedef BOOL (WINAPI* PFN_InjectTouchInput)(UINT32, const POINTER_TOUCH_INFO*);
    PFN_InitializeTouchInjection m_pfnInit = nullptr;
    PFN_InjectTouchInput m_pfnInject = nullptr;
    HMODULE m_hUser32 = nullptr;
};
