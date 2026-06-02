//
// TouchInjector.cpp
// Based on Microsoft Edge pan-tool approach:
// Single-contact touch injection at 100Hz with proper PerformanceCount timing.
// This triggers DirectManipulation scroll (rubber-band) in Windows 11 apps.
//
#include "TouchInjector.h"
#include <cstring>
#include <cstdio>

#pragma comment(lib, "winmm.lib")

TouchInjector::TouchInjector() = default;

TouchInjector::~TouchInjector() {
    if (m_hUser32) { FreeLibrary(m_hUser32); m_hUser32 = nullptr; }
}

bool TouchInjector::init() {
    m_hUser32 = LoadLibraryW(L"user32.dll");
    if (!m_hUser32) return false;

    // Try CreateSyntheticPointerDevice with PT_TOUCHPAD (value = 5)
    // This creates a virtual precision touchpad — no hardware needed
    typedef HSYNTHETICPOINTERDEVICE (WINAPI* PFN_Create)(DWORD, ULONG, DWORD);
    typedef BOOL (WINAPI* PFN_SynInject)(HSYNTHETICPOINTERDEVICE, const void*, UINT32);

    auto pfnCreate = (PFN_Create)GetProcAddress(m_hUser32, "CreateSyntheticPointerDevice");
    m_pfnSynInject = (void*)GetProcAddress(m_hUser32, "InjectSyntheticPointerInput");

    if (pfnCreate && m_pfnSynInject) {
        // PT_TOUCHPAD = 5, 2 contacts, POINTER_FEEDBACK_INDIRECT = 1
        m_synDevice = pfnCreate(5, 2, 1);
        if (m_synDevice) {
            m_useSynthetic = true;
            m_available = true;

            // Get QPC frequency
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            m_qpcFreq = freq.QuadPart;
            return true;
        }
    }

    // Fallback to InjectTouchInput (touchscreen — clicks things)
    m_pfnInit = (PFN_InitializeTouchInjection)GetProcAddress(m_hUser32, "InitializeTouchInjection");
    m_pfnInject = (PFN_InjectTouchInput)GetProcAddress(m_hUser32, "InjectTouchInput");

    if (m_pfnInit && m_pfnInject) {
        if (m_pfnInit(1, TOUCH_FEEDBACK_DEFAULT)) {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            m_qpcFreq = freq.QuadPart;
            m_available = true;
            return true;
        }
    }

    m_available = false;
    return false;
}

void TouchInjector::injectDown(long x, long y) {
    if (m_useSynthetic) {
        // Two-finger down for touchpad scroll
        injectTouchpadEvent(POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT, x, y);
    } else {
        // Legacy single-finger touch
        POINTER_TOUCH_INFO c = {};
        c.pointerInfo.pointerType = PT_TOUCH;
        c.pointerInfo.pointerId = 0;
        c.pointerInfo.ptPixelLocation.x = x;
        c.pointerInfo.ptPixelLocation.y = y;
        c.pointerInfo.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
        c.touchFlags = TOUCH_FLAG_NONE;
        c.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
        c.pressure = 512;
        c.rcContact = { x - 2, y - 2, x + 2, y + 2 };
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        c.pointerInfo.PerformanceCount = now.QuadPart;
        m_lastInjectTime = now.QuadPart;
        m_pfnInject(1, &c);
    }
    m_isDown = true;
    m_currentX = x;
    m_currentY = y;
}

bool TouchInjector::injectMove(long x, long y) {
    if (!m_isDown) return false;

    // Wait for frame interval (~10ms = 100Hz)
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    uint64_t intervalQpc = m_qpcFreq / 100;
    while ((uint64_t)(now.QuadPart - m_lastInjectTime) < intervalQpc) {
        QueryPerformanceCounter(&now);
    }
    m_lastInjectTime += intervalQpc;

    bool ok;
    if (m_useSynthetic) {
        ok = injectTouchpadEvent(POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT, x, y);
    } else {
        POINTER_TOUCH_INFO c = {};
        c.pointerInfo.pointerType = PT_TOUCH;
        c.pointerInfo.pointerId = 0;
        c.pointerInfo.ptPixelLocation.x = x;
        c.pointerInfo.ptPixelLocation.y = y;
        c.pointerInfo.pointerFlags = POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
        c.touchFlags = TOUCH_FLAG_NONE;
        c.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
        c.pressure = 512;
        c.rcContact = { x - 2, y - 2, x + 2, y + 2 };
        c.pointerInfo.PerformanceCount = m_lastInjectTime;
        ok = m_pfnInject(1, &c) != FALSE;
    }
    m_currentX = x;
    m_currentY = y;
    return ok;
}

void TouchInjector::injectUp() {
    if (!m_isDown) return;

    if (m_useSynthetic) {
        injectTouchpadEvent(POINTER_FLAG_UP, m_currentX, m_currentY);
    } else {
        POINTER_TOUCH_INFO c = {};
        c.pointerInfo.pointerType = PT_TOUCH;
        c.pointerInfo.pointerId = 0;
        c.pointerInfo.ptPixelLocation.x = m_currentX;
        c.pointerInfo.ptPixelLocation.y = m_currentY;
        c.pointerInfo.pointerFlags = POINTER_FLAG_UP;
        c.rcContact = { m_currentX - 2, m_currentY - 2, m_currentX + 2, m_currentY + 2 };
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        c.pointerInfo.PerformanceCount = now.QuadPart;
        m_pfnInject(1, &c);
    }
    m_isDown = false;
}

// Inject a 2-finger touchpad event via InjectSyntheticPointerInput
bool TouchInjector::injectTouchpadEvent(DWORD flags, long x, long y) {
    if (!m_pfnSynInject || !m_synDevice) return false;

    typedef BOOL (WINAPI* PFN_SynInject)(void*, const void*, UINT32);

    // POINTER_TYPE_INFO for PT_TOUCHPAD contains POINTER_TOUCHPAD_INFO
    // which is the same layout as POINTER_TOUCH_INFO
    // We send 2 contacts side by side
    struct TOUCHPAD_TYPE_INFO {
        POINTER_INPUT_TYPE type;
        POINTER_TOUCH_INFO info;
    };

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!(flags & POINTER_FLAG_DOWN)) {
        // Use timed injection for UPDATE/UP
    }

    TOUCHPAD_TYPE_INFO infos[2] = {};

    for (int i = 0; i < 2; i++) {
        infos[i].type = (POINTER_INPUT_TYPE)5; // PT_TOUCHPAD
        infos[i].info.pointerInfo.pointerType = (POINTER_INPUT_TYPE)5;
        infos[i].info.pointerInfo.pointerId = i;
        infos[i].info.pointerInfo.ptPixelLocation.x = x + (i == 0 ? -20 : 20);
        infos[i].info.pointerInfo.ptPixelLocation.y = y;
        infos[i].info.pointerInfo.pointerFlags = flags;
        if (i == 0) infos[i].info.pointerInfo.pointerFlags |= POINTER_FLAG_PRIMARY;
        infos[i].info.pointerInfo.PerformanceCount = now.QuadPart;
        infos[i].info.touchFlags = TOUCH_FLAG_NONE;
        infos[i].info.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_PRESSURE;
        infos[i].info.pressure = 512;
        infos[i].info.rcContact.left = infos[i].info.pointerInfo.ptPixelLocation.x - 3;
        infos[i].info.rcContact.right = infos[i].info.pointerInfo.ptPixelLocation.x + 3;
        infos[i].info.rcContact.top = y - 3;
        infos[i].info.rcContact.bottom = y + 3;
    }

    m_lastInjectTime = now.QuadPart;
    return ((PFN_SynInject)m_pfnSynInject)(m_synDevice, infos, 2) != FALSE;
}

bool TouchInjector::testInject() {
    if (!m_available) return false;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    injectDown(sw / 2, sh / 2);
    Sleep(16);
    bool ok = injectMove(sw / 2, sh / 2 - 10);
    Sleep(16);
    injectUp();
    return ok;
}

// Legacy — not used in new path
bool TouchInjector::submitReport(const WMF_PTP_REPORT&) {
    return false;
}
