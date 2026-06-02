#pragma once

//
// ContactMapper.h
// Converts integer pixel deltas into two-finger PTP contact position reports.
//
// The virtual touchpad has a 4096x4096 logical coordinate space.
// Two contacts are placed side-by-side at fixed X positions and share a Y
// coordinate that moves based on cumulative scroll.
//

#include "../shared/WmfIoctl.h"
#include <cstdint>

class ContactMapper {
public:
    ContactMapper();

    // Convert a pixel delta to a PTP report.
    // pixelDeltaY: positive = scroll down (contacts move up, Y decreases)
    // pixelDeltaX: positive = scroll right (contacts move left, X decreases)
    // liftFingers: if true, generate a finger-lift report (contact_count=0)
    WMF_PTP_REPORT map(int pixelDeltaY, int pixelDeltaX, bool liftFingers);

    // Reset contact state (call when a new scroll gesture begins)
    void reset();

    // Check if a gesture is in progress
    bool isActive() const { return m_initialized; }

    // Coordinate constants
    static constexpr int kLogicalMax    = 4095;
    static constexpr int kCenterY       = 2048;
    static constexpr int kCenterX       = 2048;
    static constexpr int kContact0X     = 1500;  // left finger X
    static constexpr int kContact1X     = 2500;  // right finger X
    static constexpr int kMargin        = 200;   // keep contacts away from edges

private:
    int      m_currentY;
    int      m_currentX;
    bool     m_initialized;
    uint16_t m_scanTime;    // increments each frame in 100µs units

    static int clamp(int val, int lo, int hi) {
        if (val < lo) return lo;
        if (val > hi) return hi;
        return val;
    }

    WMF_CONTACT makeContact(uint8_t id, uint16_t x, uint16_t y, bool down) const;
};
