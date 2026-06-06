/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwLineBrush.h"

// Qt
#include <QVector>

namespace {

int offsetCurveLayerCount(const cwLineBrush &brush)
{
    int count = 0;
    for (const auto &layer : brush.decorations) {
        if (layer.mode == cwDecorationLayer::OffsetCurve) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("Seed palette resolves wall, scrap-outline, and feature by name",
          "[cwSymbologyPalette]")
{
    const cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();

    const auto wall = palette.brush(cwSymbologyPaletteSeed::wallBrushName());
    const auto scrapOutline = palette.brush(cwSymbologyPaletteSeed::scrapOutlineBrushName());
    const auto feature = palette.brush(cwSymbologyPaletteSeed::featureBrushName());

    REQUIRE(wall.has_value());
    REQUIRE(scrapOutline.has_value());
    REQUIRE(feature.has_value());

    // scrapOutline flags: wall=true, scrap-outline=true, feature=false.
    CHECK(wall->scrapOutline);
    CHECK(scrapOutline->scrapOutline);
    CHECK_FALSE(feature->scrapOutline);
}

TEST_CASE("Seed brushes carry the expected decoration layers", "[cwSymbologyPalette]")
{
    const cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();

    const auto wall = palette.brush(cwSymbologyPaletteSeed::wallBrushName());
    const auto scrapOutline = palette.brush(cwSymbologyPaletteSeed::scrapOutlineBrushName());
    const auto feature = palette.brush(cwSymbologyPaletteSeed::featureBrushName());
    REQUIRE(wall.has_value());
    REQUIRE(scrapOutline.has_value());
    REQUIRE(feature.has_value());

    // wall + feature each have exactly one OffsetCurve(offset 0) layer; the
    // scrap-outline brush draws no ink (zero layers).
    CHECK(offsetCurveLayerCount(*wall) == 1);
    CHECK(offsetCurveLayerCount(*feature) == 1);
    CHECK(scrapOutline->decorations.isEmpty());

    REQUIRE(wall->decorations.size() == 1);
    CHECK(wall->decorations.first().offsetCurveOffsetMm == 0.0);

    // Wall paints over feature regardless of draw order.
    CHECK(wall->zOrder > feature->zOrder);
}

TEST_CASE("Palette detects duplicate brush names", "[cwSymbologyPalette]")
{
    cwLineBrush a;
    a.name = QStringLiteral("wall");
    cwLineBrush b;
    b.name = QStringLiteral("feature");
    cwLineBrush duplicate;
    duplicate.name = QStringLiteral("wall");

    CHECK(cwSymbologyPaletteData::findDuplicateBrushName({a, b}).isEmpty());
    CHECK(cwSymbologyPaletteData::findDuplicateBrushName({a, b, duplicate})
              == QStringLiteral("wall"));
}
