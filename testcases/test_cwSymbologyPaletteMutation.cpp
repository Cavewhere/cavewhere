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
#include "cwPenStroke.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"

// Qt
#include <QSignalSpy>
#include <QVector>

namespace {

cwSymbologyGlyph makeGlyph(const QString &name, const QString &label, int strokeCount = 0)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = label;
    glyph.strokes = QVector<cwPenStroke>(strokeCount);
    return glyph;
}

cwLineBrush makeBrush(const QString &name, int zOrder, const QString &label = QString())
{
    cwLineBrush brush;
    brush.name = name;
    brush.zOrder = zOrder;
    brush.displayName = label;
    return brush;
}

// Seed a writable palette's value half directly so mutation tests start from a
// known set without going through the mutators under test. Mutation is
// writable-guarded, so the palette is marked writable here.
void seed(cwSymbologyPalette &palette,
          const QVector<cwSymbologyGlyph> &glyphs,
          const QVector<cwLineBrush> &brushes = {})
{
    cwSymbologyPaletteData data;
    data.glyphs = glyphs;
    data.lineBrushes = brushes;
    palette.setData(data);
    palette.setWritable(true);
}

} // namespace

TEST_CASE("setGlyph appends a new glyph and emits glyphChanged with its name",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    palette.setWritable(true);
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
    palette.setWritable(true);
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
    palette.setWritable(true);
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
    palette.setWritable(true);
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

TEST_CASE("a read-only palette ignores every mutation",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {makeGlyph(QStringLiteral("tick"), QStringLiteral("Tick"))},
                  {makeBrush(QStringLiteral("wall"), 10)});
    palette.setWritable(false);

    QSignalSpy glyphSpy(&palette, &cwSymbologyPalette::glyphChanged);
    QSignalSpy brushSpy(&palette, &cwSymbologyPalette::brushChanged);

    palette.setGlyph(makeGlyph(QStringLiteral("new-glyph"), QStringLiteral("New")));
    palette.removeGlyph(QStringLiteral("tick"));
    palette.setBrush(makeBrush(QStringLiteral("new-brush"), 1));
    palette.removeBrush(QStringLiteral("wall"));
    CHECK(palette.duplicateGlyph(QStringLiteral("tick")).isEmpty());
    CHECK(palette.duplicateBrush(QStringLiteral("wall")).isEmpty());

    // Nothing changed; no signals fired.
    CHECK(glyphSpy.count() == 0);
    CHECK(brushSpy.count() == 0);
    CHECK(palette.glyphs().size() == 1);
    CHECK(palette.brush(QStringLiteral("wall")).has_value());
    CHECK_FALSE(palette.glyph(QStringLiteral("new-glyph")).has_value());
}

TEST_CASE("duplicateGlyph derives a unique name and copies content",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {makeGlyph(QStringLiteral("floor-step"), QStringLiteral("Floor Step"), 4)});

    // First copy gets the "-copy" suffix; strokes and a "copy"-suffixed label
    // carry over, leaving the original untouched.
    const QString first = palette.duplicateGlyph(QStringLiteral("floor-step"));
    CHECK(first == QStringLiteral("floor-step-copy"));
    const auto firstGlyph = palette.glyph(first);
    REQUIRE(firstGlyph.has_value());
    CHECK(firstGlyph->strokes.size() == 4);
    CHECK(firstGlyph->displayName == QStringLiteral("Floor Step copy"));
    CHECK(palette.glyph(QStringLiteral("floor-step")).has_value());

    // A second copy of the same source bumps the numeric suffix rather than
    // colliding with the first.
    const QString second = palette.duplicateGlyph(QStringLiteral("floor-step"));
    CHECK(second == QStringLiteral("floor-step-copy-2"));
}

TEST_CASE("duplicateGlyph rejects bad inputs",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {makeGlyph(QStringLiteral("anchor"), QStringLiteral("Anchor"), 1)});

    // Unknown glyph name -> empty, palette unchanged.
    CHECK(palette.duplicateGlyph(QStringLiteral("missing")).isEmpty());
    CHECK(palette.glyphs().size() == 1);

    // Read-only palette -> empty.
    palette.setWritable(false);
    CHECK(palette.duplicateGlyph(QStringLiteral("anchor")).isEmpty());
    CHECK(palette.glyphs().size() == 1);
}

TEST_CASE("duplicateBrush derives a unique name and copies content",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {}, {makeBrush(QStringLiteral("wall"), 10, QStringLiteral("Wall"))});

    const QString first = palette.duplicateBrush(QStringLiteral("wall"));
    CHECK(first == QStringLiteral("wall-copy"));
    const auto firstBrush = palette.brush(first);
    REQUIRE(firstBrush.has_value());
    CHECK(firstBrush->zOrder == 10);
    CHECK(firstBrush->displayName == QStringLiteral("Wall copy"));
    CHECK(palette.brush(QStringLiteral("wall")).has_value());

    // A second copy bumps the numeric suffix.
    const QString second = palette.duplicateBrush(QStringLiteral("wall"));
    CHECK(second == QStringLiteral("wall-copy-2"));

    // Unknown source -> empty.
    CHECK(palette.duplicateBrush(QStringLiteral("missing")).isEmpty());
}

TEST_CASE("duplicate of a member with no display name leaves the copy unnamed",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {makeGlyph(QStringLiteral("plain"), QString())},
                  {makeBrush(QStringLiteral("bare"), 1)});

    const QString glyphCopy = palette.duplicateGlyph(QStringLiteral("plain"));
    CHECK(glyphCopy == QStringLiteral("plain-copy"));
    REQUIRE(palette.glyph(glyphCopy).has_value());
    CHECK(palette.glyph(glyphCopy)->displayName.isEmpty());

    const QString brushCopy = palette.duplicateBrush(QStringLiteral("bare"));
    CHECK(brushCopy == QStringLiteral("bare-copy"));
    REQUIRE(palette.brush(brushCopy).has_value());
    CHECK(palette.brush(brushCopy)->displayName.isEmpty());
}

TEST_CASE("brushDisplayName returns the label or an empty string",
          "[cwSymbologyPaletteMutation]")
{
    cwSymbologyPalette palette;
    seed(palette, {}, {makeBrush(QStringLiteral("wall"), 10, QStringLiteral("Wall")),
                       makeBrush(QStringLiteral("plain"), 1)});

    CHECK(palette.brushDisplayName(QStringLiteral("wall")) == QStringLiteral("Wall"));
    CHECK(palette.brushDisplayName(QStringLiteral("plain")).isEmpty());
    CHECK(palette.brushDisplayName(QStringLiteral("missing")).isEmpty());
}
