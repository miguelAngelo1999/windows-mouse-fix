#pragma once

//
// ScrollAnalyzer.h
// Tracks scroll tick timing to compute speed and detect swipe boundaries.
// Port of Mac Mouse Fix's ScrollAnalyzer.m
//

#include <cstdint>
#include <cfloat>

struct ScrollAnalysisResult {
    // Time between this tick and the previous one, in seconds.
    // DBL_MAX if this is the first tick of a new consecutive sequence.
    double   timeBetweenTicks;

    // How many consecutive ticks have occurred in the same direction
    // without a gap > consecutiveTickIntervalMax.
    int64_t  consecutiveTickCounter;

    // Fractional swipe counter — increments by 1 every scrollSwipeThreshold ticks.
    // Used by AccelerationCurve for fast-scroll multiplier.
    double   consecutiveSwipeCounter;

    // True if the scroll direction flipped since the last tick.
    bool     scrollDirectionDidChange;

    // True if this is the first tick of a new consecutive sequence.
    bool     isFirstConsecutiveTick;
};

class ScrollAnalyzer {
public:
    ScrollAnalyzer();

    // Call once per scroll tick. tickTimestamp is in seconds (e.g. from
    // QueryPerformanceCounter). direction: +1 = down/right, -1 = up/left.
    ScrollAnalysisResult update(double tickTimestamp, int direction);

    // Reset all state (call on direction change or when hook is disabled).
    void reset();

    // --- Tunable parameters (defaults match Mac Mouse Fix) ---

    // If more than this many seconds pass between ticks, they are not consecutive.
    double consecutiveTickIntervalMax = 0.35;

    // Minimum interval — used to clamp timeBetweenTicks for acceleration.
    double consecutiveTickIntervalMin = 0.01;

    // Number of consecutive ticks that constitute one "swipe".
    int    scrollSwipeThreshold = 3;

private:
    double  m_lastTickTimestamp   = 0.0;
    int     m_lastDirection       = 0;
    int64_t m_consecutiveTicks    = 0;
    double  m_consecutiveSwipes   = 0.0;
};
