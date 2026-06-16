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
#include "cwGlyphSubPath.h"
#include "cwPaletteSnapshot.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyGlyph.h"
#include "cwDecorationLayer.h"
#include "cwPlacementRuleData.h"
#include "cwLineBrush.h"
#include "cwPenStroke.h"

// Qt
#include <QColor>
#include <QPainterPath>
#include <QPointF>
#include <QUuid>
#include <QVector>

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

    const QVector<cwGlyphSubPath> subPaths =
        cache.tessellate(cwSymbologyPaletteSeed::floorStepTickGlyphName(), kScale250);
    REQUIRE(subPaths.size() == 1);
    // feature is a Trace brush with no fill, so the tick stroke resolves to a Stroke.
    CHECK(subPaths.first().kind == cwGlyphSubPath::Stroke);

    // floor-step-tick is a 1.5 mm tick along +Y. At 1:250, 1 mm paper = 0.25 m
    // world, so the tick spans 0.375 m.
    CHECK(subPaths.first().path.boundingRect().height() == Approx(1.5 * 0.25));
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

    const QVector<cwGlyphSubPath> subPaths = cache.tessellate(QStringLiteral("not-a-glyph"), kScale250);
    CHECK(subPaths.isEmpty());
    CHECK(cache.entryCount() == 0);
}

TEST_CASE("A glyph stroke whose brush draws no ink contributes nothing",
          "[cwGlyphTessellationCache]")
{
    // A brush with zero decoration layers (the scrap-outline case) produces no
    // ink, so a glyph built from such strokes tessellates to no sub-paths.
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

    CHECK(cache.tessellate(glyph.name, kScale250).isEmpty());
}

TEST_CASE("A glyph stroke on a filled Trace brush tessellates to a filled polygon",
          "[cwGlyphTessellationCache]")
{
    // A fill brush: one decoration layer whose stack is just the Trace terminal,
    // carrying a pen (line*) and a fill (fill*). The fill is what makes the stroke
    // resolve to a Polygon rather than a bare Stroke.
    const QColor pen(QStringLiteral("#000000"));
    const QColor fillLight(QStringLiteral("#1e90ff"));
    const QColor fillDark(QStringLiteral("#88c0ff"));

    cwDecorationLayer polygonLayer;
    polygonLayer.rules = {cwPlacementRuleData{QStringLiteral("Trace"), {}}};
    polygonLayer.lineColorLight = pen;
    polygonLayer.lineColorDark = pen;
    polygonLayer.lineWidthMm = 0.15;
    polygonLayer.fillColorLight = fillLight;
    polygonLayer.fillColorDark = fillDark;

    cwLineBrush fillBrush;
    fillBrush.name = QStringLiteral("triangle-fill");
    fillBrush.decorations = {polygonLayer};

    // A triangle glyph whose single stroke is drawn by the fill brush.
    cwSymbologyGlyph glyph;
    glyph.name = QStringLiteral("triangle");
    cwPenStroke stroke;
    stroke.brushName = fillBrush.name;
    stroke.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0));
    stroke.points.append(cwPenPoint(QPointF(1.0, 0.0), 1.0));
    stroke.points.append(cwPenPoint(QPointF(0.5, 1.0), 1.0));
    glyph.strokes.append(stroke);

    cwSymbologyPaletteData palette;
    palette.id = QUuid::createUuid();
    palette.lineBrushes = {fillBrush};
    palette.glyphs = {glyph};

    cwGlyphTessellationCache cache;
    cache.setSnapshot(palette.snapshot());

    const QVector<cwGlyphSubPath> subPaths = cache.tessellate(glyph.name, kScale250);
    REQUIRE(subPaths.size() == 1);
    const cwGlyphSubPath &sub = subPaths.first();
    CHECK(sub.kind == cwGlyphSubPath::Polygon);
    CHECK(sub.penColorLight == pen);
    CHECK(sub.penWidthMm == Approx(0.15));
    CHECK(sub.fillColorLight == fillLight);
    CHECK(sub.fillColorDark == fillDark);
    CHECK_FALSE(sub.path.isEmpty());
}
