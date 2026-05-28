//
// MouseHook.cpp
//

#include "MouseHook.h"
#include <stdexcept>

MouseHook* MouseHook::s_instance = nullptr;

MouseHook::MouseHook() = default;

MouseHook::~MouseHook() {
    uninstall();
}

bool MouseHook::install(ScrollCallback cb, bool suppressEvents) {
    if (m_hook) return true;

    m_callback  = cb;
    s_instance  = this;
    m_shutdown.store(false);
    m_suppressEvents.store(suppressEvents);

    // The hook MUST be installed on a thread that runs GetMessage().
    // We create a dedicated thread for this.
    bool hookInstalled = false;
    HANDLE readyEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    m_thread = std::thread([this, readyEvent, &hookInstalled]() {
        hookThreadFunc();
    });

    // Wait for the hook thread to install the hook
    // (hookThreadFunc signals via PostThreadMessage after installing)
    // Simple approach: spin-wait with timeout
    for (int i = 0; i < 100; i++) {
        Sleep(10);
        if (m_hook != nullptr) {
            hookInstalled = true;
            break;
        }
    }

    CloseHandle(readyEvent);
    return hookInstalled;
}

void MouseHook::uninstall() {
    if (!m_hook && !m_thread.joinable()) return;

    m_shutdown.store(true);

    // Post WM_QUIT to the hook thread's message loop
    if (m_threadId != 0) {
        PostThreadMessage(m_threadId, WM_QUIT, 0, 0);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_hook     = nullptr;
    s_instance = nullptr;
}

void MouseHook::hookThreadFunc() {
    m_threadId = GetCurrentThreadId();

    // Install the hook on this thread
    m_hook = SetWindowsHookEx(
        WH_MOUSE_LL,
        lowLevelMouseProc,
        GetModuleHandle(nullptr),
        0  // 0 = system-wide
    );

    if (!m_hook) {
        return;
    }

    // Run message loop — required to keep the hook alive and receive callbacks
    MSG msg;
    while (!m_shutdown.load()) {
        BOOL ret = GetMessage(&msg, nullptr, 0, 0);
        if (ret == 0 || ret == -1) break; // WM_QUIT or error
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
}

LRESULT CALLBACK MouseHook::lowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || !s_instance) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
        MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        // Ignore events we injected ourselves (marked with kWmfMarker)
        static const ULONG_PTR kWmfMarker = 0x574D4601;
        if (ms->dwExtraInfo == kWmfMarker) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        // Extract delta from mouseData high word
        int delta = static_cast<int>(static_cast<short>(HIWORD(ms->mouseData)));

        ScrollEvent ev;
        ev.delta        = delta;
        ev.isHorizontal = (wParam == WM_MOUSEHWHEEL);
        ev.timestamp    = ms->time;

        // Fire callback (must be fast — just enqueue)
        if (s_instance->m_callback) {
            s_instance->m_callback(ev);
        }

        // Only suppress if the pipeline is active and will re-inject
        // (suppression is handled by ScrollPipeline based on driver mode)
        if (s_instance->m_suppressEvents) {
            return 1;
        }

        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
