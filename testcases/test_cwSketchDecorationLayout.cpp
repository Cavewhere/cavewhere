/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Our
#include "cwSketchDecorationLayout.h"
#include "cwGlyphSubPath.h"
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"
#include "cwPlacementRuleParams.h"
#include "cwGlyphTessellationCache.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwLineBrush.h"
#include "cwDecorationLayer.h"
#include "cwSymbologyGlyph.h"
#include "cwPenStroke.h"
#include "cwPenPoint.h"

// Qt
#include <QBrush>
#include <QColor>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QStringList>

// Std
#include <algorithm>
#include <cmath>
#include <iostream>

using Catch::Approx;

namespace {

constexpr double kScale250 = 1.0 / 250.0;

// World metres per paper mm at 1:250 (matches the layout's conversion).
constexpr double kWorldPerPaperMm250 = 1.0 / (1000.0 * kScale250);

QVector<QPointF> straightLine(double length, int segments = 1)
{
    QVector<QPointF> points;
    for (int i = 0; i <= segments; ++i) {
        points.append(QPointF(length * i / segments, 0.0));
    }
    return points;
}

QVector<QPointF> arcCurve(double radius, double sweepRad, int segments)
{
    QVector<QPointF> points;
    for (int i = 0; i <= segments; ++i) {
        const double a = sweepRad * i / segments;
        points.append(QPointF(radius * std::sin(a), radius * (1.0 - std::cos(a))));
    }
    return points;
}

cwDecorationLayer ruleLayer(const QString &glyphName, const QStringList &ruleNames)
{
    cwDecorationLayer layer;
    layer.glyphName = glyphName;
    for (const QString &name : ruleNames) {
        layer.rules.append(cwPlacementRuleData{name, {}});
    }
    return layer;
}

cwDecorationLayer offsetLayer(double widthMm)
{
    cwDecorationLayer layer = ruleLayer(QString(), {QStringLiteral("Trace")});
    layer.lineWidthMm = widthMm;
    return layer;
}

// An "Offset stroke" rule carrying a signed paper-mm lateral offset.
cwPlacementRuleData offsetStrokeRule(double offsetMm)
{
    return cwPlacementRuleData{cwOffsetStrokeRuleName(),
                               QVariant::fromValue(cwOffsetStrokeParams{offsetMm})};
}

// A "Uniform spacing" rule carrying an explicit paper-mm step (overriding the
// 2 mm default).
cwPlacementRuleData uniformSpacingRule(double spacingMm)
{
    return cwPlacementRuleData{cwUniformSpacingRuleName(),
                               QVariant::fromValue(cwUniformSpacingParams{spacingMm})};
}

// A traced rail parallel to the stroke at a signed offset (commit 4.5): the
// Offset stroke transform rebuilds the stroke, the polyline trace follows it.
cwDecorationLayer railLayer(double offsetMm, double widthMm)
{
    cwDecorationLayer layer;
    layer.rules = {offsetStrokeRule(offsetMm),
                   cwPlacementRuleData{QStringLiteral("Trace"), {}}};
    layer.lineWidthMm = widthMm;
    return layer;
}

// A glyph-stamp layer whose stroke is first pushed laterally by offsetMm (commit
// 4.6 ceiling channel): the offset rule prepended to a normal stamp stack.
cwDecorationLayer offsetRuleLayer(double offsetMm,
                                  const QString &glyphName,
                                  const QStringList &ruleNames)
{
    cwDecorationLayer layer = ruleLayer(glyphName, ruleNames);
    layer.rules.prepend(offsetStrokeRule(offsetMm));
    return layer;
}

// Repeated glyph stamped along the stroke, oriented to the tangent.
cwDecorationLayer rigidStampLayer(const QString &glyphName)
{
    return ruleLayer(glyphName, {QStringLiteral("Uniform spacing"),
                                 QStringLiteral("Align to tangent"),
                                 QStringLiteral("Rigid stamp")});
}

// Repeated glyph subdivided to curve with the stroke arclength.
cwDecorationLayer bendingStampLayer(const QString &glyphName)
{
    return ruleLayer(glyphName, {QStringLiteral("Uniform spacing"),
                                 QStringLiteral("Bending stamp")});
}

// Repeated glyph warped at its vertices only — straight chords between them.
cwDecorationLayer jointedStampLayer(const QString &glyphName)
{
    return ruleLayer(glyphName, {QStringLiteral("Uniform spacing"),
                                 QStringLiteral("Jointed stamp")});
}

// A single glyph at the stroke start (no tangent rotation).
cwDecorationLayer pointStampLayer(const QString &glyphName)
{
    return ruleLayer(glyphName, {QStringLiteral("Explicit point"),
                                 QStringLiteral("Rigid stamp")});
}

cwLineBrush makeBrush(const QString &name, const QVector<cwDecorationLayer> &layers)
{
    cwLineBrush brush;
    brush.name = name;
    brush.displayName = name;
    brush.decorations = layers;
    return brush;
}

// Railroad demo dimensions (paper-mm), one source of truth for the functional
// test and the reference-image panel.
constexpr double kRailroadHalfGaugeMm = 0.5;   // rails at +-this
constexpr double kRailroadRailWidthMm = 0.2;   // thin rails
constexpr double kRailroadTieWidthMm  = 0.8;   // thick cross-ties
constexpr double kRailroadTieSpacingMm = 0.4;  // spacing > tie width so ties stay distinct

// Railroad (commit 4.5): two thin traced rails offset +-d via the Offset stroke
// transform, plus a closely-spaced layer of thick cross-ties.
cwLineBrush makeRailroadBrush()
{
    cwDecorationLayer ties;
    ties.glyphName = QStringLiteral("rail-tie");
    ties.lineWidthMm = kRailroadTieWidthMm;   // drives the tie stamp pen
    ties.rules = {uniformSpacingRule(kRailroadTieSpacingMm),
                  cwPlacementRuleData{QStringLiteral("Align to tangent"), {}},
                  cwPlacementRuleData{QStringLiteral("Rigid stamp"), {}}};

    return makeBrush(QStringLiteral("railroad"),
                     {railLayer(kRailroadHalfGaugeMm, kRailroadRailWidthMm),
                      railLayer(-kRailroadHalfGaugeMm, kRailroadRailWidthMm),
                      ties});
}

cwPenStroke featureStroke(const QVector<QPointF> &points)
{
    cwPenStroke stroke;
    stroke.brushName = cwSymbologyPaletteSeed::featureBrushName();
    for (const QPointF &p : points) {
        stroke.points.append(cwPenPoint(p, 1.0));
    }
    return stroke;
}

// A glyph that is a single feature-inked polyline through `points` (glyph-local
// paper-mm). feature carries a traced-line layer, so the stroke contributes ink.
cwSymbologyGlyph lineGlyph(const QString &name, const QVector<QPointF> &points)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = name;
    glyph.strokes.append(featureStroke(points));
    return glyph;
}

// A glyph that is a single stroke drawn by `brushName` (glyph-local paper-mm).
cwSymbologyGlyph brushedGlyph(const QString &name, const QString &brushName,
                              const QVector<QPointF> &points)
{
    cwSymbologyGlyph glyph;
    glyph.name = name;
    glyph.displayName = name;
    cwPenStroke stroke;
    stroke.brushName = brushName;
    for (const QPointF &p : points) {
        stroke.points.append(cwPenPoint(p, 1.0));
    }
    glyph.strokes.append(stroke);
    return glyph;
}

constexpr double kStreamPenWidthMm = 0.15;
const QColor kStreamFillLight = QColor(QStringLiteral("#1e90ff"));
const QColor kStreamFillDark  = QColor(QStringLiteral("#1e90ff"));

// stream-fill: a filled Trace brush — blue body with an optional edge (a Trace
// terminal carrying a fill colour). penWidthMm == 0 leaves the pen colours unset,
// the pure-fill degenerate case (fill only, no outline). The fill-bearing analog
// of the plain (unfilled) centerline Trace brushes.
cwLineBrush makeStreamFillBrush(const QString &name, double penWidthMm)
{
    cwDecorationLayer fill;
    fill.rules = {cwPlacementRuleData{QStringLiteral("Trace"), {}}};
    if (penWidthMm > 0.0) {
        fill.lineColorLight = QColor(QStringLiteral("#000000"));
        fill.lineColorDark = QColor(QStringLiteral("#ffffff"));
    }
    fill.lineWidthMm = penWidthMm;
    fill.fillColorLight = kStreamFillLight;
    fill.fillColorDark = kStreamFillDark;
    return makeBrush(name, {fill});
}

// The closed-triangle arrowhead (paper-mm), apex pointing back along -X — the "<"
// of a "<-----" stream symbol.
const QVector<QPointF> kStreamTriangle = {QPointF(0.14, -0.14), QPointF(-0.18, 0.0),
                                          QPointF(0.14, 0.14), QPointF(0.14, -0.14)};


// Seed palette plus extra glyphs for the bending / single-stamp demos (the seed
// only exercises traced lines + rigid stamps).
cwSymbologyPaletteData demoPalette()
{
    cwSymbologyPaletteData palette = cwSymbologyPaletteSeed::create();
    palette.glyphs.append(lineGlyph(QStringLiteral("dash"),
                                    {QPointF(-0.6, 0.0), QPointF(0.6, 0.0)}));
    // A chevron opening toward the +normal side — bends visibly along a curve.
    palette.glyphs.append(lineGlyph(QStringLiteral("chevron"),
                                    {QPointF(-0.7, 0.8), QPointF(0.0, 0.0), QPointF(0.7, 0.8)}));
    palette.glyphs.append(lineGlyph(QStringLiteral("plus"),
                                    {QPointF(-0.8, 0.0), QPointF(0.8, 0.0)}));
    palette.glyphs.append(lineGlyph(QStringLiteral("plus-v"),
                                    {QPointF(0.0, -0.8), QPointF(0.0, 0.8)}));
    // A short tick along +normal, 1/4 of the 1.5 mm floor-step tick.
    palette.glyphs.append(lineGlyph(QStringLiteral("ceiling-tick"),
                                    {QPointF(0.0, 0.0), QPointF(0.0, 1.5 / 4.0)}));
    // The same tick mirrored to -normal — the inward tick for the +d side of a
    // ceiling channel (proper side-mirroring is a future Mirror-on-side rule;
    // until then the channel authors both tick glyphs).
    palette.glyphs.append(lineGlyph(QStringLiteral("ceiling-tick-down"),
                                    {QPointF(0.0, 0.0), QPointF(0.0, -1.5 / 4.0)}));
    // A cross-tie spanning the rail gauge (±0.6 mm > the ±0.5 mm rails), authored
    // along +Y so Align to tangent lays it across the stroke normal.
    palette.glyphs.append(lineGlyph(QStringLiteral("rail-tie"),
                                    {QPointF(0.0, -0.6), QPointF(0.0, 0.6)}));
    // A filled triangle arrowhead (commit 4.2): the stream-fill polygon brush
    // plus a stream-indicator glyph whose triangle stroke resolves to it, so a
    // single-stamp stack drops a pen+fill arrowhead at the stroke start. A second
    // pen-less variant shows the pure-fill (no outline) degenerate case.
    palette.lineBrushes.append(makeStreamFillBrush(QStringLiteral("stream-fill"), kStreamPenWidthMm));
    palette.lineBrushes.append(makeStreamFillBrush(QStringLiteral("stream-fill-nopen"), 0.0));
    palette.glyphs.append(brushedGlyph(QStringLiteral("stream-indicator"),
                                       QStringLiteral("stream-fill"), kStreamTriangle));
    palette.glyphs.append(brushedGlyph(QStringLiteral("stream-indicator-nopen"),
                                       QStringLiteral("stream-fill-nopen"), kStreamTriangle));
    return palette;
}

// A placed stamp's sub-paths unioned into one path, for bounds / element-count
// checks that don't care about per-sub-path style.
QPainterPath combinedStampPath(const cwResolvedStamp &stamp)
{
    QPainterPath path;
    for (const cwGlyphSubPath &sub : stamp.subPaths) {
        path.addPath(sub.path);
    }
    return path;
}

// World bounding box over all of a brush's laid-out geometry.
QRectF geometryBounds(const cwBrushDecorationGeometry &geometry)
{
    QRectF bounds;
    const auto add = [&bounds](const QRectF &r) {
        bounds = bounds.isNull() ? r : bounds.united(r);
    };
    for (const auto &layer : geometry.layers) {
        for (const cwGlyphSubPath &traced : layer.traced) {
            add(traced.path.boundingRect());
        }
        for (const cwResolvedStamp &stamp : layer.stamps) {
            add(combinedStampPath(stamp).boundingRect());
        }
    }
    return bounds;
}

// Display scale for paper-mm line widths in the reference image (e.g. a 0.3 mm
// line draws at ~2 px, a 0.6 mm wall at ~4 px). Decoupled from the per-panel
// world->pixel fit so widths stay comparable across panels.
constexpr double kOffsetWidthPxPerMm = 6.6;

// One rule's label for the composition readout, annotated with its decoded
// param (the values Commit B carries) so the readout shows not just which rules
// run but what they're parameterised with. Param-less rules show just the name;
// a param-bearing rule with absent params shows its struct default in (default …).
QString ruleLabel(const cwPlacementRuleData &ruleData)
{
    if (ruleData.name == cwOffsetStrokeRuleName()) {
        // Always set in these demos; signed (sign = side).
        const double mm = ruleData.parameters.value<cwOffsetStrokeParams>().offsetMm;
        return QStringLiteral("%1 (%2%3 mm)")
            .arg(ruleData.name,
                 mm >= 0.0 ? QStringLiteral("+") : QString(),
                 QString::number(mm, 'g', 2));
    }
    if (ruleData.name == cwUniformSpacingRuleName()) {
        const bool hasParams = ruleData.parameters.canConvert<cwUniformSpacingParams>();
        const double mm = ruleData.parameters.value<cwUniformSpacingParams>().spacingMm;
        return QStringLiteral("%1 (%2%3 mm)")
            .arg(ruleData.name,
                 hasParams ? QString() : QStringLiteral("default "),
                 QString::number(mm, 'g', 2));
    }
    return ruleData.name;
}

// Each decoration layer's rule stack as one arrow-joined line, sorted into the
// pipeline order the layout runs them (TransformStroke -> Generate -> ... ->
// Terminal) so the readout matches execution rather than authoring order.
QStringList ruleComposition(const cwLineBrush &brush)
{
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();
    const auto stageOf = [&registry](const cwPlacementRuleData &r) {
        const cwPlacementRule *rule = registry.rule(r.name);
        return rule != nullptr ? static_cast<int>(rule->stage()) : 99;
    };

    QStringList lines;
    for (int i = 0; i < brush.decorations.size(); ++i) {
        QVector<cwPlacementRuleData> rules = brush.decorations.at(i).rules;
        std::stable_sort(rules.begin(), rules.end(),
                         [&stageOf](const cwPlacementRuleData &a, const cwPlacementRuleData &b) {
                             return stageOf(a) < stageOf(b);
                         });
        QStringList names;
        for (const cwPlacementRuleData &r : rules) {
            names.append(ruleLabel(r));
        }
        const QString stack = names.isEmpty() ? QStringLiteral("(no rules)")
                                              : names.join(QStringLiteral("  →  "));
        lines.append(QStringLiteral("L%1:  %2").arg(i).arg(stack));
    }
    if (lines.isEmpty()) {
        lines.append(QStringLiteral("(no decoration layers)"));
    }
    return lines;
}

// The outline pen for a polygon sub-path: its own pen, or no outline at all when
// the pen is zero-width / colourless (the pure-fill degenerate case).
QPen polygonPen(const cwGlyphSubPath &sub)
{
    if (!sub.penColorLight.isValid() || sub.penWidthMm <= 0.0) {
        return QPen(Qt::NoPen);
    }
    QPen pen(sub.penColorLight);
    pen.setCosmetic(true);
    pen.setWidthF(std::max(1.0, sub.penWidthMm * kOffsetWidthPxPerMm));
    return pen;
}

// Draw one sub-path: a Polygon paints with its own pen + fill (pen-less = fill
// only) under a save/restore; a Stroke paints with the painter's current pen.
void drawGlyphSubPath(QPainter &painter, const cwGlyphSubPath &sub)
{
    if (sub.kind != cwGlyphSubPath::Polygon) {
        painter.drawPath(sub.path);
        return;
    }
    painter.save();
    painter.setPen(polygonPen(sub));
    painter.setBrush(sub.fillColorLight.isValid() ? QBrush(sub.fillColorLight)
                                                  : QBrush(Qt::NoBrush));
    painter.drawPath(sub.path);
    painter.restore();
}

void paintPanel(QPainter &painter,
                const QRectF &panelPixels,
                const cwLineBrush &brush,
                const cwBrushDecorationGeometry &geometry,
                const QVector<QPointF> &strokeWorld,
                const QString &caption)
{
    constexpr double kMargin = 24.0;
    // Reserve the right of the panel for the rule-composition readout; fit the
    // geometry into the left "art" region so the two never overlap.
    constexpr double kArtFraction = 0.56;
    const QRectF artRegion(panelPixels.left(), panelPixels.top(),
                           panelPixels.width() * kArtFraction, panelPixels.height());
    const QRectF inner = artRegion.adjusted(kMargin, kMargin, -kMargin, -kMargin);
    QRectF bounds = geometryBounds(geometry);
    const QRectF strokeBounds = QPolygonF(strokeWorld).boundingRect();
    bounds = bounds.isNull() ? strokeBounds : bounds.united(strokeBounds);

    painter.fillRect(panelPixels, Qt::white);
    painter.setPen(QPen(QColor(QStringLiteral("#888888"))));
    painter.drawText(QPointF(panelPixels.left() + 8.0, panelPixels.top() + 18.0), caption);

    // Rule composition readout: each layer's rule stack, in the panel's right
    // region, so the panel shows not just the result but the rules that built it.
    {
        constexpr double kLineHeight = 15.0;
        const double textX = panelPixels.left() + panelPixels.width() * kArtFraction + 8.0;
        double textY = panelPixels.top() + 40.0;

        painter.save();
        QFont compositionFont = painter.font();
        compositionFont.setPointSizeF(8.5);
        painter.setFont(compositionFont);
        painter.setPen(QPen(QColor(QStringLiteral("#555555"))));
        painter.drawText(QPointF(textX, panelPixels.top() + 22.0),
                         QStringLiteral("rule composition (pipeline order):"));
        for (const QString &line : ruleComposition(brush)) {
            painter.drawText(QPointF(textX, textY), line);
            textY += kLineHeight;
        }
        painter.restore();
    }

    // Cell border: inset by half a pixel so the 1 px stroke lands on whole
    // pixels rather than straddling the panel edge.
    painter.setPen(QPen(QColor(QStringLiteral("#cccccc"))));
    painter.drawRect(panelPixels.adjusted(0.5, 0.5, -0.5, -0.5));

    if (bounds.isNull() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return;
    }

    const double scale = std::min(inner.width() / bounds.width(), inner.height() / bounds.height());

    painter.save();
    // World -> pixel, flipping Y (world +Y is up; image +Y is down).
    QTransform toPixel;
    toPixel.translate(inner.left(), inner.bottom());
    toPixel.scale(scale, -scale);
    toPixel.translate(-bounds.left(), -bounds.top());
    painter.setTransform(toPixel, false);

    // The cartographer-drawn stroke path the decorations are generated from,
    // drawn first (underneath) so opaque decorations paint over it — e.g. a
    // filled glyph occludes the centerline, demonstrating layer paint order.
    // Offset decorations sit off-centre and leave it visible.
    QPen strokePen(QColor(QStringLiteral("#e00000")));
    strokePen.setCosmetic(true);
    strokePen.setWidthF(1.0);
    painter.setPen(strokePen);
    painter.drawPolyline(QPolygonF(strokeWorld));

    // geometry.layers is 1:1 with brush.decorations, so each layer's authored
    // lineWidthMm drives the drawn width of both its traced lines and its
    // glyph stamps (wall 0.6 mm reads as double feature/floor-step's 0.3 mm; a
    // railroad's thick ties vs thin rails come from this too).
    for (int i = 0; i < geometry.layers.size(); ++i) {
        const auto &layer = geometry.layers.at(i);

        QPen layerPen(Qt::black);
        layerPen.setCosmetic(true);
        layerPen.setWidthF(std::max(1.0, brush.decorations.at(i).lineWidthMm * kOffsetWidthPxPerMm));
        painter.setPen(layerPen);

        // Traced regions: a Stroke keeps the layer pen; a Polygon (the layer
        // carries a fill) paints its own pen + fill (a zero-width or colourless
        // pen draws fill only).
        for (const cwGlyphSubPath &traced : layer.traced) {
            drawGlyphSubPath(painter, traced);
        }
        // Stamps: same rule per sub-path — a Polygon sub-path uses its glyph
        // brush's own pen + fill, a Stroke sub-path keeps the layer pen.
        for (const cwResolvedStamp &stamp : layer.stamps) {
            for (const cwGlyphSubPath &sub : stamp.subPaths) {
                drawGlyphSubPath(painter, sub);
            }
        }
    }

    painter.restore();
}

int stampCount(const cwBrushDecorationGeometry &geometry)
{
    int count = 0;
    for (const auto &layer : geometry.layers) {
        count += layer.stamps.size();
    }
    return count;
}

} // namespace

TEST_CASE("Layout traces a line brush along the stroke path", "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto brush = cwSymbologyPaletteSeed::create().snapshot()
                           .findBrush(cwSymbologyPaletteSeed::wallBrushName());
    REQUIRE(brush.has_value());

    const QVector<QPointF> strokeWorld = straightLine(0.5, 4);
    const cwBrushDecorationGeometry geometry = layout.layout(*brush, strokeWorld, kScale250);

    REQUIRE(geometry.layers.size() == 1);
    CHECK(geometry.layers.first().kind == cwBrushDecorationGeometry::Layer::Trace);
    REQUIRE(geometry.layers.first().traced.size() == 1);
    // No fill on this brush, so the traced region is a bare Stroke equal to the stroke.
    const cwGlyphSubPath &traced = geometry.layers.first().traced.first();
    CHECK(traced.kind == cwGlyphSubPath::Stroke);
    QPainterPath expected;
    expected.addPolygon(QPolygonF(strokeWorld));
    CHECK(traced.path == expected);
    CHECK(geometry.layers.first().stamps.isEmpty());
}

TEST_CASE("Layout stamps floor-step ticks at 2 mm spacing along the tangent",
          "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto brush = cwSymbologyPaletteSeed::create().snapshot()
                           .findBrush(cwSymbologyPaletteSeed::floorStepBrushName());
    REQUIRE(brush.has_value());

    // 20 mm of paper at 1:250.
    const double lengthWorld = 20.0 * kWorldPerPaperMm250;
    const QVector<QPointF> strokeWorld = straightLine(lengthWorld, 1);

    const cwBrushDecorationGeometry geometry = layout.layout(*brush, strokeWorld, kScale250);

    // Two layers: the traced edge line, then the stamped ticks.
    REQUIRE(geometry.layers.size() == 2);
    CHECK(geometry.layers.at(0).kind == cwBrushDecorationGeometry::Layer::Trace);
    CHECK(geometry.layers.at(1).kind == cwBrushDecorationGeometry::Layer::Stamps);

    // 20 mm / 2 mm spacing, sampled at 0,2,...,20 -> 11 ticks.
    CHECK(geometry.layers.at(1).stamps.size() == 11);

    for (const cwResolvedStamp &stamp : geometry.layers.at(1).stamps) {
        CHECK_FALSE(combinedStampPath(stamp).isEmpty());
        // Horizontal stroke -> tangent angle 0.
        CHECK(stamp.position.rotationRad == Approx(0.0).margin(1e-9));
    }

    // First tick sits at the stroke start.
    CHECK(geometry.layers.at(1).stamps.first().position.anchorWorld.x() == Approx(0.0).margin(1e-9));
}

TEST_CASE("Rotate-by-tangent orients ticks to a diagonal stroke",
          "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto brush = cwSymbologyPaletteSeed::create().snapshot()
                           .findBrush(cwSymbologyPaletteSeed::floorStepBrushName());
    REQUIRE(brush.has_value());

    const double d = 20.0 * kWorldPerPaperMm250;
    const QVector<QPointF> strokeWorld = {QPointF(0.0, 0.0), QPointF(d, d)};

    const cwBrushDecorationGeometry geometry = layout.layout(*brush, strokeWorld, kScale250);
    REQUIRE(geometry.layers.size() == 2);
    REQUIRE_FALSE(geometry.layers.at(1).stamps.isEmpty());

    for (const cwResolvedStamp &stamp : geometry.layers.at(1).stamps) {
        CHECK(stamp.position.rotationRad == Approx(M_PI / 4.0).margin(1e-6));
    }
}

TEST_CASE("A brush with no decorations lays out nothing", "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto brush = cwSymbologyPaletteSeed::create().snapshot()
                           .findBrush(cwSymbologyPaletteSeed::scrapOutlineBrushName());
    REQUIRE(brush.has_value());

    const cwBrushDecorationGeometry geometry =
        layout.layout(*brush, straightLine(0.5, 2), kScale250);
    CHECK(geometry.layers.isEmpty());
}

TEST_CASE("A degenerate stroke lays out nothing", "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(cwSymbologyPaletteSeed::create().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto brush = cwSymbologyPaletteSeed::create().snapshot()
                           .findBrush(cwSymbologyPaletteSeed::wallBrushName());
    REQUIRE(brush.has_value());

    CHECK(layout.layout(*brush, {}, kScale250).layers.isEmpty());
    CHECK(layout.layout(*brush, {QPointF(0.0, 0.0)}, kScale250).layers.isEmpty());
}

TEST_CASE("A single-stamp stack places one glyph at the anchor", "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const cwLineBrush brush = makeBrush(
        QStringLiteral("point-demo"),
        {pointStampLayer(QStringLiteral("plus"))});

    const cwBrushDecorationGeometry geometry =
        layout.layout(brush, straightLine(0.5, 2), kScale250);

    CHECK(stampCount(geometry) == 1);
}

TEST_CASE("A filled Trace brush lays out a filled region with pen and fill",
          "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto brush = demoPalette().snapshot().findBrush(QStringLiteral("stream-fill"));
    REQUIRE(brush.has_value());

    // Lay the fill brush directly over the closed-triangle stroke (paper-mm -> world).
    QVector<QPointF> stroke;
    for (const QPointF &p : kStreamTriangle) {
        stroke.append(p * kWorldPerPaperMm250);
    }
    const cwBrushDecorationGeometry geometry = layout.layout(*brush, stroke, kScale250);

    REQUIRE(geometry.layers.size() == 1);
    CHECK(geometry.layers.first().kind == cwBrushDecorationGeometry::Layer::Trace);
    CHECK(geometry.layers.first().stamps.isEmpty());

    REQUIRE(geometry.layers.first().traced.size() == 1);
    const cwGlyphSubPath &polygon = geometry.layers.first().traced.first();
    CHECK(polygon.kind == cwGlyphSubPath::Polygon);
    CHECK(polygon.fillColorLight == kStreamFillLight);
    CHECK(polygon.fillColorDark == kStreamFillDark);
    CHECK(polygon.penWidthMm == Approx(kStreamPenWidthMm));
    // One path element per triangle vertex (moveTo + lineTos).
    CHECK(polygon.path.elementCount() == kStreamTriangle.size());
}

TEST_CASE("A stamped glyph carries its brush's fill through to the stamp",
          "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    // A brush that drops the filled stream-indicator glyph once at the start.
    const cwLineBrush brush = makeBrush(
        QStringLiteral("stream-point"),
        {pointStampLayer(QStringLiteral("stream-indicator"))});

    const cwBrushDecorationGeometry geometry =
        layout.layout(brush, straightLine(0.5, 2), kScale250);

    REQUIRE(geometry.layers.size() == 1);
    REQUIRE(geometry.layers.first().stamps.size() == 1);
    const cwResolvedStamp &stamp = geometry.layers.first().stamps.first();
    // The triangle is one sub-path, tagged Polygon, carrying the brush's fill —
    // the dividend of the tessellation cache surfacing per-stroke look.
    REQUIRE(stamp.subPaths.size() == 1);
    CHECK(stamp.subPaths.first().kind == cwGlyphSubPath::Polygon);
    CHECK(stamp.subPaths.first().fillColorLight == kStreamFillLight);
    CHECK_FALSE(stamp.subPaths.first().path.isEmpty());
}

TEST_CASE("An open polygon sub-path still carries a fill", "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto brush = demoPalette().snapshot().findBrush(QStringLiteral("stream-fill"));
    REQUIRE(brush.has_value());

    // The same arrowhead, but NOT closed (drop the repeated last vertex). A fill
    // closes it implicitly; the pen would leave the closing edge undrawn.
    QVector<QPointF> open;
    for (int i = 0; i < kStreamTriangle.size() - 1; ++i) {
        open.append(kStreamTriangle.at(i) * kWorldPerPaperMm250);
    }
    REQUIRE(open.size() == 3);

    const cwBrushDecorationGeometry geometry = layout.layout(*brush, open, kScale250);
    REQUIRE(geometry.layers.size() == 1);
    REQUIRE(geometry.layers.first().traced.size() == 1);
    const cwGlyphSubPath &polygon = geometry.layers.first().traced.first();
    CHECK(polygon.fillColorLight.isValid());
    CHECK(polygon.path.elementCount() == 3);
}

TEST_CASE("A bending stamp warps glyphs along a curved stroke", "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const cwLineBrush brush = makeBrush(
        QStringLiteral("bending-demo"),
        {bendingStampLayer(QStringLiteral("dash"))});

    const QVector<QPointF> strokeWorld = arcCurve(2.0, M_PI / 2.0, 24);
    const cwBrushDecorationGeometry geometry = layout.layout(brush, strokeWorld, kScale250);

    CHECK(stampCount(geometry) > 1);
    for (const auto &layer : geometry.layers) {
        for (const cwResolvedStamp &stamp : layer.stamps) {
            CHECK_FALSE(combinedStampPath(stamp).isEmpty());
        }
    }
}

TEST_CASE("The layout feeds worldPerPaperMm so bending subdivides end to end",
          "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const QVector<QPointF> strokeWorld = arcCurve(2.0, M_PI / 2.0, 24);

    // Same Generate rule and same "plus" glyph (1.6 mm -> several step lengths at
    // 1:250); only the terminal differs. Bending subdivides the glyph's long edges,
    // jointed leaves them as chords — so the bending stamp carries more vertices.
    // If the layout passed worldPerPaperMm = 0, bending wouldn't subdivide and the
    // two would match, failing this check.
    const cwLineBrush bending = makeBrush(
        QStringLiteral("bending-plus"), {bendingStampLayer(QStringLiteral("plus"))});
    const cwLineBrush jointed = makeBrush(
        QStringLiteral("jointed-plus"), {jointedStampLayer(QStringLiteral("plus"))});

    const cwBrushDecorationGeometry bg = layout.layout(bending, strokeWorld, kScale250);
    const cwBrushDecorationGeometry jg = layout.layout(jointed, strokeWorld, kScale250);

    REQUIRE_FALSE(bg.layers.isEmpty());
    REQUIRE_FALSE(jg.layers.isEmpty());
    REQUIRE_FALSE(bg.layers.first().stamps.isEmpty());
    REQUIRE(bg.layers.first().stamps.size() == jg.layers.first().stamps.size());

    const QPainterPath bendPath = combinedStampPath(bg.layers.first().stamps.first());
    const QPainterPath jointPath = combinedStampPath(jg.layers.first().stamps.first());
    REQUIRE_FALSE(jointPath.isEmpty());
    CHECK(bendPath.elementCount() > jointPath.elementCount());
}

TEST_CASE("Railroad brush traces two rails offset to opposite sides of the stroke",
          "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const cwLineBrush brush = makeRailroadBrush();

    const double lengthWorld = 20.0 * kWorldPerPaperMm250;
    const QVector<QPointF> strokeWorld = straightLine(lengthWorld, 1);
    const cwBrushDecorationGeometry geometry = layout.layout(brush, strokeWorld, kScale250);

    REQUIRE(geometry.layers.size() == 3);
    REQUIRE(geometry.layers.at(0).kind == cwBrushDecorationGeometry::Layer::Trace);
    REQUIRE(geometry.layers.at(1).kind == cwBrushDecorationGeometry::Layer::Trace);
    CHECK(geometry.layers.at(2).kind == cwBrushDecorationGeometry::Layer::Stamps);

    // Horizontal stroke on y = 0 -> left normal is +Y, so +d rails up, -d down.
    const double offsetWorld = kRailroadHalfGaugeMm * kWorldPerPaperMm250;
    REQUIRE(geometry.layers.at(0).traced.size() == 1);
    REQUIRE(geometry.layers.at(1).traced.size() == 1);
    CHECK(geometry.layers.at(0).traced.first().path.elementAt(0).y == Approx(offsetWorld));
    CHECK(geometry.layers.at(1).traced.first().path.elementAt(0).y == Approx(-offsetWorld));

    // Cross-ties were stamped (one machinery as floor-step ticks).
    CHECK(geometry.layers.at(2).stamps.size() > 1);
}

TEST_CASE("Ceiling-channel stamps ride two offset strokes on opposite sides",
          "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    constexpr double kChannelHalfWidthMm = 0.6;
    const QStringList dashedLine = {QStringLiteral("Uniform spacing"),
                                    QStringLiteral("Bending stamp")};
    const QStringList inwardTicks = {QStringLiteral("Uniform spacing"),
                                     QStringLiteral("Align to tangent"),
                                     QStringLiteral("Rigid stamp")};
    const cwLineBrush brush = makeBrush(
        QStringLiteral("ceiling-channel"),
        {offsetRuleLayer(kChannelHalfWidthMm, QStringLiteral("dash"), dashedLine),
         offsetRuleLayer(kChannelHalfWidthMm, QStringLiteral("ceiling-tick-down"), inwardTicks),
         offsetRuleLayer(-kChannelHalfWidthMm, QStringLiteral("dash"), dashedLine),
         offsetRuleLayer(-kChannelHalfWidthMm, QStringLiteral("ceiling-tick"), inwardTicks)});

    const double lengthWorld = 20.0 * kWorldPerPaperMm250;
    const QVector<QPointF> strokeWorld = straightLine(lengthWorld, 1);
    const cwBrushDecorationGeometry geometry = layout.layout(brush, strokeWorld, kScale250);

    REQUIRE(geometry.layers.size() == 4);
    for (const auto &layer : geometry.layers) {
        CHECK(layer.kind == cwBrushDecorationGeometry::Layer::Stamps);
        CHECK_FALSE(layer.stamps.isEmpty());
    }

    // The two dashed lines sit on the +d and -d offset strokes.
    const double offsetWorld = kChannelHalfWidthMm * kWorldPerPaperMm250;
    CHECK(geometry.layers.at(0).stamps.first().position.anchorWorld.y() == Approx(offsetWorld));
    CHECK(geometry.layers.at(2).stamps.first().position.anchorWorld.y() == Approx(-offsetWorld));
}

TEST_CASE("Decoration layout renders to a reference image", "[cwSketchDecorationLayout]")
{
    cwGlyphTessellationCache cache;
    cache.setSnapshot(demoPalette().snapshot());
    const cwSketchDecorationLayout layout(&cache);

    const auto snapshot = demoPalette().snapshot();
    const auto wall = snapshot.findBrush(cwSymbologyPaletteSeed::wallBrushName());
    const auto floorStep = snapshot.findBrush(cwSymbologyPaletteSeed::floorStepBrushName());
    REQUIRE(wall.has_value());
    REQUIRE(floorStep.has_value());

    // Dashed stroke: a BendingStamp of a short dash glyph along the line.
    // There is no separate "dashed line" mode — the gaps come from the dash
    // glyph being shorter than the stamp spacing.
    const cwLineBrush dashedBrush = makeBrush(
        QStringLiteral("dashed"),
        {bendingStampLayer(QStringLiteral("dash"))});
    // Ceiling step: no solid edge — a dashed line (dash glyph shorter than the
    // stamp spacing, so gaps fall between dashes) plus short rigid ticks on the
    // +normal side (Decision 10's ceiling-step example). Two variants contrast
    // the terminal warp: Bending stamp subdivides each dash so it follows the
    // curve; Jointed stamp warps only the dash vertices, so each dash is a
    // straight chord.
    const cwLineBrush ceilingStepBendingBrush = makeBrush(
        QStringLiteral("ceiling-step-bending"),
        {bendingStampLayer(QStringLiteral("dash")),
         rigidStampLayer(QStringLiteral("ceiling-tick"))});
    const cwLineBrush ceilingStepJointedBrush = makeBrush(
        QStringLiteral("ceiling-step-jointed"),
        {jointedStampLayer(QStringLiteral("dash")),
         rigidStampLayer(QStringLiteral("ceiling-tick"))});
    const cwLineBrush bendingBrush = makeBrush(
        QStringLiteral("bending"),
        {offsetLayer(0.3), bendingStampLayer(QStringLiteral("chevron"))});
    const cwLineBrush pointBrush = makeBrush(
        QStringLiteral("point"),
        {pointStampLayer(QStringLiteral("plus")),
         pointStampLayer(QStringLiteral("plus-v"))});
    // Stream indicator "<-----": a traced line plus a single filled triangle
    // arrowhead (commit 4.2) dropped at the stroke start by the Explicit-point
    // rule. The arrowhead glyph's stroke resolves to the stream-fill polygon
    // brush, so it stamps with a black edge + blue body.
    const cwLineBrush streamBrush = makeBrush(
        QStringLiteral("stream"),
        {offsetLayer(0.3),
         pointStampLayer(QStringLiteral("stream-indicator"))});
    // The same symbol with a pure-fill arrowhead (no outline pen): the polygon
    // primitive's zero-width-pen degenerate case.
    const cwLineBrush streamFillOnlyBrush = makeBrush(
        QStringLiteral("stream-fillonly"),
        {offsetLayer(0.3),
         pointStampLayer(QStringLiteral("stream-indicator-nopen"))});
    // Railroad (commit 4.5): two thick traced rails + closely-spaced cross-ties.
    const cwLineBrush railroadBrush = makeRailroadBrush();
    // Ceiling channel (commit 4.6): two dashed lines at +-0.6 mm, ticks inward.
    const QStringList dashedLine = {QStringLiteral("Uniform spacing"),
                                    QStringLiteral("Bending stamp")};
    const QStringList inwardTicks = {QStringLiteral("Uniform spacing"),
                                     QStringLiteral("Align to tangent"),
                                     QStringLiteral("Rigid stamp")};
    const cwLineBrush ceilingChannelBrush = makeBrush(
        QStringLiteral("ceiling-channel"),
        {offsetRuleLayer(0.6, QStringLiteral("dash"), dashedLine),
         offsetRuleLayer(0.6, QStringLiteral("ceiling-tick-down"), inwardTicks),
         offsetRuleLayer(-0.6, QStringLiteral("dash"), dashedLine),
         offsetRuleLayer(-0.6, QStringLiteral("ceiling-tick"), inwardTicks)});

    const QVector<QPointF> gentle = arcCurve(2.0, M_PI / 3.0, 32);
    const QVector<QPointF> tighter = arcCurve(1.2, M_PI / 2.0, 32);

    struct Panel {
        cwLineBrush brush;
        QVector<QPointF> strokeWorld;
        QString caption;
    };
    const QVector<Panel> panels = {
        {*wall, gentle, QStringLiteral("wall  —  traced offset polyline (offset 0)")},
        {dashedBrush, gentle, QStringLiteral("dashed  —  bending dash glyph (dashed stroke)")},
        {*floorStep, gentle, QStringLiteral("floor-step  —  traced edge + rigid ticks")},
        {ceilingStepBendingBrush, tighter, QStringLiteral("ceiling-step (bending)  —  dashes (with gaps) subdivided to follow the curve")},
        {ceilingStepJointedBrush, tighter, QStringLiteral("ceiling-step (jointed)  —  dashes (with gaps) warped at vertices only (chords)")},
        {bendingBrush, tighter, QStringLiteral("bending  —  traced line + bending chevrons")},
        {pointBrush, straightLine(0.4, 1), QStringLiteral("point  —  single stamped glyph")},
        {streamBrush, straightLine(0.8, 1), QStringLiteral("stream  —  traced line + single triangle arrowhead (<-----)")},
        {streamBrush, {QPointF(0.0, 0.0), QPointF(0.57, 0.57)},
         QStringLiteral("stream 45deg  —  line follows, but the single arrow is NOT tangent-rotated")},
        {streamFillOnlyBrush, arcCurve(0.5, M_PI / 2.0, 24),
         QStringLiteral("stream curve (fill only)  —  pure-fill arrowhead, no outline pen; line follows the curve")},
        {railroadBrush, gentle,
         QStringLiteral("railroad (4.5)  —  two traced rails offset +-d follow the curve + cross-ties")},
        {ceilingChannelBrush, tighter,
         QStringLiteral("ceiling-channel (4.6)  —  two dashed lines offset +-d, ticks pointing inward")},
    };

    constexpr int kWidth = 900;
    constexpr int kPanelHeight = 200;
    const int height = kPanelHeight * panels.size();

    QImage image(kWidth, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (int i = 0; i < panels.size(); ++i) {
            const Panel &panel = panels.at(i);
            const cwBrushDecorationGeometry geometry =
                layout.layout(panel.brush, panel.strokeWorld, kScale250);
            const QRectF panelPixels(0.0, kPanelHeight * i, kWidth, kPanelHeight);
            paintPanel(painter, panelPixels, panel.brush, geometry, panel.strokeWorld, panel.caption);
        }
    }

    QImage blank(image.size(), image.format());
    blank.fill(Qt::white);
    CHECK(image != blank);

    QString path = qEnvironmentVariable("CW_DECORATION_LAYOUT_PNG");
    if (path.isEmpty()) {
        path = QDir(QDir::tempPath()).filePath(QStringLiteral("cavewhere-decoration-layout.png"));
    }
    REQUIRE(image.save(path, "PNG"));
    std::cout << "[cwSketchDecorationLayout] reference image written to: "
              << path.toStdString() << std::endl;
}
