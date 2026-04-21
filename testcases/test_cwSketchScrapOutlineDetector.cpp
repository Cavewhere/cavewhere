//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QPointF>
#include <QUuid>

//Std includes
#include <algorithm>
#include <cmath>

//Our includes
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwSketchScrapOutline.h"
#include "cwSketchScrapOutlineDetector.h"

namespace {

constexpr double kSimplifyTolerance = 0.005; // 5 mm — fine enough to keep corners

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

QVector<QUuid> sorted(QVector<QUuid> ids)
{
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

TEST_CASE("Closed Wall stroke produces one CCW outline", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect({wall}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == QVector<QUuid>{wall.id});
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

    const auto out = cwSketchScrapOutlineDetector::detect({outline}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == QVector<QUuid>{outline.id});
}

TEST_CASE("Closed Feature stroke is ignored", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke feature = makeStroke(cwPenStroke::Feature, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect({feature}, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("Single open stroke self-caps into a closed ring", "[cwSketchScrapOutlineDetector]") {
    // Last point ends 3 cm short of the first. With nearest-endpoint
    // matching the stroke self-pairs and the two endpoints collapse to
    // their midpoint (0.015, 0).
    const cwPenStroke stroke = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.03, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect({stroke}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == QVector<QUuid>{stroke.id});
    CHECK(out.first().tripLocalPolygon.first().x() == 0.015);
    CHECK(out.first().tripLocalPolygon.first().y() == 0.0);
}

TEST_CASE("Self-intersecting bowtie salvages into one outer lobe",
          "[cwSketchScrapOutlineDetector]") {
    // Bowtie: two triangles crossing at the middle edge. Under OddEvenFill,
    // QPainterPath::simplified resolves the figure-8 into two disjoint
    // triangle subpaths; the salvage picks the larger one (or either, since
    // the two lobes are congruent). The user's drawing intent for a bowtie
    // is ambiguous — this tests that Patch 1 emits one outline rather than
    // silently dropping the input.
    const cwPenStroke bowtie = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect({bowtie}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    const QPolygonF &ring = out.first().tripLocalPolygon;
    CHECK(ring.size() == 3); // a triangle lobe
    CHECK(signedArea(ring) > 0.0); // normalized to CCW
}

TEST_CASE("Tiny hook at seam is salvaged instead of dropping the outline",
          "[cwSketchScrapOutlineDetector]") {
    // User drew an almost-closed unit square, then restarted drawing on
    // the start edge and overshot by 3 cm past the origin before releasing.
    // The last segment crosses the first edge, producing a self-intersecting
    // ring with a tiny hook lobe. Patch 1 must salvage the main square.
    const cwPenStroke hooked = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0},
        {1.0, 0.0},
        {1.0, 1.0},
        {0.0, 1.0},
        {0.0, -0.03}, // overshoot past origin along the start edge
        {0.03, 0.0}   // tiny hook re-entering the square
    });

    const auto out = cwSketchScrapOutlineDetector::detect({hooked}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    const QPolygonF &ring = out.first().tripLocalPolygon;
    CHECK(signedArea(ring) > 0.0);

    // Main square area dominates the tiny hook sliver: the salvaged ring
    // should be close to 1 m² and clearly not the hook lobe.
    CHECK(signedArea(ring) > 0.9);

    // All corners of the input square lie on or inside the salvaged ring.
    CHECK(ring.containsPoint(QPointF(0.5, 0.5), Qt::OddEvenFill));
}

TEST_CASE("Degenerate strokes (< 2 points) are rejected", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke empty = makeStroke(cwPenStroke::Wall, {});
    const cwPenStroke one   = makeStroke(cwPenStroke::Wall, {{0.0, 0.0}});

    const auto out = cwSketchScrapOutlineDetector::detect({empty, one}, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("detectWithDiagnostics reports too-few-points rejections",
          "[cwSketchScrapOutlineDetector][diagnostics]") {
    const cwPenStroke empty = makeStroke(cwPenStroke::Wall, {});
    const cwPenStroke one   = makeStroke(cwPenStroke::Wall, {{0.0, 0.0}});
    const cwPenStroke valid = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto result = cwSketchScrapOutlineDetector::detectWithDiagnostics(
        {empty, one, valid}, kSimplifyTolerance);

    REQUIRE(result.outlines.size() == 1);
    REQUIRE(result.rejected.size() == 2);

    QVector<QUuid> rejectedIds;
    for (const auto &r : result.rejected) {
        rejectedIds.append(r.id);
        CHECK(r.reason == QString::fromLatin1(
            cwSketchScrapRejectReasons::TooFewPoints));
    }
    std::sort(rejectedIds.begin(), rejectedIds.end());
    QVector<QUuid> expected{empty.id, one.id};
    std::sort(expected.begin(), expected.end());
    CHECK(rejectedIds == expected);
}

TEST_CASE("detectWithDiagnostics flags kept-but-rejected feature strokes as silent",
          "[cwSketchScrapOutlineDetector][diagnostics]") {
    // Feature strokes are intentionally ignored — no rejection record
    // (they're not failing outlines, they're a different stroke kind).
    const cwPenStroke feature = makeStroke(cwPenStroke::Feature, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto result = cwSketchScrapOutlineDetector::detectWithDiagnostics(
        {feature}, kSimplifyTolerance);

    CHECK(result.outlines.isEmpty());
    CHECK(result.rejected.isEmpty());
}

TEST_CASE("detect wrapper returns the same outlines as detectWithDiagnostics",
          "[cwSketchScrapOutlineDetector][diagnostics]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto wrapped = cwSketchScrapOutlineDetector::detect({wall}, kSimplifyTolerance);
    const auto full    = cwSketchScrapOutlineDetector::detectWithDiagnostics({wall}, kSimplifyTolerance);

    REQUIRE(wrapped.size() == full.outlines.size());
    CHECK(wrapped == full.outlines);
}

TEST_CASE("Lone straight two-point stroke collapses below ring threshold",
          "[cwSketchScrapOutlineDetector]") {
    // Self-pair merges the two endpoints into a single midpoint vertex.
    // The resulting ring has only one point — well below the ≥3-vertex
    // ring guard, so no outline is emitted.
    const cwPenStroke line = makeStroke(cwPenStroke::Wall, {{0.0, 0.0}, {0.001, 0.0}});

    const auto out = cwSketchScrapOutlineDetector::detect({line}, kSimplifyTolerance);

    CHECK(out.isEmpty());
}

TEST_CASE("Clockwise input is normalized to CCW", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke cw = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect({cw}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(signedArea(out.first().tripLocalPolygon) > 0.0);
}

TEST_CASE("Douglas–Peucker drops collinear interior points", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke dense = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0},
        {0.25, 0.0}, {0.50, 0.0}, {0.75, 0.0},
        {1.0, 0.0},
        {1.0, 1.0},
        {0.0, 1.0},
        {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect({dense}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().tripLocalPolygon.size() == 4);
}

TEST_CASE("Zero outset leaves polygon unchanged", "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto baseline = cwSketchScrapOutlineDetector::detect({wall}, kSimplifyTolerance);
    const auto zero     = cwSketchScrapOutlineDetector::detect({wall}, kSimplifyTolerance, 0.0);

    REQUIRE(baseline.size() == 1);
    REQUIRE(zero.size() == 1);
    REQUIRE(baseline.first().tripLocalPolygon.size() == zero.first().tripLocalPolygon.size());
    for (int i = 0; i < baseline.first().tripLocalPolygon.size(); ++i) {
        CHECK(baseline.first().tripLocalPolygon.at(i) == zero.first().tripLocalPolygon.at(i));
    }
}

TEST_CASE("Positive outset enlarges a square polygon by the offset distance",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.1;
    const auto out = cwSketchScrapOutlineDetector::detect({wall}, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 1);
    const QPolygonF &ring = out.first().tripLocalPolygon;
    CHECK(ring.size() == 4);

    const QRectF bbox = ring.boundingRect();
    const double eps = 1e-9;
    CHECK(std::abs(bbox.left()   - (-offset))      < eps);
    CHECK(std::abs(bbox.top()    - (-offset))      < eps);
    CHECK(std::abs(bbox.right()  - (1.0 + offset)) < eps);
    CHECK(std::abs(bbox.bottom() - (1.0 + offset)) < eps);
    CHECK(signedArea(ring) > 1.0);
}

TEST_CASE("Positive outset enlarges a triangle and keeps it a triangle",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.05;
    const auto out = cwSketchScrapOutlineDetector::detect({wall}, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 1);
    const QPolygonF &ring = out.first().tripLocalPolygon;
    CHECK(ring.size() == 3);
    CHECK(signedArea(ring) > 0.0);

    const QRectF bbox = ring.boundingRect();
    CHECK(bbox.left()   <= -offset + 1e-9);
    CHECK(bbox.top()    <= -offset + 1e-9);
    CHECK(bbox.right()  >= 1.0 + offset - 1e-9);
    CHECK(bbox.bottom() >= 1.0 + offset - 1e-9);
}

TEST_CASE("Outset preserves CCW orientation regardless of input winding",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke cw = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect({cw}, kSimplifyTolerance, 0.05);

    REQUIRE(out.size() == 1);
    CHECK(signedArea(out.first().tripLocalPolygon) > 0.0);
}

TEST_CASE("Outset on a non-convex polygon moves every vertex outside the input",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke lshape = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.0},
        {1.0, 2.0}, {0.0, 2.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.05;
    const auto baseline = cwSketchScrapOutlineDetector::detect({lshape}, kSimplifyTolerance);
    const auto out      = cwSketchScrapOutlineDetector::detect({lshape}, kSimplifyTolerance, offset);

    REQUIRE(baseline.size() == 1);
    REQUIRE(out.size() == 1);
    const QPolygonF &inputRing = baseline.first().tripLocalPolygon;
    const QPolygonF &outRing   = out.first().tripLocalPolygon;

    CHECK(signedArea(outRing) > 0.0);
    CHECK(signedArea(outRing) > signedArea(inputRing));

    for (const QPointF &p : outRing) {
        CHECK_FALSE(inputRing.containsPoint(p, Qt::OddEvenFill));
    }
}

TEST_CASE("Miter cap prevents spikes on sharp acute corners",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke dart = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.05}, {0.0, 0.0}
    });

    constexpr double offset        = 0.02;
    constexpr double kMiterCapFact = 4.0;
    const auto baseline = cwSketchScrapOutlineDetector::detect({dart}, kSimplifyTolerance);
    const auto out      = cwSketchScrapOutlineDetector::detect({dart}, kSimplifyTolerance, offset);

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

TEST_CASE("Outset larger than notch half-width falls back to un-offset ring",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke cshape = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.0},
        {1.0, 3.0}, {2.0, 3.0}, {2.0, 4.0}, {0.0, 4.0}, {0.0, 0.0}
    });

    const auto baseline = cwSketchScrapOutlineDetector::detect({cshape}, kSimplifyTolerance);
    const auto out      = cwSketchScrapOutlineDetector::detect({cshape}, kSimplifyTolerance, 1.0);

    REQUIRE(baseline.size() == 1);
    REQUIRE(out.size() == 1);
    const QPolygonF &baseRing = baseline.first().tripLocalPolygon;
    const QPolygonF &outRing  = out.first().tripLocalPolygon;
    REQUIRE(baseRing.size() == outRing.size());
    for (int i = 0; i < baseRing.size(); ++i) {
        CHECK(baseRing.at(i) == outRing.at(i));
    }
}

TEST_CASE("Outset applies to ScrapOutline strokes as well as Wall strokes",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke outline = makeStroke(cwPenStroke::ScrapOutline, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.1;
    const auto out = cwSketchScrapOutlineDetector::detect({outline}, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 1);
    const QRectF bbox = out.first().tripLocalPolygon.boundingRect();
    const double eps = 1e-9;
    CHECK(std::abs(bbox.left()   - (-offset))      < eps);
    CHECK(std::abs(bbox.top()    - (-offset))      < eps);
    CHECK(std::abs(bbox.right()  - (1.0 + offset)) < eps);
    CHECK(std::abs(bbox.bottom() - (1.0 + offset)) < eps);
}

TEST_CASE("Mixed list: outset applied independently per outline",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });
    const cwPenStroke outline = makeStroke(cwPenStroke::ScrapOutline, {
        {20.0, 0.0}, {21.0, 0.0}, {21.0, 1.0}, {20.0, 1.0}, {20.0, 0.0}
    });

    constexpr double offset = 0.05;
    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall, outline}, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 2);

    const double eps = 1e-9;

    auto findOutlineContaining = [&](const QPointF &p) -> const cwSketchScrapOutline* {
        for (const auto &o : out) {
            if (o.tripLocalPolygon.containsPoint(p, Qt::OddEvenFill)) {
                return &o;
            }
        }
        return nullptr;
    };

    const auto *nearOrigin = findOutlineContaining({0.5, 0.5});
    const auto *nearTwenty = findOutlineContaining({20.5, 0.5});
    REQUIRE(nearOrigin != nullptr);
    REQUIRE(nearTwenty != nullptr);

    const QRectF b0 = nearOrigin->tripLocalPolygon.boundingRect();
    CHECK(std::abs(b0.left()  - (-offset))      < eps);
    CHECK(std::abs(b0.right() - (1.0 + offset)) < eps);

    const QRectF b1 = nearTwenty->tripLocalPolygon.boundingRect();
    CHECK(std::abs(b1.left()  - (20.0 - offset)) < eps);
    CHECK(std::abs(b1.right() - (21.0 + offset)) < eps);
}

TEST_CASE("Feature strokes are ignored by chaining",
          "[cwSketchScrapOutlineDetector]") {
    // One closed Wall plus one Feature stroke with endpoints touching the
    // wall's corners. The Feature must be invisible to the matcher — the
    // outline contains only the Wall id (self-paired).
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });
    const cwPenStroke feature = makeStroke(cwPenStroke::Feature, {
        {0.0, 0.0}, {0.5, 0.5}, {1.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall, feature}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == QVector<QUuid>{wall.id});
}

TEST_CASE("Two open walls forming a closed square chain",
          "[cwSketchScrapOutlineDetector]") {
    // Two L-shaped Wall strokes whose free endpoints meet at opposite
    // corners of a unit square.
    const cwPenStroke wall1 = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}
    });
    const cwPenStroke wall2 = makeStroke(cwPenStroke::Wall, {
        {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall1, wall2}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == sorted({wall1.id, wall2.id}));
    CHECK(signedArea(out.first().tripLocalPolygon) > 0.0);
    // Ring simplifies to the four corners of the unit square.
    CHECK(out.first().tripLocalPolygon.size() == 4);
}

TEST_CASE("Three strokes mixing Wall and ScrapOutline chain into a triangle",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke edgeA = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {2.0, 0.0}
    });
    const cwPenStroke edgeB = makeStroke(cwPenStroke::Wall, {
        {2.0, 0.0}, {1.0, 2.0}
    });
    const cwPenStroke edgeC = makeStroke(cwPenStroke::ScrapOutline, {
        {1.0, 2.0}, {0.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {edgeA, edgeB, edgeC}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == sorted({edgeA.id, edgeB.id, edgeC.id}));
    CHECK(out.first().tripLocalPolygon.size() == 3);
    CHECK(signedArea(out.first().tripLocalPolygon) > 0.0);
}

TEST_CASE("Auto-cap: two parallel walls with wide leads close into one outline",
          "[cwSketchScrapOutlineDetector]") {
    // Parallel walls 5 m apart. With no distance cutoff, each wall's
    // endpoints pair with the nearest endpoints of the other wall, auto-
    // capping into a closed passage.
    const cwPenStroke wallLeft = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {0.0, 10.0}
    });
    const cwPenStroke wallRight = makeStroke(cwPenStroke::Wall, {
        {5.0, 0.0}, {5.0, 10.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wallLeft, wallRight}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == sorted({wallLeft.id, wallRight.id}));

    const QRectF bbox = out.first().tripLocalPolygon.boundingRect();
    CHECK(bbox.left()   <= 0.0 + 1e-9);
    CHECK(bbox.right()  >= 5.0 - 1e-9);
    CHECK(bbox.top()    <= 0.0 + 1e-9);
    CHECK(bbox.bottom() >= 10.0 - 1e-9);
}

TEST_CASE("Explicit ScrapOutline stroke overrides the auto-cap",
          "[cwSketchScrapOutlineDetector]") {
    // Two parallel walls plus a short ScrapOutline cap placed between the
    // walls' near ends. The cap's endpoints are closer to wall ends than
    // the opposite wall's ends, so greedy matching picks the cap first.
    // The outline must walk through the cap on that side instead of
    // bridging the full passage.
    const cwPenStroke wallLeft = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {0.0, 10.0}
    });
    const cwPenStroke wallRight = makeStroke(cwPenStroke::Wall, {
        {5.0, 0.0}, {5.0, 10.0}
    });
    const cwPenStroke cap = makeStroke(cwPenStroke::ScrapOutline, {
        {0.0, 0.0}, {5.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wallLeft, wallRight, cap}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    CHECK(out.first().memberStrokeIds == sorted({wallLeft.id, wallRight.id, cap.id}));
    // The polygon should include the explicit cap edge on the south side
    // and a mid-point auto-cap on the north side — four distinct vertices
    // after simplification.
    CHECK(out.first().tripLocalPolygon.size() >= 3);
}

TEST_CASE("Two separated passages yield two outlines with disjoint members",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke pA_left  = makeStroke(cwPenStroke::Wall, {{0.0, 0.0},  {0.0, 4.0}});
    const cwPenStroke pA_right = makeStroke(cwPenStroke::Wall, {{2.0, 0.0},  {2.0, 4.0}});
    const cwPenStroke pB_left  = makeStroke(cwPenStroke::Wall, {{50.0, 0.0}, {50.0, 4.0}});
    const cwPenStroke pB_right = makeStroke(cwPenStroke::Wall, {{52.0, 0.0}, {52.0, 4.0}});

    const auto out = cwSketchScrapOutlineDetector::detect(
        {pA_left, pA_right, pB_left, pB_right}, kSimplifyTolerance);

    REQUIRE(out.size() == 2);

    // Each outline contains exactly two member ids, never crossing the gap.
    const QVector<QUuid> passageA = sorted({pA_left.id, pA_right.id});
    const QVector<QUuid> passageB = sorted({pB_left.id, pB_right.id});

    QVector<QVector<QUuid>> seen;
    for (const auto &o : out) {
        seen.append(o.memberStrokeIds);
    }
    std::sort(seen.begin(), seen.end());
    QVector<QVector<QUuid>> expected{passageA, passageB};
    std::sort(expected.begin(), expected.end());
    CHECK(seen == expected);
}

TEST_CASE("Chain memberStrokeIds are sorted ascending",
          "[cwSketchScrapOutlineDetector]") {
    // Force two ids so wall2.id < wall1.id regardless of QUuid::createUuid()
    // output. The emitted vector must be ascending — that's the canonical
    // form cwScrapManager's hash key depends on.
    QUuid high = QUuid::createUuid();
    QUuid low  = QUuid::createUuid();
    if (high < low) {
        std::swap(high, low);
    }
    cwPenStroke wall1 = makeStroke(cwPenStroke::Wall,
                                   {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}});
    wall1.id = high;
    cwPenStroke wall2 = makeStroke(cwPenStroke::Wall,
                                   {{1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}});
    wall2.id = low;

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall1, wall2}, kSimplifyTolerance);

    REQUIRE(out.size() == 1);
    REQUIRE(out.first().memberStrokeIds.size() == 2);
    CHECK(out.first().memberStrokeIds.at(0) == low);
    CHECK(out.first().memberStrokeIds.at(1) == high);
}

TEST_CASE("Chained ring honors outsetMeters",
          "[cwSketchScrapOutlineDetector]") {
    // Two-stroke chain that closes a unit square. Regression guard for
    // seams producing near-duplicate points that would otherwise break
    // offset-normal math.
    const cwPenStroke wall1 = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}
    });
    const cwPenStroke wall2 = makeStroke(cwPenStroke::Wall, {
        {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });

    constexpr double offset = 0.05;
    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall1, wall2}, kSimplifyTolerance, offset);

    REQUIRE(out.size() == 1);
    const QRectF bbox = out.first().tripLocalPolygon.boundingRect();
    const double eps = 1e-6;
    CHECK(std::abs(bbox.left()   - (-offset))      < eps);
    CHECK(std::abs(bbox.top()    - (-offset))      < eps);
    CHECK(std::abs(bbox.right()  - (1.0 + offset)) < eps);
    CHECK(std::abs(bbox.bottom() - (1.0 + offset)) < eps);
}

TEST_CASE("Mixed stroke list produces one outline per chain",
          "[cwSketchScrapOutlineDetector]") {
    const cwPenStroke wall = makeStroke(cwPenStroke::Wall, {
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}
    });
    const cwPenStroke feature = makeStroke(cwPenStroke::Feature, {
        {5.0, 5.0}, {6.0, 5.0}, {6.0, 6.0}, {5.0, 6.0}, {5.0, 5.0}
    });
    const cwPenStroke outline = makeStroke(cwPenStroke::ScrapOutline, {
        {20.0, 0.0}, {21.0, 0.0}, {21.0, 1.0}, {20.0, 1.0}, {20.0, 0.0}
    });

    const auto out = cwSketchScrapOutlineDetector::detect(
        {wall, feature, outline}, kSimplifyTolerance);

    REQUIRE(out.size() == 2);

    QVector<QVector<QUuid>> seen;
    for (const auto &o : out) {
        seen.append(o.memberStrokeIds);
    }
    std::sort(seen.begin(), seen.end());
    QVector<QVector<QUuid>> expected{QVector<QUuid>{wall.id}, QVector<QUuid>{outline.id}};
    std::sort(expected.begin(), expected.end());
    CHECK(seen == expected);
}
