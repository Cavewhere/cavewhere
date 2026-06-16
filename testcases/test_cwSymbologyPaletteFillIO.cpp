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
#include "cwLineBrush.h"
#include "cwDecorationLayer.h"
#include "cwPlacementRuleData.h"

// Qt
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QUuid>
#include <QVector>

namespace {

QString uniqueDir(const QTemporaryDir &temp, const QString &base)
{
    return QDir(temp.path())
        .filePath(QStringLiteral("%1-%2").arg(base).arg(QCoreApplication::applicationPid()));
}

// A one-brush palette whose single decoration layer is a Trace stack carrying the
// given pen and fill (the fill is what makes it a filled region).
cwSymbologyPaletteData filledTracePalette(const QColor &penLight, const QColor &penDark,
                                          double penWidthMm,
                                          const QColor &fillLight, const QColor &fillDark)
{
    cwDecorationLayer layer;
    layer.rules = {cwPlacementRuleData{QStringLiteral("Trace"), {}}};
    layer.lineColorLight = penLight;
    layer.lineColorDark = penDark;
    layer.lineWidthMm = penWidthMm;
    layer.fillColorLight = fillLight;
    layer.fillColorDark = fillDark;

    cwLineBrush brush;
    brush.name = QStringLiteral("stream-fill");
    brush.displayName = QStringLiteral("Stream Fill");
    brush.decorations = {layer};

    cwSymbologyPaletteData palette;
    palette.id = QUuid::createUuid();
    palette.name = QStringLiteral("Fill Test");
    palette.lineBrushes = {brush};
    return palette;
}

} // namespace

TEST_CASE("A filled Trace layer's pen and fill colours survive a round-trip",
          "[cwSymbologyPaletteFillIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("fill-roundtrip"));

    const QColor pen(QStringLiteral("#000000"));
    const QColor fillLight(QStringLiteral("#1e90ff"));
    const QColor fillDark(QStringLiteral("#88c0ff"));
    const cwSymbologyPaletteData source = filledTracePalette(pen, pen, 0.15, fillLight, fillDark);

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());
    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());

    // Whole-palette equality (cwDecorationLayer::operator== includes the fill
    // fields), then explicit fill assertions so a regression names the culprit.
    // Copy the palette into a local: loaded.value() is a temporary, so a
    // reference into its sub-objects would dangle past the full expression.
    const cwSymbologyPaletteData palette = loaded.value().palette;
    CHECK(palette == source);

    REQUIRE(palette.lineBrushes.size() == 1);
    REQUIRE(palette.lineBrushes.first().decorations.size() == 1);
    const cwDecorationLayer &layer = palette.lineBrushes.first().decorations.first();
    CHECK(layer.fillColorLight == fillLight);
    CHECK(layer.fillColorDark == fillDark);
    CHECK(layer.lineColorLight == pen);
}

TEST_CASE("A Trace layer with no fill round-trips as outline-only",
          "[cwSymbologyPaletteFillIO]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString dir = uniqueDir(temp, QStringLiteral("fill-none"));

    const QColor pen(QStringLiteral("#000000"));
    // Default-constructed (invalid) fill colours = no fill.
    const cwSymbologyPaletteData source = filledTracePalette(pen, pen, 0.2, QColor(), QColor());

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(source, dir).hasError());
    const auto loaded = cwSymbologyPaletteIO::load(dir);
    REQUIRE_FALSE(loaded.hasError());

    const cwSymbologyPaletteData palette = loaded.value().palette;
    REQUIRE(palette.lineBrushes.size() == 1);
    const cwDecorationLayer &layer = palette.lineBrushes.first().decorations.first();
    CHECK_FALSE(layer.fillColorLight.isValid());
    CHECK_FALSE(layer.fillColorDark.isValid());
    CHECK(palette == source);
}
