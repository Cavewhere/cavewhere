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

// A line layer is one whose rule stack traces an offset polyline (what used to
// be "OffsetCurve mode").
int offsetCurveLayerCount(const cwLineBrush &brush)
{
    int count = 0;
    for (const auto &layer : brush.decorations) {
        for (const auto &rule : layer.rules) {
            if (rule.name == QStringLiteral("Trace offset polyline")) {
                ++count;
                break;
            }
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

TEST_CASE("Seed ships the floor-step brush and floor-step-tick glyph",
          "[cwSymbologyPalette]")
{
    const cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();

    const auto floorStep = palette.brush(cwSymbologyPaletteSeed::floorStepBrushName());
    REQUIRE(floorStep.has_value());
    CHECK_FALSE(floorStep->scrapOutline);

    // floor-step has two layers: a traced edge line + a rigid-stamp stack for
    // the tick glyph.
    CHECK(offsetCurveLayerCount(*floorStep) == 1);
    REQUIRE(floorStep->decorations.size() == 2);
    const auto &stamp = floorStep->decorations.at(1);
    REQUIRE_FALSE(stamp.rules.isEmpty());
    CHECK(stamp.rules.last().name == QStringLiteral("Rigid stamp"));
    CHECK(stamp.glyphName == cwSymbologyPaletteSeed::floorStepTickGlyphName());

    // floor-step paints between feature and wall.
    const auto feature = palette.brush(cwSymbologyPaletteSeed::featureBrushName());
    const auto wall = palette.brush(cwSymbologyPaletteSeed::wallBrushName());
    REQUIRE(feature.has_value());
    REQUIRE(wall.has_value());
    CHECK(floorStep->zOrder > feature->zOrder);
    CHECK(floorStep->zOrder < wall->zOrder);

    // The referenced glyph resolves and carries its tick stroke.
    const auto tick = palette.glyph(cwSymbologyPaletteSeed::floorStepTickGlyphName());
    REQUIRE(tick.has_value());
    REQUIRE(tick->strokes.size() == 1);
    CHECK(tick->strokes.first().brushName == cwSymbologyPaletteSeed::featureBrushName());
    CHECK(tick->boundingBox() == tick->strokes.first().boundingBox());
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

TEST_CASE("Palette detects duplicate glyph names", "[cwSymbologyPalette]")
{
    cwSymbologyGlyph a;
    a.name = QStringLiteral("tick");
    cwSymbologyGlyph b;
    b.name = QStringLiteral("crossbar");
    cwSymbologyGlyph duplicate;
    duplicate.name = QStringLiteral("tick");

    CHECK(cwSymbologyPaletteData::findDuplicateGlyphName({a, b}).isEmpty());
    CHECK(cwSymbologyPaletteData::findDuplicateGlyphName({a, b, duplicate})
              == QStringLiteral("tick"));
}
