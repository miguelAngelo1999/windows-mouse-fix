//
// DragCurve.cpp
// Port of Mac Mouse Fix's DragCurve.swift
//
// Physics: dv/dt = -c * |v|^(e-1) * v
//
// For e != 1, the closed-form solution is:
//   v(t) = v0 * (1 - (1-e)*c*v0^(e-1)*t)^(1/(1-e))
//
// Let K = (1-e)*c*v0^(e-1)
//   v(t) = v0 * (1 - K*t)^(1/(1-e))
//
// Position (integral of v):
//   x(t) = v0 * [ (1 - K*t)^(1/(1-e)+1) / (-(K) * (1/(1-e)+1)) ]
//         evaluated from 0 to t
//
// Duration: solve v(t) = stopSpeed for t:
//   t_stop = (1 - (stopSpeed/v0)^(1-e)) / K
//

#include "DragCurve.h"
#include <cmath>
#include <algorithm>
#include <cassert>

DragCurve::DragCurve(double initialSpeed, double dragCoeff, double dragExp, double stopSpeed)
    : m_v0(initialSpeed)
    , m_c(dragCoeff)
    , m_e(dragExp)
    , m_stopSpeed(stopSpeed)
{
    assert(initialSpeed > 0.0);
    assert(dragCoeff    > 0.0);
    assert(dragExp      > 0.0 && dragExp < 1.0); // must be < 1 for convergence
    assert(stopSpeed    > 0.0);
    assert(initialSpeed > stopSpeed);

    // Cache duration and distance
    m_duration = duration();
    m_distance = positionAt(m_duration);
}

// ---------------------------------------------------------------------------
// Internal: velocity at time t (no clamping)
// ---------------------------------------------------------------------------
double DragCurve::velocityAt(double t) const {
    // K = (1-e)*c*v0^(e-1)
    double K = (1.0 - m_e) * m_c * std::pow(m_v0, m_e - 1.0);
    double base = 1.0 - K * t;
    if (base <= 0.0) return 0.0;
    double exponent = 1.0 / (1.0 - m_e);
    return m_v0 * std::pow(base, exponent);
}

// ---------------------------------------------------------------------------
// Internal: position at time t (no clamping)
// ---------------------------------------------------------------------------
double DragCurve::positionAt(double t) const {
    // x(t) = integral_0^t v(s) ds
    // = v0 / (K * (1/(1-e) + 1)) * [1 - (1 - K*t)^(1/(1-e)+1)]
    double K = (1.0 - m_e) * m_c * std::pow(m_v0, m_e - 1.0);
    if (K <= 0.0) return m_v0 * t; // degenerate case

    double alpha    = 1.0 / (1.0 - m_e);       // = 1/(1-e)
    double alphap1  = alpha + 1.0;              // = 1/(1-e) + 1
    double base     = 1.0 - K * t;
    if (base < 0.0) base = 0.0;

    double integral = (m_v0 / (K * alphap1)) * (1.0 - std::pow(base, alphap1));
    return integral;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

double DragCurve::velocity(double t) const {
    if (t <= 0.0)          return m_v0;
    if (t >= m_duration)   return 0.0;
    return velocityAt(t);
}

double DragCurve::position(double t) const {
    if (t <= 0.0)          return 0.0;
    if (t >= m_duration)   return m_distance;
    return positionAt(t);
}

double DragCurve::duration() const {
    // Solve v(t) = stopSpeed:
    // stopSpeed = v0 * (1 - K*t)^(1/(1-e))
    // (stopSpeed/v0)^(1-e) = 1 - K*t
    // t = (1 - (stopSpeed/v0)^(1-e)) / K
    double K = (1.0 - m_e) * m_c * std::pow(m_v0, m_e - 1.0);
    if (K <= 0.0) return 0.0;

    double ratio = m_stopSpeed / m_v0;
    double t = (1.0 - std::pow(ratio, 1.0 - m_e)) / K;
    return std::max(t, 0.0);
}

double DragCurve::totalDistance() const {
    return m_distance;
}
