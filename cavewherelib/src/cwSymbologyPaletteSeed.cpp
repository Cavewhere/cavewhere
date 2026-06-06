/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyPaletteData.h"
#include "cwLineBrush.h"

//Qt includes
#include <QColor>

namespace {

// Authored in paper millimetres.
constexpr double kWallWidthMm    = 0.6;
constexpr double kFeatureWidthMm = 0.3;

// Paint order: wall over feature, regardless of draw order.
constexpr int kWallZOrder    = 100;
constexpr int kFeatureZOrder = 10;

const QColor kInkLight = QColor(QStringLiteral("#000000"));
const QColor kInkDark  = QColor(QStringLiteral("#ffffff"));

// A single offset-0 OffsetCurve layer — the visible line on a normal brush.
// There is no separate "centerline" concept.
cwDecorationLayer centerlineLayer(double widthMm)
{
    cwDecorationLayer layer;
    layer.mode = cwDecorationLayer::OffsetCurve;
    layer.offsetCurveColorLight = kInkLight;
    layer.offsetCurveColorDark = kInkDark;
    layer.offsetCurveWidthMm = widthMm;
    layer.offsetCurveOffsetMm = 0.0;
    layer.offsetCurveDash = Qt::SolidLine;
    layer.offsetCurveCap = Qt::RoundCap;
    layer.offsetCurveJoin = Qt::RoundJoin;
    return layer;
}

} // namespace

namespace cwSymbologyPaletteSeed {

QUuid id()
{
    // Stable across rebuilds; the seed's identity.
    return QUuid(QStringLiteral("5eed0000-0000-4000-8000-000000000001"));
}

QString name()
{
    return QStringLiteral("CaveWhere Seed");
}

QString wallBrushName()         { return QStringLiteral("wall"); }
QString scrapOutlineBrushName() { return QStringLiteral("scrap-outline"); }
QString featureBrushName()      { return QStringLiteral("feature"); }

cwSymbologyPaletteData create()
{
    cwSymbologyPaletteData palette;
    palette.id = id();
    palette.name = name();
    palette.description = QStringLiteral("Built-in minimal palette mirroring CaveWhere's prototype symbols.");
    palette.author = QStringLiteral("CaveWhere");
    palette.version = QStringLiteral("1.0");

    QVector<cwLineBrush> brushes;

    // wall — visible line + scrap topology. displayName/category are authored
    // strings, same as any other palette on disk — not run through tr().
    cwLineBrush wall;
    wall.name = wallBrushName();
    wall.displayName = QStringLiteral("Wall");
    wall.category = QStringLiteral("Walls");
    wall.zOrder = kWallZOrder;
    wall.scrapOutline = true;
    wall.decorations.append(centerlineLayer(kWallWidthMm));
    brushes.append(wall);

    // scrap-outline — no ink, only topology (zero decoration layers).
    cwLineBrush scrapOutline;
    scrapOutline.name = scrapOutlineBrushName();
    scrapOutline.displayName = QStringLiteral("Scrap Outline");
    scrapOutline.category = QStringLiteral("Walls");
    scrapOutline.zOrder = kWallZOrder;
    scrapOutline.scrapOutline = true;
    brushes.append(scrapOutline);

    // feature — visible line, no topology.
    cwLineBrush feature;
    feature.name = featureBrushName();
    feature.displayName = QStringLiteral("Feature");
    feature.category = QStringLiteral("Features");
    feature.zOrder = kFeatureZOrder;
    feature.scrapOutline = false;
    feature.decorations.append(centerlineLayer(kFeatureWidthMm));
    brushes.append(feature);

    palette.lineBrushes = brushes;
    return palette;
}

} // namespace cwSymbologyPaletteSeed
