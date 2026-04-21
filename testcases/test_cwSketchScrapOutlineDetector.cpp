//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QPointF>
#include <QUuid>

//Our includes
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwSketchScrapOutline.h"
#include "cwSketchScrapOutlineDetector.h"

namespace {

constexpr double kCloseThreshold     = 0.05; // 5 cm
constexpr double kSimplifyTolerance  = 0.005; // 5 mm (fine enough to keep corners)

cwPenStroke makeStroke(cwPenStroke::Kind kind, const QVector<QPointF> &pts)
{
    cwPenStroke s;
    s.kind  = kind;
    s.width = 2.0;
    s.id    = QUuid::createUuid();
    s.points.reserve(pts.size());
    for (const auto &p : pts) {
        s.points.append(cwPenPoint(p, 1.0, 0));
    }
    return s;
}

double signedArea(const QPolygonF &ring)
{
    double sum = 0.0;
    const int n = ring.size();
    for (int i = 0; i < n; ++i) {
        const QPointF &a = ring.at(i);
        const QPointF &b = ring.at((i + 1) % n);
        sum += a.x() * b.y() - b.x() * a.y();
    }
    return 0.5 * sum;
}

} // namespace

TEST_CASE("Closed Wall stroke produces one CCW outline", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall}, kCloseThreshold, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().sourceStrokeId == wall.id);
    CHECK(out.first().tripLocalPolygon.size() == 4); // closure collapses duplicate endpoint
    CHECK(signedArea(out.first().tripLocalPolygon) > 0.0); // CCW

    const QRectF bbox = out.first().tripLocalPolygon.boundingRect();
    CHECK(bbox.left()   == 0.0);
    CHECK(bbox.top()    == 0.0);
    CHECK(bbox.right()  == 1.0);
    CHECK(bbox.bottom() == 1.0);
}

TEST_CASE("Closed ScrapOutline stroke produces one outline", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke outline = makeStroke(cwPenStroke::ScrapOutline, {
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {outline}, kCloseThreshold, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().sourceStrokeId == outline.id);
}

TEST_CASE("Closed Feature stroke is ignored", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke feature = makeStroke(cwPenStroke::Feature, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {feature}, kCloseThreshold, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("Open Wall polyline is ignored", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke open = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}
    }); // does NOT close back to (0,0)

    const auto out = cwSketchScrapOutlineDetector::detect(
        {open}, kCloseThreshold, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("Near-closed stroke inside threshold snaps to closed", "[cwSketchScrapOutlineDetector]") {
    // Last point ends 3cm short of first — well inside the 5cm threshold.
    const cwPenStroke nearClosed = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.03, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {nearClosed}, kCloseThreshold, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    // Midpoint of first (0,0) and last (0.03,0) is (0.015,0).
    CHECK(out.first().tripLocalPolygon.first().x() == 0.015);
    CHECK(out.first().tripLocalPolygon.first().y() == 0.0);
}

TEST_CASE("Near-closed stroke outside threshold is rejected", "[cwSketchScrapOutlineDetector]") {
    // 10 cm gap — larger than the 5 cm threshold.
    const cwPenStroke tooOpen = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.10, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {tooOpen}, kCloseThreshold, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("Self-intersecting closed stroke is rejected", "[cwSketchScrapOutlineDetector]") {
    // Bowtie: two triangles crossing at the middle edge.
    const cwPenStroke bowtie = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {bowtie}, kCloseThreshold, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("Degenerate strokes (< 3 points) are rejected", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke empty = makeStroke(cwPenStroke::Wall, {});
    const cwPenStroke one   = makeStroke(cwPenStroke::Wall, {{0.0, 0.0}});
    const cwPenStroke two   = makeStroke(cwPenStroke::Wall, {{0.0, 0.0}, {0.001, 0.0}});

    const auto out = cwSketchScrapOutlineDetector::detect(
        {empty, one, two}, kCloseThreshold, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("Clockwise input is normalized to CCW", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke cw = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {cw}, kCloseThreshold, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(signedArea(out.first().tripLocalPolygon) > 0.0);
}

TEST_CASE("Douglas–Peucker drops collinear interior points", "[cwSketchScrapOutlineDetector]") {
    // A square with extra interior points on one edge; all collinear
    // points between the corners should be removed.
    const cwPenStroke dense = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0},
        {0.25, 0.0}, {0.50, 0.0}, {0.75, 0.0},
        {1.0, 0.0},
        {1.0, 1.0},
        {0.0, 1.0},
        {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {dense}, kCloseThreshold, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().tripLocalPolygon.size() == 4);
}

TEST_CASE("Zero outset leaves polygon unchanged", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto baseline = cwSketchScrapOutlineDetector::detect(
        {wall}, kCloseThreshold, kSimplifyTolerance);
    const auto zero = cwSketchScrapOutlineDetector::detect(
        {wall}, kCloseThreshold, kSimplifyTolerance, 0.0);

    REQUIRE(baseline.size() == 1);
    REQUIRE(zero.size() == 1);
    REQUIRE(baseline.first().tripLocalPolygon.size() == zero.first().tripLocalPolygon.size());
    for (int i = 0; i < baseline.first().tripLocalPolygon.size(); ++i) {
        CHECK(baseline.first().tripLocalPolygon.at(i) == zero.first().tripLocalPolygon.at(i));
    }
}

TEST_CASE("Positive outset enlarges a square polygon by the offset distance", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.1;
    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall}, kCloseThreshold, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 1);
    const QPolygonF &ring = out.first().tripLocalPolygon;
    CHECK(ring.size() == 4); // right-angle corners: miter stays in one vertex per corner

    const QRectF bbox = ring.boundingRect();
    const double eps = 1e-9;
    CHECK(std::abs(bbox.left()   - (-offset))          < eps);
    CHECK(std::abs(bbox.top()    - (-offset))          < eps);
    CHECK(std::abs(bbox.right()  - (1.0 + offset))     < eps);
    CHECK(std::abs(bbox.bottom() - (1.0 + offset))     < eps);
    CHECK(signedArea(ring) > 1.0); // area grew beyond the unit square
}

TEST_CASE("Positive outset enlarges a triangle and keeps it a triangle", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.05;
    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall}, kCloseThreshold, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 1);
    const QPolygonF &ring = out.first().tripLocalPolygon;
    CHECK(ring.size() == 3);
    CHECK(signedArea(ring) > 0.0);

    const QRectF bbox = ring.boundingRect();
    // Every edge moves outward by at least `offset`, so the bbox grows by
    // at least `offset` on all four sides.
    CHECK(bbox.left()   <= -offset + 1e-9);
    CHECK(bbox.top()    <= -offset + 1e-9);
    CHECK(bbox.right()  >= 1.0 + offset - 1e-9);
    CHECK(bbox.bottom() >= 1.0 + offset - 1e-9);
}

TEST_CASE("Outset preserves CCW orientation regardless of input winding", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke cw = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {cw}, kCloseThreshold, kSimplifyTolerance, 0.05);

    REQUIRE(out.size() == 1);
    CHECK(signedArea(out.first().tripLocalPolygon) > 0.0);
}

TEST_CASE("Outset on a non-convex polygon moves every vertex outside the input", "[cwSketchScrapOutlineDetector]") {
    // L-shape with one reflex (concave) corner at (1,1). CCW winding.
    const cwPenStroke lshape = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.0},
        {1.0, 2.0}, {0.0, 2.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.05;
    const auto baseline = cwSketchScrapOutlineDetector::detect(
        {lshape}, kCloseThreshold, kSimplifyTolerance);
    const auto out = cwSketchScrapOutlineDetector::detect(
        {lshape}, kCloseThreshold, kSimplifyTolerance, offset);

    REQUIRE(baseline.size() == 1);
    REQUIRE(out.size() == 1);
    const QPolygonF &inputRing = baseline.first().tripLocalPolygon;
    const QPolygonF &outRing   = out.first().tripLocalPolygon;

    CHECK(signedArea(outRing) > 0.0); // still CCW
    CHECK(signedArea(outRing) > signedArea(inputRing)); // enlarged

    // Every output vertex sits outside (or on) the original polygon — the
    // reflex corner at (1,1) must move into the notch interior, away from
    // the original boundary, while convex corners bulge outward.
    for (const QPointF &p : outRing) {
        CHECK_FALSE(inputRing.containsPoint(p, Qt::OddEvenFill));
    }
}

TEST_CASE("Miter cap prevents spikes on sharp acute corners", "[cwSketchScrapOutlineDetector]") {
    // A narrow dart: one vertex at (1,0) forms a very acute angle. Without
    // capping, the bisector miter would shoot out roughly 1/sin(halfAngle)
    // times the offset distance. kMiterCap = 4.0 in the implementation.
    const cwPenStroke dart = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.05}, {0.0, 0.0}
    });

    constexpr double offset        = 0.02;
    constexpr double kMiterCapFact = 4.0;
    const auto baseline = cwSketchScrapOutlineDetector::detect(
        {dart}, kCloseThreshold, kSimplifyTolerance);
    const auto out = cwSketchScrapOutlineDetector::detect(
        {dart}, kCloseThreshold, kSimplifyTolerance, offset);

    REQUIRE(baseline.size() == 1);
    REQUIRE(out.size() == 1);
    const QPolygonF &inputRing = baseline.first().tripLocalPolygon;
    const QPolygonF &outRing   = out.first().tripLocalPolygon;
    REQUIRE(outRing.size() == inputRing.size());

    bool checked = false;
    for (int i = 0; i < inputRing.size(); ++i) {
        if (std::abs(inputRing.at(i).x() - 1.0) < 1e-9
            && std::abs(inputRing.at(i).y() - 0.0) < 1e-9) {
            const QPointF d = outRing.at(i) - inputRing.at(i);
            const double moved = std::hypot(d.x(), d.y());
            CHECK(moved <= kMiterCapFact * offset + 1e-9);
            checked = true;
        }
    }
    CHECK(checked);
}

TEST_CASE("Outset larger than notch half-width falls back to un-offset ring", "[cwSketchScrapOutlineDetector]") {
    // CCW C-shape with a 1 m wide notch on the right. Outward-offsetting by
    // 1.0 m forces the two reflex corners (1,1) and (1,3) to converge onto
    // the same point, making the offset ring self-intersect across the
    // former notch — detect() must fall back to the un-offset polygon.
    const cwPenStroke cshape = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.0},
        {1.0, 3.0}, {2.0, 3.0}, {2.0, 4.0}, {0.0, 4.0}, {0.0, 0.0}
    });

    const auto baseline = cwSketchScrapOutlineDetector::detect(
        {cshape}, kCloseThreshold, kSimplifyTolerance);
    const auto out = cwSketchScrapOutlineDetector::detect(
        {cshape}, kCloseThreshold, kSimplifyTolerance, 1.0);

    REQUIRE(baseline.size() == 1);
    REQUIRE(out.size() == 1);
    const QPolygonF &baseRing = baseline.first().tripLocalPolygon;
    const QPolygonF &outRing  = out.first().tripLocalPolygon;
    REQUIRE(baseRing.size() == outRing.size());
    for (int i = 0; i < baseRing.size(); ++i) {
        CHECK(baseRing.at(i) == outRing.at(i));
    }
}

TEST_CASE("Outset applies to ScrapOutline strokes as well as Wall strokes", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke outline = makeStroke(cwPenStroke::ScrapOutline, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.1;
    const auto out = cwSketchScrapOutlineDetector::detect(
        {outline}, kCloseThreshold, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 1);
    const QRectF bbox = out.first().tripLocalPolygon.boundingRect();
    const double eps = 1e-9;
    CHECK(std::abs(bbox.left()   - (-offset))      < eps);
    CHECK(std::abs(bbox.top()    - (-offset))      < eps);
    CHECK(std::abs(bbox.right()  - (1.0 + offset)) < eps);
    CHECK(std::abs(bbox.bottom() - (1.0 + offset)) < eps);
}

TEST_CASE("Mixed list: outset applied independently per outline", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });
    const cwPenStroke outline = makeStroke(cwPenStroke::ScrapOutline, {
        {20.0, 0.0}, {21.0, 0.0}, {21.0, 1.0}, {20.0, 1.0}, {20.0, 0.0}
    });

    constexpr double offset = 0.05;
    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall, outline}, kCloseThreshold, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 2);

    const double eps = 1e-9;
    const QRectF b0 = out.at(0).tripLocalPolygon.boundingRect();
    CHECK(std::abs(b0.left()   - (-offset))      < eps);
    CHECK(std::abs(b0.right()  - (1.0 + offset)) < eps);

    const QRectF b1 = out.at(1).tripLocalPolygon.boundingRect();
    CHECK(std::abs(b1.left()   - (20.0 - offset)) < eps);
    CHECK(std::abs(b1.right()  - (21.0 + offset)) < eps);
}

TEST_CASE("Mixed stroke list produces one outline per qualifying stroke", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });
    const cwPenStroke feature = makeStroke(cwPenStroke::Feature, {
        {5.0, 5.0}, {6.0, 5.0}, {6.0, 6.0}, {5.0, 6.0}, {5.0, 5.0}
    });
    const cwPenStroke openWall = makeStroke(cwPenStroke::Wall, {
        {10.0, 10.0}, {11.0, 10.0}, {11.0, 11.0}
    });
    const cwPenStroke outline = makeStroke(cwPenStroke::ScrapOutline, {
        {20.0, 0.0}, {21.0, 0.0}, {21.0, 1.0}, {20.0, 1.0}, {20.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall, feature, openWall, outline}, kCloseThreshold, kSimplifyTolerance);

    REQUIRE(out.size() == 2);
    CHECK(out.at(0).sourceStrokeId == wall.id);
    CHECK(out.at(1).sourceStrokeId == outline.id);
}
