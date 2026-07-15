// Our includes
#include "cwCaptureLabelPlacer.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt includes
#include <QElapsedTimer>
#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

// Std includes
#include <utility>

TEST_CASE("segmentsCross flags crossing X but not endpoint touches",
          "[cwCaptureLabelPlacer]")
{
    SECTION("crossing X returns true") {
        const QLineF a(0.0, 0.0, 10.0, 10.0);
        const QLineF b(0.0, 10.0, 10.0, 0.0);
        CHECK(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("disjoint segments return false") {
        const QLineF a(0.0, 0.0, 1.0, 1.0);
        const QLineF b(10.0, 10.0, 11.0, 11.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("parallel non-overlapping segments return false") {
        const QLineF a(0.0, 0.0, 10.0, 0.0);
        const QLineF b(0.0, 1.0, 10.0, 1.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("shared endpoint is a touch, not a cross") {
        const QLineF a(0.0, 0.0, 10.0, 0.0);
        const QLineF b(10.0, 0.0, 10.0, 10.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("T-touch where endpoint lies on interior is not a cross") {
        // b's p1 sits in the middle of a; rigid X-crossing this is not.
        const QLineF a(0.0, 0.0, 10.0, 0.0);
        const QLineF b(5.0, 0.0, 5.0, 10.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("collinear non-overlapping returns false") {
        const QLineF a(0.0, 0.0, 5.0, 0.0);
        const QLineF b(10.0, 0.0, 15.0, 0.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }
}

namespace {

cwCaptureLabelPlacer makePlacer(const QRectF& bounds, qreal cellSize)
{
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(bounds, cellSize);
    placer.setViewportBounds(bounds);
    placer.setLabelMarginPaperPx(1.0);
    return placer;
}

} // namespace

TEST_CASE("placeLabel keeps leaders apart when a viable cell exists",
          "[cwCaptureLabelPlacer]")
{
    // A wide obstacle band forces both anchors' labels to spiral out far
    // enough that their leaders are non-trivial; the second placement must
    // not cross the first's already-registered leader.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0, 180, 400, 40));
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);

    cwCaptureLabelPlacer::LabelRequest reqA{
        QStringLiteral("A"), QPointF(100.0, 200.0), labelSize};
    const auto placementA = placer.placeLabel(reqA);
    REQUIRE(placementA.placed);
    REQUIRE(QLineF(placementA.leaderStart, placementA.leaderEnd).length() > 0.0);

    placer.addLineObstacle(
        QLineF(placementA.leaderStart, placementA.leaderEnd),
        1.0);

    cwCaptureLabelPlacer::LabelRequest reqB{
        QStringLiteral("B"), QPointF(300.0, 200.0), labelSize};
    const auto placementB = placer.placeLabel(reqB);
    REQUIRE(placementB.placed);

    const QLineF leaderA(placementA.leaderStart, placementA.leaderEnd);
    const QLineF leaderB(placementB.leaderStart, placementB.leaderEnd);
    CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(leaderA, leaderB));
}

TEST_CASE("placeLabel rejects candidates whose leader would cross an obstacle",
          "[cwCaptureLabelPlacer]")
{
    // Anchor sits inside a horizontal obstacle band, so the spiral picks a
    // cell well above or below the band. A line obstacle at y=60 blocks
    // upward leaders from the anchor at y=200 — but the spiral can still
    // find a cell below the band whose leader stays clear.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0, 180, 400, 40));
    placer.finalize();
    placer.addLineObstacle(QLineF(0.0, 60.0, 400.0, 60.0), 1.0);

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("X"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);

    const QLineF leader(placement.leaderStart, placement.leaderEnd);
    const QLineF lineObstacle(0.0, 60.0, 400.0, 60.0);
    CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(leader, lineObstacle));
    // Resulting placement should be on the unblocked (downward) side.
    CHECK(placement.labelRect.top() > 60.0);
}

TEST_CASE("placeLabel drops a label when every candidate's leader must cross",
          "[cwCaptureLabelPlacer]")
{
    // Sandwich the anchor inside an obstacle band so every viable label
    // sits beyond the band edges. Line obstacles inside the band (between
    // the band edge and the anchor in y) force every candidate leader to
    // cross one of them on its way to the anchor.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0, 80, 400, 240));
    placer.finalize();
    placer.addLineObstacle(QLineF(0.0, 120.0, 400.0, 120.0), 1.0);
    placer.addLineObstacle(QLineF(0.0, 280.0, 400.0, 280.0), 1.0);

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("Y"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    CHECK_FALSE(placement.placed);
}

TEST_CASE("placeLabel prefers candidates whose leader avoids soft obstacles",
          "[cwCaptureLabelPlacer]")
{
    // Hard obstacle at the anchor forces a non-zero leader. A single soft
    // segment above the anchor would only be crossed by upward-going
    // leaders; the scorer should prefer a sideways or downward placement.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(195.0, 195.0, 10.0, 10.0));
    placer.finalize();
    const QLineF softLine(150.0, 190.0, 250.0, 190.0);
    placer.addSoftLineObstacle(softLine, 1.0);

    const QSizeF labelSize(10.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("S"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);

    const QLineF leader(placement.leaderStart, placement.leaderEnd);
    CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(leader, softLine));
}

TEST_CASE("placeLabel biases toward the DT-gradient direction",
          "[cwCaptureLabelPlacer]")
{
    // A wide obstacle band south of the anchor means the DT gradient
    // points north. With no soft obstacles and a symmetric viewport,
    // the scored search should pick a cell north of the anchor.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0.0, 210.0, 400.0, 190.0));
    placer.finalize();

    const QSizeF labelSize(10.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("D"), QPointF(200.0, 210.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);
    CHECK(placement.labelRect.center().y() < 210.0);
}

TEST_CASE("placeLabel culls anchors outside the viewport",
          "[cwCaptureLabelPlacer]")
{
    // The obstacle grid spans the whole page, but the export viewport is a
    // small window in its center. A station whose anchor sits outside that
    // window (plus the label-diagonal margin) must be dropped — it would
    // otherwise get an in-window label pointing off-screen at a dot that isn't
    // drawn. There are no obstacles, so clearance never blocks; placement is
    // governed purely by the viewport clamp, the spiral cap, and the cull.
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(QRectF(0, 0, 400, 400), 2.0);
    placer.setViewportBounds(QRectF(150, 150, 100, 100));
    placer.setLabelMarginPaperPx(1.0);
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);

    // The cull margin is requiredClearanceParent = halfDiag(20x10)+margin ≈
    // 12.18, so cullRect ≈ (137.8, 137.8)-(262.2, 262.2). The two anchors below
    // sit 5 px apart, straddling that boundary at x≈262: both are outside the
    // viewport yet close enough that a clamp-valid label cell (~centered at
    // x=240) is well within the spiral cap. So the ONLY thing that differs
    // between them is which side of cullRect they land on — which isolates the
    // cull from the independent clamp-containment guard.
    SECTION("anchor inside the viewport is placed") {
        cwCaptureLabelPlacer::LabelRequest req{
            QStringLiteral("in"), QPointF(200.0, 200.0), labelSize};
        CHECK(placer.placeLabel(req).placed);
    }

    SECTION("anchor just outside the viewport but within the cull margin still places") {
        cwCaptureLabelPlacer::LabelRequest req{
            QStringLiteral("edge"), QPointF(260.0, 200.0), labelSize};
        CHECK(placer.placeLabel(req).placed);
    }

    SECTION("anchor just beyond the cull margin is culled") {
        // Only the cull rejects this: geometry is all but identical to the
        // section above (a reachable in-viewport cell exists), so a regression
        // that removed the cull would place it and fail this check.
        cwCaptureLabelPlacer::LabelRequest req{
            QStringLiteral("out"), QPointF(265.0, 200.0), labelSize};
        CHECK_FALSE(placer.placeLabel(req).placed);
    }
}

TEST_CASE("placeLabel cull is a no-op when the viewport covers the whole page",
          "[cwCaptureLabelPlacer]")
{
    // The common whole-cave export path sets viewportBounds == bounds. The cull
    // must never fire there: a station near the page edge still gets its label.
    cwCaptureLabelPlacer placer;
    const QRectF page(0, 0, 400, 400);
    placer.setObstacleBounds(page, 2.0);
    placer.setViewportBounds(page);
    placer.setLabelMarginPaperPx(1.0);
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("edge"), QPointF(395.0, 200.0), labelSize};
    CHECK(placer.placeLabel(req).placed);
}

TEST_CASE("placeLabel still places when a clear cell exists within the spiral cap",
          "[cwCaptureLabelPlacer]")
{
    // Guards against the spiral cap (kMaxSearchClearanceMultiple) being set too
    // tight. The anchor is buried in a 120px-tall obstacle band, so its label
    // must escape ~37 cells to the nearest clear cell — comfortably inside the
    // ~50-cell cap for this label, but well beyond what a much smaller cap
    // would reach. Every other cap test asserts placed==false; this is the one
    // that fails if the cap is lowered below what real passages need.
    cwCaptureLabelPlacer placer;
    const QRectF page(0, 0, 400, 400);
    placer.setObstacleBounds(page, 2.0);
    placer.setViewportBounds(page);
    placer.setLabelMarginPaperPx(1.0);
    placer.addObstacleRect(QRectF(0.0, 140.0, 400.0, 120.0));
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("band"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);
    // The winning cell must sit clear of the band (above or below it).
    CHECK((placement.labelRect.bottom() < 140.0 || placement.labelRect.top() > 260.0));
}

namespace {

// A well-spaced grid of anchors with a dot obstacle at each, so every label
// needs a small non-trivial placement. Shared by the placeAll/windowing tests.
struct GridCave {
    QRectF           bounds;
    QVector<QPointF> anchors;
};

GridCave makeGridCave(const QRectF& bounds, const QPointF& origin,
                      int columns, int rows, qreal spacing)
{
    GridCave cave;
    cave.bounds = bounds;
    cave.anchors.reserve(columns * rows);
    for(int row = 0; row < rows; row++) {
        for(int col = 0; col < columns; col++) {
            cave.anchors.append(origin + QPointF(col * spacing, row * spacing));
        }
    }
    return cave;
}

constexpr qreal GridDotHalfPx = 3.0;

cwCaptureLabelPlacer makeGridPlacer(const GridCave& cave, qreal windowSizePaperPx)
{
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(cave.bounds, 2.0);
    placer.setViewportBounds(cave.bounds);
    placer.setLabelMarginPaperPx(1.0);
    placer.setPlacementWindowSize(windowSizePaperPx);
    for(const QPointF& anchor : cave.anchors) {
        placer.addObstacleRect(QRectF(anchor.x() - GridDotHalfPx,
                                      anchor.y() - GridDotHalfPx,
                                      GridDotHalfPx * 2.0, GridDotHalfPx * 2.0));
    }
    placer.finalize();
    return placer;
}

QVector<cwCaptureLabelPlacer::LabelRequest> makeGridRequests(
    const GridCave& cave, const QSizeF& labelSize,
    qreal leaderThickness = 0.0)
{
    QVector<cwCaptureLabelPlacer::LabelRequest> requests;
    requests.reserve(cave.anchors.size());
    for(int i = 0; i < cave.anchors.size(); i++) {
        cwCaptureLabelPlacer::LabelRequest req{
            QString::number(i), cave.anchors.at(i), labelSize};
        req.leaderObstacleThickness = leaderThickness;
        requests.append(req);
    }
    return requests;
}

} // namespace

TEST_CASE("placeAll on a single-window page matches the sequential placeLabel loop",
          "[cwCaptureLabelPlacer]")
{
    // The batch API window-sorts requests, but on a page that fits one DT
    // window the stable sort is the identity — so placeAll must reproduce the
    // sequential loop placement-for-placement. This pins the single-window
    // path (every preview and typical single-sheet export) to the legacy
    // algorithm exactly.
    const GridCave cave = makeGridCave(QRectF(0, 0, 900, 900),
                                       QPointF(60.0, 60.0), 15, 15, 52.0);
    const QSizeF labelSize(24.0, 10.0);
    constexpr qreal leaderThickness = 1.0;

    cwCaptureLabelPlacer batchPlacer = makeGridPlacer(cave, 0.0); // whole page
    const auto requests = makeGridRequests(cave, labelSize, leaderThickness);
    const auto batch = batchPlacer.placeAll(requests);

    cwCaptureLabelPlacer loopPlacer = makeGridPlacer(cave, 0.0);
    int matched = 0;
    for(int i = 0; i < requests.size(); i++) {
        const auto p = loopPlacer.placeLabel(requests.at(i));
        if(p.placed) {
            loopPlacer.addLineObstacle(QLineF(p.leaderStart, p.leaderEnd),
                                       leaderThickness);
        }
        REQUIRE(batch.at(i).placed == p.placed);
        if(p.placed) {
            CHECK(batch.at(i).labelRect == p.labelRect);
            CHECK(batch.at(i).leaderStart == p.leaderStart);
            CHECK(batch.at(i).leaderEnd == p.leaderEnd);
            matched++;
        }
    }
    CHECK(matched > 0);
}

TEST_CASE("windowed placement bounds the DT to window-sized regions",
          "[cwCaptureLabelPlacer]")
{
    // The whole-cave OOM regression guard: a huge, mostly-empty page (here
    // 20000x20000 cells = 400M — a whole-page DT would be 1.6 GB) with two
    // small survey clusters far apart. The placer must only ever build
    // window-sized DT grids around the clusters, never the page.
    const QRectF page(0, 0, 40000, 40000);
    GridCave cave = makeGridCave(page, QPointF(2000.0, 2000.0), 5, 5, 52.0);
    const GridCave farCluster = makeGridCave(page, QPointF(37000.0, 37000.0),
                                             5, 5, 52.0);
    cave.anchors += farCluster.anchors;

    cwCaptureLabelPlacer placer = makeGridPlacer(cave, 4096.0);
    const auto placements = placer.placeAll(makeGridRequests(cave, QSizeF(24.0, 10.0)));

    int placed = 0;
    for(const auto& p : placements) {
        if(p.placed) { placed++; }
    }
    CHECK(placed == cave.anchors.size());

    const auto& stats = placer.stats();
    // The logical page is huge…
    CHECK(stats.gridCells == qint64(20000) * 20000);
    // …but no DT ever exceeded one window plus its halo (2048-cell window
    // core; the halo for these label sizes is a few hundred cells).
    CHECK(stats.maxWindowCells < 8'000'000);
    // Each cluster fits one window (they can straddle a boundary, so allow a
    // handful) — and crucially no rebuild thrash.
    CHECK(stats.windowsBuilt >= 2);
    CHECK(stats.windowsBuilt <= 8);
}

TEST_CASE("windowed placement never overlaps labels across window seams",
          "[cwCaptureLabelPlacer]")
{
    // Force windows far smaller than the page so the anchor grid straddles
    // many seams, then check the global no-overlap invariant and that
    // windowing doesn't drop labels wholesale versus a single-window run.
    const GridCave cave = makeGridCave(QRectF(0, 0, 1200, 1200),
                                       QPointF(60.0, 60.0), 21, 21, 52.0);
    const QSizeF labelSize(24.0, 10.0);

    cwCaptureLabelPlacer windowed = makeGridPlacer(cave, 300.0);
    const auto windowedPlacements = windowed.placeAll(makeGridRequests(cave, labelSize));
    CHECK(windowed.stats().windowsBuilt > 1);

    cwCaptureLabelPlacer single = makeGridPlacer(cave, 0.0);
    const auto singlePlacements = single.placeAll(makeGridRequests(cave, labelSize));

    QVector<QRectF> windowedRects;
    int singlePlaced = 0;
    for(const auto& p : windowedPlacements) {
        if(p.placed) { windowedRects.append(p.labelRect); }
    }
    for(const auto& p : singlePlacements) {
        if(p.placed) { singlePlaced++; }
    }

    // Collision state is global across windows, so two labels can never
    // overlap no matter which windows placed them.
    for(int i = 0; i < windowedRects.size(); i++) {
        for(int j = i + 1; j < windowedRects.size(); j++) {
            INFO("labels " << i << " and " << j << " overlap across a seam");
            CHECK_FALSE(windowedRects.at(i).intersects(windowedRects.at(j)));
        }
    }

    // Window-major ordering may shift which of two competing labels wins a
    // spot, but the drop rate must stay comparable to the unwindowed run.
    CHECK(singlePlaced > 0);
    CHECK(windowedRects.size() >= qRound(0.9 * singlePlaced));

    // A halo that under-covers the spiral reach would let seam-adjacent
    // candidates read unrasterized DT cells and land on ink — which the
    // pairwise label check above can never catch (label collisions go through
    // the global grids, not the DT). Every placed label must clear every dot.
    int labelsOnInk = 0;
    for(const QRectF& rect : std::as_const(windowedRects)) {
        for(const QPointF& anchor : cave.anchors) {
            const QRectF dotRect(anchor.x() - GridDotHalfPx,
                                 anchor.y() - GridDotHalfPx,
                                 GridDotHalfPx * 2.0, GridDotHalfPx * 2.0);
            if(rect.intersects(dotRect)) {
                labelsOnInk++;
            }
        }
    }
    CHECK(labelsOnInk == 0);
}

TEST_CASE("tile-alpha obstacles rasterize like rect obstacles across windows",
          "[cwCaptureLabelPlacer]")
{
    // Rendered cave ink reaches the placer only as tile images, re-rasterized
    // into every DT window they overlap. Feed the same cell-aligned ink dots
    // once as obstacle rects and once as ONE page-sized tile at 2x scale; a
    // many-window run over each must place bit-identically, pinning the
    // per-window pixel-loop clamps and the tileScale mapping at seams.
    const GridCave cave = makeGridCave(QRectF(0, 0, 1200, 1200),
                                       QPointF(60.0, 60.0), 21, 21, 52.0);
    const QSizeF labelSize(24.0, 10.0);
    constexpr qreal windowSize = 300.0;
    constexpr qreal cellSize = 2.0;
    constexpr qreal tileScale = 2.0;
    // Anchors sit on even coordinates, so a +/-4 dot has edges on cell AND
    // tile-pixel boundaries — the two rasterization paths must mark exactly
    // the same cells, making the bit-exact comparison below legitimate.
    constexpr qreal dotHalf = 4.0;

    auto makeBarePlacer = [&cave]() {
        cwCaptureLabelPlacer placer;
        placer.setObstacleBounds(cave.bounds, cellSize);
        placer.setViewportBounds(cave.bounds);
        placer.setLabelMarginPaperPx(1.0);
        placer.setPlacementWindowSize(windowSize);
        return placer;
    };

    cwCaptureLabelPlacer rectPlacer = makeBarePlacer();
    for(const QPointF& anchor : cave.anchors) {
        rectPlacer.addObstacleRect(QRectF(anchor.x() - dotHalf,
                                          anchor.y() - dotHalf,
                                          dotHalf * 2.0, dotHalf * 2.0));
    }
    rectPlacer.finalize();
    const auto rectPlacements = rectPlacer.placeAll(makeGridRequests(cave, labelSize));
    REQUIRE(rectPlacer.stats().windowsBuilt > 1);

    QImage tile(qRound(cave.bounds.width() / tileScale),
                qRound(cave.bounds.height() / tileScale),
                QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);
    {
        QPainter painter(&tile);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        for(const QPointF& anchor : cave.anchors) {
            painter.drawRect(QRectF((anchor.x() - dotHalf) / tileScale,
                                    (anchor.y() - dotHalf) / tileScale,
                                    dotHalf * 2.0 / tileScale,
                                    dotHalf * 2.0 / tileScale));
        }
    }

    cwCaptureLabelPlacer tilePlacer = makeBarePlacer();
    tilePlacer.addTileAlpha(tile, cave.bounds.topLeft(), tileScale);
    tilePlacer.finalize();
    const auto tilePlacements = tilePlacer.placeAll(makeGridRequests(cave, labelSize));

    REQUIRE(tilePlacements.size() == rectPlacements.size());
    int placedCount = 0;
    for(int i = 0; i < tilePlacements.size(); i++) {
        const auto& fromTile = tilePlacements.at(i);
        const auto& fromRect = rectPlacements.at(i);
        REQUIRE(fromTile.placed == fromRect.placed);
        if(fromTile.placed) {
            placedCount++;
            CHECK(fromTile.labelRect == fromRect.labelRect);
            CHECK(fromTile.leaderStart == fromRect.leaderStart);
            CHECK(fromTile.leaderEnd == fromRect.leaderEnd);
            // Non-vacuity: the ink must have registered — a buried anchor
            // can't hold a zero-leader (label centered on the dot) placement.
            CHECK(QLineF(fromTile.leaderStart, fromTile.leaderEnd).length() > 0.0);
        }
    }
    CHECK(placedCount > 0);
}

TEST_CASE("placeAll sizes each window's halo from its own labels",
          "[cwCaptureLabelPlacer]")
{
    // One oversized label (a long lead text) must not inflate the DT halo of
    // every other window — on a real whole-cave export a batch-wide halo made
    // the windows mostly halo (6x the page area of DT built). The big label's
    // own window pays its big halo; the far-away small-label window must not.
    const QRectF page(0, 0, 40000, 40000);
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(page, 2.0);
    placer.setViewportBounds(page);
    placer.setLabelMarginPaperPx(1.0);
    placer.setPlacementWindowSize(2000.0);
    placer.finalize();

    QVector<cwCaptureLabelPlacer::LabelRequest> requests;
    requests.append(cwCaptureLabelPlacer::LabelRequest{
        QStringLiteral("a long lead description"), QPointF(1000.0, 1000.0),
        QSizeF(400.0, 30.0)});
    requests.append(cwCaptureLabelPlacer::LabelRequest{
        QStringLiteral("A1"), QPointF(39000.0, 39000.0), QSizeF(24.0, 10.0)});

    const auto placements = placer.placeAll(requests);
    CHECK(placements.at(0).placed);
    CHECK(placements.at(1).placed);

    const auto& stats = placer.stats();
    REQUIRE(stats.windowsBuilt == 2);
    // The small label's window is core + its own small halo; if the big
    // label's halo leaked into it, both windows would be maxWindowCells-sized.
    const qint64 smallWindowCells = stats.windowCellsBuilt - stats.maxWindowCells;
    CHECK(smallWindowCells < stats.maxWindowCells / 2);
}

TEST_CASE("default placement window is anchored in cells, not local units",
          "[cwCaptureLabelPlacer]")
{
    // Previews express the page in local units where one unit spans many
    // paper px (tiny cell sizes). The default window span is defined in DT
    // cells, so such a page still gets windowed — a local-unit default made a
    // preview's whole 50M-cell page one window (measured, 39 s + 200 MB).
    cwCaptureLabelPlacer placer;
    const QRectF page(0, 0, 800, 800); // local units; 0.1 cells => 8000x8000 cells
    placer.setObstacleBounds(page, 0.1);
    placer.setViewportBounds(page);
    placer.setLabelMarginPaperPx(0.05);
    placer.finalize();

    QVector<cwCaptureLabelPlacer::LabelRequest> requests;
    requests.append(cwCaptureLabelPlacer::LabelRequest{
        QStringLiteral("nearOrigin"), QPointF(50.0, 50.0), QSizeF(2.4, 1.0)});
    requests.append(cwCaptureLabelPlacer::LabelRequest{
        QStringLiteral("farCorner"), QPointF(750.0, 750.0), QSizeF(2.4, 1.0)});
    const auto placements = placer.placeAll(requests);
    CHECK(placements.at(0).placed);
    CHECK(placements.at(1).placed);

    const auto& stats = placer.stats();
    // 8000x8000 = 64M logical cells; each window must stay near the 2048-cell
    // default span (plus a small-label halo), far below the page.
    CHECK(stats.gridCells == qint64(8000) * 8000);
    CHECK(stats.windowsBuilt == 2);
    CHECK(stats.maxWindowCells < qint64(10'000'000));
}

TEST_CASE("placeAll registers lead leaders as obstacles across windows",
          "[cwCaptureLabelPlacer]")
{
    // Leads (requests with a leader-obstacle thickness) place first and their
    // leaders become hard obstacles for every later label — including labels
    // placed in other DT windows. An obstacle band forces every label off its
    // anchor, so all leaders are non-trivial.
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(QRectF(0, 0, 800, 400), 2.0);
    placer.setViewportBounds(QRectF(0, 0, 800, 400));
    placer.setLabelMarginPaperPx(1.0);
    placer.setPlacementWindowSize(300.0);
    placer.addObstacleRect(QRectF(0, 180, 800, 40));
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);
    QVector<cwCaptureLabelPlacer::LabelRequest> requests;
    // Leads spread across the page's window columns…
    for(int i = 0; i < 4; i++) {
        cwCaptureLabelPlacer::LabelRequest lead{
            QStringLiteral("L%1").arg(i), QPointF(130.0 + i * 180.0, 200.0), labelSize};
        lead.leaderObstacleThickness = 1.0;
        requests.append(lead);
    }
    const int leadCount = requests.size();
    // …then stations at nearby anchors that must respect those leaders.
    for(int i = 0; i < 8; i++) {
        requests.append(cwCaptureLabelPlacer::LabelRequest{
            QStringLiteral("S%1").arg(i), QPointF(90.0 + i * 90.0, 200.0), labelSize});
    }

    const auto placements = placer.placeAll(requests);

    QVector<QLineF> leadLeaders;
    for(int i = 0; i < leadCount; i++) {
        if(placements.at(i).placed && placements.at(i).hasLeader) {
            leadLeaders.append(QLineF(placements.at(i).leaderStart,
                                      placements.at(i).leaderEnd));
        }
    }
    REQUIRE(!leadLeaders.isEmpty());

    // No registered leader may cross another registered leader…
    for(int i = 0; i < leadLeaders.size(); i++) {
        for(int j = i + 1; j < leadLeaders.size(); j++) {
            CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(leadLeaders.at(i),
                                                            leadLeaders.at(j)));
        }
    }
    // …and no later label may sit on a registered leader, wherever it placed.
    for(int i = 0; i < requests.size(); i++) {
        if(!placements.at(i).placed) {
            continue;
        }
        for(const QLineF& leader : std::as_const(leadLeaders)) {
            if(placements.at(i).leaderStart == leader.p1()
               && placements.at(i).leaderEnd == leader.p2()) {
                continue; // the leader's own label
            }
            INFO("label " << i << " sits on a lead leader");
            CHECK_FALSE(cwCaptureLabelPlacer::segmentIntersectsRect(
                leader, placements.at(i).labelRect));
        }
    }
}

TEST_CASE("placeAll cancels mid-batch leaving the remainder unplaced",
          "[cwCaptureLabelPlacer]")
{
    const GridCave cave = makeGridCave(QRectF(0, 0, 900, 900),
                                       QPointF(60.0, 60.0), 7, 7, 60.0);
    cwCaptureLabelPlacer placer = makeGridPlacer(cave, 0.0);
    const auto requests = makeGridRequests(cave, QSizeF(24.0, 10.0));

    constexpr int cancelAfter = 5;
    int ticks = 0;
    cwLabelPlacementControl control;
    control.isCanceled = [&ticks]() { return ticks >= cancelAfter; };
    control.labelProcessed = [&ticks]() { ticks++; };

    const auto placements = placer.placeAll(requests, control);

    REQUIRE(placements.size() == requests.size());
    CHECK(ticks == cancelAfter);
    int placed = 0;
    for(const auto& p : placements) {
        if(p.placed) { placed++; }
    }
    // Only the requests processed before the cancel could place (single
    // window, so processing order is request order)…
    CHECK(placed <= cancelAfter);
    CHECK(placed > 0);
    // …and everything after the cancel point is untouched.
    for(int i = cancelAfter; i < placements.size(); i++) {
        CHECK_FALSE(placements.at(i).placed);
    }
}

TEST_CASE("placeLabel gives up quickly for a buried anchor on a huge page",
          "[cwCaptureLabelPlacer]")
{
    // Regression guard for the export hang: an anchor buried in a large
    // obstacle on a very large page used to spiral out to the full page
    // diagonal (m_maskW + m_maskH cells), rescanning obstacles at every ring.
    // The spiral is now capped near the label, so the hopeless placement is
    // dropped in O(label size). placed==false holds with OR without the cap, so
    // the timer is the only signal that distinguishes them: the capped path is
    // sub-millisecond while the regressed full-page spiral here takes seconds to
    // tens of seconds. The 5 s ceiling (matching the modest-survey guard above)
    // stays discriminating while being robust to a loaded CI box.
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(QRectF(0, 0, 20000, 20000), 2.0);
    placer.setViewportBounds(QRectF(0, 0, 20000, 20000));
    placer.setLabelMarginPaperPx(1.0);
    // A large solid obstacle around the anchor: no clear cell exists within
    // any sane radius, so the label cannot be placed.
    placer.addObstacleRect(QRectF(9000, 9000, 2000, 2000));
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("buried"), QPointF(10000.0, 10000.0), labelSize};

    QElapsedTimer timer;
    timer.start();
    const auto placement = placer.placeLabel(req);
    const qint64 elapsedMs = timer.elapsed();

    CHECK_FALSE(placement.placed);
    CHECK(elapsedMs < 5000);

    // Deterministic proof the cap held: the spiral visits ~(2*cap)^2 ≈ 10k
    // cells with the cap, versus ~(2*(maskW+maskH))^2 ≈ 1.6e9 without it. This
    // counter is the non-flaky companion to the wall-clock guard above.
    CHECK(placer.stats().cellsTried < 100000);
}
