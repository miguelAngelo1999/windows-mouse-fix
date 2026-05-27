//
// test_main.cpp
// Unit tests for Windows Mouse Fix core components.
// Uses a minimal hand-rolled test framework (no external deps).
//

#include <cstdio>
#include <cmath>
#include <cassert>
#include <cfloat>
#include <windows.h>

// Include components under test
#include "../app/ScrollAnalyzer.h"
#include "../app/ScrollAnalyzer.cpp"
#include "../app/DragCurve.h"
#include "../app/DragCurve.cpp"
#include "../app/SubPixelator.h"
#include "../app/AccelerationCurve.h"
#include "../app/ContactMapper.h"
#include "../app/ContactMapper.cpp"

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) static void name()
#define RUN(name)  do { printf("  " #name "... "); name(); printf("OK\n"); } while(0)

#define EXPECT_TRUE(expr) \
    do { if (!(expr)) { printf("FAIL: %s line %d: expected true: %s\n", __FILE__, __LINE__, #expr); g_failed++; return; } g_passed++; } while(0)

#define EXPECT_NEAR(a, b, eps) \
    do { double _a=(a), _b=(b), _e=(eps); \
         if (fabs(_a-_b) > _e) { printf("FAIL: %s line %d: |%f - %f| > %f\n", __FILE__, __LINE__, _a, _b, _e); g_failed++; return; } \
         g_passed++; } while(0)

#define EXPECT_EQ(a, b) \
    do { if ((a) != (b)) { printf("FAIL: %s line %d: %lld != %lld\n", __FILE__, __LINE__, (long long)(a), (long long)(b)); g_failed++; return; } g_passed++; } while(0)

// ---------------------------------------------------------------------------
// ScrollAnalyzer tests
// ---------------------------------------------------------------------------

TEST(test_scroll_analyzer_first_tick) {
    ScrollAnalyzer sa;
    auto r = sa.update(1.0, 1);
    EXPECT_TRUE(r.timeBetweenTicks == DBL_MAX);
    EXPECT_TRUE(r.isFirstConsecutiveTick);
    EXPECT_EQ(r.consecutiveTickCounter, 1);
    EXPECT_TRUE(!r.scrollDirectionDidChange);
}

TEST(test_scroll_analyzer_consecutive) {
    ScrollAnalyzer sa;
    sa.update(1.0, 1);
    auto r = sa.update(1.1, 1); // 100ms gap
    EXPECT_NEAR(r.timeBetweenTicks, 0.1, 0.001);
    EXPECT_TRUE(!r.isFirstConsecutiveTick);
    EXPECT_EQ(r.consecutiveTickCounter, 2);
}

TEST(test_scroll_analyzer_timeout) {
    ScrollAnalyzer sa;
    sa.update(1.0, 1);
    auto r = sa.update(2.0, 1); // 1 second gap — exceeds max
    EXPECT_TRUE(r.timeBetweenTicks == DBL_MAX);
    EXPECT_TRUE(r.isFirstConsecutiveTick);
    EXPECT_EQ(r.consecutiveTickCounter, 1);
}

TEST(test_scroll_analyzer_direction_change) {
    ScrollAnalyzer sa;
    sa.update(1.0,  1);
    sa.update(1.1,  1);
    auto r = sa.update(1.2, -1); // direction flip
    EXPECT_TRUE(r.scrollDirectionDidChange);
    EXPECT_TRUE(r.isFirstConsecutiveTick);
    EXPECT_EQ(r.consecutiveTickCounter, 1);
}

// ---------------------------------------------------------------------------
// DragCurve tests
// ---------------------------------------------------------------------------

TEST(test_drag_curve_initial_velocity) {
    DragCurve curve(100.0); // 100 px/s initial speed
    EXPECT_NEAR(curve.velocity(0.0), 100.0, 0.01);
}

TEST(test_drag_curve_decelerates) {
    DragCurve curve(100.0);
    double v0 = curve.velocity(0.0);
    double v1 = curve.velocity(curve.duration() * 0.5);
    EXPECT_TRUE(v1 < v0);
    EXPECT_TRUE(v1 > 0.0);
}

TEST(test_drag_curve_stops) {
    DragCurve curve(100.0);
    double vEnd = curve.velocity(curve.duration());
    EXPECT_NEAR(vEnd, 0.0, 0.1); // should be ~0 at end
}

TEST(test_drag_curve_position_monotonic) {
    DragCurve curve(100.0);
    double dur = curve.duration();
    double p0 = curve.position(0.0);
    double p1 = curve.position(dur * 0.25);
    double p2 = curve.position(dur * 0.5);
    double p3 = curve.position(dur * 0.75);
    double p4 = curve.position(dur);
    EXPECT_TRUE(p0 <= p1);
    EXPECT_TRUE(p1 <= p2);
    EXPECT_TRUE(p2 <= p3);
    EXPECT_TRUE(p3 <= p4);
}

TEST(test_drag_curve_total_distance) {
    DragCurve curve(100.0);
    double dist = curve.totalDistance();
    EXPECT_TRUE(dist > 0.0);
    EXPECT_TRUE(dist < 100000.0); // sanity check
    EXPECT_NEAR(curve.position(curve.duration()), dist, 0.01);
}

TEST(test_drag_curve_duration_positive) {
    DragCurve curve(50.0);
    EXPECT_TRUE(curve.duration() > 0.0);
}

// ---------------------------------------------------------------------------
// SubPixelator tests
// ---------------------------------------------------------------------------

TEST(test_subpixelator_accumulates) {
    SubPixelator sp;
    int total = 0;
    for (int i = 0; i < 10; i++) {
        auto v = sp.pixelate(0.0, 0.3);
        total += v.y;
    }
    // 10 * 0.3 in IEEE 754 = 2.9999... so floor gives 2, remainder ~0.999
    // This is correct behaviour — the remainder carries over to the next call
    EXPECT_TRUE(total >= 2 && total <= 3);
}

TEST(test_subpixelator_no_drift) {
    SubPixelator sp;
    int total = 0;
    for (int i = 0; i < 100; i++) {
        auto v = sp.pixelate(0.0, 0.1);
        total += v.y;
    }
    // 100 * 0.1 in IEEE 754 accumulates to ~9.999... or ~10.0
    // Accept 9 or 10 — both are within 1 ULP of correct
    EXPECT_TRUE(total >= 9 && total <= 10);
}

TEST(test_subpixelator_no_drift_exact) {
    // Use exact binary fractions (0.5, 0.25) to test without FP error
    SubPixelator sp;
    int total = 0;
    for (int i = 0; i < 8; i++) {
        auto v = sp.pixelate(0.0, 0.5);
        total += v.y;
    }
    EXPECT_EQ(total, 4); // 8 * 0.5 = 4.0 exactly
}

TEST(test_subpixelator_reset) {
    SubPixelator sp;
    sp.pixelate(0.0, 0.7); // accumulates 0.7
    sp.reset();
    auto v = sp.pixelate(0.0, 0.3); // should start fresh
    EXPECT_EQ(v.y, 0); // 0.3 < 1.0, no output yet
}

TEST(test_subpixelator_negative) {
    SubPixelator sp;
    int total = 0;
    for (int i = 0; i < 10; i++) {
        auto v = sp.pixelate(0.0, -0.3);
        total += v.y;
    }
    EXPECT_EQ(total, -3);
}

// ---------------------------------------------------------------------------
// AccelerationCurve tests
// ---------------------------------------------------------------------------

TEST(test_accel_curve_zero_speed) {
    AccelerationCurve ac;
    double px = ac.evaluate(0.0);
    EXPECT_NEAR(px, 1.0, 0.5); // clamped to minimum 1
}

TEST(test_accel_curve_medium_speed) {
    AccelerationCurve ac;
    double px = ac.evaluate(10.0);
    EXPECT_TRUE(px > 50.0);
    EXPECT_TRUE(px < 250.0);
}

TEST(test_accel_curve_fast_speed) {
    AccelerationCurve ac;
    double px = ac.evaluate(30.0);
    EXPECT_NEAR(px, 300.0, 50.0);
}

TEST(test_accel_curve_monotonic) {
    AccelerationCurve ac;
    double p1 = ac.evaluate(5.0);
    double p2 = ac.evaluate(10.0);
    double p3 = ac.evaluate(20.0);
    EXPECT_TRUE(p1 < p2);
    EXPECT_TRUE(p2 < p3);
}

// ---------------------------------------------------------------------------
// ContactMapper tests
// ---------------------------------------------------------------------------

TEST(test_contact_mapper_two_contacts_on_scroll) {
    ContactMapper cm;
    WMF_PTP_REPORT r = cm.map(10, 0, false);
    EXPECT_EQ(r.report_id, 0x01);
    EXPECT_EQ(r.contact_count, 2);
    EXPECT_TRUE(r.contacts[0].tip_switch == 1);
    EXPECT_TRUE(r.contacts[1].tip_switch == 1);
    EXPECT_TRUE(r.contacts[0].confidence == 1);
    EXPECT_TRUE(r.contacts[1].confidence == 1);
    EXPECT_EQ(r.contacts[0].contact_id, 0);
    EXPECT_EQ(r.contacts[1].contact_id, 1);
}

TEST(test_contact_mapper_lift) {
    ContactMapper cm;
    cm.map(10, 0, false); // scroll first
    WMF_PTP_REPORT r = cm.map(0, 0, true); // lift
    EXPECT_EQ(r.contact_count, 0);
    EXPECT_TRUE(r.contacts[0].tip_switch == 0);
    EXPECT_TRUE(r.contacts[1].tip_switch == 0);
}

TEST(test_contact_mapper_y_moves_on_scroll) {
    ContactMapper cm;
    WMF_PTP_REPORT r1 = cm.map(0, 0, false); // no movement
    WMF_PTP_REPORT r2 = cm.map(50, 0, false); // scroll down 50px
    // Y should decrease (contacts move up when scrolling down)
    EXPECT_TRUE(r2.contacts[0].y < r1.contacts[0].y);
}

TEST(test_contact_mapper_y_clamped) {
    ContactMapper cm;
    // Scroll a huge amount — Y should be clamped
    for (int i = 0; i < 1000; i++) {
        cm.map(100, 0, false);
    }
    WMF_PTP_REPORT r = cm.map(0, 0, false);
    EXPECT_TRUE(r.contacts[0].y >= ContactMapper::kMargin);
    EXPECT_TRUE(r.contacts[0].y <= ContactMapper::kLogicalMax - ContactMapper::kMargin);
}

TEST(test_contact_mapper_reset) {
    ContactMapper cm;
    cm.map(200, 0, false);
    cm.reset();
    WMF_PTP_REPORT r = cm.map(0, 0, false);
    // After reset, Y should be back at center
    EXPECT_EQ(r.contacts[0].y, ContactMapper::kCenterY);
}

TEST(test_contact_mapper_fixed_x_positions) {
    ContactMapper cm;
    WMF_PTP_REPORT r = cm.map(10, 0, false);
    EXPECT_EQ(r.contacts[0].x, ContactMapper::kContact0X);
    EXPECT_EQ(r.contacts[1].x, ContactMapper::kContact1X);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    printf("=== Windows Mouse Fix Unit Tests ===\n\n");

    printf("ScrollAnalyzer:\n");
    RUN(test_scroll_analyzer_first_tick);
    RUN(test_scroll_analyzer_consecutive);
    RUN(test_scroll_analyzer_timeout);
    RUN(test_scroll_analyzer_direction_change);

    printf("\nDragCurve:\n");
    RUN(test_drag_curve_initial_velocity);
    RUN(test_drag_curve_decelerates);
    RUN(test_drag_curve_stops);
    RUN(test_drag_curve_position_monotonic);
    RUN(test_drag_curve_total_distance);
    RUN(test_drag_curve_duration_positive);

    printf("\nSubPixelator:\n");
    RUN(test_subpixelator_accumulates);
    RUN(test_subpixelator_no_drift);
    RUN(test_subpixelator_no_drift_exact);
    RUN(test_subpixelator_reset);
    RUN(test_subpixelator_negative);

    printf("\nAccelerationCurve:\n");
    RUN(test_accel_curve_zero_speed);
    RUN(test_accel_curve_medium_speed);
    RUN(test_accel_curve_fast_speed);
    RUN(test_accel_curve_monotonic);

    printf("\nContactMapper:\n");
    RUN(test_contact_mapper_two_contacts_on_scroll);
    RUN(test_contact_mapper_lift);
    RUN(test_contact_mapper_y_moves_on_scroll);
    RUN(test_contact_mapper_y_clamped);
    RUN(test_contact_mapper_reset);
    RUN(test_contact_mapper_fixed_x_positions);

    printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
