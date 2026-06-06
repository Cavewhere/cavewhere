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
#include "cwLineBrush.h"

// Qt
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
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

TEST_CASE("Palette round-trips through JSON", "[cwSymbologyPaletteIO]")
{
    const cwSymbologyPaletteData source = cwSymbologyPaletteSeed::create();

    const auto loaded = cwSymbologyPaletteIO::fromJson(cwSymbologyPaletteIO::toJson(source));
    REQUIRE_FALSE(loaded.hasError());
    CHECK(loaded.value() == source);
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
