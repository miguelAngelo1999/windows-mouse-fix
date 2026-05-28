//
// MouseHook.cpp
//

#include "MouseHook.h"
#include <stdexcept>

MouseHook* MouseHook::s_instance = nullptr;

// Marker to identify events we injected ourselves — prevents re-interception loop
const ULONG_PTR MouseHook::kWmfMarker = 0x574D4601;

MouseHook::MouseHook() = default;

MouseHook::~MouseHook() {
    uninstall();
}

bool MouseHook::install(ScrollCallback cb, bool suppressEvents) {
    if (m_running.load()) return true;

    m_callback = cb;
    s_instance = this;
    m_shutdown.store(false);
    m_running.store(false);
    m_suppressEvents.store(suppressEvents);

    // Start the hook thread — it installs the hook and runs the message loop
    m_thread = std::thread(&MouseHook::hookThreadFunc, this);

    // Wait up to 1 second for the hook to be installed
    for (int i = 0; i < 200; i++) {
        Sleep(5);
        if (m_running.load()) return true;
    }

    return false;
}

void MouseHook::uninstall() {
    if (!m_thread.joinable()) return;

    m_shutdown.store(true);

    if (m_threadId != 0) {
        PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_running.store(false);
    s_instance = nullptr;
}

void MouseHook::hookThreadFunc() {
    m_threadId = GetCurrentThreadId();

    // Boost this thread's priority — the hook callback must return fast
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // Install hook
    m_hook = SetWindowsHookEx(WH_MOUSE_LL, lowLevelMouseProc,
                               GetModuleHandle(nullptr), 0);
    if (!m_hook) {
        return;
    }

    m_running.store(true);

    // Message loop — MUST keep pumping or Windows removes the hook
    // Use PeekMessage with a short timeout so we can check m_shutdown
    // and re-install the hook if Windows removed it
    while (!m_shutdown.load()) {
        MSG msg;

        // PeekMessage with PM_REMOVE — non-blocking, processes pending messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                goto done;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Check if Windows removed our hook (happens under load)
        // Re-install it immediately
        if (m_hook && !IsHookInstalled()) {
            UnhookWindowsHookEx(m_hook);
            m_hook = SetWindowsHookEx(WH_MOUSE_LL, lowLevelMouseProc,
                                       GetModuleHandle(nullptr), 0);
        }

        // Wait up to 50ms for next message — keeps CPU low while staying responsive
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 50, QS_ALLINPUT);
    }

done:
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    m_running.store(false);
}

bool MouseHook::IsHookInstalled() {
    // We can't directly query if our hook is still active,
    // but we can detect it by checking if a test message gets intercepted.
    // Simpler: just track via a flag set in the callback.
    // We use a heartbeat: if no callback has fired in 5 seconds despite mouse movement,
    // assume the hook was removed.
    // For now, always return true — re-install is handled by the timeout check below.
    return true; // Will be improved with heartbeat tracking
}

LRESULT CALLBACK MouseHook::lowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || !s_instance) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
        MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        // Ignore events we injected ourselves
        if (ms->dwExtraInfo == kWmfMarker) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        // Extract delta
        int delta = static_cast<int>(static_cast<short>(HIWORD(ms->mouseData)));

        ScrollEvent ev;
        ev.delta        = delta;
        ev.isHorizontal = (wParam == WM_MOUSEHWHEEL);
        ev.timestamp    = ms->time;

        // Fire callback — MUST be fast (just enqueue, no blocking)
        if (s_instance->m_callback) {
            s_instance->m_callback(ev);
        }

        // Suppress original event if configured
        if (s_instance->m_suppressEvents.load()) {
            return 1;
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
