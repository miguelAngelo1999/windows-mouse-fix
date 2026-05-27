#pragma once

//
// TrayIcon.h
// System tray icon with context menu.
//

#include <windows.h>
#include <shellapi.h>
#include <functional>
#include "Settings.h"

// Callback types for tray actions
using ToggleCallback   = std::function<void(bool enabled)>;
using SettingsCallback = std::function<void()>;
using ExitCallback     = std::function<void()>;

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool create(HINSTANCE hInstance,
                ToggleCallback   onToggle,
                SettingsCallback onSettings,
                ExitCallback     onExit);

    void destroy();

    // Update the icon to reflect enabled/disabled state
    void setEnabled(bool enabled);

    // Update tooltip text
    void setTooltip(const wchar_t* text);

    // Run the message loop (blocks until WM_QUIT)
    void runMessageLoop();

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void showContextMenu();

    HWND            m_hwnd      = nullptr;
    NOTIFYICONDATAW m_nid       = {};
    bool            m_created   = false;
    bool            m_enabled   = true;

    ToggleCallback   m_onToggle;
    SettingsCallback m_onSettings;
    ExitCallback     m_onExit;

    static TrayIcon* s_instance;

    // Window class name and tray message ID
    static constexpr wchar_t kClassName[]  = L"WmfTrayWindow";
    static constexpr UINT    kTrayMsgId    = WM_APP + 1;
    static constexpr UINT    kMenuEnable   = 1001;
    static constexpr UINT    kMenuSettings = 1002;
    static constexpr UINT    kMenuAbout    = 1003;
    static constexpr UINT    kMenuExit     = 1004;
};
