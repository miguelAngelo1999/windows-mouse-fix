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
    // TOUCH_FEEDBACK_DEFAULT = 1 (NONE fails on some ARM64 configs)
    if (!m_pfnInit(2, TOUCH_FEEDBACK_DEFAULT)) {
        // Try INDIRECT as fallback
        if (!m_pfnInit(2, TOUCH_FEEDBACK_INDIRECT)) {
            m_available = false;
            return false;
        }
    }

    m_available = true;
    return true;
}

bool TouchInjector::submitReport(const WMF_PTP_REPORT& report) {
    if (!m_available) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    if (report.contact_count == 0) {
        // Finger lift — send UP for all active contacts
        for (int i = 0; i < 2; i++) {
            if (m_contactDown[i]) {
                m_contacts[i].pointerInfo.pointerFlags =
                    POINTER_FLAG_UP | POINTER_FLAG_INRANGE;
                m_contactDown[i] = false;
            } else {
                // Already up — send a no-op update
                m_contacts[i].pointerInfo.pointerFlags = POINTER_FLAG_NONE;
            }
        }
        // Only inject contacts that were actually down
        int count = 0;
        POINTER_TOUCH_INFO toSend[2];
        for (int i = 0; i < 2; i++) {
            if (m_contacts[i].pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
                toSend[count++] = m_contacts[i];
            }
        }
        if (count > 0) {
            return m_pfnInject(count, toSend) != FALSE;
        }
        return true;
    }

    int count = (report.contact_count < 2) ? (int)report.contact_count : 2;
    for (int i = 0; i < count; i++) {
        const WMF_CONTACT& c = report.contacts[i];
        POINTER_TOUCH_INFO& pt = m_contacts[i];
        memset(&pt, 0, sizeof(pt));

        pt.pointerInfo.pointerType = PT_TOUCH;
        pt.pointerInfo.pointerId   = c.contact_id;

        pt.pointerInfo.ptPixelLocation.x = (c.x * screenW) / 4095;
        pt.pointerInfo.ptPixelLocation.y = (c.y * screenH) / 4095;

        // ptHimetricLocation must be set (100 himetric units per mm, ~2540 per inch)
        // Convert pixels to himetric: multiply by 2540 / DPI
        pt.pointerInfo.ptHimetricLocation.x = pt.pointerInfo.ptPixelLocation.x * 2540 / 96;
        pt.pointerInfo.ptHimetricLocation.y = pt.pointerInfo.ptPixelLocation.y * 2540 / 96;

        bool isDown = (c.flags & WMF_CONTACT_FLAG_TIP_SWITCH) != 0;
        bool wasPreviouslyDown = m_contactDown[i];

        DWORD flags = POINTER_FLAG_INRANGE;
        if (isDown) {
            flags |= POINTER_FLAG_INCONTACT;
            if (!wasPreviouslyDown) {
                flags |= POINTER_FLAG_DOWN;  // first touch
            } else {
                flags |= POINTER_FLAG_UPDATE; // continuing touch
            }
            m_contactDown[i] = true;
        } else {
            flags |= POINTER_FLAG_UP;
            m_contactDown[i] = false;
        }

        if (i == 0) flags |= POINTER_FLAG_PRIMARY;

        pt.pointerInfo.pointerFlags = flags;
        pt.touchFlags   = TOUCH_FLAG_NONE;
        pt.touchMask    = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_PRESSURE;
        pt.pressure     = 512;
        pt.rcContact.left   = pt.pointerInfo.ptPixelLocation.x - 3;
        pt.rcContact.right  = pt.pointerInfo.ptPixelLocation.x + 3;
        pt.rcContact.top    = pt.pointerInfo.ptPixelLocation.y - 3;
        pt.rcContact.bottom = pt.pointerInfo.ptPixelLocation.y + 3;
    }

    return m_pfnInject(count, m_contacts) != FALSE;
}

