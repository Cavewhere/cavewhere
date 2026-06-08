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
#include "cwSymbologyPaletteIO.h"
#include "cwSymbologyGlyph.h"
#include "cwLineBrush.h"
#include "cwPenStroke.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QPointF>
#include <QTemporaryDir>
#include <QUuid>

namespace {

cwSymbologyGlyph glyphWithBrush(const QString &name, const QString &brushName)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    cwPenStroke stroke;
    stroke.brushName = brushName;
    stroke.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0));
    stroke.points.append(cwPenPoint(QPointF(0.0, 1.0), 1.0));
    glyph.strokes.append(stroke);
    return glyph;
}

cwLineBrush brushStampingGlyph(const QString &name, const QString &glyphName)
{
    cwLineBrush brush;
    brush.name = name;
    cwDecorationLayer layer;
    layer.mode = cwDecorationLayer::RigidStamp;
    layer.glyphName = glyphName;
    brush.decorations.append(layer);
    return brush;
}

// glyph tick -> brush tick-brush -> glyph crossbar -> brush tick-brush (cycle).
cwSymbologyPaletteData cyclicPalette()
{
    cwSymbologyPaletteData palette;
    palette.id = QUuid::createUuid();
    palette.lineBrushes = {brushStampingGlyph(QStringLiteral("tick-brush"),
                                              QStringLiteral("crossbar"))};
    palette.glyphs = {glyphWithBrush(QStringLiteral("tick"), QStringLiteral("tick-brush")),
                      glyphWithBrush(QStringLiteral("crossbar"), QStringLiteral("tick-brush"))};
    return palette;
}

} // namespace

TEST_CASE("An acyclic glyph graph reports no cycle", "[cwSymbologyGlyph]")
{
    const cwSymbologyPaletteData seed = cwSymbologyPaletteSeed::create();
    CHECK(seed.findGlyphDependencyCycle().isEmpty());
}

TEST_CASE("A glyph -> brush -> glyph cycle is detected and named", "[cwSymbologyGlyph]")
{
    const cwSymbologyPaletteData palette = cyclicPalette();

    const QString cycle = palette.findGlyphDependencyCycle();
    REQUIRE_FALSE(cycle.isEmpty());
    CHECK(cycle == QStringLiteral("tick → tick-brush → crossbar → tick-brush"));
}

TEST_CASE("Loading a palette with a glyph cycle fails", "[cwSymbologyGlyph]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = QDir(temp.path())
                            .filePath(QStringLiteral("cyclic-%1")
                                          .arg(QCoreApplication::applicationPid()));

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(cyclicPalette(), dir).hasError());

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    CHECK(loaded.hasError());
    CHECK(loaded.errorMessage().contains(QStringLiteral("cycle")));
}
