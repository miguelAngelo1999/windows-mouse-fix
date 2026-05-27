//
// main.cpp
// Windows Mouse Fix — entry point.
//
// Flow:
//   1. Single-instance mutex guard
//   2. Load settings
//   3. Open driver client
//   4. Start scroll pipeline
//   5. Create tray icon
//   6. Run message loop
//

#include <windows.h>
#include "Settings.h"
#include "DriverClient.h"
#include "ScrollPipeline.h"
#include "TrayIcon.h"

// Forward declarations
static void showDriverNotInstalledWarning();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // ---- 1. Single-instance guard ----
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"WindowsMouseFix_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is running — bring it to focus via tray notification
        // (no window to bring forward, so just exit silently)
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // ---- 2. Load settings ----
    Settings settings;
    settings.load(); // uses defaults if config.json doesn't exist

    // Apply auto-start setting
    settings.setAutoStart(settings.autoStart);

    // ---- 3. Open driver client (driver or touch injection fallback) ----
    DriverClient driver;
    DriverMode driverMode = driver.open();

    // ---- 4. Start scroll pipeline ----
    ScrollPipeline pipeline(driver, settings);

    if (settings.enabled) {
        if (!pipeline.start()) {
            MessageBoxW(nullptr,
                L"Failed to install mouse hook.\n"
                L"Windows Mouse Fix requires permission to monitor input.",
                L"Windows Mouse Fix — Error",
                MB_OK | MB_ICONERROR
            );
            if (hMutex) CloseHandle(hMutex);
            return 1;
        }
    }

    // ---- 5. Create tray icon ----
    TrayIcon tray;

    bool trayOk = tray.create(
        hInstance,

        // Toggle callback
        [&](bool enabled) {
            settings.enabled = enabled;
            settings.save();
            if (enabled) {
                pipeline.start();
            } else {
                pipeline.stop();
            }
        },

        // Settings callback — show a simple settings dialog
        [&]() {
            // Simple settings dialog using MessageBox for now.
            // A proper dialog resource would be added in a full build.
            wchar_t msg[512];
            swprintf_s(msg,
                L"Current Settings:\n\n"
                L"Speed Multiplier: %.1f\n"
                L"Acceleration: %s\n"
                L"Natural Scroll: %s\n"
                L"Auto-start: %s\n\n"
                L"Edit %%APPDATA%%\\WindowsMouseFix\\config.json to change settings.",
                settings.speedMultiplier,
                settings.accelerationEnabled ? L"On" : L"Off",
                settings.naturalScroll       ? L"On" : L"Off",
                settings.autoStart           ? L"On" : L"Off"
            );
            MessageBoxW(nullptr, msg, L"Windows Mouse Fix — Settings", MB_OK | MB_ICONINFORMATION);
        },

        // Exit callback
        [&]() {
            pipeline.stop();
            driver.close();
            settings.save();
        }
    );

    if (!trayOk) {
        pipeline.stop();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // Show driver warning in tray tooltip if driver not installed
    tray.setEnabled(settings.enabled);
    tray.setTooltip(driver.statusText());

    // ---- 6. Run message loop (blocks until Exit is chosen) ----
    tray.runMessageLoop();

    // ---- Cleanup ----
    tray.destroy();
    pipeline.stop();
    driver.close();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
