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

} // namespace

TEST_CASE("Palette JSON round-trips brushes and glyph metadata", "[cwSymbologyPaletteIO]")
{
    const cwSymbologyPaletteData source = cwSymbologyPaletteSeed::create();

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    REQUIRE_FALSE(loaded.hasError());

    const cwSymbologyPaletteData palette = loaded.value();
    // Brushes survive the palette file fully.
    CHECK(palette.lineBrushes == source.lineBrushes);

    // Glyph metadata survives, but strokes live in the per-glyph files — they
    // do not travel in the palette JSON.
    REQUIRE(palette.glyphs.size() == source.glyphs.size());
    for (int i = 0; i < palette.glyphs.size(); ++i) {
        CHECK(palette.glyphs.at(i).name == source.glyphs.at(i).name);
        CHECK(palette.glyphs.at(i).displayName == source.glyphs.at(i).displayName);
        CHECK(palette.glyphs.at(i).strokes.isEmpty());
    }
}

TEST_CASE("Color hex round-trips opaque and translucent values", "[cwSymbologyPaletteIO]")
{
    const QColor opaque(QStringLiteral("#abcdef"));      // "#RRGGBB"
    const QColor translucent(0x12, 0x34, 0x56, 0x78);    // "#RRGGBBAA"

    cwDecorationLayer layer;
    layer.mode = cwDecorationLayer::OffsetCurve;
    layer.offsetCurveColorLight = opaque;
    layer.offsetCurveColorDark = translucent;

    cwLineBrush brush;
    brush.name = QStringLiteral("colortest");
    brush.decorations.append(layer);

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.lineBrushes = {brush};
    REQUIRE(source.lineBrushes.value(0).decorations.size() == 1);

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    REQUIRE_FALSE(loaded.hasError());

    const cwSymbologyPaletteData palette = loaded.value();
    REQUIRE(palette.lineBrushes.size() == 1);
    const QVector<cwDecorationLayer> layers = palette.lineBrushes.first().decorations;
    REQUIRE(layers.size() == 1);
    CHECK(layers.first().offsetCurveColorLight == opaque);
    CHECK(layers.first().offsetCurveColorDark == translucent);
}

TEST_CASE("Palette saves and loads from a directory", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("seed"));

    const cwSymbologyPaletteData source = cwSymbologyPaletteSeed::create();
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    CHECK(QFileInfo::exists(QDir(dir).filePath(cwSymbologyPaletteIO::kPaletteJsonFileName)));

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    CHECK(loaded.value() == source);
}

TEST_CASE("Loading a palette with duplicate brush names fails", "[cwSymbologyPaletteIO]")
{
    cwLineBrush a;
    a.name = QStringLiteral("wall");
    cwLineBrush b;
    b.name = QStringLiteral("wall"); // duplicate

    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.lineBrushes = {a, b};

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    CHECK(loaded.hasError());
    CHECK(loaded.errorMessage().contains(QStringLiteral("wall")));
}

TEST_CASE("Loading a palette with a null id fails", "[cwSymbologyPaletteIO]")
{
    cwLineBrush brush;
    brush.name = QStringLiteral("wall");

    cwSymbologyPaletteData source; // default-constructed id is null
    source.lineBrushes = {brush};

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    CHECK(loaded.hasError());
}

TEST_CASE("Loading from a directory with no palette file fails", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    CHECK(cwSymbologyPaletteIO::load(temp.path()).hasError());
}

namespace {

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

QByteArray glyphFileBytes(const QString &dir, const QString &glyphName)
{
    const QString path = QDir(QDir(dir).filePath(cwSymbologyPaletteIO::kGlyphsSubdirName))
                             .filePath(glyphName + cwSymbologyPaletteIO::kGlyphFileSuffix);
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

} // namespace

TEST_CASE("Glyph strokes round-trip through per-glyph files", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("glyphs"));

    const cwSymbologyPaletteData source = cwSymbologyPaletteSeed::create();
    REQUIRE_FALSE(source.glyphs.isEmpty());
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());

    // One file per glyph under glyphs/.
    for (const auto &glyph : source.glyphs) {
        const QString path = QDir(QDir(dir).filePath(cwSymbologyPaletteIO::kGlyphsSubdirName))
                                 .filePath(glyph.name + cwSymbologyPaletteIO::kGlyphFileSuffix);
        CHECK(QFileInfo::exists(path));
    }

    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());
    // Full fidelity, glyph strokes included.
    CHECK(loaded.value() == source);
}

TEST_CASE("Editing one glyph leaves the other glyph files untouched", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("multi"));

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

TEST_CASE("A glyph file declaring a mismatched name fails the load", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("mismatch"));

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

TEST_CASE("Loading a palette with duplicate glyph names fails", "[cwSymbologyPaletteIO]")
{
    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.glyphs = {makeGlyph(QStringLiteral("tick"), 1.0),
                     makeGlyph(QStringLiteral("tick"), 2.0)}; // duplicate name

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    CHECK(loaded.hasError());
    CHECK(loaded.errorMessage().contains(QStringLiteral("tick")));
}

TEST_CASE("Loading a palette whose glyph name escapes glyphs/ fails", "[cwSymbologyPaletteIO]")
{
    // The glyph name becomes a path component (glyphs/<name>.cwglyph), so a
    // palette from an untrusted source must not be able to smuggle a traversal
    // sequence through it.
    cwSymbologyPaletteData source;
    source.id = QUuid::createUuid();
    source.glyphs = {makeGlyph(QStringLiteral("../escape"), 1.0)};

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    CHECK(loaded.hasError());
    CHECK(loaded.errorMessage().contains(QStringLiteral("invalid glyph name")));
}

TEST_CASE("saveGlyph rejects a name that escapes glyphs/", "[cwSymbologyPaletteIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("traversal"));

    const auto result =
        cwSymbologyPaletteIO::saveGlyph(makeGlyph(QStringLiteral("../pwned"), 1.0), dir);
    REQUIRE(result.hasError());

    // The traversal target (one level above the would-be glyphs/ dir) must not
    // have been written.
    CHECK_FALSE(QFileInfo::exists(QDir(dir).filePath(QStringLiteral("pwned.cwglyph"))));
}
