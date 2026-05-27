#pragma once

//
// MouseHook.h
// Low-level mouse hook that intercepts WM_MOUSEWHEEL system-wide.
//

#include <windows.h>
#include <functional>
#include <thread>
#include <atomic>

struct ScrollEvent {
    int   delta;          // WHEEL_DELTA units (positive = up/away, negative = down/toward)
    bool  isHorizontal;   // true for WM_MOUSEHWHEEL
    DWORD timestamp;      // GetMessageTime() result in ms
};

class MouseHook {
public:
    using ScrollCallback = std::function<void(const ScrollEvent&)>;

    MouseHook();
    ~MouseHook();

    // Install the hook. The callback is called on the hook thread (fast — just enqueues).
    // Returns false if SetWindowsHookEx fails.
    bool install(ScrollCallback cb);

    // Uninstall the hook and stop the hook thread.
    void uninstall();

    bool isInstalled() const { return m_hook != nullptr; }

private:
    // The hook must be installed on a thread that runs a message loop.
    void hookThreadFunc();

    static LRESULT CALLBACK lowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    HHOOK               m_hook      = nullptr;
    std::thread         m_thread;
    std::atomic<bool>   m_shutdown  { false };
    DWORD               m_threadId  = 0;

    // Static pointer to the single instance (WH_MOUSE_LL callback is static)
    static MouseHook*   s_instance;
    ScrollCallback      m_callback;
};
