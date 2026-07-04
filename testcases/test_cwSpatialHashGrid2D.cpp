// Our includes
#include "cwSpatialHashGrid2D.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt includes
#include <QLineF>
#include <QList>
#include <QRectF>

// Std includes
#include <algorithm>

namespace {

// Collects every id query() reports for a region, sorted for stable compare.
QList<int> queryIds(const cwSpatialHashGrid2D& grid, const QRectF& region)
{
    QList<int> ids;
    grid.query(region, [&](int id) { ids.append(id); });
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

TEST_CASE("cwSpatialHashGrid2D reports rects whose cells overlap the query",
          "[cwSpatialHashGrid2D]")
{
    cwSpatialHashGrid2D grid(10.0);

    grid.insert(0, QRectF(0.0, 0.0, 5.0, 5.0));      // cell (0,0)
    grid.insert(1, QRectF(100.0, 100.0, 5.0, 5.0));  // cell (10,10)
    grid.insert(2, QRectF(200.0, 0.0, 5.0, 5.0));    // cell (20,0)

    SECTION("a region over item 0 reports only item 0") {
        CHECK(queryIds(grid, QRectF(0.0, 0.0, 5.0, 5.0)) == QList<int>{0});
    }

    SECTION("a region far from every item reports nothing") {
        CHECK(queryIds(grid, QRectF(500.0, 500.0, 5.0, 5.0)).isEmpty());
    }

    SECTION("a region spanning two items reports both") {
        CHECK(queryIds(grid, QRectF(0.0, 0.0, 205.0, 5.0)) == (QList<int>{0, 2}));
    }
}

TEST_CASE("cwSpatialHashGrid2D reports each id at most once per query",
          "[cwSpatialHashGrid2D]")
{
    cwSpatialHashGrid2D grid(10.0);

    // A rect spanning many cells, all of which the query region also covers.
    grid.insert(0, QRectF(0.0, 0.0, 100.0, 100.0));

    int visits = 0;
    grid.query(QRectF(0.0, 0.0, 100.0, 100.0), [&](int) { visits++; });
    CHECK(visits == 1);
}

TEST_CASE("cwSpatialHashGrid2D indexes segments along their path, not their bbox",
          "[cwSpatialHashGrid2D]")
{
    cwSpatialHashGrid2D grid(10.0);

    // A steep diagonal from (5,5) to (95,95). Its bounding box covers the whole
    // 10x10 cell block, but the segment only passes through the cells on the
    // main diagonal — the off-diagonal corner (0,90)-(10,100) is not on it.
    grid.insert(0, QLineF(5.0, 5.0, 95.0, 95.0));

    SECTION("a cell the segment passes through reports it") {
        CHECK(queryIds(grid, QRectF(45.0, 45.0, 1.0, 1.0)) == QList<int>{0});
    }

    SECTION("an off-diagonal corner cell does not report it") {
        // Cell (0,9): x in [0,10), y in [90,100). The diagonal is at x==y, so
        // it never enters this cell.
        CHECK(queryIds(grid, QRectF(1.0, 91.0, 1.0, 1.0)).isEmpty());
    }

    SECTION("both endpoints' cells report it") {
        CHECK(queryIds(grid, QRectF(5.0, 5.0, 1.0, 1.0)) == QList<int>{0});
        CHECK(queryIds(grid, QRectF(94.0, 94.0, 1.0, 1.0)) == QList<int>{0});
    }
}

TEST_CASE("cwSpatialHashGrid2D handles axis-aligned and negative-space segments",
          "[cwSpatialHashGrid2D]")
{
    cwSpatialHashGrid2D grid(10.0);

    SECTION("a horizontal segment is found all along its length") {
        grid.insert(0, QLineF(-50.0, 5.0, 50.0, 5.0));
        CHECK(queryIds(grid, QRectF(-45.0, 5.0, 1.0, 1.0)) == QList<int>{0});
        CHECK(queryIds(grid, QRectF(0.0, 5.0, 1.0, 1.0)) == QList<int>{0});
        CHECK(queryIds(grid, QRectF(45.0, 5.0, 1.0, 1.0)) == QList<int>{0});
    }

    SECTION("a vertical segment is found all along its length") {
        grid.insert(0, QLineF(5.0, -50.0, 5.0, 50.0));
        CHECK(queryIds(grid, QRectF(5.0, -45.0, 1.0, 1.0)) == QList<int>{0});
        CHECK(queryIds(grid, QRectF(5.0, 45.0, 1.0, 1.0)) == QList<int>{0});
    }

    SECTION("a zero-length segment occupies exactly its own cell") {
        grid.insert(0, QLineF(5.0, 5.0, 5.0, 5.0));
        CHECK(queryIds(grid, QRectF(5.0, 5.0, 1.0, 1.0)) == QList<int>{0});
        CHECK(queryIds(grid, QRectF(15.0, 5.0, 1.0, 1.0)).isEmpty());
    }
}

TEST_CASE("cwSpatialHashGrid2D anyOf visits each id at most once per call",
          "[cwSpatialHashGrid2D]")
{
    cwSpatialHashGrid2D grid(10.0);

    // One item spanning many cells the query also covers; a rejecting predicate
    // forces anyOf to walk every cell. Its dedup (separate from query()'s) must
    // still present the id only once.
    grid.insert(0, QRectF(0.0, 0.0, 100.0, 100.0));

    int visits = 0;
    const bool found = grid.anyOf(QRectF(0.0, 0.0, 100.0, 100.0), [&](int) {
        visits++;
        return false; // never matches → forces a full walk
    });
    CHECK_FALSE(found);
    CHECK(visits == 1);
}

TEST_CASE("cwSpatialHashGrid2D anyOf short-circuits on the first match",
          "[cwSpatialHashGrid2D]")
{
    cwSpatialHashGrid2D grid(10.0);
    grid.insert(0, QRectF(0.0, 0.0, 5.0, 5.0));
    grid.insert(1, QRectF(2.0, 2.0, 5.0, 5.0));

    SECTION("returns true when a candidate satisfies the predicate") {
        CHECK(grid.anyOf(QRectF(0.0, 0.0, 5.0, 5.0), [](int) { return true; }));
    }

    SECTION("returns false when no cell holds anything") {
        CHECK_FALSE(grid.anyOf(QRectF(500.0, 500.0, 5.0, 5.0),
                               [](int) { return true; }));
    }

    SECTION("returns false when the predicate rejects every candidate") {
        CHECK_FALSE(grid.anyOf(QRectF(0.0, 0.0, 5.0, 5.0),
                               [](int) { return false; }));
    }
}

TEST_CASE("cwSpatialHashGrid2D clear empties the grid but keeps it usable",
          "[cwSpatialHashGrid2D]")
{
    cwSpatialHashGrid2D grid(10.0);
    grid.insert(0, QRectF(0.0, 0.0, 5.0, 5.0));

    grid.clear();
    CHECK(queryIds(grid, QRectF(0.0, 0.0, 5.0, 5.0)).isEmpty());

    // A fresh insert after clear must be queryable, and stale visit stamps from
    // before the clear must not suppress it.
    grid.insert(3, QRectF(0.0, 0.0, 5.0, 5.0));
    CHECK(queryIds(grid, QRectF(0.0, 0.0, 5.0, 5.0)) == QList<int>{3});
}
