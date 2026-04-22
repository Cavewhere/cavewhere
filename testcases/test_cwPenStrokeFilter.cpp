//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QPointF>
#include <QVector>

//Std includes
#include <cmath>

//Our includes
#include "cwPenPoint.h"
#include "cwPenStrokeFilter.h"

namespace {

QVector<cwPenPoint> makeStroke(std::initializer_list<QPointF> positions)
{
    QVector<cwPenPoint> out;
    out.reserve(static_cast<int>(positions.size()));
    for (const QPointF &p : positions) {
        out.append(cwPenPoint(p, 1.0, 0));
    }
    return out;
}

// For tests that use a short slice of a much longer real stroke, the
// 15%-of-total fraction cap collapses far below what the real stroke's
// body would allow. This helper returns Params with the fraction cap
// effectively disabled so the tests exercise only the absolute cap.
cwPenStrokeFilter::Params paramsForSlice()
{
    cwPenStrokeFilter::Params p;
    p.maxHookFraction = 10.0; // disable
    return p;
}

}

TEST_CASE("cwPenStrokeFilter trims a classic Apple-Pencil start hook", "[cwPenStrokeFilter]") {
    // The first three samples drift 1 mm in +Y (the press-down spur), then
    // the pen reverses and proceeds along +X for centimeters. The hook arm
    // sits under the 2 mm cap and the reversal is ~180°, so trimHooks
    // should drop the leading spur.
    auto stroke = makeStroke({
        {0.0,    0.0},
        {0.0,    0.0005},
        {0.0,    0.0010}, // hook tip — farthest from start
        {0.0005, 0.0007},
        {0.001,  0.0003},
        {0.002,  0.0},
        {0.003,  0.0},
        {0.004,  0.0},
        {0.005,  0.0},
        {0.006,  0.0},
        {0.010,  0.0},
        {0.015,  0.0},
        {0.020,  0.0},
        {0.050,  0.0},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() < stroke.size());
    // The spur — points[0..2] with distance ≤ 1 mm — should be gone.
    CHECK(trimmed.first().position != stroke.first().position);
    // Tail is unchanged.
    CHECK(trimmed.last().position == QPointF(0.050, 0.0));
}

TEST_CASE("cwPenStrokeFilter leaves a clean straight stroke alone", "[cwPenStrokeFilter]") {
    auto stroke = makeStroke({
        {0.00, 0.0}, {0.01, 0.0}, {0.02, 0.0}, {0.03, 0.0}, {0.04, 0.0},
        {0.05, 0.0}, {0.06, 0.0}, {0.07, 0.0}, {0.08, 0.0}, {0.09, 0.0},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() == stroke.size());
    for (int i = 0; i < stroke.size(); ++i) {
        CHECK(trimmed[i].position == stroke[i].position);
    }
}

TEST_CASE("cwPenStrokeFilter is a no-op for very short strokes", "[cwPenStrokeFilter]") {
    auto stroke = makeStroke({ {0.0, 0.0}, {0.001, 0.0}, {0.002, 0.0} });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() == stroke.size());
}

TEST_CASE("cwPenStrokeFilter does not eat legitimate short direction changes", "[cwPenStrokeFilter]") {
    // A stroke that goes 2 cm one way and then makes a right-angle turn —
    // the "hook arm" would be 2 cm, far above the 2 mm cap, so nothing
    // should be trimmed even though the angle looks hook-like.
    auto stroke = makeStroke({
        {0.0,  0.0},
        {0.01, 0.0},
        {0.02, 0.0},
        {0.02, 0.01},
        {0.02, 0.02},
        {0.02, 0.03},
        {0.02, 0.04},
        {0.02, 0.05},
        {0.02, 0.06},
        {0.02, 0.07},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() == stroke.size());
}

TEST_CASE("cwPenStrokeFilter trims a real Apple-Pencil iPad trace", "[cwPenStrokeFilter]") {
    // Captured from an iPad sketch session (see output.txt).
    // Samples 0..~19 sit on a 5.5 mm -X spur (with the tablet repeating
    // duplicate coordinates during press-down); then the pen reverses
    // and the real stroke travels ~37 cm in +X. The filter must see
    // through the duplicates and trim the spur.
    auto stroke = makeStroke({
        {-6.484399, 1.260644}, {-6.484399, 1.260644}, {-6.484399, 1.260644},
        {-6.485771, 1.260644}, {-6.485771, 1.260644}, {-6.485771, 1.260644},
        {-6.487144, 1.260644}, {-6.487144, 1.260644},
        {-6.488517, 1.260644}, {-6.488517, 1.260644}, {-6.488517, 1.260644},
        {-6.488517, 1.260644}, {-6.488517, 1.260644},
        {-6.489889, 1.260644}, {-6.489889, 1.260644}, {-6.489889, 1.260644},
        {-6.489889, 1.260644}, {-6.489889, 1.260644}, {-6.489889, 1.260644},
        {-6.489889, 1.260644},
        {-6.488517, 1.260644}, {-6.488517, 1.260644},
        {-6.485771, 1.260644}, {-6.485771, 1.260644},
        {-6.480624, 1.260644}, {-6.480624, 1.260644},
        {-6.473762, 1.259357}, {-6.473762, 1.259357},
        {-6.465869, 1.259357}, {-6.465869, 1.259357},
        {-6.456262, 1.259357}, {-6.456262, 1.259357},
        {-6.446997, 1.259357}, {-6.446997, 1.259357},
        {-6.434987, 1.259357}, {-6.434987, 1.259357},
        {-6.421605, 1.259357}, {-6.421605, 1.259357},
        {-6.406850, 1.259357}, {-6.406850, 1.259357},
        {-6.390722, 1.259357}, {-6.390722, 1.259357},
        {-6.371850, 1.259357}, {-6.371850, 1.259357},
        {-6.352977, 1.261931}, {-6.352977, 1.261931},
        {-6.334448, 1.264762}, {-6.334448, 1.264762},
        {-6.316948, 1.267335}, {-6.316948, 1.267335},
        {-6.295330, 1.271453}, {-6.295330, 1.271453},
        {-6.276800, 1.274027}, {-6.276800, 1.274027},
        {-6.259300, 1.276600}, {-6.259300, 1.276600},
        {-6.241800, 1.279431}, {-6.241800, 1.279431},
        {-6.227045, 1.282005}, {-6.227045, 1.282005},
        {-6.210918, 1.286122}, {-6.210918, 1.286122},
        {-6.194790, 1.288696}, {-6.194790, 1.288696},
        {-6.180035, 1.291527}, {-6.180035, 1.291527},
        {-6.166653, 1.294100}, {-6.166653, 1.294100},
        {-6.154643, 1.295387}, {-6.154643, 1.295387},
        {-6.146751, 1.298218}, {-6.146751, 1.298218},
        {-6.138516, 1.299505}, {-6.138516, 1.299505},
        {-6.133369, 1.300791}, {-6.133369, 1.300791},
        {-6.129251, 1.302078}, {-6.129251, 1.302078},
        {-6.126506, 1.302078}, {-6.126506, 1.302078},
        {-6.125133, 1.303365}, {-6.125133, 1.303365},
        {-6.123761, 1.303365}, {-6.123761, 1.303365},
        {-6.122388, 1.303365}, {-6.122388, 1.303365},
        {-6.122388, 1.303365}, {-6.122388, 1.303365},
        {-6.122388, 1.303365}, {-6.122388, 1.303365},
        {-6.122388, 1.303365},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() < stroke.size());
    // Clip lands at the V-reversal apex — the deepest point of the
    // -X spur, at (-6.489889, 1.260644). The retrace that returns to
    // the body is retained (it overlaps the body direction visually).
    CHECK(trimmed.first().position == QPointF(-6.489889, 1.260644));
    CHECK(trimmed.last().position == QPointF(-6.122388, 1.303365));
}

TEST_CASE("cwPenStrokeFilter trims a large iPad hook on a moderate stroke",
          "[cwPenStrokeFilter]") {
    // Captured iPad stroke #8 from output.txt: 11.8 mm hook arm on a
    // ~14 cm stroke body. The arm is past the old 10 mm absolute cap
    // but passes the 15%-of-stroke relative cap (arm = 8% of body).
    auto stroke = makeStroke({
        {-3.831344, 3.080256}, {-3.831344, 3.080256}, {-3.831344, 3.080256},
        {-3.831344, 3.077195}, {-3.831344, 3.077195},
        {-3.834609, 3.073522}, {-3.834609, 3.073522}, {-3.834609, 3.073522},
        {-3.834609, 3.070461}, {-3.834609, 3.070461}, {-3.834609, 3.070461},
        {-3.837874, 3.070461}, {-3.837874, 3.070461}, {-3.837874, 3.070461},
        {-3.837874, 3.070461}, {-3.837874, 3.070461}, {-3.837874, 3.070461},
        {-3.837874, 3.070461}, {-3.837874, 3.070461},
        {-3.834609, 3.067400}, {-3.834609, 3.067400},
        {-3.831344, 3.067400}, {-3.831344, 3.067400},
        {-3.821549, 3.067400}, {-3.821549, 3.067400},
        {-3.812570, 3.064339}, {-3.812570, 3.064339},
        {-3.802775, 3.064339}, {-3.802775, 3.064339},
        {-3.792980, 3.064339}, {-3.792980, 3.064339},
        {-3.780736, 3.064339}, {-3.780736, 3.064339},
        {-3.770941, 3.064339}, {-3.770941, 3.064339},
        {-3.757881, 3.064339}, {-3.757881, 3.064339},
        {-3.748086, 3.064339}, {-3.748086, 3.064339},
        {-3.739107, 3.064339}, {-3.739107, 3.064339},
        {-3.726047, 3.064339}, {-3.726047, 3.064339},
        {-3.716252, 3.064339}, {-3.716252, 3.064339},
        {-3.710538, 3.064339}, {-3.710538, 3.064339},
        {-3.700743, 3.064339}, {-3.700743, 3.064339},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() < stroke.size());
    // Clip lands at the V-reversal apex (-3.837874, 3.070461). The
    // retrace back through the touchdown area into the body direction
    // is retained.
    CHECK(trimmed.first().position == QPointF(-3.837874, 3.070461));
    CHECK(trimmed.last().position == QPointF(-3.700743, 3.064339));
}

TEST_CASE("cwPenStrokeFilter preserves a deliberate short tick with a reversal",
          "[cwPenStrokeFilter]") {
    // A 2 cm user-drawn tick that happens to have a ~1 cm sharp
    // reversal inside it. arm/body = 50%, far above the 15% fraction
    // cap, so the filter must leave it alone.
    auto stroke = makeStroke({
        {0.000, 0.000},
        {0.002, 0.000},
        {0.004, 0.000},
        {0.006, 0.000},
        {0.008, 0.000},
        {0.010, 0.000}, // apex
        {0.008, 0.000},
        {0.006, 0.000},
        {0.004, 0.000},
        {0.002, 0.000},
        {0.000, 0.000},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() == stroke.size());
    for (int i = 0; i < stroke.size(); ++i) {
        CHECK(trimmed[i].position == stroke[i].position);
    }
}

TEST_CASE("cwPenStrokeFilter trims an L-shaped iPad touchdown hook",
          "[cwPenStrokeFilter]") {
    // Captured iPad stroke (output.txt stroke 5, first ~30 raw samples
    // shown — real stroke is 824 samples long). The pen touches down
    // and first drifts in -X for 2 mm, then in -Y for 2.4 mm, then the
    // real stroke emerges in +X with a -Y component. Each local angle
    // change is only 90° so the old per-vertex reversal detector
    // missed it; the stable-direction detector should catch it.
    auto stroke = makeStroke({
        {-6.138726, -3.525257}, {-6.138726, -3.525257},
        {-6.138726, -3.525257}, {-6.138726, -3.525257},
        {-6.138726, -3.525257}, {-6.138726, -3.525257},
        {-6.138726, -3.525257},
        {-6.140869, -3.525257}, {-6.140869, -3.525257},
        {-6.140869, -3.525257}, {-6.140869, -3.525257},
        {-6.140869, -3.525257}, {-6.140869, -3.525257},
        {-6.140869, -3.525257},
        {-6.140869, -3.527667}, {-6.140869, -3.527667},
        {-6.138726, -3.527667}, {-6.138726, -3.527667},
        {-6.136583, -3.527667}, {-6.136583, -3.527667},
        {-6.132298, -3.529676}, {-6.132298, -3.529676},
        {-6.128012, -3.533694}, {-6.128012, -3.533694},
        {-6.121584, -3.535702}, {-6.121584, -3.535702},
        {-6.115692, -3.540122}, {-6.115692, -3.540122},
        {-6.107121, -3.542131}, {-6.107121, -3.542131},
        {-6.100693, -3.548559}, {-6.100693, -3.548559},
        {-6.092658, -3.552576}, {-6.092658, -3.552576},
        {-6.081944, -3.559004}, {-6.081944, -3.559004},
        {-6.073373, -3.561013}, {-6.073373, -3.561013},
        {-6.065338, -3.565031}, {-6.065338, -3.565031},
        {-6.054625, -3.567040}, {-6.054625, -3.567040},
        {-6.044447, -3.569450}, {-6.044447, -3.569450},
        {-6.031591, -3.571459}, {-6.031591, -3.571459},
        {-6.021413, -3.571459}, {-6.021413, -3.571459},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() < stroke.size());
    // Clip lands at the L-hook pivot — the last sample whose
    // displacement from the endpoint pointed off the stable direction,
    // at (-6.138726, -3.527667). The few trailing dup samples at the
    // same position are retained.
    CHECK(trimmed.first().position == QPointF(-6.138726, -3.527667));
    CHECK(trimmed.last().position == QPointF(-6.021413, -3.571459));
}

TEST_CASE("cwPenStrokeFilter trims a V-hook at the end of a long iPad stroke",
          "[cwPenStrokeFilter]") {
    // Captured tail of iPad stroke 2 from output.txt (~last 45 raw
    // samples of a 579-sample stroke, ~2.37 m total path). Pen goes up
    // +Y toward the apex at y=0.629317 then retraces back down to
    // y=0.587534 — classic V at the end. The retrace is 42 mm, well
    // over the old 10 mm cap but fine under the 50 mm absolute /
    // 15%-of-stroke fraction cap. The whole 13-sample scan window
    // sits inside the retrace+approach region, so the L-hook stable-
    // direction detector can't help here — only the V-reversal stage
    // catches it.
    auto stroke = makeStroke({
        {-4.022268, 0.539324}, {-4.022268, 0.539324},
        {-4.024410, 0.543743}, {-4.024410, 0.543743},
        {-4.030839, 0.547760}, {-4.030839, 0.547760},
        {-4.035124, 0.549769}, {-4.035124, 0.549769},
        {-4.036731, 0.554189}, {-4.036731, 0.554189},
        {-4.041016, 0.558206}, {-4.041016, 0.558206},
        {-4.045302, 0.562625}, {-4.045302, 0.562625},
        {-4.047444, 0.566643}, {-4.047444, 0.566643},
        {-4.049587, 0.570661}, {-4.049587, 0.570661},
        {-4.051730, 0.577089}, {-4.051730, 0.577089},
        {-4.053873, 0.581106}, {-4.053873, 0.581106},
        {-4.053873, 0.587534}, {-4.053873, 0.587534},
        {-4.056015, 0.591552}, {-4.056015, 0.591552},
        {-4.056015, 0.597980}, {-4.056015, 0.597980},
        {-4.058158, 0.604408}, {-4.058158, 0.604408},
        {-4.058158, 0.608426}, {-4.058158, 0.608426},
        {-4.058158, 0.614854}, {-4.058158, 0.614854},
        {-4.059765, 0.618872}, {-4.059765, 0.618872},
        {-4.059765, 0.620880}, {-4.059765, 0.620880},
        {-4.059765, 0.625300}, {-4.059765, 0.625300},
        {-4.059765, 0.627308}, {-4.059765, 0.627308},
        {-4.061908, 0.629317}, {-4.061908, 0.629317}, {-4.061908, 0.629317},
        {-4.061908, 0.629317}, {-4.061908, 0.629317}, {-4.061908, 0.629317},
        {-4.061908, 0.625300}, {-4.061908, 0.625300},
        {-4.061908, 0.612443}, {-4.061908, 0.612443},
        {-4.061908, 0.599989}, {-4.061908, 0.599989},
        {-4.061908, 0.587534}, {-4.061908, 0.587534},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke, paramsForSlice());

    REQUIRE(trimmed.size() < stroke.size());
    // The retrace (last ~8 raw samples dropping from y=0.629 back to
    // y=0.587 at x=-4.062) must be gone: the trimmed tail should be
    // at or beyond the apex (y >= 0.627).
    CHECK(trimmed.last().position.y() >= 0.627);
    // Head is untouched — the body of the stroke is preserved.
    CHECK(trimmed.first().position == QPointF(-4.022268, 0.539324));
}

TEST_CASE("cwPenStrokeFilter trims a stationary-apex L-hook (iPad)",
          "[cwPenStrokeFilter]") {
    // Captured head of iPad stroke 6 (first ~65 raw samples of a
    // 453-sample, ~2.04 m stroke). Pen lands at (-1.6278, -0.9110),
    // drifts down-left to (-1.6508, -0.9275), sits there for 22
    // duplicate samples, then the real stroke emerges in the +X, -Y
    // direction. Each local angle is ~90° (L-shape), so V-reversal
    // misses it; the stable-direction detector must catch it.
    auto stroke = makeStroke({
        {-1.627793, -0.911021}, {-1.627793, -0.911021}, {-1.627793, -0.911021},
        {-1.636364, -0.913030}, {-1.636364, -0.913030},
        {-1.640114, -0.913030}, {-1.640114, -0.913030},
        {-1.642256, -0.915038}, {-1.642256, -0.915038},
        {-1.644399, -0.917047}, {-1.644399, -0.917047},
        {-1.646542, -0.919458}, {-1.646542, -0.919458},
        {-1.648685, -0.921467}, {-1.648685, -0.921467},
        {-1.650827, -0.923475}, {-1.650827, -0.923475},
        {-1.650827, -0.925484}, {-1.650827, -0.925484}, {-1.650827, -0.925484},
        {-1.650827, -0.925484}, {-1.650827, -0.925484},
        {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
        {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
        {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
        {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
        {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
        {-1.650827, -0.927493}, {-1.650827, -0.927493},
        {-1.648685, -0.927493}, {-1.648685, -0.927493},
        {-1.646542, -0.929904}, {-1.646542, -0.929904},
        {-1.642256, -0.931912}, {-1.642256, -0.931912},
        {-1.636364, -0.933921}, {-1.636364, -0.933921},
        {-1.634221, -0.935930}, {-1.634221, -0.935930},
        {-1.627793, -0.940349}, {-1.627793, -0.940349},
        {-1.621365, -0.942358}, {-1.621365, -0.942358},
        {-1.614937, -0.944367}, {-1.614937, -0.944367},
        {-1.606902, -0.946376}, {-1.606902, -0.946376},
        {-1.600474, -0.948384}, {-1.600474, -0.948384},
        {-1.591903, -0.950795}, {-1.591903, -0.950795},
        {-1.583868, -0.952804}, {-1.583868, -0.952804},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke, paramsForSlice());

    REQUIRE(trimmed.size() < stroke.size());
    // Clip lands at the L-hook pivot — the last sample misaligned
    // with the stable direction, which sits inside the stationary
    // cluster at (-1.650827, -0.927493).
    CHECK(trimmed.first().position == QPointF(-1.650827, -0.927493));
    CHECK(trimmed.last().position == QPointF(-1.583868, -0.952804));
}

TEST_CASE("cwPenStrokeFilter preserves a clean long diagonal iPad stroke head",
          "[cwPenStrokeFilter]") {
    // Captured head of iPad stroke 1 (first ~55 raw samples of a
    // 498-sample, ~1.85 m stroke). The pen lands and immediately moves
    // in a clean -X, -Y diagonal. There is a 90° local angle between
    // the first +Y-only step and the subsequent -X-only step, but the
    // net displacement stays aligned with the stable direction, so
    // neither detector stage should fire. We keep every sample.
    auto stroke = makeStroke({
        {0.071909, 1.143567}, {0.071909, 1.143567}, {0.071909, 1.143567},
        {0.071909, 1.143567}, {0.071909, 1.143567}, {0.071909, 1.143567},
        {0.071909, 1.141156}, {0.071909, 1.141156},
        {0.069766, 1.141156}, {0.069766, 1.141156},
        {0.067623, 1.139147}, {0.067623, 1.139147},
        {0.065481, 1.137139}, {0.065481, 1.137139},
        {0.061731, 1.135130}, {0.061731, 1.135130},
        {0.059588, 1.133121}, {0.059588, 1.133121},
        {0.055303, 1.128702}, {0.055303, 1.128702},
        {0.053160, 1.126693}, {0.053160, 1.126693},
        {0.051017, 1.122675}, {0.051017, 1.122675},
        {0.046732, 1.118256}, {0.046732, 1.118256},
        {0.044589, 1.116247}, {0.044589, 1.116247},
        {0.040304, 1.112230}, {0.040304, 1.112230},
        {0.038697, 1.105802}, {0.038697, 1.105802},
        {0.034411, 1.101784}, {0.034411, 1.101784},
        {0.030126, 1.097365}, {0.030126, 1.097365},
        {0.023698, 1.091338}, {0.023698, 1.091338},
        {0.017270, 1.084910}, {0.017270, 1.084910},
        {0.011377, 1.080893}, {0.011377, 1.080893},
        {0.004949, 1.072456}, {0.004949, 1.072456},
        {-0.003622, 1.066028}, {-0.003622, 1.066028},
        {-0.011657, 1.060001}, {-0.011657, 1.060001},
        {-0.022370, 1.051564}, {-0.022370, 1.051564},
        {-0.032548, 1.045136}, {-0.032548, 1.045136},
        {-0.038976, 1.039110},
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke, paramsForSlice());

    // Nothing should be trimmed: the stroke is clean.
    REQUIRE(trimmed.size() == stroke.size());
    CHECK(trimmed.first().position == QPointF(0.071909, 1.143567));
    CHECK(trimmed.last().position == QPointF(-0.038976, 1.039110));
}

TEST_CASE("cwPenStrokeFilter trims a symmetric end hook", "[cwPenStrokeFilter]") {
    // Mirror of the start-hook test: the stroke travels +X, then the
    // last three samples reverse and drift back in -X+Y for < 1 mm.
    auto stroke = makeStroke({
        {0.00,  0.0},
        {0.010, 0.0},
        {0.020, 0.0},
        {0.030, 0.0},
        {0.040, 0.0},
        {0.050, 0.0},
        {0.060, 0.0},
        {0.070, 0.0},
        {0.080, 0.0},
        {0.090, 0.0},
        {0.100,  0.0},     // true stroke end
        {0.0995, 0.0003},
        {0.0990, 0.0007},
        {0.0990, 0.0010},  // hook tip
    });

    auto trimmed = cwPenStrokeFilter::trimHooks(stroke);

    REQUIRE(trimmed.size() < stroke.size());
    CHECK(trimmed.first().position == QPointF(0.00, 0.0));
    CHECK(trimmed.last().position != stroke.last().position);
}
