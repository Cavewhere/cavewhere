/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwLineBrush.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"

// Qt
#include <QSignalSpy>
#include <QVector>

namespace {

cwSymbologyGlyph makeGlyph(const QString &name, const QString &label)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = label;
    return glyph;
}

cwLineBrush makeBrush(const QString &name, int zOrder)
{
    cwLineBrush brush;
    brush.name = name;
    brush.zOrder = zOrder;
    return brush;
}

// Seed a palette object's value half directly so mutation tests start from a
// known set without going through the mutators under test.
void seed(cwSymbologyPalette &palette,
          const QVector<cwSymbologyGlyph> &glyphs,
          const QVector<cwLineBrush> &brushes = {})
{
    cwSymbologyPaletteData data;
    data.glyphs = glyphs;
    data.lineBrushes = brushes;
    palette.setData(data);
}

} // namespace

TEST_CASE("setGlyph appends a new glyph and emits glyphChanged with its name",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    QSignalSpy spy(&palette, &cwSymbologyPalette::glyphChanged);

    palette.setGlyph(makeGlyph(QStringLiteral("tick"), QStringLiteral("Tick")));

    const auto stored = palette.glyph(QStringLiteral("tick"));
    REQUIRE(stored.has_value());
    CHECK(stored->displayName == QStringLiteral("Tick"));
    CHECK(palette.glyphs().size() == 1);

    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("tick"));
}

TEST_CASE("setGlyph with unchanged content is a no-op (no signal)",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    const auto glyph = makeGlyph(QStringLiteral("tick"), QStringLiteral("Tick"));
    palette.setGlyph(glyph);

    QSignalSpy spy(&palette, &cwSymbologyPalette::glyphChanged);
    palette.setGlyph(glyph);

    CHECK(spy.count() == 0);
    CHECK(palette.glyphs().size() == 1);
}

TEST_CASE("setGlyph replaces an existing glyph in place, preserving order",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {makeGlyph(QStringLiteral("a"), QStringLiteral("A")),
                   makeGlyph(QStringLiteral("b"), QStringLiteral("B")),
                   makeGlyph(QStringLiteral("c"), QStringLiteral("C"))});

    QSignalSpy spy(&palette, &cwSymbologyPalette::glyphChanged);
    palette.setGlyph(makeGlyph(QStringLiteral("b"), QStringLiteral("B prime")));

    const auto glyphs = palette.glyphs();
    REQUIRE(glyphs.size() == 3);
    CHECK(glyphs.at(0).name == QStringLiteral("a"));
    CHECK(glyphs.at(1).name == QStringLiteral("b"));
    CHECK(glyphs.at(1).displayName == QStringLiteral("B prime"));
    CHECK(glyphs.at(2).name == QStringLiteral("c"));

    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("b"));
}

TEST_CASE("removeGlyph drops the named glyph and emits glyphChanged",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {makeGlyph(QStringLiteral("a"), QStringLiteral("A")),
                   makeGlyph(QStringLiteral("b"), QStringLiteral("B"))});

    QSignalSpy spy(&palette, &cwSymbologyPalette::glyphChanged);
    palette.removeGlyph(QStringLiteral("a"));

    CHECK_FALSE(palette.glyph(QStringLiteral("a")).has_value());
    CHECK(palette.glyphs().size() == 1);

    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("a"));
}

TEST_CASE("removeGlyph of an unknown name is a no-op (no signal)",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {makeGlyph(QStringLiteral("a"), QStringLiteral("A"))});

    QSignalSpy spy(&palette, &cwSymbologyPalette::glyphChanged);
    palette.removeGlyph(QStringLiteral("missing"));

    CHECK(spy.count() == 0);
    CHECK(palette.glyphs().size() == 1);
}

TEST_CASE("setBrush appends, replaces, and is idempotent; emits brushChanged",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    QSignalSpy spy(&palette, &cwSymbologyPalette::brushChanged);

    palette.setBrush(makeBrush(QStringLiteral("wall"), 10));
    REQUIRE(palette.brush(QStringLiteral("wall")).has_value());
    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("wall"));

    // Unchanged content: no emission.
    palette.setBrush(makeBrush(QStringLiteral("wall"), 10));
    CHECK(spy.count() == 0);

    // Changed content: replace in place + one emission.
    palette.setBrush(makeBrush(QStringLiteral("wall"), 20));
    CHECK(palette.brush(QStringLiteral("wall"))->zOrder == 20);
    CHECK(palette.lineBrushes().size() == 1);
    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("wall"));
}

TEST_CASE("removeBrush drops the named brush and emits brushChanged",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {}, {makeBrush(QStringLiteral("wall"), 10),
                       makeBrush(QStringLiteral("feature"), 5),
                       makeBrush(QStringLiteral("ceiling"), 7)});

    QSignalSpy spy(&palette, &cwSymbologyPalette::brushChanged);
    palette.removeBrush(QStringLiteral("feature"));

    CHECK_FALSE(palette.brush(QStringLiteral("feature")).has_value());

    // Removing the middle brush preserves the order of the survivors.
    const auto brushes = palette.lineBrushes();
    REQUIRE(brushes.size() == 2);
    CHECK(brushes.at(0).name == QStringLiteral("wall"));
    CHECK(brushes.at(1).name == QStringLiteral("ceiling"));

    REQUIRE(spy.count() == 1);
    CHECK(spy.takeFirst().at(0).toString() == QStringLiteral("feature"));

    // Unknown removal is silent.
    palette.removeBrush(QStringLiteral("missing"));
    CHECK(spy.count() == 0);
}

TEST_CASE("snapshot reflects glyph and brush mutations",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    palette.setGlyph(makeGlyph(QStringLiteral("tick"), QStringLiteral("Tick")));
    palette.setBrush(makeBrush(QStringLiteral("wall"), 10));

    const cwPaletteSnapshot afterAdd = palette.snapshot();
    CHECK(afterAdd.findGlyph(QStringLiteral("tick")).has_value());
    CHECK(afterAdd.findBrush(QStringLiteral("wall")).has_value());

    palette.removeGlyph(QStringLiteral("tick"));
    const cwPaletteSnapshot afterRemove = palette.snapshot();
    CHECK_FALSE(afterRemove.findGlyph(QStringLiteral("tick")).has_value());
    CHECK(afterRemove.findBrush(QStringLiteral("wall")).has_value());
}
