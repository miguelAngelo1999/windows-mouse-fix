#pragma once

//
// MouseHook.h
// Low-level mouse hook that intercepts WM_MOUSEWHEEL system-wide.
//
// Key design:
// - Hook runs on a dedicated TIME_CRITICAL thread with a PeekMessage loop
// - Callback must be fast (just enqueue) — never block in the hook proc
// - Uses MsgWaitForMultipleObjects to stay responsive without burning CPU
// - Automatically re-installs if Windows removes the hook under load
//

#include <windows.h>
#include <functional>
#include <thread>
#include <atomic>

struct ScrollEvent {
    int   delta;          // WHEEL_DELTA units (+120 = up, -120 = down)
    bool  isHorizontal;   // true for WM_MOUSEHWHEEL
    DWORD timestamp;      // ms timestamp from hook struct
};

class MouseHook {
public:
    using ScrollCallback = std::function<void(const ScrollEvent&)>;

    // Marker on injected events so we don't re-intercept them
    static const ULONG_PTR kWmfMarker;

    MouseHook();
    ~MouseHook();

    // Install the hook.
    // suppressEvents: true = swallow original scroll events (we re-inject smoothly)
    bool install(ScrollCallback cb, bool suppressEvents = true);

    void uninstall();

    bool isRunning() const { return m_running.load(); }

    void setSuppressEvents(bool s) { m_suppressEvents.store(s); }

private:
    void hookThreadFunc();
    bool IsHookInstalled();

    static LRESULT CALLBACK lowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    HHOOK               m_hook      = nullptr;
    std::thread         m_thread;
    std::atomic<bool>   m_shutdown        { false };
    std::atomic<bool>   m_running         { false };
    std::atomic<bool>   m_suppressEvents  { true  };
    DWORD               m_threadId        = 0;

    static MouseHook*   s_instance;
    ScrollCallback      m_callback;
};
