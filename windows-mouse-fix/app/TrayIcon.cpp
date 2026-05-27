//
// TrayIcon.cpp
//

#include "TrayIcon.h"
#include <shellapi.h>
#include <commctrl.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")

TrayIcon* TrayIcon::s_instance = nullptr;

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    destroy();
}

bool TrayIcon::create(HINSTANCE hInstance,
                      ToggleCallback   onToggle,
                      SettingsCallback onSettings,
                      ExitCallback     onExit)
{
    m_onToggle   = onToggle;
    m_onSettings = onSettings;
    m_onExit     = onExit;
    s_instance   = this;

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    // Create message-only window (not visible)
    m_hwnd = CreateWindowExW(
        0, kClassName, L"WindowsMouseFix",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr
    );

    if (!m_hwnd) return false;

    // Add tray icon
    m_nid.cbSize           = sizeof(m_nid);
    m_nid.hWnd             = m_hwnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    m_nid.uCallbackMessage = kTrayMsgId;
    m_nid.hIcon            = LoadIcon(nullptr, IDI_APPLICATION); // placeholder icon
    wcscpy_s(m_nid.szTip, L"Windows Mouse Fix — Enabled");

    Shell_NotifyIconW(NIM_ADD, &m_nid);

    // Set version for balloon tips
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);

    m_created = true;
    return true;
}

void TrayIcon::destroy() {
    if (m_created) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_created = false;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void TrayIcon::setEnabled(bool enabled) {
    m_enabled = enabled;
    const wchar_t* tip = enabled
        ? L"Windows Mouse Fix — Enabled"
        : L"Windows Mouse Fix — Disabled";
    setTooltip(tip);
}

void TrayIcon::setTooltip(const wchar_t* text) {
    wcscpy_s(m_nid.szTip, text);
    m_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::runMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void TrayIcon::showContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Enable/Disable toggle
    AppendMenuW(hMenu, MF_STRING | (m_enabled ? MF_CHECKED : 0),
                kMenuEnable, L"Enabled");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, kMenuSettings, L"Settings...");
    AppendMenuW(hMenu, MF_STRING, kMenuAbout,    L"About");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, kMenuExit,     L"Exit");

    // Get cursor position
    POINT pt;
    GetCursorPos(&pt);

    // Required to make the menu dismiss when clicking elsewhere
    SetForegroundWindow(m_hwnd);

    UINT cmd = TrackPopupMenu(
        hMenu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        pt.x, pt.y, 0, m_hwnd, nullptr
    );

    DestroyMenu(hMenu);

    switch (cmd) {
    case kMenuEnable:
        m_enabled = !m_enabled;
        setEnabled(m_enabled);
        if (m_onToggle) m_onToggle(m_enabled);
        break;
    case kMenuSettings:
        if (m_onSettings) m_onSettings();
        break;
    case kMenuAbout:
        MessageBoxW(nullptr,
            L"Windows Mouse Fix v1.0\n\n"
            L"Translates mouse scroll wheel events into\n"
            L"Precision Touchpad input for smooth scrolling,\n"
            L"momentum, and rubber-band effects.\n\n"
            L"Inspired by Mac Mouse Fix by Noah Nuebling.\n"
            L"https://github.com/noah-nuebling/mac-mouse-fix",
            L"About Windows Mouse Fix",
            MB_OK | MB_ICONINFORMATION
        );
        break;
    case kMenuExit:
        if (m_onExit) m_onExit();
        PostQuitMessage(0);
        break;
    }
}

LRESULT CALLBACK TrayIcon::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!s_instance) return DefWindowProcW(hwnd, msg, wParam, lParam);

    if (msg == s_instance->kTrayMsgId) {
        UINT event = LOWORD(lParam);
        if (event == WM_RBUTTONUP || event == NIN_KEYSELECT) {
            s_instance->showContextMenu();
        }
        return 0;
    }

    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
