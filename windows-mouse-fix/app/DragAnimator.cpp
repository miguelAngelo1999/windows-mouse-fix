//
// DragAnimator.cpp
// Display-synced animation loop. Port of Mac Mouse Fix's TouchAnimator.swift.
//

#include "DragAnimator.h"
#include <windows.h>
#include <dwmapi.h>
#include <cassert>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

DragAnimator::DragAnimator() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_qpcFreq = freq.QuadPart;

    // Start the animation thread (it will wait until start() is called)
    m_thread = std::thread(&DragAnimator::animationThreadFunc, this);

    // Boost thread priority for smooth animation
    SetThreadPriority(m_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
}

DragAnimator::~DragAnimator() {
    m_shutdown.store(true);
    m_cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

// ---------------------------------------------------------------------------
// start() — called from ScrollPipeline on the ScrollThread
// ---------------------------------------------------------------------------

void DragAnimator::start(double distancePixels, double initialSpeed, int direction, FrameCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);

    double combinedDistance = distancePixels;

    // If already running, add remaining distance (accumulating momentum)
    if (m_running.load() && m_state.curve) {
        double elapsed = getTimestampSeconds() - m_state.startTime;
        double alreadyScrolled = m_state.curve->position(elapsed) + m_state.lastPosition;
        double total = m_state.curve->totalDistance() + m_state.remainingDist;
        double remaining = total - alreadyScrolled;
        if (remaining > 0.0 && direction == m_state.direction) {
            combinedDistance += remaining;
        }
        // Reset sub-pixelator on restart to avoid drift
        m_subPixelator.reset();
    }

    // Clamp distance
    combinedDistance = std::max(1.0, std::min(combinedDistance, 50000.0));

    // Build new drag curve
    // initialSpeed must be > stopSpeed (1.0); clamp if needed
    double speed = std::max(initialSpeed, 2.0);

    m_state.curve         = std::make_unique<DragCurve>(speed);
    m_state.startTime     = getTimestampSeconds();
    m_state.remainingDist = combinedDistance;
    m_state.lastPosition  = 0.0;
    m_state.direction     = direction;
    m_state.callback      = cb;
    m_state.newRequest    = true;

    m_running.store(true);
    m_cv.notify_one();
}

// ---------------------------------------------------------------------------
// cancel()
// ---------------------------------------------------------------------------

void DragAnimator::cancel() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state.newRequest = false;
        m_state.curve.reset();
        m_state.callback = nullptr;
    }
    m_running.store(false);
    m_currentSpeed.store(0.0);
    m_subPixelator.reset();
}

// ---------------------------------------------------------------------------
// Animation thread
// ---------------------------------------------------------------------------

void DragAnimator::animationThreadFunc() {
    while (!m_shutdown.load()) {

        // Wait for a start() call
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] {
                return m_state.newRequest || m_shutdown.load();
            });
            if (m_shutdown.load()) break;
            m_state.newRequest = false;
        }

        // Animation loop
        while (m_running.load() && !m_shutdown.load()) {

            // Sync to display vsync
            syncToDisplay();

            double now = getTimestampSeconds();

            // Read state under lock
            DragCurve*    curve     = nullptr;
            double        startTime = 0.0;
            double        totalDist = 0.0;
            double        lastPos   = 0.0;
            int           dir       = 1;
            FrameCallback cb;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_state.curve) {
                    m_running.store(false);
                    break;
                }
                curve     = m_state.curve.get();
                startTime = m_state.startTime;
                totalDist = m_state.remainingDist;
                lastPos   = m_state.lastPosition;
                dir       = m_state.direction;
                cb        = m_state.callback;
            }

            double elapsed = now - startTime;

            // Compute position on the drag curve
            double curvePos = curve->position(elapsed);

            // Scale curve position to our total distance
            // (curve goes from 0 to totalDistance(); we want 0 to totalDist)
            double scale = (curve->totalDistance() > 0.0)
                           ? totalDist / curve->totalDistance()
                           : 1.0;
            double scaledPos = curvePos * scale;

            // Delta since last frame
            double frameDelta = scaledPos - lastPos;

            // Update last position
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_state.curve.get() == curve) { // still same animation
                    m_state.lastPosition = scaledPos;
                }
            }

            // Sub-pixelate
            double dxDouble = (dir > 0) ? frameDelta : -frameDelta;
            IntVec2 intDelta = m_subPixelator.pixelate(0.0, dxDouble);

            // Update current speed
            double speed = curve->velocity(elapsed) * scale;
            m_currentSpeed.store(speed);

            // Check if animation is done
            bool isLast = (elapsed >= curve->duration());

            // Fire callback (only if delta is non-zero or it's the last frame)
            if ((intDelta.x != 0 || intDelta.y != 0) || isLast) {
                if (cb) cb(intDelta.x, intDelta.y, isLast);
            }

            if (isLast) {
                m_running.store(false);
                m_currentSpeed.store(0.0);
                m_subPixelator.reset();
                break;
            }

            // Check if a new start() arrived (newRequest set)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_state.newRequest) {
                    m_state.newRequest = false;
                    // Restart the inner loop with new state
                    break;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// syncToDisplay — blocks until next vsync
// ---------------------------------------------------------------------------

void DragAnimator::syncToDisplay() {
    // DwmFlush() blocks until the next DWM vsync event.
    // This is the Windows equivalent of CVDisplayLink on macOS.
    // Falls back to a 16ms sleep if DWM composition is disabled.
    HRESULT hr = DwmFlush();
    if (FAILED(hr)) {
        Sleep(16);
    }
}

// ---------------------------------------------------------------------------
// getTimestampSeconds — high-resolution timestamp
// ---------------------------------------------------------------------------

double DragAnimator::getTimestampSeconds() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(m_qpcFreq);
}
