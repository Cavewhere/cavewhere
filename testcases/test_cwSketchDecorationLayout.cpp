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
#include "cwGlyphTessellationCache.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwLineBrush.h"
#include "cwDecorationLayer.h"
#include "cwSymbologyGlyph.h"
#include "cwPenStroke.h"
#include "cwPenPoint.h"

// Qt
#include <QColor>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QStringList>

// Std
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
    cwDecorationLayer layer = ruleLayer(QString(), {QStringLiteral("Trace offset polyline")});
    layer.offsetCurveWidthMm = widthMm;
    layer.offsetCurveOffsetMm = 0.0;
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
    // A closed triangle whose apex points back along -X — the "<" arrowhead of a
    // "<-----" stream symbol. Outline only until fill lands (commit 4.2); a
    // single-stamp stack drops it at the stroke start.
    palette.glyphs.append(lineGlyph(QStringLiteral("stream-triangle"),
                                    {QPointF(0.14, -0.14), QPointF(-0.18, 0.0),
                                     QPointF(0.14, 0.14), QPointF(0.14, -0.14)}));
    return palette;
}

// World bounding box over all of a brush's laid-out geometry.
QRectF geometryBounds(const cwBrushDecorationGeometry &geometry)
{
    QRectF bounds;
    const auto add = [&bounds](const QRectF &r) {
        bounds = bounds.isNull() ? r : bounds.united(r);
    };
    for (const auto &layer : geometry.layers) {
        for (const QPolygonF &polyline : layer.offsetPolylines) {
            add(polyline.boundingRect());
        }
        for (const cwResolvedStamp &stamp : layer.stamps) {
            add(stamp.path.boundingRect());
        }
    }
    return bounds;
}

// Display scale for paper-mm line widths in the reference image (e.g. a 0.3 mm
// line draws at ~2 px, a 0.6 mm wall at ~4 px). Decoupled from the per-panel
// world->pixel fit so widths stay comparable across panels.
constexpr double kOffsetWidthPxPerMm = 6.6;

void paintPanel(QPainter &painter,
                const QRectF &panelPixels,
                const cwLineBrush &brush,
                const cwBrushDecorationGeometry &geometry,
                const QVector<QPointF> &strokeWorld,
                const QString &caption)
{
    constexpr double kMargin = 24.0;
    const QRectF inner = panelPixels.adjusted(kMargin, kMargin, -kMargin, -kMargin);
    QRectF bounds = geometryBounds(geometry);
    const QRectF strokeBounds = QPolygonF(strokeWorld).boundingRect();
    bounds = bounds.isNull() ? strokeBounds : bounds.united(strokeBounds);

    painter.fillRect(panelPixels, Qt::white);
    painter.setPen(QPen(QColor(QStringLiteral("#888888"))));
    painter.drawText(QPointF(panelPixels.left() + 8.0, panelPixels.top() + 18.0), caption);

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

    QPen stampPen(Qt::black);
    stampPen.setCosmetic(true);
    stampPen.setWidthF(1.4);

    // geometry.layers is 1:1 with brush.decorations, so an OffsetCurve layer's
    // authored offsetCurveWidthMm drives its drawn width (wall 0.6 mm reads as
    // double feature/floor-step's 0.3 mm).
    for (int i = 0; i < geometry.layers.size(); ++i) {
        const auto &layer = geometry.layers.at(i);

        QPen offsetPen(Qt::black);
        offsetPen.setCosmetic(true);
        offsetPen.setWidthF(std::max(1.0, brush.decorations.at(i).offsetCurveWidthMm * kOffsetWidthPxPerMm));
        painter.setPen(offsetPen);
        for (const QPolygonF &polyline : layer.offsetPolylines) {
            painter.drawPolyline(polyline);
        }
        painter.setPen(stampPen);
        for (const cwResolvedStamp &stamp : layer.stamps) {
            painter.drawPath(stamp.path);
        }
    }

    // The cartographer-drawn stroke path the decorations are generated from,
    // drawn last so it reads on top: solid 1 px red.
    QPen strokePen(QColor(QStringLiteral("#e00000")));
    strokePen.setCosmetic(true);
    strokePen.setWidthF(1.0);
    painter.setPen(strokePen);
    painter.drawPolyline(QPolygonF(strokeWorld));

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
    CHECK(geometry.layers.first().kind == cwBrushDecorationGeometry::Layer::Polylines);
    REQUIRE(geometry.layers.first().offsetPolylines.size() == 1);
    CHECK(geometry.layers.first().offsetPolylines.first() == QPolygonF(strokeWorld));
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
    CHECK(geometry.layers.at(0).kind == cwBrushDecorationGeometry::Layer::Polylines);
    CHECK(geometry.layers.at(1).kind == cwBrushDecorationGeometry::Layer::Stamps);

    // 20 mm / 2 mm spacing, sampled at 0,2,...,20 -> 11 ticks.
    CHECK(geometry.layers.at(1).stamps.size() == 11);

    for (const cwResolvedStamp &stamp : geometry.layers.at(1).stamps) {
        CHECK_FALSE(stamp.path.isEmpty());
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
            CHECK_FALSE(stamp.path.isEmpty());
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

    const QPainterPath bendPath = bg.layers.first().stamps.first().path;
    const QPainterPath jointPath = jg.layers.first().stamps.first().path;
    REQUIRE_FALSE(jointPath.isEmpty());
    CHECK(bendPath.elementCount() > jointPath.elementCount());
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
    // Stream indicator "<-----": a traced line plus a single triangle arrowhead
    // dropped at the stroke start by the Explicit-point rule. Triangle outline
    // only (fill is commit 4.2).
    const cwLineBrush streamBrush = makeBrush(
        QStringLiteral("stream"),
        {offsetLayer(0.3),
         pointStampLayer(QStringLiteral("stream-triangle"))});

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
        {streamBrush, arcCurve(0.5, M_PI / 2.0, 24),
         QStringLiteral("stream curve  —  line follows the curve; arrow stays fixed at the start")},
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
