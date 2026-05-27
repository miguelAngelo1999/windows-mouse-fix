# Architecture — Windows Mouse Fix

## Overview

Windows Mouse Fix intercepts mouse scroll wheel events and translates them into Windows Precision Touchpad (PTP) HID contact reports. The OS's own `PrecisionTouchPad.sys` driver then handles momentum scrolling, rubber-band/elastic overscroll, and inertia — exactly as it would for a real hardware trackpad.

---

## Why a Virtual HID Driver?

On macOS, Mac Mouse Fix uses `CGEventPost()` to inject crafted scroll events with gesture phase fields (`kIOHIDEventPhaseBegan/Changed/Ended`). This works because rubber-band and momentum logic lives inside each app's `NSScrollView`.

On Windows, that logic lives in `PrecisionTouchPad.sys` — a kernel driver. Apps just receive already-processed scroll messages. To get rubber-band and momentum, you must feed raw contact data to `PrecisionTouchPad.sys` from below — which requires a virtual HID device.

`InjectTouchInput()` (the user-mode touch injection API) bypasses `PrecisionTouchPad.sys` entirely and won't produce rubber-band. The virtual HID driver is the only correct approach.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        USER MODE                                │
│                                                                 │
│  Physical Mouse ──WM_MOUSEWHEEL──► MouseHook (WH_MOUSE_LL)     │
│                                         │ suppressed            │
│                                         ▼                       │
│                                   ScrollThread                  │
│                                         │                       │
│                              ┌──────────▼──────────┐           │
│                              │   ScrollAnalyzer     │           │
│                              │   tick timing,       │           │
│                              │   speed, direction   │           │
│                              └──────────┬──────────┘           │
│                                         │ timeBetweenTicks      │
│                              ┌──────────▼──────────┐           │
│                              │  AccelerationCurve   │           │
│                              │  Bezier: speed→px    │           │
│                              └──────────┬──────────┘           │
│                                         │ pxForThisTick         │
│                              ┌──────────▼──────────┐           │
│                              │    DragAnimator      │           │
│                              │  DwmFlush() vsync    │           │
│                              │  DragCurve physics   │           │
│                              └──────────┬──────────┘           │
│                                         │ px delta/frame        │
│                              ┌──────────▼──────────┐           │
│                              │    SubPixelator      │           │
│                              │  fractional accum    │           │
│                              └──────────┬──────────┘           │
│                                         │ int pixels            │
│                              ┌──────────▼──────────┐           │
│                              │   ContactMapper      │           │
│                              │  px → 2-finger PTP   │           │
│                              │  contact positions   │           │
│                              └──────────┬──────────┘           │
│                                         │ WMF_PTP_REPORT        │
│                              ┌──────────▼──────────┐           │
│                              │    DriverClient      │           │
│                              │  DeviceIoControl     │           │
│                              └──────────┬──────────┘           │
└─────────────────────────────────────────┼───────────────────────┘
                          IOCTL_WMF_SUBMIT_REPORT
┌─────────────────────────────────────────┼───────────────────────┐
│                       KERNEL MODE       │                       │
│                                         ▼                       │
│                    ┌────────────────────────────────────┐       │
│                    │   WmfVirtualPad.dll  (UMDF2)        │       │
│                    │                                    │       │
│                    │   Virtual HID device               │       │
│                    │   PTP report descriptor            │       │
│                    │   Forwards bytes to hidclass.sys   │       │
│                    └────────────────┬───────────────────┘       │
│                                     │                           │
│                    ┌────────────────▼───────────────────┐       │
│                    │   PrecisionTouchPad.sys (Microsoft) │       │
│                    │                                    │       │
│                    │   Interprets 2-finger contacts     │       │
│                    │   Applies momentum + rubber-band   │       │
│                    │   Delivers WM_POINTER to apps      │       │
│                    └────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────────┘
```

---

## Threading Model

```
Main Thread
  └── Win32 message loop (tray icon, WM_QUIT)

Hook Thread  (created by MouseHook)
  └── GetMessage loop — required to keep WH_MOUSE_LL alive
  └── lowLevelMouseProc callback — enqueues ScrollEvent, returns fast

ScrollThread  (high priority)
  └── Dequeues ScrollEvents from lock-free queue
  └── Runs ScrollAnalyzer, AccelerationCurve
  └── Calls DragAnimator::start()

AnimationThread  (time-critical priority, inside DragAnimator)
  └── DwmFlush() — blocks until vsync
  └── Evaluates DragCurve::position(t) for frame delta
  └── Calls FrameCallback → ContactMapper → DriverClient::submitReport()

LiftThread  (inside ScrollPipeline)
  └── Waits on condition variable
  └── After 80ms idle: sends finger-lift PTP report
  └── Cancelled immediately when new scroll tick arrives
```

**Key invariant:** The hook callback never blocks. It only pushes to a queue. All physics runs on ScrollThread. All output runs on AnimationThread. These are the only two threads that touch the driver.

---

## Component Responsibilities

### MouseHook
Installs `WH_MOUSE_LL` system-wide. Intercepts `WM_MOUSEWHEEL` and `WM_MOUSEHWHEEL`, extracts the delta, and enqueues a `ScrollEvent`. Returns `1` to suppress the original event so no application receives it.

### ScrollAnalyzer
Tracks the timing of consecutive scroll ticks. Two ticks are "consecutive" if they occur within `consecutiveTickIntervalMax` (350ms) of each other in the same direction. Computes `timeBetweenTicks` (used for acceleration) and `consecutiveSwipeCounter` (used for fast-scroll). Resets on direction change or timeout.

Port of Mac Mouse Fix's `ScrollAnalyzer.m`.

### AccelerationCurve
Maps scroll speed (ticks/second) to pixels-per-tick using a cubic Bezier curve. Faster spinning = more pixels per tick. Default control points tuned to match macOS feel.

### DragCurve
Physics model for scroll deceleration:
```
dv/dt = -dragCoeff × |v|^(dragExp-1) × v
```
Default: `dragCoeff=30.0`, `dragExp=0.7`, `stopSpeed=1.0` (same as Mac Mouse Fix).

Provides closed-form `velocity(t)` and `position(t)` — no numerical integration needed, so it's exact and cheap.

Port of Mac Mouse Fix's `DragCurve.swift`.

### DragAnimator
Drives the per-frame output. Syncs to display vsync via `DwmFlush()`. On each frame, evaluates `DragCurve::position(t_now) - position(t_prev)` for the delta.

**Restart logic:** When a new scroll tick arrives while animation is running, the remaining distance is added to the new tick's distance. This gives the "accumulating momentum" feel — continuous scrolling builds up speed naturally.

Port of Mac Mouse Fix's `TouchAnimator.swift`.

### SubPixelator
Accumulates fractional pixel values across frames to prevent rounding drift. Without this, slow scrolling would produce no output (all deltas round to 0) or jerky output.

### ContactMapper
Converts integer pixel deltas into two-finger PTP contact positions in the 4096×4096 logical coordinate space. Two contacts are placed at fixed X positions (1500 and 2500) and share a Y coordinate that moves based on cumulative scroll. When `liftFingers=true`, sets `contact_count=0` to signal finger lift to `PrecisionTouchPad.sys`.

### DriverClient
Opens `\\.\WmfVirtualPad` via `CreateFile` and submits `WMF_PTP_REPORT` structs via `DeviceIoControl(IOCTL_WMF_SUBMIT_REPORT)`. Auto-reconnects if the driver is restarted.

### WmfVirtualPad (UMDF2 Driver)
A minimal UMDF2 driver that:
1. Presents a virtual HID device with the PTP report descriptor
2. Responds to `IOCTL_HID_GET_REPORT_DESCRIPTOR` with the descriptor bytes
3. Responds to `IOCTL_HID_GET_DEVICE_ATTRIBUTES` with VID/PID
4. Responds to `IOCTL_HID_GET_FEATURE` with contact count maximum
5. Pends `IOCTL_HID_READ_REPORT` requests from `hidclass.sys`
6. On `IOCTL_WMF_SUBMIT_REPORT` from user mode: completes the oldest pending read with the report data

The driver is a dumb pipe. All logic lives in user mode.

### ScrollPipeline
Wires all components together. Manages the scroll worker thread, the lift timer thread, and the interaction between them. Handles direction changes (cancels animation, resets state). Applies settings (speed multiplier, natural scroll direction).

---

## PTP HID Report Format

The virtual device presents a standard Windows Precision Touchpad HID descriptor. Each input report (report ID `0x01`) contains:

```
Offset  Size  Field
------  ----  -----
0       1     Report ID (0x01)
1       1     Contact 0: tip_switch(1) + confidence(1) + padding(6)
2       1     Contact 0: contact_id
3       2     Contact 0: X (0-4095)
5       2     Contact 0: Y (0-4095)
7       1     Contact 1: tip_switch(1) + confidence(1) + padding(6)
8       1     Contact 1: contact_id
9       2     Contact 1: X (0-4095)
11      2     Contact 1: Y (0-4095)
13-27         Contacts 2-4 (zeroed, unused)
28      1     Contact Count (0 or 2)
29      2     Scan Time (100µs units)
Total: 31 bytes
```

**Two-finger scroll gesture:**
- `contact_count = 2`, both contacts `tip_switch=1, confidence=1`
- Both contacts move in the same Y direction (up for scroll-down)
- Fixed X separation: contact 0 at X=1500, contact 1 at X=2500

**Finger lift (triggers OS momentum):**
- `contact_count = 0`, all contacts zeroed

---

## Scroll-to-Contact Coordinate Mapping

```
Mouse wheel delta (WHEEL_DELTA units, ±120 per notch)
    ↓ AccelerationCurve
pixels per tick (e.g. 80px)
    ↓ DragAnimator (spread over ~0.8s)
~3px per frame at 60fps
    ↓ SubPixelator
integer pixels (e.g. 3)
    ↓ ContactMapper
Y position change: currentY -= 3  (scroll down = contacts move up)
    ↓
PTP report: contacts[0].y = 2045, contacts[1].y = 2045
```

The Y coordinate starts at 2048 (center of the 4096-unit space) at the beginning of each gesture and moves from there. It's clamped to [200, 3896] to stay within the touchpad bounds.

---

## Physics Deep Dive

The drag curve models a decelerating object subject to velocity-dependent drag:

```
dv/dt = -c × |v|^(e-1) × v
```

For `e < 1` (our case: `e = 0.7`), this has a closed-form solution:

```
v(t) = v₀ / (1 + (1-e)·c·v₀^(e-1)·t)^(1/(1-e))
```

The position integral is also analytical:

```
x(t) = v₀ / (K·(α+1)) × [1 - (1 - K·t)^(α+1)]

where K = (1-e)·c·v₀^(e-1)
      α = 1/(1-e)
```

This gives us exact values at any time `t` without numerical integration — important for a display-synced loop where we need `position(t_now) - position(t_prev)` on every frame.

The animation stops when `v(t) ≤ stopSpeed` (default 1.0 px/s). At that point, the finger lift is sent and `PrecisionTouchPad.sys` takes over with its own momentum phase.

---

## Key Design Decisions

**UMDF2 over KMDF:** UMDF2 drivers run in user mode (hosted by `WUDFHost.exe`). A bug won't blue-screen the machine. Performance overhead is negligible at 120Hz HID input.

**No `InjectTouchInput`:** This API inserts above `PrecisionTouchPad.sys`, bypassing it entirely. Rubber-band and momentum won't work.

**Physics in user mode:** The driver is a dumb pipe. All interesting logic is in the app, making it easy to iterate without touching the driver or rebooting.

**`DwmFlush()` for vsync:** Blocks until the next DWM vsync event — the Windows equivalent of `CVDisplayLink` on macOS. Falls back to 16ms sleep if DWM composition is disabled.

**Lift timer:** After 80ms of no new scroll ticks, fingers are lifted. This triggers `PrecisionTouchPad.sys`'s momentum phase. The timer is cancelled immediately when a new tick arrives, so continuous scrolling never lifts.
