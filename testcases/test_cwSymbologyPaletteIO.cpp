/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSymbologyPaletteIO.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyGlyph.h"
#include "cwLineBrush.h"
#include "cwPenStroke.h"

// Qt
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointF>
#include <QTemporaryDir>
#include <QUuid>
#include <QVector>

namespace {

QString uniqueDir(const QTemporaryDir &temp, const QString &base)
{
    const QString name = QStringLiteral("%1-%2")
                             .arg(base)
                             .arg(QCoreApplication::applicationPid());
    return QDir(temp.path()).filePath(name);
}

cwLineBrush makeBrush(const QString &name, int zOrder)
{
    cwLineBrush brush;
    brush.name = name;
    brush.displayName = name + QStringLiteral(" display");
    brush.category = QStringLiteral("test");
    brush.zOrder = zOrder;
    return brush;
}

cwSymbologyGlyph makeGlyph(const QString &name, double tickLength)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = name + QStringLiteral(" display");
    cwPenStroke stroke;
    stroke.brushName = QStringLiteral("feature");
    stroke.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0));
    stroke.points.append(cwPenPoint(QPointF(0.0, tickLength), 1.0));
    glyph.strokes.append(stroke);
    return glyph;
}

QByteArray fileBytes(const QString &dir, const QString &subdir,
                     const QString &name, const QString &suffix)
{
    const QString path = QDir(QDir(dir).filePath(subdir)).filePath(name + suffix);
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

QByteArray glyphFileBytes(const QString &dir, const QString &glyphName)
{
    return fileBytes(dir, cwSymbologyPaletteIO::kGlyphsSubdirName, glyphName,
                     cwSymbologyPaletteIO::kGlyphFileSuffix);
}

QByteArray brushFileBytes(const QString &dir, const QString &brushName)
{
    return fileBytes(dir, cwSymbologyPaletteIO::kBrushesSubdirName, brushName,
                     cwSymbologyPaletteIO::kBrushFileSuffix);
}

} // namespace

TEST_CASE("Palette JSON round-trips palette identity only", "[cwSymbologyPaletteIO]")
{
    const cwSymbologyPaletteData source = cwSymbologyPaletteSeed::create();
    REQUIRE_FALSE(source.lineBrushes.isEmpty());
    REQUIRE_FALSE(source.glyphs.isEmpty());

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    REQUIRE_FALSE(loaded.hasError());

    const cwSymbologyPaletteData palette = loaded.value();
    // Identity round-trips through palette.json.
    CHECK(palette.id == source.id);
    CHECK(palette.name == source.name);
    CHECK(palette.description == source.description);
    CHECK(palette.author == source.author);
    CHECK(palette.version == source.version);

    // Brushes and glyphs are per-file now; palette.json carries neither.
    CHECK(palette.lineBrushes.isEmpty());
    CHECK(palette.glyphs.isEmpty());
}

TEST_CASE("Brush color hex round-trips opaque and translucent values", "[cwSymbologyPaletteIO]")
{
    const QColor opaque(QStringLiteral("#abcdef"));      // "#RRGGBB"
    const QColor translucent(0x12, 0x34, 0x56, 0x78);    // "#RRGGBBAA"

    cwDecorationLayer layer;
    layer.mode = cwDecorationLayer::OffsetCurve;
    layer.offsetCurveColorLight = opaque;
    layer.offsetCurveColorDark = translucent;

    cwLineBrush brush = makeBrush(QStringLiteral("colortest"), 0);
    brush.decorations.append(layer);

    const auto loaded = cwSymbologyPaletteIO::brushFromJson(cwSymbologyPaletteIO::brushToJson(brush));
    REQUIRE_FALSE(loaded.hasError());

    const QVector<cwDecorationLayer> layers = loaded.value().decorations;
    REQUIRE(layers.size() == 1);
    CHECK(layers.first().offsetCurveColorLight == opaque);
    CHECK(layers.first().offsetCurveColorDark == translucent);
}

TEST_CASE("Loading a palette with a null id fails", "[cwSymbologyPaletteIO]")
{
    cwSymbologyPaletteData source; // default-constructed id is null

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    CHECK(loaded.hasError());
}

TEST_CASE("Palette saves and loads from a directory", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("seed"));

    const cwSymbologyPaletteData source = cwSymbologyPaletteSeed::create();
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    CHECK(QFileInfo::exists(QDir(dir).filePath(cwSymbologyPaletteIO::kPaletteJsonFileName)));
    // One file per brush and per glyph.
    CHECK_FALSE(brushFileBytes(dir, cwSymbologyPaletteSeed::wallBrushName()).isEmpty());
    CHECK_FALSE(glyphFileBytes(dir, cwSymbologyPaletteSeed::floorStepTickGlyphName()).isEmpty());

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    CHECK(loaded.value() == source);
}

TEST_CASE("Loading from a directory with no palette file fails", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    CHECK(cwSymbologyPaletteIO::load(temp.path()).hasError());
}

TEST_CASE("Scan discovers brushes dropped in without editing palette.json",
          "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("scan"));

    const cwSymbologyPaletteData source = cwSymbologyPaletteSeed::create();
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    // A new brush file lands in brushes/ with no change to palette.json — the
    // merge-friendly case (two cartographers adding symbols on separate branches).
    REQUIRE_FALSE(
        cwSymbologyPaletteIO::saveBrush(makeBrush(QStringLiteral("extra-brush"), 99), dir).hasError());

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    CHECK(loaded.value().brush(QStringLiteral("extra-brush")).has_value());
    CHECK(loaded.value().brush(cwSymbologyPaletteSeed::wallBrushName()).has_value());
}

TEST_CASE("save() reconciles deletes against the in-memory palette", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("reconcile"));

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.lineBrushes = {makeBrush(QStringLiteral("alpha"), 1),
                          makeBrush(QStringLiteral("beta"), 2)};
    source.glyphs = {makeGlyph(QStringLiteral("g-one"), 1.0),
                     makeGlyph(QStringLiteral("g-two"), 2.0)};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    const QByteArray alphaBefore = brushFileBytes(dir, QStringLiteral("alpha"));
    const QByteArray gOneBefore = glyphFileBytes(dir, QStringLiteral("g-one"));
    REQUIRE_FALSE(alphaBefore.isEmpty());
    REQUIRE_FALSE(gOneBefore.isEmpty());

    // Re-save with beta and g-two removed.
    cwSymbologyPaletteData trimmed = source;
    trimmed.lineBrushes = {makeBrush(QStringLiteral("alpha"), 1)};
    trimmed.glyphs = {makeGlyph(QStringLiteral("g-one"), 1.0)};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(trimmed, dir).hasError());

    // The dropped files are gone; the kept ones are byte-for-byte intact.
    CHECK(brushFileBytes(dir, QStringLiteral("beta")).isEmpty());
    CHECK(glyphFileBytes(dir, QStringLiteral("g-two")).isEmpty());
    CHECK(brushFileBytes(dir, QStringLiteral("alpha")) == alphaBefore);
    CHECK(glyphFileBytes(dir, QStringLiteral("g-one")) == gOneBefore);

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    CHECK(loaded.value() == trimmed);
}

TEST_CASE("Brushes load sorted by name regardless of write order", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("order"));

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.lineBrushes = {makeBrush(QStringLiteral("zebra"), 1),
                          makeBrush(QStringLiteral("alpha"), 2)};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    REQUIRE(loaded.value().lineBrushes.size() == 2);
    CHECK(loaded.value().lineBrushes.at(0).name == QStringLiteral("alpha"));
    CHECK(loaded.value().lineBrushes.at(1).name == QStringLiteral("zebra"));
}

TEST_CASE("Editing one brush leaves the other brush files untouched", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("brush-multi"));

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.lineBrushes = {makeBrush(QStringLiteral("alpha"), 1),
                          makeBrush(QStringLiteral("beta"), 2)};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    const QByteArray betaBefore = brushFileBytes(dir, QStringLiteral("beta"));
    REQUIRE_FALSE(betaBefore.isEmpty());

    // Rewrite only alpha.
    const cwLineBrush editedAlpha = makeBrush(QStringLiteral("alpha"), 42);
    REQUIRE_FALSE(cwSymbologyPaletteIO::saveBrush(editedAlpha, dir).hasError());

    CHECK(brushFileBytes(dir, QStringLiteral("beta")) == betaBefore);

    const auto reloadedAlpha = cwSymbologyPaletteIO::loadBrush(dir, QStringLiteral("alpha"));
    REQUIRE_FALSE(reloadedAlpha.hasError());
    CHECK(reloadedAlpha.value() == editedAlpha);
}

TEST_CASE("Editing one glyph leaves the other glyph files untouched", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("glyph-multi"));

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.glyphs = {makeGlyph(QStringLiteral("alpha"), 1.0),
                     makeGlyph(QStringLiteral("beta"), 2.0)};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    const QByteArray betaBefore = glyphFileBytes(dir, QStringLiteral("beta"));
    REQUIRE_FALSE(betaBefore.isEmpty());

    // Rewrite only alpha.
    const cwSymbologyGlyph editedAlpha = makeGlyph(QStringLiteral("alpha"), 5.0);
    REQUIRE_FALSE(cwSymbologyPaletteIO::saveGlyph(editedAlpha, dir).hasError());

    CHECK(glyphFileBytes(dir, QStringLiteral("beta")) == betaBefore);

    const auto reloadedAlpha = cwSymbologyPaletteIO::loadGlyph(dir, QStringLiteral("alpha"));
    REQUIRE_FALSE(reloadedAlpha.hasError());
    CHECK(reloadedAlpha.value() == editedAlpha);
}

TEST_CASE("A brush file declaring a mismatched name fails the load", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("brush-mismatch"));

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.lineBrushes = {makeBrush(QStringLiteral("alpha"), 1)};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    // Overwrite alpha's file with a brush that names itself "beta".
    REQUIRE_FALSE(cwSymbologyPaletteIO::saveBrush(makeBrush(QStringLiteral("beta"), 1), dir).hasError());
    const QString brushesDir = QDir(dir).filePath(cwSymbologyPaletteIO::kBrushesSubdirName);
    const QString suffix = cwSymbologyPaletteIO::kBrushFileSuffix;
    REQUIRE(QFile::remove(QDir(brushesDir).filePath(QStringLiteral("alpha") + suffix)));
    REQUIRE(QFile::rename(QDir(brushesDir).filePath(QStringLiteral("beta") + suffix),
                          QDir(brushesDir).filePath(QStringLiteral("alpha") + suffix)));

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    CHECK(loaded.hasError());
}

TEST_CASE("A glyph file declaring a mismatched name fails the load", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("glyph-mismatch"));

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.glyphs = {makeGlyph(QStringLiteral("alpha"), 1.0)};
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    // Overwrite alpha's file with a glyph that names itself "beta".
    REQUIRE_FALSE(
        cwSymbologyPaletteIO::saveGlyph(makeGlyph(QStringLiteral("beta"), 1.0), dir).hasError());
    const QString glyphsDir = QDir(dir).filePath(cwSymbologyPaletteIO::kGlyphsSubdirName);
    const QString suffix = cwSymbologyPaletteIO::kGlyphFileSuffix;
    REQUIRE(QFile::remove(QDir(glyphsDir).filePath(QStringLiteral("alpha") + suffix)));
    REQUIRE(QFile::rename(QDir(glyphsDir).filePath(QStringLiteral("beta") + suffix),
                          QDir(glyphsDir).filePath(QStringLiteral("alpha") + suffix)));

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    CHECK(loaded.hasError());
}

TEST_CASE("saveBrush rejects a name that escapes brushes/", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("brush-traversal"));

    const auto result = cwSymbologyPaletteIO::saveBrush(makeBrush(QStringLiteral("../pwned"), 1), dir);
    REQUIRE(result.hasError());
    CHECK(result.errorMessage().contains(QStringLiteral("invalid brush name")));

    // The traversal target (one level above the would-be brushes/ dir) must not
    // have been written.
    CHECK_FALSE(QFileInfo::exists(QDir(dir).filePath(QStringLiteral("pwned.cwbrush"))));
}

TEST_CASE("saveGlyph rejects a name that escapes glyphs/", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("glyph-traversal"));

    const auto result =
        cwSymbologyPaletteIO::saveGlyph(makeGlyph(QStringLiteral("../pwned"), 1.0), dir);
    REQUIRE(result.hasError());
    CHECK(result.errorMessage().contains(QStringLiteral("invalid glyph name")));

    // The traversal target (one level above the would-be glyphs/ dir) must not
    // have been written.
    CHECK_FALSE(QFileInfo::exists(QDir(dir).filePath(QStringLiteral("pwned.cwglyph"))));
}
