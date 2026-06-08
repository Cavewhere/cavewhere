/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Our
#include "cwGlyphTessellationCache.h"
#include "cwPaletteSnapshot.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyGlyph.h"
#include "cwLineBrush.h"
#include "cwPenStroke.h"

// Qt
#include <QPainterPath>
#include <QPointF>
#include <QUuid>

using Catch::Approx;

namespace {

constexpr double kScale250  = 1.0 / 250.0;
constexpr double kScale1000 = 1.0 / 1000.0;

} // namespace

TEST_CASE("Tessellating a seed glyph produces world-metre geometry",
          "[cwGlyphTessellationCache]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());

    const QPainterPath path =
        cache.tessellate(cwSymbologyPaletteSeed::floorStepTickGlyphName(), kScale250);
    REQUIRE_FALSE(path.isEmpty());

    // floor-step-tick is a 1.5 mm tick along +Y. At 1:250, 1 mm paper = 0.25 m
    // world, so the tick spans 0.375 m.
    CHECK(path.boundingRect().height() == Approx(1.5 * 0.25));
}

TEST_CASE("Tessellation caches by (glyphName, mapScale)", "[cwGlyphTessellationCache]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    const QString glyph = cwSymbologyPaletteSeed::floorStepTickGlyphName();

    cache.tessellate(glyph, kScale250);
    cache.tessellate(glyph, kScale250); // same key — hits the cache
    CHECK(cache.entryCount() == 1);

    cache.tessellate(glyph, kScale1000); // new scale — new entry
    CHECK(cache.entryCount() == 2);
}

TEST_CASE("Tessellation cache invalidation is scoped", "[cwGlyphTessellationCache]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    const QString glyph = cwSymbologyPaletteSeed::floorStepTickGlyphName();

    cache.tessellate(glyph, kScale250);
    cache.tessellate(glyph, kScale1000);
    REQUIRE(cache.entryCount() == 2);

    // Changing the map scale drops only entries keyed at the old scale.
    cache.invalidateScale(kScale250);
    CHECK(cache.entryCount() == 1);

    cache.tessellate(glyph, kScale250);
    REQUIRE(cache.entryCount() == 2);

    // Editing the glyph drops every entry for that glyph name.
    cache.invalidateGlyph(glyph);
    CHECK(cache.entryCount() == 0);
}

TEST_CASE("Replacing the snapshot clears the cache", "[cwGlyphTessellationCache]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    cache.tessellate(cwSymbologyPaletteSeed::floorStepTickGlyphName(), kScale250);
    REQUIRE(cache.entryCount() == 1);

    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    CHECK(cache.entryCount() == 0);
}

TEST_CASE("An unknown glyph tessellates to an empty, uncached path",
          "[cwGlyphTessellationCache]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());

    const QPainterPath path = cache.tessellate(QStringLiteral("not-a-glyph"), kScale250);
    CHECK(path.isEmpty());
    CHECK(cache.entryCount() == 0);
}

TEST_CASE("A glyph stroke whose brush draws no ink contributes nothing",
          "[cwGlyphTessellationCache]")
{
    // A brush with zero OffsetCurve layers (the scrap-outline case) produces no
    // ink, so a glyph built from such strokes tessellates to an empty path.
    cwLineBrush inkless;
    inkless.name = QStringLiteral("inkless");

    cwSymbologyGlyph glyph;
    glyph.name = QStringLiteral("ghost");
    cwPenStroke stroke;
    stroke.brushName = inkless.name;
    stroke.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0));
    stroke.points.append(cwPenPoint(QPointF(1.0, 1.0), 1.0));
    glyph.strokes.append(stroke);

    cwSymbologyPaletteData palette;
    palette.id = QUuid::createUuid();
    palette.lineBrushes = {inkless};
    palette.glyphs = {glyph};

    cwGlyphTessellationCache cache;
    cache.setSnapshot(palette.snapshot());

    const QPainterPath path = cache.tessellate(glyph.name, kScale250);
    CHECK(path.isEmpty());
}
