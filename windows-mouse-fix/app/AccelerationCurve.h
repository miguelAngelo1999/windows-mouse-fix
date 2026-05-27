#pragma once

//
// AccelerationCurve.h
// Maps scroll speed (ticks/sec) to pixels-per-tick via a cubic Bezier curve.
// Matches Mac Mouse Fix's acceleration curve approach.
//

#include <array>
#include <utility>
#include <algorithm>

class AccelerationCurve {
public:
    using Point = std::pair<double, double>;

    // Default control points tuned to feel like macOS scrolling.
    // Input:  scroll speed in ticks/sec  (x-axis)
    // Output: pixels per tick            (y-axis)
    AccelerationCurve() {
        m_points = {{
            {  0.0,   0.0 },   // P0 — anchor
            {  5.0,  40.0 },   // P1 — control
            { 15.0, 180.0 },   // P2 — control
            { 30.0, 300.0 }    // P3 — anchor
        }};
    }

    // Evaluate the curve at the given scroll speed.
    // Returns pixels per tick, clamped to [1, 500].
    double evaluate(double ticksPerSecond) const {
        // Clamp input to curve range
        double tMax = m_points[3].first;
        double t    = std::max(0.0, std::min(ticksPerSecond / tMax, 1.0));

        // Cubic Bezier: B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)*t^2*P2 + t^3*P3
        double u  = 1.0 - t;
        double u2 = u * u;
        double u3 = u2 * u;
        double t2 = t * t;
        double t3 = t2 * t;

        double y = u3 * m_points[0].second
                 + 3.0 * u2 * t  * m_points[1].second
                 + 3.0 * u  * t2 * m_points[2].second
                 + t3 * m_points[3].second;

        return std::max(1.0, std::min(y, 500.0));
    }

    void setControlPoints(std::array<Point, 4> pts) {
        m_points = pts;
    }

    static AccelerationCurve makeDefault() {
        return AccelerationCurve();
    }

private:
    std::array<Point, 4> m_points;
};
