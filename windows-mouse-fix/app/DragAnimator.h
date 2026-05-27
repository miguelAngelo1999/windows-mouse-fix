#pragma once

//
// DragAnimator.h
// Display-synced animation loop that drives scroll output via DragCurve physics.
// Port of Mac Mouse Fix's TouchAnimator.swift
//
// Runs on a dedicated high-priority thread, synced to display vsync via DwmFlush().
// Each frame it computes the pixel delta from the DragCurve and calls FrameCallback.
//

#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "DragCurve.h"
#include "SubPixelator.h"

// Called on the animation thread each frame.
// dx, dy: integer pixel deltas for this frame (after sub-pixelation)
// isLast: true on the final frame (speed dropped to stopSpeed)
using FrameCallback = std::function<void(int dx, int dy, bool isLast)>;

class DragAnimator {
public:
    DragAnimator();
    ~DragAnimator();

    // Start or restart the animation.
    // If already running, the remaining distance is added to the new distance
    // and the curve restarts from the current velocity — giving the "accumulating
    // momentum" feel when the user scrolls continuously.
    //
    // distancePixels: total pixels to scroll (positive = down/right)
    // initialSpeed:   pixels/second at start of this tick
    // direction:      +1 or -1
    void start(double distancePixels, double initialSpeed, int direction, FrameCallback cb);

    // Cancel the animation immediately (e.g. on direction change).
    void cancel();

    bool isRunning() const { return m_running.load(); }

    // Current speed in px/s (for direction-change detection in ScrollPipeline)
    double currentSpeed() const { return m_currentSpeed.load(); }

private:
    void animationThreadFunc();
    void syncToDisplay();           // blocks until next vsync via DwmFlush
    double getTimestampSeconds();   // QueryPerformanceCounter -> seconds

    std::thread             m_thread;
    std::atomic<bool>       m_running   { false };
    std::atomic<bool>       m_shutdown  { false };
    std::atomic<double>     m_currentSpeed { 0.0 };

    // Protected by m_mutex
    std::mutex              m_mutex;
    std::condition_variable m_cv;

    // Animation state (written under m_mutex, read on animation thread)
    struct AnimState {
        std::unique_ptr<DragCurve> curve;
        double   startTime      = 0.0;   // QPC seconds when animation started
        double   remainingDist  = 0.0;   // pixels left from previous animation
        double   lastPosition   = 0.0;   // position at last frame
        int      direction      = 1;     // +1 or -1
        bool     newRequest     = false; // signals animation thread to restart
        FrameCallback callback;
    } m_state;

    SubPixelator m_subPixelator;

    // QPC frequency (cached)
    long long m_qpcFreq = 0;
};
