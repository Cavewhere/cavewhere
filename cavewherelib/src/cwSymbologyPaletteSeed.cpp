/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyGlyph.h"
#include "cwLineBrush.h"
#include "cwPenStroke.h"

//Qt includes
#include <QColor>
#include <QPointF>

namespace {

// Authored in paper millimetres.
constexpr double kWallWidthMm    = 0.6;
constexpr double kFeatureWidthMm = 0.3;

// floor-step: an edge line plus a tick glyph stamped along the +normal side.
constexpr double kFloorStepEdgeWidthMm = 0.3;
constexpr double kFloorStepTickLengthMm = 1.5;

// Paint order: wall over feature, regardless of draw order. floor-step sits
// between the two.
constexpr int kWallZOrder      = 100;
constexpr int kFloorStepZOrder = 50;
constexpr int kFeatureZOrder   = 10;

const QColor kInkLight = QColor(QStringLiteral("#000000"));
const QColor kInkDark  = QColor(QStringLiteral("#ffffff"));

// A single offset-0 line layer — the visible line on a normal brush. Its rule
// stack is just the polyline trace; there is no separate "centerline" concept.
cwDecorationLayer centerlineLayer(double widthMm)
{
    cwDecorationLayer layer;
    layer.rules = {cwPlacementRuleData{QStringLiteral("Trace offset polyline"), {}}};
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
QString floorStepBrushName()    { return QStringLiteral("floor-step"); }

QString floorStepTickGlyphName() { return QStringLiteral("floor-step-tick"); }

cwSymbologyPaletteData create()
{
    cwSymbologyPaletteData palette;
    palette.id = id();
    palette.name = name();
    palette.description = QStringLiteral("Built-in minimal palette mirroring CaveWhere's prototype symbols.");
    palette.author = QStringLiteral("CaveWhere");
    palette.version = QStringLiteral("1.0");

    // Brushes are appended in name-sorted order to match the canonical order a
    // palette load derives from the directory (load sorts the per-file brushes
    // by name). The QVector order is not semantic — paint order is zOrder and
    // picker grouping is category — so this only keeps a seed equal to its own
    // reload.
    QVector<cwLineBrush> brushes;

    // feature — visible line, no topology. displayName/category are authored
    // strings, same as any other palette on disk — not run through tr().
    cwLineBrush feature;
    feature.name = featureBrushName();
    feature.displayName = QStringLiteral("Feature");
    feature.category = QStringLiteral("Features");
    feature.zOrder = kFeatureZOrder;
    feature.scrapOutline = false;
    feature.decorations.append(centerlineLayer(kFeatureWidthMm));
    brushes.append(feature);

    // floor-step — the shipped glyph-stamping demo: an edge line plus a tick
    // glyph repeated along the +normal side. The tick layer's rule stack repeats
    // the glyph at a uniform spacing, oriented to the local tangent, then stamps
    // it rigidly.
    cwLineBrush floorStep;
    floorStep.name = floorStepBrushName();
    floorStep.displayName = QStringLiteral("Floor Step");
    floorStep.category = QStringLiteral("Features");
    floorStep.zOrder = kFloorStepZOrder;
    floorStep.scrapOutline = false;
    floorStep.decorations.append(centerlineLayer(kFloorStepEdgeWidthMm));
    cwDecorationLayer tickLayer;
    tickLayer.glyphName = floorStepTickGlyphName();
    tickLayer.rules = {cwPlacementRuleData{QStringLiteral("Uniform spacing"), {}},
                       cwPlacementRuleData{QStringLiteral("Align to tangent"), {}},
                       cwPlacementRuleData{QStringLiteral("Rigid stamp"), {}}};
    floorStep.decorations.append(tickLayer);
    brushes.append(floorStep);

    // scrap-outline — no ink, only topology (zero decoration layers).
    cwLineBrush scrapOutline;
    scrapOutline.name = scrapOutlineBrushName();
    scrapOutline.displayName = QStringLiteral("Scrap Outline");
    scrapOutline.category = QStringLiteral("Walls");
    scrapOutline.zOrder = kWallZOrder;
    scrapOutline.scrapOutline = true;
    brushes.append(scrapOutline);

    // wall — visible line + scrap topology.
    cwLineBrush wall;
    wall.name = wallBrushName();
    wall.displayName = QStringLiteral("Wall");
    wall.category = QStringLiteral("Walls");
    wall.zOrder = kWallZOrder;
    wall.scrapOutline = true;
    wall.decorations.append(centerlineLayer(kWallWidthMm));
    brushes.append(wall);

    palette.lineBrushes = brushes;

    // floor-step-tick — one feature-brushed tick from the glyph origin along
    // +normal (glyph-local +Y), authored in paper-mm.
    cwSymbologyGlyph tick;
    tick.name = floorStepTickGlyphName();
    tick.displayName = QStringLiteral("Floor Step Tick");
    cwPenStroke tickStroke;
    tickStroke.brushName = featureBrushName();
    tickStroke.points.append(cwPenPoint(QPointF(0.0, 0.0), 1.0));
    tickStroke.points.append(cwPenPoint(QPointF(0.0, kFloorStepTickLengthMm), 1.0));
    tick.strokes.append(tickStroke);
    palette.glyphs.append(tick);

    return palette;
}

} // namespace cwSymbologyPaletteSeed
