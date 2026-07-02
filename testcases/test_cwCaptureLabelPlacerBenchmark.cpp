// Our includes
#include "cwCaptureLabelPlacer.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt includes
#include <QElapsedTimer>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

// Std includes
#include <cmath>
#include <iomanip>
#include <iostream>

// This file reproduces the export hang on very large surveys (e.g. the
// 150-mile Fisher Ridge file, ~tens of thousands of stations). Label
// placement in cwCaptureLabelPlacer::placeLabel is at least O(n^2): every
// candidate cell rescans the growing m_placedLabels list, and every candidate
// is scored against every soft centerline obstacle. The benchmark below drives
// the placer exactly as cwCaptureViewport::placeLabelsAfterTiles does — station
// dots seeded as hard obstacles before finalize(), legs registered as soft
// obstacles after, then one label placed per station in sorted order — so the
// measured cost matches the real export path without needing a camera or RHI.

namespace {

// Geometry, mirrored from placeLabelsAfterTiles' paper-px setup so the grid,
// clearance, and obstacle footprints match what the real export produces.
constexpr qreal StationSpacingPaperPx    = 40.0;
constexpr qreal CellSizePaperPx          = 2.0;
constexpr qreal LabelMarginPaperPx       = 1.0;
constexpr qreal StationDotHalfPaperPx    = 3.0;
constexpr qreal SoftLegThicknessPaperPx  = 1.0;
constexpr qreal LabelWidthPaperPx        = 24.0;
constexpr qreal LabelHeightPaperPx       = 10.0;
constexpr qreal PageMarginPaperPx        = StationSpacingPaperPx;

struct SyntheticCave {
    QRectF          bounds;
    QVector<QPointF> stations;
    QVector<QLineF>  legs;      // one per consecutive station pair
};

// Lay `stationCount` stations on a serpentine (boustrophedon) path filling a
// roughly square page — a stand-in for a large sprawling survey packed onto a
// single export sheet. Consecutive stations are joined by a leg, giving
// stationCount-1 soft obstacles, matching the real 1-leg-per-shot density.
SyntheticCave makeSerpentineCave(int stationCount)
{
    SyntheticCave cave;
    cave.stations.reserve(stationCount);
    cave.legs.reserve(qMax(0, stationCount - 1));

    const int perRow = qMax(1, int(std::ceil(std::sqrt(double(stationCount)))));

    for(int i = 0; i < stationCount; i++) {
        const int row = i / perRow;
        int       col = i % perRow;
        if(row % 2 == 1) {
            col = perRow - 1 - col; // reverse alternate rows so legs stay short
        }

        const QPointF pos((col + 1) * StationSpacingPaperPx,
                          (row + 1) * StationSpacingPaperPx);
        cave.stations.append(pos);
        if(i > 0) {
            cave.legs.append(QLineF(cave.stations.at(i - 1), pos));
        }
    }

    const int rows = (stationCount + perRow - 1) / perRow;
    const qreal width  = (perRow + 1) * StationSpacingPaperPx + PageMarginPaperPx;
    const qreal height = (rows + 1)   * StationSpacingPaperPx + PageMarginPaperPx;
    cave.bounds = QRectF(0.0, 0.0, width, height);
    return cave;
}

// Drives the full placement path once and returns how many labels were placed.
// This is the code under benchmark: it mirrors the obstacle-seeding order and
// per-station placeLabel loop of cwCaptureViewport::placeLabelsAfterTiles.
int placeAllLabels(const SyntheticCave& cave)
{
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(cave.bounds, CellSizePaperPx);
    placer.setViewportBounds(cave.bounds);
    placer.setLabelMarginPaperPx(LabelMarginPaperPx);

    // Station dots are hard obstacles seeded before finalize() (a label may
    // not sit on its own dot), exactly as cwCaptureViewport does.
    for(const QPointF& pos : cave.stations) {
        placer.addObstacleRect(QRectF(pos.x() - StationDotHalfPaperPx,
                                      pos.y() - StationDotHalfPaperPx,
                                      StationDotHalfPaperPx * 2.0,
                                      StationDotHalfPaperPx * 2.0));
    }

    placer.finalize();

    // Centerline legs become soft obstacles after finalize().
    for(const QLineF& leg : cave.legs) {
        placer.addSoftLineObstacle(leg, SoftLegThicknessPaperPx);
    }

    const QSizeF labelSize(LabelWidthPaperPx, LabelHeightPaperPx);
    int placed = 0;
    for(int i = 0; i < cave.stations.size(); i++) {
        const cwCaptureLabelPlacer::LabelRequest request{
            QString::number(i), cave.stations.at(i), labelSize};
        if(placer.placeLabel(request).placed) {
            placed++;
        }
    }
    return placed;
}

} // namespace

// Fast regression guard that DOES run in the normal suite: a modest cave must
// place its labels and finish quickly. Keeps the placement path covered without
// hanging CI on the pathological sizes below.
TEST_CASE("cwCaptureLabelPlacer places labels for a modest survey",
          "[cwCaptureLabelPlacer]")
{
    const SyntheticCave cave = makeSerpentineCave(200);

    QElapsedTimer timer;
    timer.start();
    const int placed = placeAllLabels(cave);
    const qint64 elapsedMs = timer.elapsed();

    CHECK(placed > 0);
    // Generous ceiling — this is a smoke check, not a timing assertion. If a
    // 200-station cave takes seconds, something has regressed badly.
    CHECK(elapsedMs < 5000);
}

// Opt-in scaling benchmark. Hidden ([.]) so it never runs in the default suite
// — at the largest sizes the current O(n^2) placement takes a very long time,
// which is precisely the export hang we are measuring. Run explicitly:
//   ./cavewhere-test "[benchmark]"
// The printed table should show near-quadratic growth today (each doubling of
// n roughly quadruples the time) and should flatten toward linear once the
// placement path is accelerated.
// Tagged only [.][benchmark] — deliberately NOT [cwCaptureLabelPlacer], so
// selecting the placer's normal tag doesn't drag in this multi-second run.
TEST_CASE("cwCaptureLabelPlacer placement scaling",
          "[.][benchmark]")
{
    const QVector<int> stationCounts = {500, 1000, 2000, 4000, 10000};

    std::cout << "\n  cwCaptureLabelPlacer placement scaling\n"
              << "  ----------------------------------------------------\n"
              << "     stations     placed     time(ms)     us/station\n";

    qint64 previousMs = 0;
    int    previousN  = 0;
    for(int n : stationCounts) {
        const SyntheticCave cave = makeSerpentineCave(n);

        QElapsedTimer timer;
        timer.start();
        const int placed = placeAllLabels(cave);
        const qint64 elapsedMs = timer.elapsed();

        const double usPerStation = n > 0 ? (double(elapsedMs) * 1000.0 / n) : 0.0;
        std::cout << "  " << std::setw(11) << n
                  << std::setw(11) << placed
                  << std::setw(13) << elapsedMs
                  << std::setw(15) << qRound(usPerStation) << "\n";

        // Growth ratio vs the previous size, to make super-linear scaling
        // obvious in the log without a spreadsheet.
        if(previousN > 0 && previousMs > 0) {
            const double timeRatio = double(elapsedMs) / double(previousMs);
            const double sizeRatio = double(n) / double(previousN);
            std::cout << "               (time x" << QString::number(timeRatio, 'f', 1).toStdString()
                      << " for size x" << QString::number(sizeRatio, 'f', 1).toStdString()
                      << ")\n";
        }
        previousMs = elapsedMs;
        previousN  = n;

        CHECK(placed > 0);
    }
    std::cout << std::endl;
}
