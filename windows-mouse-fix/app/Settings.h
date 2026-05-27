#pragma once

//
// Settings.h
// Application settings — persisted to %APPDATA%\WindowsMouseFix\config.json
//

#include <string>
#include <windows.h>

struct Settings {
    float speedMultiplier     = 1.0f;   // scroll speed multiplier (0.5 - 5.0)
    bool  accelerationEnabled = true;   // use acceleration curve
    bool  naturalScroll       = true;   // scroll direction matches finger movement
    bool  autoStart           = true;   // launch at Windows startup
    bool  enabled             = true;   // hook active

    // Load from config file. Returns true on success; uses defaults on failure.
    bool load();

    // Save to config file. Returns true on success.
    bool save() const;

    // Set/clear the Windows startup registry key.
    bool setAutoStart(bool enable) const;

    // Get the config file path: %APPDATA%\WindowsMouseFix\config.json
    static std::wstring configPath();

private:
    static std::wstring appDataDir();
};
