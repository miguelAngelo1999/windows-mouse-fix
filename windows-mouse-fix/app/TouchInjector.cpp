//
// TouchInjector.cpp
// Fallback touch injector using InjectTouchInput.
// Used for pipeline testing when the WmfVirtualPad driver is not installed.
//
#include "TouchInjector.h"
#include <cstring>

TouchInjector::TouchInjector() = default;

TouchInjector::~TouchInjector() {
    if (m_hUser32) {
        FreeLibrary(m_hUser32);
        m_hUser32 = nullptr;
    }
}

bool TouchInjector::init() {
    m_hUser32 = LoadLibraryW(L"user32.dll");
    if (!m_hUser32) return false;

    m_pfnInit = (PFN_InitializeTouchInjection)
        GetProcAddress(m_hUser32, "InitializeTouchInjection");
    m_pfnInject = (PFN_InjectTouchInput)
        GetProcAddress(m_hUser32, "InjectTouchInput");

    if (!m_pfnInit || !m_pfnInject) {
        m_available = false;
        return false;
    }

    // Initialize touch injection with max 2 contacts
    // TOUCH_FEEDBACK_NONE = no visual feedback dots
    if (!m_pfnInit(2, TOUCH_FEEDBACK_NONE)) {
        m_available = false;
        return false;
    }

    m_available = true;
    return true;
}

bool TouchInjector::submitReport(const WMF_PTP_REPORT& report) {
    if (!m_available) return false;

    if (report.contact_count == 0) {
        // Lift all fingers
        for (int i = 0; i < 2; i++) {
            m_contacts[i].pointerInfo.pointerFlags =
                POINTER_FLAG_UP | POINTER_FLAG_INRANGE;
        }
        return m_pfnInject(2, m_contacts) != FALSE;
    }

    // Map PTP logical coords (0-4095) to screen pixels
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int count = (report.contact_count < 2) ? (int)report.contact_count : 2;
    for (int i = 0; i < count; i++) {
        const WMF_CONTACT& c = report.contacts[i];

        POINTER_TOUCH_INFO& pt = m_contacts[i];
        memset(&pt, 0, sizeof(pt));

        pt.pointerInfo.pointerType = PT_TOUCH;
        pt.pointerInfo.pointerId   = c.contact_id;

        // Convert logical 0-4095 to screen coordinates
        pt.pointerInfo.ptPixelLocation.x = (c.x * screenW) / 4095;
        pt.pointerInfo.ptPixelLocation.y = (c.y * screenH) / 4095;

        pt.pointerInfo.pointerFlags =
            POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT |
            (i == 0 ? POINTER_FLAG_PRIMARY : 0) |
            ((c.flags & WMF_CONTACT_FLAG_TIP_SWITCH) ? POINTER_FLAG_DOWN : POINTER_FLAG_UP);

        pt.touchFlags   = TOUCH_FLAG_NONE;
        pt.touchMask    = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_PRESSURE;
        pt.pressure     = 512;
        pt.rcContact.left   = pt.pointerInfo.ptPixelLocation.x - 2;
        pt.rcContact.right  = pt.pointerInfo.ptPixelLocation.x + 2;
        pt.rcContact.top    = pt.pointerInfo.ptPixelLocation.y - 2;
        pt.rcContact.bottom = pt.pointerInfo.ptPixelLocation.y + 2;
    }

    return m_pfnInject(count, m_contacts) != FALSE;
}
