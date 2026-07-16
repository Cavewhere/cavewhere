//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwOffscreenAtlasGrid.h"

//Qt includes
#include <QSet>

namespace {
    // A 224px square tile (a common CNN classifier input size — the workload this path
    // was built for), and limits in the range the renderer queries on a desktop
    // Metal/Vulkan backend (TextureSizeMax is commonly 8192-16384).
    constexpr int kTile224 = 224;
    constexpr int kDesktopTextureMax = 16384;
    constexpr qint64 kMb = 1024 * 1024;
}

TEST_CASE("cwOffscreenAtlasGrid maps tile indices to row-major sub-rects", "[OffscreenAtlasGrid]")
{
    // A fixed 16x16 grid of 224px tiles (the 256-cell atlas the spike measured).
    cwOffscreenAtlasGrid grid;
    grid.tileSize = QSize(kTile224, kTile224);
    grid.columns = 16;
    grid.rows = 16;

    REQUIRE(grid.isValid());
    REQUIRE(grid.capacity() == 256);
    REQUIRE(grid.atlasSize() == QSize(16 * kTile224, 16 * kTile224));

    SECTION("corners and row wrap") {
        CHECK(grid.tileRect(0) == QRect(0, 0, kTile224, kTile224));
        CHECK(grid.tileRect(1) == QRect(kTile224, 0, kTile224, kTile224));
        CHECK(grid.tileRect(15) == QRect(15 * kTile224, 0, kTile224, kTile224));
        // index 16 wraps to the start of row 1
        CHECK(grid.tileRect(16) == QRect(0, kTile224, kTile224, kTile224));
        CHECK(grid.tileRect(255) == QRect(15 * kTile224, 15 * kTile224, kTile224, kTile224));
    }

    SECTION("out-of-range indices yield a null rect") {
        CHECK(grid.tileRect(-1).isNull());
        CHECK(grid.tileRect(256).isNull());
    }

    SECTION("every cell is in-bounds and the cells tile the atlas without overlap") {
        const QSize atlas = grid.atlasSize();
        QSet<QPair<int, int>> origins;
        for (int i = 0; i < grid.capacity(); ++i) {
            const QRect r = grid.tileRect(i);
            CHECK(r.size() == grid.tileSize);
            CHECK(r.left() >= 0);
            CHECK(r.top() >= 0);
            CHECK(r.right() < atlas.width());
            CHECK(r.bottom() < atlas.height());
            origins.insert({ r.left(), r.top() });
        }
        // distinct top-left per cell -> no two cells share a position
        CHECK(origins.size() == grid.capacity());
    }

    SECTION("atlasSizeForCount shrinks to the rows a partial batch fills") {
        // Full width is kept (tileRect lays cells out by column), only the row count
        // tracks the batch — so a partial batch reads back fewer rows, not fewer columns.
        CHECK(grid.atlasSizeForCount(256) == grid.atlasSize());      // full -> full atlas
        CHECK(grid.atlasSizeForCount(128) == QSize(16 * kTile224, 8 * kTile224));
        CHECK(grid.atlasSizeForCount(16) == QSize(16 * kTile224, 1 * kTile224)); // exactly one row
        CHECK(grid.atlasSizeForCount(17) == QSize(16 * kTile224, 2 * kTile224)); // spills to a 2nd row
        CHECK(grid.atlasSizeForCount(1) == QSize(16 * kTile224, 1 * kTile224));  // >=1 row

        // Every cell of a partial batch stays within its shrunk atlas.
        const QSize partial = grid.atlasSizeForCount(128);
        for (int i = 0; i < 128; ++i) {
            const QRect r = grid.tileRect(i);
            CHECK(r.right() < partial.width());
            CHECK(r.bottom() < partial.height());
        }

        // A count at/over capacity never exceeds the full atlas (clamped).
        CHECK(grid.atlasSizeForCount(512) == grid.atlasSize());
    }

    SECTION("atlasSizeForCount is safe on an invalid grid") {
        // A default/invalid grid (columns or rows == 0) must not divide by zero or
        // call std::clamp with low > high — it returns atlasSize() (an empty size).
        cwOffscreenAtlasGrid invalid;
        invalid.tileSize = QSize(kTile224, kTile224);
        CHECK(invalid.atlasSizeForCount(10) == invalid.atlasSize());

        cwOffscreenAtlasGrid noRows;
        noRows.tileSize = QSize(kTile224, kTile224);
        noRows.columns = 16;
        noRows.rows = 0;
        CHECK(noRows.atlasSizeForCount(10) == noRows.atlasSize());
    }
}

TEST_CASE("cwOffscreenAtlasGrid::choose honours the binding cap", "[OffscreenAtlasGrid]")
{
    const QSize tile(kTile224, kTile224);

    SECTION("per-frame work cap binds (the real classifier case)") {
        // Generous texture (16384/224 = 73 per axis -> 5329) and budget (128 MB buys
        // ~666 cells), so the 256 work cap is the smallest -> a 16x16 atlas.
        const cwOffscreenAtlasGrid grid =
            cwOffscreenAtlasGrid::choose(tile, kDesktopTextureMax, 128 * kMb, 256);
        REQUIRE(grid.isValid());
        CHECK(grid.columns == 16);
        CHECK(grid.rows == 16);
        CHECK(grid.capacity() == 256);
        CHECK(grid.atlasSize() == QSize(16 * kTile224, 16 * kTile224));
    }

    SECTION("texture-dimension cap binds") {
        // 700/224 = 3 tiles per axis -> 9 cells, well under budget and work cap.
        const cwOffscreenAtlasGrid grid =
            cwOffscreenAtlasGrid::choose(tile, 700, 128 * kMb, 1000);
        REQUIRE(grid.isValid());
        CHECK(grid.columns == 3);
        CHECK(grid.rows == 3);
        CHECK(grid.capacity() == 9);
        // atlas never exceeds the texture limit
        CHECK(grid.atlasSize().width() <= 700);
        CHECK(grid.atlasSize().height() <= 700);
    }

    SECTION("byte budget binds") {
        // 224^2 RGBA8 = ~196 KB/atlas-cell; a ~3 MB budget (minus the scratch) holds a
        // handful of cells, far below the texture and work caps.
        const cwOffscreenAtlasGrid grid =
            cwOffscreenAtlasGrid::choose(tile, kDesktopTextureMax, 3 * kMb, 1000);
        REQUIRE(grid.isValid());
        // square-ish and never above the budget's cell count
        CHECK(grid.capacity() >= 1);
        CHECK(grid.capacity() <= 16);
        CHECK(grid.atlasSize().width() <= kDesktopTextureMax);
    }
}

TEST_CASE("cwOffscreenAtlasGrid::choose rejects when nothing fits", "[OffscreenAtlasGrid]")
{
    SECTION("a single tile larger than the texture limit") {
        const cwOffscreenAtlasGrid grid =
            cwOffscreenAtlasGrid::choose(QSize(512, 512), 256, 128 * kMb, 256);
        CHECK_FALSE(grid.isValid());
    }

    SECTION("budget too small for the scratch plus one atlas cell") {
        // One 224^2 atlas cell is ~196 KB and the scratch (colour+depth) ~392 KB, so a
        // 64 KB budget can't hold either.
        const cwOffscreenAtlasGrid grid =
            cwOffscreenAtlasGrid::choose(QSize(kTile224, kTile224), kDesktopTextureMax,
                                         64 * 1024, 256);
        CHECK_FALSE(grid.isValid());
    }

    SECTION("degenerate inputs") {
        CHECK_FALSE(cwOffscreenAtlasGrid::choose(QSize(), kDesktopTextureMax, 128 * kMb, 256).isValid());
        CHECK_FALSE(cwOffscreenAtlasGrid::choose(QSize(kTile224, kTile224), 0, 128 * kMb, 256).isValid());
        CHECK_FALSE(cwOffscreenAtlasGrid::choose(QSize(kTile224, kTile224), kDesktopTextureMax, 0, 256).isValid());
        CHECK_FALSE(cwOffscreenAtlasGrid::choose(QSize(kTile224, kTile224), kDesktopTextureMax, 128 * kMb, 0).isValid());
    }
}
