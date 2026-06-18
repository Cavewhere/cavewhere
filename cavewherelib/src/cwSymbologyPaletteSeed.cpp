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

// floor-step: a solid edge line plus a tick glyph stamped along the +normal side.
constexpr double kFloorStepEdgeWidthMm = 0.3;
constexpr double kFloorStepTickLengthMm = 1.5;

// ceiling-step: a dashed line plus short perpendicular ticks on the +normal side.
// The dash is a short glyph repeated along the stroke (the gaps fall between
// dashes), not a lineDash pen — so it renders on the live canvas, which can't
// dash a stroke (see cwSketchPathSource). The tick is 1/4 the floor-step tick.
constexpr double kCeilingStepDashHalfLengthMm = 0.6;
constexpr double kCeilingStepTickLengthMm = kFloorStepTickLengthMm / 4.0;

// Paint order: wall over feature, regardless of draw order. The step features
// sit between the two, in the same band.
constexpr int kWallZOrder        = 100;
constexpr int kFloorStepZOrder   = 50;
constexpr int kCeilingStepZOrder = 50;
constexpr int kFeatureZOrder     = 10;

const QColor kInkLight = QColor(QStringLiteral("#000000"));
const QColor kInkDark  = QColor(QStringLiteral("#ffffff"));

// A single offset-0 line layer — the visible line on a normal brush. Its rule
// stack is just the trace; there is no separate "centerline" concept.
cwDecorationLayer centerlineLayer(double widthMm)
{
    cwDecorationLayer layer;
    layer.rules = {cwPlacementRuleData{QStringLiteral("Trace"), {}}};
    layer.lineColorLight = kInkLight;
    layer.lineColorDark = kInkDark;
    layer.lineWidthMm = widthMm;
    layer.lineDash = Qt::SolidLine;
    layer.lineCap = Qt::RoundCap;
    layer.lineJoin = Qt::RoundJoin;
    return layer;
}

// The tick decoration layer the step brushes share: stamp the glyph rigidly at a
// uniform spacing, oriented to the local tangent.
cwDecorationLayer tickLayer(const QString &glyphName)
{
    cwDecorationLayer layer;
    layer.glyphName = glyphName;
    layer.rules = {cwPlacementRuleData{QStringLiteral("Uniform spacing"), {}},
                   cwPlacementRuleData{QStringLiteral("Align to tangent"), {}},
                   cwPlacementRuleData{QStringLiteral("Rigid stamp"), {}}};
    return layer;
}

// A short feature-brushed glyph — a single 2-point stroke (a tick or dash),
// authored in paper-mm.
cwSymbologyGlyph lineGlyph(const QString &name, const QString &displayName,
                           QPointF start, QPointF end)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = displayName;
    cwPenStroke stroke;
    stroke.brushName = cwSymbologyPaletteSeed::featureBrushName();
    stroke.points.append(cwPenPoint(start, 1.0));
    stroke.points.append(cwPenPoint(end, 1.0));
    glyph.strokes.append(stroke);
    return glyph;
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
QString ceilingStepBrushName()  { return QStringLiteral("ceiling-step"); }

QString floorStepTickGlyphName()   { return QStringLiteral("floor-step-tick"); }
QString ceilingStepTickGlyphName() { return QStringLiteral("ceiling-step-tick"); }
QString ceilingStepDashGlyphName() { return QStringLiteral("ceiling-step-dash"); }

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

    // ceiling-step — a dashed line plus short perpendicular ticks on the +normal
    // side. Both layers stamp glyphs: the dash glyph bends along the stroke to
    // make the dashed line (no solid edge), and the tick glyph is rigidly stamped
    // across the tangent.
    cwLineBrush ceilingStep;
    ceilingStep.name = ceilingStepBrushName();
    ceilingStep.displayName = QStringLiteral("Ceiling Step");
    ceilingStep.category = QStringLiteral("Features");
    ceilingStep.zOrder = kCeilingStepZOrder;
    ceilingStep.scrapOutline = false;
    cwDecorationLayer ceilingDashLayer;
    ceilingDashLayer.glyphName = ceilingStepDashGlyphName();
    ceilingDashLayer.rules = {cwPlacementRuleData{QStringLiteral("Uniform spacing"), {}},
                              cwPlacementRuleData{QStringLiteral("Bending stamp"), {}}};
    ceilingStep.decorations.append(ceilingDashLayer);
    ceilingStep.decorations.append(tickLayer(ceilingStepTickGlyphName()));
    brushes.append(ceilingStep);

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

    // floor-step — the shipped glyph-stamping demo: a solid edge line plus a tick
    // glyph repeated along the +normal side.
    cwLineBrush floorStep;
    floorStep.name = floorStepBrushName();
    floorStep.displayName = QStringLiteral("Floor Step");
    floorStep.category = QStringLiteral("Features");
    floorStep.zOrder = kFloorStepZOrder;
    floorStep.scrapOutline = false;
    floorStep.decorations.append(centerlineLayer(kFloorStepEdgeWidthMm));
    floorStep.decorations.append(tickLayer(floorStepTickGlyphName()));
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

    // Glyphs are appended in name-sorted order to match a directory reload
    // (load scans the glyphs/ dir by name), so the seed equals its own reload.

    // ceiling-step-dash — a short segment along the tangent (glyph-local X).
    // Bending-stamped at the default spacing it makes a dashed line: the dash is
    // shorter than the spacing, so gaps fall between dashes.
    palette.glyphs.append(lineGlyph(ceilingStepDashGlyphName(),
                                    QStringLiteral("Ceiling Step Dash"),
                                    QPointF(-kCeilingStepDashHalfLengthMm, 0.0),
                                    QPointF(kCeilingStepDashHalfLengthMm, 0.0)));

    // ceiling-step-tick — a short perpendicular tick along +normal (glyph-local
    // +Y), 1/4 the floor tick.
    palette.glyphs.append(lineGlyph(ceilingStepTickGlyphName(),
                                    QStringLiteral("Ceiling Step Tick"),
                                    QPointF(0.0, 0.0),
                                    QPointF(0.0, kCeilingStepTickLengthMm)));

    // floor-step-tick — a tick from the glyph origin along +normal (glyph-local +Y).
    palette.glyphs.append(lineGlyph(floorStepTickGlyphName(),
                                    QStringLiteral("Floor Step Tick"),
                                    QPointF(0.0, 0.0),
                                    QPointF(0.0, kFloorStepTickLengthMm)));

    return palette;
}

} // namespace cwSymbologyPaletteSeed
