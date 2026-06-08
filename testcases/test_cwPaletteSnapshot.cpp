/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwPaletteSnapshot.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwLineBrush.h"

// Std
#include <optional>

TEST_CASE("cwPaletteSnapshot resolves brushes by name", "[cwPaletteSnapshot]")
{
    const cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();

    const cwPaletteSnapshot snapshot = palette.snapshot();
    CHECK(snapshot.brushCount() == palette.lineBrushes.size());

    const auto wall = snapshot.findBrush(cwSymbologyPaletteSeed::wallBrushName());
    REQUIRE(wall.has_value());
    CHECK(wall->name == cwSymbologyPaletteSeed::wallBrushName());
    CHECK(wall->scrapOutline);
}

TEST_CASE("cwPaletteSnapshot returns nullopt for a missing brush", "[cwPaletteSnapshot]")
{
    const cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();
    const cwPaletteSnapshot snapshot = palette.snapshot();

    CHECK_FALSE(snapshot.findBrush(QStringLiteral("does-not-exist")).has_value());
}

TEST_CASE("cwPaletteSnapshot brush stays valid after the snapshot is destroyed",
          "[cwPaletteSnapshot]")
{
    // findBrush returns a self-contained value: destroying the snapshot must
    // not invalidate it.
    std::optional<cwLineBrush> brush;
    {
        const cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();
        const cwPaletteSnapshot snapshot = palette.snapshot();
        brush = snapshot.findBrush(cwSymbologyPaletteSeed::featureBrushName());
    }
    REQUIRE(brush.has_value());
    CHECK(brush->name == cwSymbologyPaletteSeed::featureBrushName());
    CHECK(brush->decorations.size() == 1);
}

TEST_CASE("cwPaletteSnapshot is unaffected by later palette mutation",
          "[cwPaletteSnapshot]")
{
    cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();

    const cwPaletteSnapshot snapshot = palette.snapshot();
    const int originalCount = snapshot.brushCount();

    // Mutating the live palette data must leave the snapshot's view intact.
    palette.lineBrushes.clear();

    CHECK(snapshot.brushCount() == originalCount);
    CHECK(snapshot.findBrush(cwSymbologyPaletteSeed::wallBrushName()).has_value());
}

TEST_CASE("cwPaletteSnapshot resolves glyphs by name", "[cwPaletteSnapshot]")
{
    const cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();
    const cwPaletteSnapshot snapshot = palette.snapshot();

    CHECK(snapshot.glyphCount() == palette.glyphs.size());

    const auto tick = snapshot.findGlyph(cwSymbologyPaletteSeed::floorStepTickGlyphName());
    REQUIRE(tick.has_value());
    CHECK(tick->name == cwSymbologyPaletteSeed::floorStepTickGlyphName());
    CHECK_FALSE(tick->strokes.isEmpty());

    CHECK_FALSE(snapshot.findGlyph(QStringLiteral("does-not-exist")).has_value());
}
