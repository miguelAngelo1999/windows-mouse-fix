//
// ContactMapper.cpp
//

#include "ContactMapper.h"
#include <cstring>

ContactMapper::ContactMapper()
    : m_currentY(kCenterY)
    , m_currentX(kCenterX)
    , m_initialized(false)
    , m_scanTime(0)
{
}

void ContactMapper::reset() {
    m_currentY    = kCenterY;
    m_currentX    = kCenterX;
    m_initialized = false;
    // Don't reset scan_time — it should be monotonically increasing
}

WMF_CONTACT ContactMapper::makeContact(uint8_t id, uint16_t x, uint16_t y, bool down) const {
    WMF_CONTACT c = {};
    c.flags      = down ? WMF_CONTACT_FLAGS_DOWN : WMF_CONTACT_FLAGS_UP;
    c.contact_id = id;
    c.x          = x;
    c.y          = y;
    return c;
}

WMF_PTP_REPORT ContactMapper::map(int pixelDeltaY, int pixelDeltaX, bool liftFingers) {
    WMF_PTP_REPORT report = {};
    report.report_id = 0x01;

    // Advance scan time by ~16ms worth of 100µs units = 160 units per frame
    // (wraps naturally at uint16 max = 65535)
    m_scanTime += 160;
    report.scan_time = m_scanTime;

    if (liftFingers) {
        // Finger lift: contact_count=0, all contacts zeroed
        report.contact_count = 0;
        // contacts are already zeroed by the struct initializer
        return report;
    }

    if (!m_initialized) {
        m_currentY    = kCenterY;
        m_currentX    = kCenterX;
        m_initialized = true;
    }

    // Move Y: scroll down (positive delta) = fingers move up = Y decreases
    // This matches Windows "natural scroll" (same direction as macOS default)
    m_currentY = clamp(m_currentY - pixelDeltaY, kMargin, kLogicalMax - kMargin);

    // Move X: scroll right (positive delta) = fingers move left = X decreases
    m_currentX = clamp(m_currentX - pixelDeltaX, kMargin, kLogicalMax - kMargin);

    // Two contacts side by side, sharing the same Y
    report.contacts[0] = makeContact(0, (uint16_t)kContact0X, (uint16_t)m_currentY, true);
    report.contacts[1] = makeContact(1, (uint16_t)kContact1X, (uint16_t)m_currentY, true);
    // contacts[2..4] remain zeroed
    report.contact_count = 2;

    return report;
}
