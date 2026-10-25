//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCollisionRectGrid.h"

//Qt includes
#include <QElapsedTimer>
#include <QList>
#include <QRandomGenerator>
#include <QRect>
#include <QSize>

//Std includes
#include <algorithm>
#include <limits>
#include <utility>

namespace {

// Reference collision detector: linear scan over every accepted rect with the
// same QRect::intersects test the grid uses.
class BruteForceRects
{
public:
    bool addRect(const QRect& rect)
    {
        for (const QRect& placed : std::as_const(m_rects)) {
            if (placed.intersects(rect)) {
                return false;
            }
        }
        if (!rect.isEmpty()) {
            m_rects.append(rect);
        }
        return true;
    }

    int rectCount() const { return m_rects.size(); }

private:
    QList<QRect> m_rects;
};

constexpr int kBoundsWidth = 2000;
constexpr int kBoundsHeight = 1200;
const QRect kBounds(0, 0, kBoundsWidth, kBoundsHeight);

constexpr int kMinRandomWidth = 10;
constexpr int kMaxRandomWidth = 200;
constexpr int kMinRandomHeight = 8;
constexpr int kMaxRandomHeight = 40;
constexpr int kOutsideMargin = 250;
constexpr int kRandomRectCount = 3000;

QRect randomRect(QRandomGenerator& random, const QRect& bounds)
{
    const int width = random.bounded(kMinRandomWidth, kMaxRandomWidth + 1);
    const int height = random.bounded(kMinRandomHeight, kMaxRandomHeight + 1);
    const int x = random.bounded(bounds.left() - kOutsideMargin,
                                 bounds.right() + kOutsideMargin);
    const int y = random.bounded(bounds.top() - kOutsideMargin,
                                 bounds.bottom() + kOutsideMargin);
    return QRect(x, y, width, height);
}

} // namespace

TEST_CASE("cwCollisionRectGrid matches a brute-force checker exactly",
          "[cwCollisionRectGrid]")
{
    const QList<QSize> cellSizes = {
        QSize(5, 5),       // much smaller than every rect
        QSize(16, 8),
        QSize(84, 15),     // roughly a label
        QSize(250, 250),   // larger than every rect
        QSize(5000, 5000), // one cell covers the whole bounds
        QSize(3, 2),       // many cells per rect
    };
    const QList<QRect> boundsList = {
        kBounds,
        QRect(137, -42, kBoundsWidth, kBoundsHeight), // offset origin
    };
    const QList<quint32> seeds = {1u, 42u, 714u};

    for (const QRect& bounds : boundsList) {
        for (const QSize& cellSize : cellSizes) {
            for (quint32 seed : seeds) {
                CAPTURE(bounds, cellSize, seed);

                QRandomGenerator random(seed);
                cwCollisionRectGrid grid;
                grid.reset(bounds, cellSize);
                BruteForceRects bruteForce;

                int acceptedCount = 0;
                int rejectedCount = 0;
                for (int i = 0; i < kRandomRectCount; ++i) {
                    const QRect rect = randomRect(random, bounds);
                    CAPTURE(i, rect);
                    const bool expected = bruteForce.addRect(rect);
                    REQUIRE(grid.addRect(rect) == expected);
                    if (expected) {
                        ++acceptedCount;
                    } else {
                        ++rejectedCount;
                    }
                }

                CHECK(acceptedCount > 0);
                CHECK(rejectedCount > 0);
                CHECK(grid.rectCount() == bruteForce.rectCount());
            }
        }
    }
}

TEST_CASE("cwCollisionRectGrid accepts non-overlapping and edge-adjacent rects",
          "[cwCollisionRectGrid]")
{
    constexpr int kTileSize = 10;
    constexpr int kTileColumns = 30;
    constexpr int kTileRows = 20;

    cwCollisionRectGrid grid;
    grid.reset(kBounds, QSize(25, 25));

    // A tiling of adjacent integer rects: each shares an edge with its
    // neighbors, and QRect::intersects is false for shared edges.
    for (int row = 0; row < kTileRows; ++row) {
        for (int column = 0; column < kTileColumns; ++column) {
            const QRect tile(column * kTileSize, row * kTileSize, kTileSize, kTileSize);
            CAPTURE(tile);
            CHECK(grid.addRect(tile));
        }
    }
    CHECK(grid.rectCount() == kTileColumns * kTileRows);

    // Touching the tiling's right and bottom edges is accepted.
    CHECK(grid.addRect(QRect(kTileColumns * kTileSize, 0, kTileSize, kTileSize)));
    CHECK(grid.addRect(QRect(0, kTileRows * kTileSize, kTileSize, kTileSize)));

    // One pixel of overlap is rejected.
    CHECK_FALSE(grid.addRect(QRect(kTileColumns * kTileSize - 1, 50, kTileSize, kTileSize)));
    CHECK_FALSE(grid.addRect(QRect(55, 55, 1, 1)));

    CHECK(grid.rectCount() == kTileColumns * kTileRows + 2);
}

TEST_CASE("cwCollisionRectGrid detects a rect spanning many cells from any covered cell",
          "[cwCollisionRectGrid]")
{
    const QSize cellSize(20, 20);
    const QRect bigRect(100, 100, 1000, 500); // 50 x 25 cells

    SECTION("Small rects inside or touching a placed big rect are rejected")
    {
        cwCollisionRectGrid grid;
        grid.reset(kBounds, cellSize);
        REQUIRE(grid.addRect(bigRect));

        const QList<QRect> overlapping = {
            QRect(600, 350, 4, 4),                            // deep interior
            QRect(bigRect.left(), bigRect.top(), 1, 1),       // top-left pixel
            QRect(bigRect.right(), bigRect.bottom(), 1, 1),   // bottom-right pixel
            QRect(bigRect.right() - 2, 300, 30, 5),           // straddles right edge
            QRect(500, bigRect.bottom() - 1, 5, 30),          // straddles bottom edge
            QRect(0, 0, kBoundsWidth, kBoundsHeight),         // covers it entirely
        };
        for (const QRect& rect : overlapping) {
            CAPTURE(rect);
            CHECK_FALSE(grid.addRect(rect));
        }

        const QList<QRect> outside = {
            QRect(bigRect.right() + 1, 300, 10, 10),
            QRect(300, bigRect.bottom() + 1, 10, 10),
            QRect(bigRect.left() - 10, bigRect.top() - 10, 10, 10),
        };
        for (const QRect& rect : outside) {
            CAPTURE(rect);
            CHECK(grid.addRect(rect));
        }
        CHECK(grid.rectCount() == 1 + outside.size());
    }

    SECTION("A big rect is rejected by one small rect in any of its cells")
    {
        const QList<QRect> smallRects = {
            QRect(600, 350, 2, 2),
            QRect(bigRect.left(), bigRect.bottom(), 1, 1),
            QRect(bigRect.right(), bigRect.top(), 1, 1),
        };
        for (const QRect& small : smallRects) {
            CAPTURE(small);
            cwCollisionRectGrid grid;
            grid.reset(kBounds, cellSize);
            REQUIRE(grid.addRect(small));
            CHECK_FALSE(grid.addRect(bigRect));
            CHECK(grid.rectCount() == 1);
        }
    }
}

TEST_CASE("cwCollisionRectGrid detects collisions outside its bounds",
          "[cwCollisionRectGrid]")
{
    cwCollisionRectGrid grid;
    grid.reset(kBounds, QSize(100, 100));

    SECTION("Rects with negative coordinates collide with each other")
    {
        REQUIRE(grid.addRect(QRect(-500, -500, 100, 100)));
        CHECK_FALSE(grid.addRect(QRect(-450, -450, 100, 100)));
        // Far apart, but both clamp to the same corner cell.
        CHECK(grid.addRect(QRect(-1000, -1000, 100, 100)));
    }

    SECTION("Rects beyond the width and height collide with each other")
    {
        REQUIRE(grid.addRect(QRect(2500, 1500, 50, 50)));
        CHECK_FALSE(grid.addRect(QRect(2520, 1520, 50, 50)));
        CHECK(grid.addRect(QRect(2600, 1600, 50, 50)));
    }

    SECTION("Rects straddling the bounds collide with edge rects")
    {
        REQUIRE(grid.addRect(QRect(-50, 10, 100, 20)));
        CHECK_FALSE(grid.addRect(QRect(10, 15, 20, 5)));
        CHECK_FALSE(grid.addRect(QRect(-200, 20, 160, 5)));

        REQUIRE(grid.addRect(QRect(kBoundsWidth - 10, 600, 10, 10)));
        CHECK_FALSE(grid.addRect(QRect(kBoundsWidth - 5, 605, 300, 3)));
    }

    SECTION("A rect outside the bounds ignores a non-overlapping edge rect")
    {
        REQUIRE(grid.addRect(QRect(-300, 500, 100, 20)));
        CHECK(grid.addRect(QRect(0, 500, 50, 20)));
        CHECK_FALSE(grid.addRect(QRect(-250, 510, 300, 2)));
    }

    SECTION("A rect larger than the bounds collides with everything inside")
    {
        REQUIRE(grid.addRect(QRect(-100, -100, kBoundsWidth + 200, kBoundsHeight + 200)));
        CHECK_FALSE(grid.addRect(QRect(1000, 600, 1, 1)));
        CHECK_FALSE(grid.addRect(QRect(-50, -50, 1, 1)));
        CHECK(grid.addRect(QRect(kBoundsWidth + 101, 0, 10, 10)));
    }

    SECTION("An empty bounds still detects collisions")
    {
        grid.reset(QRect(), QSize(84, 15));
        REQUIRE(grid.addRect(QRect(10, 10, 80, 14)));
        CHECK_FALSE(grid.addRect(QRect(50, 15, 80, 14)));
        CHECK(grid.addRect(QRect(500, 500, 80, 14)));
        CHECK(grid.rectCount() == 2);
    }
}

TEST_CASE("cwCollisionRectGrid reset and clear start over",
          "[cwCollisionRectGrid]")
{
    const QList<QRect> rects = {
        QRect(10, 10, 80, 14),
        QRect(200, 300, 80, 14),
        QRect(1500, 900, 80, 14),
    };

    cwCollisionRectGrid grid;
    grid.reset(kBounds, QSize(84, 15));
    CHECK(grid.bounds() == kBounds);
    CHECK(grid.cellSize() == QSize(84, 15));
    CHECK(grid.rectCount() == 0);

    for (const QRect& rect : rects) {
        REQUIRE(grid.addRect(rect));
    }
    for (const QRect& rect : rects) {
        CHECK_FALSE(grid.addRect(rect));
    }
    CHECK(grid.rectCount() == rects.size());

    SECTION("clear keeps bounds and cell size and accepts the same rects again")
    {
        grid.clear();
        CHECK(grid.rectCount() == 0);
        CHECK(grid.bounds() == kBounds);
        CHECK(grid.cellSize() == QSize(84, 15));

        for (const QRect& rect : rects) {
            CHECK(grid.addRect(rect));
        }
        for (const QRect& rect : rects) {
            CHECK_FALSE(grid.addRect(rect));
        }
        CHECK(grid.rectCount() == rects.size());
    }

    SECTION("reset with new bounds and cell size empties the grid")
    {
        const QRect newBounds(-100, -50, 800, 600);
        grid.reset(newBounds, QSize(30, 7));
        CHECK(grid.rectCount() == 0);
        CHECK(grid.bounds() == newBounds);
        CHECK(grid.cellSize() == QSize(30, 7));

        for (const QRect& rect : rects) {
            CHECK(grid.addRect(rect));
        }
        CHECK_FALSE(grid.addRect(QRect(20, 12, 5, 5)));
        CHECK(grid.rectCount() == rects.size());
    }

    SECTION("reset to a larger grid and back keeps working")
    {
        grid.reset(QRect(0, 0, 8000, 6000), QSize(10, 10));
        CHECK(grid.rectCount() == 0);
        for (const QRect& rect : rects) {
            CHECK(grid.addRect(rect));
        }
        grid.reset(kBounds, QSize(84, 15));
        CHECK(grid.rectCount() == 0);
        for (const QRect& rect : rects) {
            CHECK(grid.addRect(rect));
        }
        CHECK_FALSE(grid.addRect(rects.first()));
    }

    SECTION("reset clamps the cell size to the minimum")
    {
        grid.reset(QRect(0, 0, 100, 100), QSize(0, -5));
        CHECK(grid.cellSize() == QSize(cwCollisionRectGrid::kMinCellSize,
                                       cwCollisionRectGrid::kMinCellSize));
        CHECK(grid.rectCount() == 0);
        REQUIRE(grid.addRect(QRect(10, 10, 5, 5)));
        CHECK_FALSE(grid.addRect(QRect(12, 12, 5, 5)));
    }

    SECTION("reset grows tiny cells so each axis has at most kMaxCellsPerAxis cells")
    {
        const int maxCells = cwCollisionRectGrid::kMaxCellsPerAxis;
        const QRect largeBounds(0, 0, maxCells * 4, maxCells * 3);
        grid.reset(largeBounds, QSize(1, 1));
        CHECK(grid.cellSize() == QSize(4, 3));
        REQUIRE(grid.addRect(QRect(10, 10, 5, 5)));
        CHECK_FALSE(grid.addRect(QRect(12, 12, 5, 5)));
        CHECK(grid.addRect(QRect(15, 10, 5, 5)));

        grid.reset(largeBounds, QSize(50, 20));
        CHECK(grid.cellSize() == QSize(50, 20));
    }
}

TEST_CASE("cwCollisionRectGrid works before any reset",
          "[cwCollisionRectGrid]")
{
    cwCollisionRectGrid grid;
    CHECK(grid.rectCount() == 0);
    REQUIRE(grid.addRect(QRect(10, 10, 80, 14)));
    CHECK_FALSE(grid.addRect(QRect(50, 15, 80, 14)));
    CHECK(grid.addRect(QRect(-500, 300, 80, 14)));
    CHECK(grid.rectCount() == 2);
}

TEST_CASE("cwCollisionRectGrid accepts empty rects without recording them",
          "[cwCollisionRectGrid]")
{
    cwCollisionRectGrid grid;
    grid.reset(kBounds, QSize(84, 15));

    const QList<QRect> emptyRects = {
        QRect(),                               // null
        QRect(100, 100, 0, 20),                // zero width
        QRect(100, 100, 20, 0),                // zero height
        QRect(QPoint(120, 120), QPoint(100, 100)), // inverted
        QRect(-500, -500, 0, 0),               // outside bounds
    };
    for (const QRect& rect : emptyRects) {
        CAPTURE(rect);
        REQUIRE(rect.isEmpty());
        CHECK(grid.addRect(rect));
    }
    CHECK(grid.rectCount() == 0);

    // A rect covering every empty rect's position is accepted.
    CHECK(grid.addRect(QRect(-600, -600, 800, 800)));
    CHECK(grid.rectCount() == 1);

    // Adding an empty rect inside a placed rect is still accepted.
    CHECK(grid.addRect(QRect(50, 50, 0, 0)));
    CHECK(grid.rectCount() == 1);
}

TEST_CASE("cwCollisionRectGrid declutter benchmark",
          "[.][Benchmark]")
{
    constexpr int kLabelCount = 50000;
    constexpr int kLabelWidth = 80;
    constexpr int kLabelHeight = 14;
    const QSize cellSize(84, 15);
    constexpr int kPasses = 5;
    constexpr qint64 kMaxReleaseMsPerPass = 20;

    QRandomGenerator random(714u);
    QList<QRect> labels;
    labels.reserve(kLabelCount);
    for (int i = 0; i < kLabelCount; ++i) {
        labels.append(QRect(random.bounded(kBoundsWidth - kLabelWidth),
                            random.bounded(kBoundsHeight - kLabelHeight),
                            kLabelWidth,
                            kLabelHeight));
    }

    BruteForceRects bruteForce;
    int expectedAccepted = 0;
    for (const QRect& label : std::as_const(labels)) {
        if (bruteForce.addRect(label)) {
            ++expectedAccepted;
        }
    }

    cwCollisionRectGrid grid;
    qint64 bestNs = std::numeric_limits<qint64>::max();
    int accepted = 0;
    for (int pass = 0; pass < kPasses; ++pass) {
        QElapsedTimer timer;
        timer.start();
        grid.reset(kBounds, cellSize);
        accepted = 0;
        for (const QRect& label : std::as_const(labels)) {
            if (grid.addRect(label)) {
                ++accepted;
            }
        }
        bestNs = std::min(bestNs, timer.nsecsElapsed());
    }

    const double bestMs = double(bestNs) / 1.0e6;
    WARN("cwCollisionRectGrid: " << kLabelCount << " labels, " << accepted
         << " accepted, best of " << kPasses << " passes: " << bestMs << " ms");

    CHECK(accepted == expectedAccepted);
    CHECK(grid.rectCount() == expectedAccepted);
#ifndef QT_DEBUG
    CHECK(bestMs < double(kMaxReleaseMsPerPass));
#endif
}
