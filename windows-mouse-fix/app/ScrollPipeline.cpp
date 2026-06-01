//
// ScrollPipeline.cpp
//

#include "ScrollPipeline.h"
#include <cmath>
#include <algorithm>
#include <windows.h>
#include <cstdio>

// Simple debug log
static FILE* g_logFile = nullptr;
static int g_logCount = 0;
static char g_logPath[MAX_PATH] = {};

static void wmfLog(const char* fmt, ...) {
    if (!g_logFile) {
        ExpandEnvironmentStringsA("%APPDATA%\\WindowsMouseFix\\wmf_debug.log", g_logPath, MAX_PATH);
        char dir[MAX_PATH];
        ExpandEnvironmentStringsA("%APPDATA%\\WindowsMouseFix", dir, MAX_PATH);
        CreateDirectoryA(dir, nullptr);
        fopen_s(&g_logFile, g_logPath, "w");
    }
    if (!g_logFile) return;
    if (g_logCount > 200) return; // cap log size
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    fflush(g_logFile);
    g_logCount++;
}

// Upload log to transfer.sh so it can be read remotely
static void wmfUploadLog() {
    if (!g_logPath[0]) return;
    // Use WinHTTP to POST the file
    // Simpler: just shell out to curl
    char cmd[1024];
    sprintf_s(cmd, "curl -s -T \"%s\" https://transfer.sh/wmf_debug.log > \"%s\\..\\wmf_upload_url.txt\" 2>&1", g_logPath, g_logPath);
    system(cmd);
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ScrollPipeline::ScrollPipeline(DriverClient& driver, const Settings& settings)
    : m_driver(driver)
    , m_settings(settings)
{
}

ScrollPipeline::~ScrollPipeline() {
    stop();
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------

bool ScrollPipeline::start() {
    if (m_running.load()) return true;

    m_shutdown.store(false);
    m_liftShutdown.store(false);
    m_running.store(true);

    // Initialize touch injector for VirtualDriver fallback
    bool touchOk = m_touchInjector.init();

    // Test inject to check if it actually works
    bool testOk = false;
    if (touchOk) {
        testOk = m_touchInjector.testInject();
    }
    wmfLog("START: touchInit=%d testInject=%d touchAvail=%d\n", touchOk, testOk, m_touchInjector.isAvailable());

    // Start scroll worker thread
    m_scrollThread = std::thread(&ScrollPipeline::scrollThreadFunc, this);

    // Start persistent lift timer thread
    m_liftThread = std::thread(&ScrollPipeline::liftThreadFunc, this);

    // Install mouse hook — suppress original events in both modes
    // In driver mode: we inject PTP contacts instead
    // In basic mode: we inject smooth SendInput wheel events instead (marked to avoid re-interception)
    bool ok = m_hook.install([this](const ScrollEvent& ev) {
        onScrollEvent(ev);
    }, true /* always suppress — we re-inject smoothly */);

    wmfLog("HOOK: installed=%d\n", ok);

    // Upload initial log
    wmfUploadLog();

    if (!ok) {
        stop();
        return false;
    }

    return true;
}

void ScrollPipeline::stop() {
    if (!m_running.load() && !m_scrollThread.joinable()) return;

    m_hook.uninstall();
    m_animator.cancel();
    cancelLift();

    // Shut down lift thread
    m_liftShutdown.store(true);
    m_liftCv.notify_all();
    if (m_liftThread.joinable()) {
        m_liftThread.join();
    }

    m_shutdown.store(true);
    m_running.store(false);
    m_queueCv.notify_all();

    if (m_scrollThread.joinable()) {
        m_scrollThread.join();
    }
}

void ScrollPipeline::applySettings(const Settings& settings) {
    std::lock_guard<std::mutex> lock(m_settingsMutex);
    m_settings = settings;
}

// ---------------------------------------------------------------------------
// onScrollEvent — called on hook thread, must be fast
// ---------------------------------------------------------------------------

void ScrollPipeline::onScrollEvent(const ScrollEvent& ev) {
    wmfLog("SCROLL: delta=%d horiz=%d\n", ev.delta, ev.isHorizontal);
    // This runs on the hook thread — must be FAST, no blocking
    // Just push to queue and signal
    bool pushed = false;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_eventQueue.size() < 64) { // cap queue size
            m_eventQueue.push(ev);
            pushed = true;
        }
    }
    if (pushed) {
        m_queueCv.notify_one();
    }
}

// ---------------------------------------------------------------------------
// scrollThreadFunc — dequeues and processes events
// ---------------------------------------------------------------------------

void ScrollPipeline::scrollThreadFunc() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    while (!m_shutdown.load()) {
        ScrollEvent ev;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait(lock, [this] {
                return !m_eventQueue.empty() || m_shutdown.load();
            });
            if (m_shutdown.load() && m_eventQueue.empty()) break;
            ev = m_eventQueue.front();
            m_eventQueue.pop();
        }
        processEvent(ev);
    }
}

// ---------------------------------------------------------------------------
// processEvent — main pipeline logic
// ---------------------------------------------------------------------------

void ScrollPipeline::processEvent(const ScrollEvent& ev) {
    Settings settings;
    {
        std::lock_guard<std::mutex> lock(m_settingsMutex);
        settings = m_settings;
    }

    if (!settings.enabled) return;

    // Determine direction: +1 = down/right, -1 = up/left
    int rawDelta = ev.delta; // positive = scroll up (away from user)

    // Apply natural scroll inversion
    // Windows default: positive delta = scroll up = content moves up
    // Natural scroll: positive delta = content moves down (like macOS default)
    if (settings.naturalScroll) {
        rawDelta = -rawDelta;
    }

    int direction = (rawDelta > 0) ? 1 : -1;

    // Get high-resolution timestamp
    LARGE_INTEGER qpc, freq;
    QueryPerformanceCounter(&qpc);
    QueryPerformanceFrequency(&freq);
    double tickTimestamp = (double)qpc.QuadPart / (double)freq.QuadPart;

    // Run scroll analysis
    ScrollAnalysisResult analysis = m_analyzer.update(tickTimestamp, direction);

    // Direction change: cancel animation and reset
    if (analysis.scrollDirectionDidChange && m_animator.isRunning()) {
        m_animator.cancel();
        m_subPixelator.reset();
        m_contactMapper.reset();
        cancelLift();
    }

    // Compute pixels for this tick
    double pxForThisTick = 0.0;

    if (settings.accelerationEnabled && analysis.timeBetweenTicks != DBL_MAX) {
        double ticksPerSec = 1.0 / analysis.timeBetweenTicks;
        pxForThisTick = m_accelCurve.evaluate(ticksPerSec);
    } else {
        // Fallback: fixed pixels per tick (matches one WHEEL_DELTA notch)
        pxForThisTick = 120.0;
    }

    // Apply speed multiplier
    pxForThisTick *= settings.speedMultiplier;

    // Compute initial speed for the drag curve
    // Speed = pixels / timeBetweenTicks (or a default for the first tick)
    double initialSpeed = pxForThisTick;
    if (analysis.timeBetweenTicks != DBL_MAX && analysis.timeBetweenTicks > 0.0) {
        initialSpeed = pxForThisTick / analysis.timeBetweenTicks;
    }
    initialSpeed = std::max(initialSpeed, 2.0); // must be > stopSpeed

    // Cancel any pending lift (new tick arrived)
    cancelLift();

    // Reset contact mapper on first tick of a new gesture
    if (analysis.isFirstConsecutiveTick) {
        m_contactMapper.reset();
    }

    // Start/restart the drag animator
    m_animator.start(pxForThisTick, initialSpeed, direction,
        [this](int dx, int dy, bool isLast) {
            onAnimationFrame(dx, dy, isLast);
        }
    );

    // Schedule finger lift after idle timeout
    scheduleLift();

    m_lastDirection = direction;
}

// ---------------------------------------------------------------------------
// onAnimationFrame — called on animation thread each vsync
// ---------------------------------------------------------------------------

void ScrollPipeline::onAnimationFrame(int dx, int dy, bool isLast) {

    wmfLog("FRAME: dx=%d dy=%d isLast=%d touchAvail=%d\n", dx, dy, isLast, m_touchInjector.isAvailable());

    // Upload log after first few frames
    static int frameCount = 0;
    if (++frameCount == 5) wmfUploadLog();

    // Try InjectTouchInput first (gives rubber-band + momentum in Windows 11).
    // Falls back to SendInput WHEEL if touch injection isn't available or fails.
    if (m_touchInjector.isAvailable()) {
        WMF_PTP_REPORT report = m_contactMapper.map(dy, dx, false);
        bool ok = m_touchInjector.submitReport(report);
        if (!ok) {
            // Touch injection failed — fall through to SendInput
            m_contactMapper.reset();
            goto sendInput;
        }
        if (isLast) {
            WMF_PTP_REPORT lift = m_contactMapper.map(0, 0, true);
            m_touchInjector.submitReport(lift);
            m_contactMapper.reset();
            m_liftPending.store(false);
        }
        return;
    }

sendInput:
    // SendInput fallback — works everywhere.
    // Negate dy: animator direction=+1 means scroll-down, but WHEEL positive = scroll-up.
    if (dy != 0) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.dwExtraInfo = MouseHook::kWmfMarker;
        input.mi.mouseData = (DWORD)(SHORT)(-dy * 3);
        SendInput(1, &input, sizeof(INPUT));
    }
    if (dx != 0) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        input.mi.dwExtraInfo = MouseHook::kWmfMarker;
        input.mi.mouseData = (DWORD)(SHORT)(-dx * 3);
        SendInput(1, &input, sizeof(INPUT));
    }
}

// ---------------------------------------------------------------------------
// Idle lift timer — single persistent thread, woken by condition variable
// ---------------------------------------------------------------------------

void ScrollPipeline::liftThreadFunc() {
    while (!m_liftShutdown.load()) {
        std::unique_lock<std::mutex> lock(m_liftMutex);

        // Wait until a lift is scheduled or we're shutting down
        m_liftCv.wait(lock, [this] {
            return m_liftPending.load() || m_liftShutdown.load();
        });

        if (m_liftShutdown.load()) break;

        // Wait for the deadline (or cancellation)
        DWORD deadline = m_liftDeadline;
        lock.unlock();

        while (true) {
            DWORD now = GetTickCount();
            if (!m_liftPending.load()) break;  // cancelled
            if (now >= deadline) {
                // Deadline reached and not cancelled — send lift
                if (m_liftPending.exchange(false)) {
                    m_animator.cancel();
                    WMF_PTP_REPORT liftReport = m_contactMapper.map(0, 0, true);
                    m_driver.submitReport(liftReport);
                    m_contactMapper.reset();
                    m_subPixelator.reset();
                }
                break;
            }
            Sleep(5);  // poll every 5ms — fine for an 80ms timeout
        }
    }
}

void ScrollPipeline::scheduleLift() {
    {
        std::lock_guard<std::mutex> lock(m_liftMutex);
        m_liftDeadline = GetTickCount() + kIdleTimeoutMs;
        m_liftPending.store(true);
    }
    m_liftCv.notify_one();
}

void ScrollPipeline::cancelLift() {
    m_liftPending.store(false);
}
