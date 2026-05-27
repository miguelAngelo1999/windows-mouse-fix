#pragma once

//
// DragCurve.h
// Physics model for scroll deceleration.
// Port of Mac Mouse Fix's DragCurve.swift
//
// Models: dv/dt = -dragCoeff * |v|^(dragExp-1) * v
//
// Closed-form solution:
//   v(t) = v0 / (1 + (dragExp-1) * dragCoeff * v0^(dragExp-1) * t)^(1/(dragExp-1))
//   x(t) = integral of v(t) dt  (analytical)
//

class DragCurve {
public:
    // initialSpeed: pixels/second at t=0
    // dragCoeff:    resistance coefficient (default 30.0 from MMF)
    // dragExp:      resistance exponent    (default 0.7  from MMF)
    // stopSpeed:    animation stops when speed drops below this (default 1.0)
    DragCurve(double initialSpeed,
              double dragCoeff = 30.0,
              double dragExp   = 0.7,
              double stopSpeed = 1.0);

    // Velocity at time t (pixels/second). Returns 0 if t >= duration().
    double velocity(double t) const;

    // Distance travelled from start to time t (pixels).
    double position(double t) const;

    // Time at which speed drops to stopSpeed.
    double duration() const;

    // Total distance travelled over the full animation.
    double totalDistance() const;

    // Getters
    double initialSpeed() const { return m_v0; }
    double dragCoeff()    const { return m_c;  }
    double dragExp()      const { return m_e;  }
    double stopSpeed()    const { return m_stopSpeed; }

private:
    double m_v0;        // initial speed
    double m_c;         // drag coefficient
    double m_e;         // drag exponent
    double m_stopSpeed;
    double m_duration;  // cached
    double m_distance;  // cached

    // Internal helpers
    double velocityAt(double t) const;
    double positionAt(double t) const;
};
