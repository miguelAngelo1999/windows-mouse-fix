//
// ScrollAnalyzer.cpp
// Port of Mac Mouse Fix's ScrollAnalyzer.m
//

#include "ScrollAnalyzer.h"
#include <algorithm>

ScrollAnalyzer::ScrollAnalyzer() {
    reset();
}

void ScrollAnalyzer::reset() {
    m_lastTickTimestamp = 0.0;
    m_lastDirection     = 0;
    m_consecutiveTicks  = 0;
    m_consecutiveSwipes = 0.0;
}

ScrollAnalysisResult ScrollAnalyzer::update(double tickTimestamp, int direction) {
    ScrollAnalysisResult result = {};

    // --- Direction change detection ---
    bool directionChanged = (m_lastDirection != 0 && direction != m_lastDirection);
    result.scrollDirectionDidChange = directionChanged;

    if (directionChanged) {
        // Reset everything on direction flip
        m_consecutiveTicks  = 0;
        m_consecutiveSwipes = 0.0;
        m_lastTickTimestamp = 0.0;
    }

    // --- Compute time between ticks ---
    double timeBetweenTicks = DBL_MAX;

    if (m_lastTickTimestamp > 0.0 && !directionChanged) {
        double gap = tickTimestamp - m_lastTickTimestamp;

        if (gap > consecutiveTickIntervalMax) {
            // Gap too large — start a new consecutive sequence
            m_consecutiveTicks  = 0;
            m_consecutiveSwipes = 0.0;
            timeBetweenTicks    = DBL_MAX;
        } else {
            // Clamp to valid range
            timeBetweenTicks = std::max(gap, consecutiveTickIntervalMin);
        }
    }

    // --- Update counters ---
    m_consecutiveTicks++;

    // Increment swipe counter every scrollSwipeThreshold ticks
    if (scrollSwipeThreshold > 0) {
        m_consecutiveSwipes = (double)(m_consecutiveTicks - 1) / (double)scrollSwipeThreshold;
    }

    // --- Fill result ---
    result.timeBetweenTicks        = timeBetweenTicks;
    result.consecutiveTickCounter  = m_consecutiveTicks;
    result.consecutiveSwipeCounter = m_consecutiveSwipes;
    result.isFirstConsecutiveTick  = (m_consecutiveTicks == 1);

    // --- Update state ---
    m_lastTickTimestamp = tickTimestamp;
    m_lastDirection     = direction;

    return result;
}
