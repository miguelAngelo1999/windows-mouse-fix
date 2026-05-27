# Build Guide — Windows Mouse Fix

This guide covers everything needed to build, test, and install Windows Mouse Fix from source.

---

## Prerequisites

### Required for the user-mode app

| Tool | Version | Download |
|---|---|---|
| Visual Studio | 2022 (any edition) | https://visualstudio.microsoft.com/ |
| Windows SDK | 10.0.22621+ | Included with VS installer |
| CMake | 3.20+ | https://cmake.org/download/ |
| Git | Any | https://git-scm.com/ |

In the Visual Studio installer, select:
- **Desktop development with C++**
- **Windows 10/11 SDK** (latest)

### Required for the driver

| Tool | Version | Download |
|---|---|---|
| Windows Driver Kit (WDK) | Matching your Windows SDK | https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk |

> The WDK version must match your Windows SDK version exactly. The WDK installer will tell you which SDK version it targets.

### Required for the installer

| Tool | Version | Download |
|---|---|---|
| NSIS | 3.x | https://nsis.sourceforge.io/Download |

---

## Quick Start (App only, no driver)

If you just want to build and run the tests or the app without the driver:

```powershell
git clone https://github.com/miguelAngelo1999/windows-mouse-fix
cd windows-mouse-fix

mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release --target WmfTests

# Run tests
.\Release\WmfTests.exe
```

---

## Full Build (App + Driver)

### Step 1 — Clone

```powershell
git clone https://github.com/miguelAngelo1999/windows-mouse-fix
cd windows-mouse-fix
```

### Step 2 — Build the user-mode app

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Output: `build\Release\WindowsMouseFix.exe`

### Step 3 — Build the driver

The UMDF2 driver must be built with Visual Studio + WDK. CMake does not support WDK projects.

1. Open Visual Studio 2022
2. File → Open → Project/Solution → select `driver\WmfVirtualPad.vcxproj`

   > If the `.vcxproj` doesn't exist yet, create it:
   > File → New → Project → search "UMDF v2 Driver (Empty)" → name it `WmfVirtualPad` → add existing files from `driver\`

3. Set **Configuration** to `Release`, **Platform** to `x64`
4. Project Properties → Driver Settings → Target OS Version: `Windows 10 and later`
5. Build → Build Solution

Output: `driver\x64\Release\WmfVirtualPad.dll` + `WmfVirtualPad.cat`

### Step 4 — Enable test-signing (development only)

> Skip this step for release builds signed via Microsoft Hardware Dev Center.

Open an **Administrator** PowerShell:

```powershell
bcdedit /set testsigning on
```

Reboot. You will see a "Test Mode" watermark on the desktop — this is normal.

### Step 5 — Self-sign the driver (development only)

Open **x64 Native Tools Command Prompt for VS 2022** as Administrator:

```powershell
# Create a test certificate (one-time)
makecert -r -pe -ss PrivateCertStore -n "CN=WmfTestCert" WmfTestCert.cer

# Import it to Trusted Root (required for driver loading)
certutil -addstore Root WmfTestCert.cer

# Sign the driver DLL
signtool sign /s PrivateCertStore /n "WmfTestCert" /t http://timestamp.digicert.com driver\x64\Release\WmfVirtualPad.dll

# Generate catalog
inf2cat /driver:driver\ /os:10_X64,Server10_X64

# Sign the catalog
signtool sign /s PrivateCertStore /n "WmfTestCert" /t http://timestamp.digicert.com driver\WmfVirtualPad.cat
```

### Step 6 — Install the driver

Open **Administrator** PowerShell:

```powershell
pnputil /add-driver driver\WmfVirtualPad.inf /install
```

Expected output:
```
Microsoft PnP Utility
Adding driver package:  WmfVirtualPad.inf
Driver package added successfully.
Published Name:         oem42.inf
```

### Step 7 — Verify driver installation

Open **Device Manager** (Win+X → Device Manager):
- Expand **Human Interface Devices**
- You should see **"Windows Mouse Fix Virtual Precision Touchpad"**
- Right-click → Properties → Driver tab should show `PrecisionTouchPad.sys` as the function driver

### Step 8 — Run the app

```powershell
.\build\Release\WindowsMouseFix.exe
```

The app starts silently and appears in the system tray (bottom-right). Scroll your mouse wheel in Chrome or Edge — you should get smooth momentum scrolling with rubber-band overscroll.

---

## Running Tests

### Option A — Single-file compile (fastest, no CMake needed)

Open **x64 Native Tools Command Prompt for VS 2022**:

```powershell
cd tests
run_tests.bat
```

### Option B — CMake build

```powershell
cd tests
run_tests_cmake.bat
```

### Option C — Manual

```powershell
cd tests
cl /nologo /std:c++17 /EHsc /W3 /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0A00 /I..\shared /I..\app test_main.cpp /Fe:WmfTests.exe user32.lib advapi32.lib shell32.lib
WmfTests.exe
```

Expected output:
```
=== Windows Mouse Fix Unit Tests ===

ScrollAnalyzer:
  test_scroll_analyzer_first_tick... OK
  test_scroll_analyzer_consecutive... OK
  test_scroll_analyzer_timeout... OK
  test_scroll_analyzer_direction_change... OK

DragCurve:
  test_drag_curve_initial_velocity... OK
  ...

=== Results: 22 passed, 0 failed ===
```

---

## Building the Installer

Requires NSIS 3.x installed and `makensis` on PATH.

```powershell
# Copy built files to installer staging area first
copy build\Release\WindowsMouseFix.exe installer\
copy driver\WmfVirtualPad.dll installer\
copy driver\WmfVirtualPad.inf installer\
copy driver\WmfVirtualPad.cat installer\

# Build installer
cd installer
makensis installer.nsi
```

Output: `installer\WindowsMouseFix-Setup-1.0.0.exe`

---

## Uninstalling the Driver

```powershell
# Find the published name first
pnputil /enum-drivers | findstr -i wmf

# Remove it (replace oem42.inf with your actual published name)
pnputil /delete-driver oem42.inf /uninstall
```

Or simply run the uninstaller from Add/Remove Programs.

---

## Disabling Test-Signing (after development)

```powershell
bcdedit /set testsigning off
# Reboot
```

---

## Troubleshooting

**Driver won't install — "The third-party INF does not contain digital signature information"**
→ You're not in test-signing mode. Run `bcdedit /set testsigning on` and reboot.

**Driver installs but device shows Code 10 error**
→ The certificate isn't trusted. Run `certutil -addstore Root WmfTestCert.cer` as Administrator.

**App starts but scrolling has no effect**
→ Open Device Manager and confirm the virtual touchpad is present. If not, reinstall the driver.

**App starts but no tray icon**
→ Check the system tray overflow area (click the ^ arrow in the taskbar).

**Hook doesn't intercept elevated windows (e.g. Task Manager)**
→ Run `WindowsMouseFix.exe` as Administrator, or set the exe to always run as Administrator via Properties → Compatibility.

**`DwmFlush()` returns failure / animation stutters**
→ DWM composition may be disabled. The app falls back to 16ms Sleep. Enable "Visual Effects" in System Properties → Advanced → Performance.
