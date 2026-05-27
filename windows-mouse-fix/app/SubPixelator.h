#pragma once

//
// SubPixelator.h
// Accumulates fractional pixel values to prevent rounding drift.
// Port of Mac Mouse Fix's VectorSubPixelator.
//

struct IntVec2 {
    int x = 0;
    int y = 0;
};

class SubPixelator {
public:
    SubPixelator() : m_accumX(0.0), m_accumY(0.0) {}

    // Feed a fractional delta; returns the integer pixels to actually move.
    // Remainder is accumulated for the next call.
    // Uses floor() so that the sign of the remainder always matches the
    // direction of accumulation — prevents drift on repeated small deltas.
    IntVec2 pixelate(double dx, double dy) {
        m_accumX += dx;
        m_accumY += dy;

        int ix = static_cast<int>(std::floor(m_accumX));
        int iy = static_cast<int>(std::floor(m_accumY));

        m_accumX -= static_cast<double>(ix);
        m_accumY -= static_cast<double>(iy);

        return { ix, iy };
    }

    // Single-axis variant
    int pixelate1D(double d) {
        m_accumX += d;
        int i = static_cast<int>(std::floor(m_accumX));
        m_accumX -= static_cast<double>(i);
        return i;
    }

    void reset() {
        m_accumX = 0.0;
        m_accumY = 0.0;
    }

    double remainderX() const { return m_accumX; }
    double remainderY() const { return m_accumY; }

private:
    double m_accumX;
    double m_accumY;
};
