# Windows Mouse Fix

Translates mouse scroll wheel events into Windows Precision Touchpad (PTP) input, giving you native OS-level momentum scrolling, rubber-band/elastic overscroll, and smooth deceleration — the same experience as a MacBook trackpad, on any Windows mouse.

Inspired by [Mac Mouse Fix](https://github.com/noah-nuebling/mac-mouse-fix) by Noah Nuebling.

---

## How it works

```
Mouse scroll wheel
      ↓
WH_MOUSE_LL hook (intercepts + suppresses WM_MOUSEWHEEL)
      ↓
ScrollAnalyzer + DragCurve physics (smooth deceleration)
      ↓
ContactMapper (pixel delta → two-finger PTP contact positions)
      ↓
WmfVirtualPad UMDF2 driver (virtual HID Precision Touchpad)
      ↓
PrecisionTouchPad.sys (Microsoft's driver — handles momentum + rubber-band)
      ↓
Apps receive native touchpad scroll events
```

The virtual touchpad driver makes Windows treat the synthesized input identically to a real hardware precision touchpad. Rubber-band and momentum are handled by the OS, not by the app.

---

## Building

### Prerequisites

- Visual Studio 2022 (with C++ desktop workload)
- [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) — same version as your Windows SDK
- CMake 3.20+

### Build the user-mode app

```powershell
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Build the driver

The UMDF2 driver must be built with Visual Studio + WDK (CMake doesn't support WDK projects natively).

1. Open `driver/WmfVirtualPad.vcxproj` in Visual Studio
2. Set configuration to **Release x64**
3. Build → produces `WmfVirtualPad.dll` and `WmfVirtualPad.cat`

### Run tests

```powershell
cd build
ctest -C Release --output-on-failure
# or directly:
.\Release\WmfTests.exe
```

---

## Installation (Development / Test Signing)

> **Note:** For development, you need to enable test-signing mode. This is not required for release builds signed via Microsoft Hardware Dev Center.

### 1. Enable test-signing (one-time, requires reboot)

```powershell
# Run as Administrator
bcdedit /set testsigning on
# Reboot
```

### 2. Self-sign the driver (development only)

```powershell
# Create a test certificate
makecert -r -pe -ss PrivateCertStore -n "CN=WmfTestCert" WmfTestCert.cer

# Sign the driver
signtool sign /s PrivateCertStore /n "WmfTestCert" /t http://timestamp.digicert.com driver\WmfVirtualPad.dll

# Create catalog
inf2cat /driver:driver\ /os:10_X64
signtool sign /s PrivateCertStore /n "WmfTestCert" /t http://timestamp.digicert.com driver\WmfVirtualPad.cat
```

### 3. Install the driver

```powershell
# Run as Administrator
pnputil /add-driver driver\WmfVirtualPad.inf /install
```

### 4. Verify installation

Open Device Manager → Human Interface Devices → you should see **"Windows Mouse Fix Virtual Precision Touchpad"**.

### 5. Run the app

```powershell
build\Release\WindowsMouseFix.exe
```

The app appears in the system tray. Scroll your mouse wheel — you should get smooth momentum scrolling in Chrome, Edge, and other apps.

---

## Release Signing (Distribution)

For distributing to end users without requiring test-signing mode:

1. Create a Microsoft Hardware Dev Center account
2. Submit the driver package for **attestation signing** (free, no WHQL testing required)
3. Microsoft returns a signed `.cat` file
4. Use the NSIS installer: `makensis installer\installer.nsi`

---

## Project Structure

```
windows-mouse-fix/
├── shared/
│   └── WmfIoctl.h              # IOCTL codes + PTP report structs (shared)
├── driver/
│   ├── WmfVirtualPad.c         # UMDF2 driver implementation
│   ├── WmfVirtualPad.h
│   ├── HidReportDescriptor.h   # PTP HID descriptor bytes
│   └── WmfVirtualPad.inf       # Driver installation manifest
├── app/
│   ├── main.cpp                # Entry point
│   ├── MouseHook.cpp/.h        # WH_MOUSE_LL hook
│   ├── ScrollAnalyzer.cpp/.h   # Tick timing & swipe detection
│   ├── DragCurve.cpp/.h        # Physics: F = -c*v^e deceleration
│   ├── DragAnimator.cpp/.h     # Display-synced animation loop
│   ├── SubPixelator.h          # Fractional pixel accumulator
│   ├── AccelerationCurve.h     # Bezier scroll acceleration
│   ├── ContactMapper.cpp/.h    # Pixel delta → PTP contacts
│   ├── DriverClient.cpp/.h     # IOCTL communication with driver
│   ├── ScrollPipeline.cpp/.h   # Pipeline orchestration
│   ├── Settings.cpp/.h         # Config persistence
│   └── TrayIcon.cpp/.h         # System tray UI
├── tests/
│   └── test_main.cpp           # Unit tests
├── installer/
│   └── installer.nsi           # NSIS installer script
├── CMakeLists.txt
└── README.md
```

---

## Settings

Edit `%APPDATA%\WindowsMouseFix\config.json`:

```json
{
  "speedMultiplier": 1.0,
  "accelerationEnabled": true,
  "naturalScroll": true,
  "autoStart": true,
  "enabled": true
}
```

| Setting | Default | Description |
|---|---|---|
| `speedMultiplier` | 1.0 | Scale scroll distance (0.1–10.0) |
| `accelerationEnabled` | true | Faster spinning = more pixels per tick |
| `naturalScroll` | true | Scroll direction matches finger movement |
| `autoStart` | true | Launch at Windows startup |
| `enabled` | true | Enable/disable the hook |

---

## Physics

The scroll animation uses a drag curve model ported from Mac Mouse Fix:

```
dv/dt = -dragCoeff × |v|^(dragExp-1) × v
```

Default params: `dragCoeff=30.0`, `dragExp=0.7`, `stopSpeed=1.0`

This produces natural-feeling deceleration that matches the feel of macOS momentum scrolling. The animation runs display-synced via `DwmFlush()` (Windows equivalent of `CVDisplayLink`).

---

## License

MIT License. See [LICENSE](../License) file.

Physics model and architecture inspired by [Mac Mouse Fix](https://github.com/noah-nuebling/mac-mouse-fix) (MIT License).
