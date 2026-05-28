// test_hook.cpp
// Standalone diagnostic: installs WH_MOUSE_LL and prints scroll events to console.
// Run this to verify the hook works on this machine independently of the full app.
//
// Build: cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN
//           /D_WIN32_WINNT=0x0A00 test_hook.cpp /Fe:test_hook.exe user32.lib
// Run:   test_hook.exe  (then scroll your mouse — you should see output)

#include <windows.h>
#include <cstdio>
#include <atomic>

static HHOOK         g_hook     = nullptr;
static std::atomic<int> g_count = 0;

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
            MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lParam;
            int delta = (short)HIWORD(ms->mouseData);
            g_count++;
            printf("[HOOK] Scroll event #%d: delta=%d %s\n",
                g_count.load(), delta,
                wParam == WM_MOUSEHWHEEL ? "(horizontal)" : "(vertical)");
            fflush(stdout);
            // Don't suppress — let it pass through so normal scrolling still works
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

int main() {
    printf("=== Windows Mouse Fix — Hook Diagnostic ===\n");
    printf("Installing WH_MOUSE_LL hook...\n");

    g_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(nullptr), 0);

    if (!g_hook) {
        DWORD err = GetLastError();
        printf("FAILED to install hook! Error: %lu\n", err);
        printf("\nCommon causes:\n");
        printf("  - Error 5 (Access Denied): Run as Administrator\n");
        printf("  - Error 1428: No message loop on this thread (shouldn't happen here)\n");
        printf("\nPress Enter to exit.\n");
        getchar();
        return 1;
    }

    printf("Hook installed successfully! Handle: %p\n", (void*)g_hook);
    printf("\nScroll your mouse wheel now...\n");
    printf("(Press Ctrl+C or close window to stop)\n\n");

    // Message loop — required to keep the hook alive
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(g_hook);
    printf("\nHook removed. Total scroll events captured: %d\n", g_count.load());
    return 0;
}
