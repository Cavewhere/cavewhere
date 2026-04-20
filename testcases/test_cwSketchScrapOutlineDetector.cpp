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
