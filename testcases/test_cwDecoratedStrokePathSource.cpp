/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QColor>
#include <QDir>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QStyleHints>
#include <QTransform>
#include <QVector>

//Our includes
#include "cwDecoratedStrokePathSource.h"
#include "cwDecorationLayer.h"
#include "cwGlyphTessellationCache.h"
#include "cwLineBrush.h"
#include "cwPaletteSnapshot.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwPlacementRuleData.h"
#include "cwSketch.h"
#include "cwSketchDrawQPainter.h"
#include "cwSketchPainter.h"
#include "cwSketchPathSource.h"
#include "cwSymbologyPaletteSeed.h"

//Std includes
#include <algorithm>
#include <iostream>

using Catch::Approx;

namespace {

constexpr double kScale250 = 1.0 / 250.0;

using Path = cwSketchPathSource::Path;

cwPenStroke strokeFor(const QString &brushName, const QVector<QPointF> &points)
{
    cwPenStroke stroke;
    stroke.brushName = brushName;
    for (const QPointF &p : points) {
        stroke.points.append(cwPenPoint(p, 1.0));
    }
    return stroke;
}

QVector<QPointF> straightLine(double length, int segments)
{
    QVector<QPointF> points;
    for (int i = 0; i <= segments; ++i) {
        points.append(QPointF(length * i / segments, 0.0));
    }
    return points;
}

// Disjoint piece count: one per moveTo, so a batch holding a traced edge plus N
// stamped glyphs reads as 1 + N.
int subPathCount(const QPainterPath &path)
{
    int count = 0;
    for (int i = 0; i < path.elementCount(); ++i) {
        if (path.elementAt(i).isMoveTo()) {
            ++count;
        }
    }
    return count;
}

// The finished-stroke batches paths() returns (element 0 is the active preview).
QList<Path> finishedPaths(const cwDecoratedStrokePathSource &model)
{
    QList<Path> all = model.paths();
    all.removeFirst();
    return all;
}

QRectF finishedBounds(const cwDecoratedStrokePathSource &model)
{
    QRectF bounds;
    for (const Path &path : finishedPaths(model)) {
        bounds = bounds.isNull() ? path.painterPath.boundingRect()
                                 : bounds.united(path.painterPath.boundingRect());
    }
    return bounds;
}

cwPaletteSnapshot seedSnapshot()
{
    return cwSymbologyPaletteSeed::create().snapshot();
}

// Force a deterministic color scheme so resolved-color assertions don't depend
// on the host's dark/light setting (process-global; restore in the toggle test).
void useLightScheme()
{
    if (QStyleHints *hints = QGuiApplication::styleHints()) {
        hints->setColorScheme(Qt::ColorScheme::Light);
    }
}

} // namespace

TEST_CASE("cwDecoratedStrokePathSource defaults", "[cwDecoratedStrokePathSource]")
{
    cwDecoratedStrokePathSource model;
    CHECK(model.paths().size() == 1);          // only the (empty) active preview
    CHECK(model.activeStrokeIndex() == -1);
    CHECK(model.sketch() == nullptr);
    CHECK(model.paths().first().painterPath.isEmpty());
}

TEST_CASE("A wall stroke traces one world-metre pen batch matching the centerline",
          "[cwDecoratedStrokePathSource]")
{
    useLightScheme();

    const cwPaletteSnapshot snap = seedSnapshot();
    const auto wallBrush = snap.findBrush(cwSymbologyPaletteSeed::wallBrushName());
    REQUIRE(wallBrush.has_value());

    const QVector<QPointF> centerline = straightLine(1.0, 4);

    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::wallBrushName(), centerline)});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(snap);
    model.setMapScaleRatio(kScale250);
    model.setSketch(&sketch);

    const QList<Path> finished = finishedPaths(model);
    REQUIRE(finished.size() == 1);

    const Path &path = finished.first();
    CHECK(path.widthInWorldMetres);
    const double expectedWidth = wallBrush->decorations.first().lineWidthMm
                                 * cwGlyphTessellationCache::paperMmToWorldM(kScale250);
    CHECK(path.strokeWidth == Approx(expectedWidth));
    CHECK(path.strokeColor == wallBrush->decorations.first().lineColorLight);

    // One traced region equal to the centerline: a single subpath of N points.
    CHECK(subPathCount(path.painterPath) == 1);
    CHECK(path.painterPath.elementCount() == centerline.size());
}

TEST_CASE("A floor-step stroke flattens the traced edge plus stamped ticks",
          "[cwDecoratedStrokePathSource]")
{
    useLightScheme();

    const double lengthWorld = 20.0 * cwGlyphTessellationCache::paperMmToWorldM(kScale250);

    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::floorStepBrushName(),
                                 straightLine(lengthWorld, 1))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(seedSnapshot());
    model.setMapScaleRatio(kScale250);
    model.setSketch(&sketch);

    // The edge (0.3 mm feature ink) and the tick glyph (also 0.3 mm feature ink)
    // share a resolved style, so they fold into one batch: 1 edge subpath + 11
    // ticks at 2 mm spacing over 20 mm = 12 disjoint pieces. The headline "stamps
    // now render" assertion.
    int totalSubPaths = 0;
    for (const Path &path : finishedPaths(model)) {
        totalSubPaths += subPathCount(path.painterPath);
    }
    CHECK(totalSubPaths == 12);

    // Ticks rise off the y=0 edge along +normal, so the finished ink has height;
    // a bare traced wall would be flat.
    CHECK(finishedBounds(model).height() > 0.0);
}

TEST_CASE("A filled glyph emits a separate fill batch and pen batch",
          "[cwDecoratedStrokePathSource]")
{
    useLightScheme();

    const QColor fill(QStringLiteral("#1e90ff"));
    const QColor pen(QStringLiteral("#000000"));

    cwLineBrush brush;
    brush.name = QStringLiteral("fillbox");
    brush.zOrder = 5;
    cwDecorationLayer layer;
    layer.rules = {cwPlacementRuleData{QStringLiteral("Trace"), {}}};
    layer.lineColorLight = pen;
    layer.lineColorDark = Qt::white;
    layer.lineWidthMm = 0.2;
    layer.fillColorLight = fill;
    layer.fillColorDark = fill;
    brush.decorations.append(layer);

    QHash<QString, cwLineBrush> brushes;
    brushes.insert(brush.name, brush);
    const cwPaletteSnapshot snap(brushes, {});

    // A closed triangle in world metres.
    const QVector<QPointF> triangle = {QPointF(0.0, 0.0), QPointF(0.5, 0.0),
                                       QPointF(0.25, 0.4), QPointF(0.0, 0.0)};

    cwSketch sketch;
    sketch.setStrokes({strokeFor(brush.name, triangle)});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(snap);
    model.setMapScaleRatio(kScale250);
    model.setSketch(&sketch);

    const QList<Path> finished = finishedPaths(model);
    REQUIRE(finished.size() == 2);

    int fillBatches = 0;
    int penBatches = 0;
    for (const Path &path : finished) {
        if (path.strokeWidth <= 0.0) {
            ++fillBatches;
            CHECK(path.strokeColor == fill);
        } else {
            ++penBatches;
            CHECK(path.strokeColor == pen);
            CHECK(path.widthInWorldMetres);
        }
    }
    CHECK(fillBatches == 1);
    CHECK(penBatches == 1);
}

TEST_CASE("A scrap-outline stroke produces no finished ink",
          "[cwDecoratedStrokePathSource]")
{
    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::scrapOutlineBrushName(),
                                 straightLine(1.0, 3))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(seedSnapshot());
    model.setSketch(&sketch);

    CHECK(finishedPaths(model).isEmpty());
}

TEST_CASE("A stroke whose brush is missing from the snapshot draws nothing",
          "[cwDecoratedStrokePathSource]")
{
    cwSketch sketch;
    sketch.setStrokes({strokeFor(QStringLiteral("not-a-real-brush"),
                                 straightLine(1.0, 3))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(seedSnapshot());
    model.setSketch(&sketch);

    CHECK(finishedPaths(model).isEmpty());     // no items, no crash
}

TEST_CASE("A color-scheme toggle re-resolves color without re-laying-out geometry",
          "[cwDecoratedStrokePathSource]")
{
    useLightScheme();

    const cwPaletteSnapshot snap = seedSnapshot();
    const auto wallBrush = snap.findBrush(cwSymbologyPaletteSeed::wallBrushName());
    REQUIRE(wallBrush.has_value());

    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::wallBrushName(),
                                 straightLine(1.0, 4))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(snap);
    model.setSketch(&sketch);

    REQUIRE(finishedPaths(model).size() == 1);
    const Path lightPath = finishedPaths(model).first();
    CHECK(lightPath.strokeColor == wallBrush->decorations.first().lineColorLight);

    QStyleHints *hints = QGuiApplication::styleHints();
    REQUIRE(hints != nullptr);
    hints->setColorScheme(Qt::ColorScheme::Dark);

    const Path darkPath = finishedPaths(model).first();
    // Color flips to the dark source...
    CHECK(darkPath.strokeColor == wallBrush->decorations.first().lineColorDark);
    CHECK(darkPath.strokeColor != lightPath.strokeColor);
    // ...but the geometry is bit-for-bit identical (proves no re-layout).
    CHECK(darkPath.painterPath == lightPath.painterPath);

    hints->setColorScheme(Qt::ColorScheme::Light);
}

TEST_CASE("Changing the map scale re-runs the layout", "[cwDecoratedStrokePathSource]")
{
    useLightScheme();

    const double lengthWorld = 20.0 * cwGlyphTessellationCache::paperMmToWorldM(kScale250);

    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::floorStepBrushName(),
                                 straightLine(lengthWorld, 1))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(seedSnapshot());
    model.setMapScaleRatio(kScale250);
    model.setSketch(&sketch);

    const auto countSubPaths = [&]() {
        int total = 0;
        for (const Path &path : finishedPaths(model)) {
            total += subPathCount(path.painterPath);
        }
        return total;
    };

    const int at250 = countSubPaths();
    model.setMapScaleRatio(1.0 / 500.0);   // coarser scale -> wider world spacing -> fewer ticks
    const int at500 = countSubPaths();

    CHECK(at250 != at500);
}

TEST_CASE("Setting the snapshot brings a stroke's ink to life",
          "[cwDecoratedStrokePathSource]")
{
    useLightScheme();

    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::wallBrushName(),
                                 straightLine(1.0, 4))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(cwPaletteSnapshot());    // empty: wall is unknown
    model.setSketch(&sketch);
    CHECK(finishedPaths(model).isEmpty());

    model.setSnapshot(seedSnapshot());         // wall now resolves
    CHECK(finishedPaths(model).size() == 1);
}

TEST_CASE("Finished batches emit sorted by brush z-order",
          "[cwDecoratedStrokePathSource]")
{
    useLightScheme();

    cwSketch sketch;
    sketch.setStrokes({
        strokeFor(cwSymbologyPaletteSeed::wallBrushName(), straightLine(1.0, 2)),    // z=100
        strokeFor(cwSymbologyPaletteSeed::featureBrushName(), straightLine(1.0, 2)), // z=10
    });

    cwDecoratedStrokePathSource model;
    model.setSnapshot(seedSnapshot());
    model.setSketch(&sketch);

    const QList<Path> finished = finishedPaths(model);
    REQUIRE(finished.size() == 2);
    CHECK(finished.at(0).z < finished.at(1).z);   // feature (10) before wall (100)
    CHECK(finished.at(0).z == Approx(10.0));
    CHECK(finished.at(1).z == Approx(100.0));
}

TEST_CASE("The active stroke renders a screen-pixel centerline preview",
          "[cwDecoratedStrokePathSource]")
{
    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::wallBrushName(),
                                 straightLine(1.0, 4))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(seedSnapshot());
    model.setActiveStrokeIndex(0);   // claim row 0 before wiring the sketch
    model.setSketch(&sketch);

    const Path preview = model.paths().first();
    CHECK_FALSE(preview.painterPath.isEmpty());
    CHECK_FALSE(preview.widthInWorldMetres);   // preview is a screen-pixel width
    CHECK(preview.strokeWidth > 0.0);

    // Row 0 is the active preview, so it is not also a finished batch.
    CHECK(finishedPaths(model).isEmpty());
}

TEST_CASE("Decorated strokes render through the painter in both color schemes",
          "[cwDecoratedStrokePathSource]")
{
    QStyleHints *hints = QGuiApplication::styleHints();
    REQUIRE(hints != nullptr);

    // floor-step gives a traced edge plus stamped ticks — a stamp-bearing brush.
    const double lengthWorld = 20.0 * cwGlyphTessellationCache::paperMmToWorldM(kScale250);

    cwSketch sketch;
    sketch.setStrokes({strokeFor(cwSymbologyPaletteSeed::floorStepBrushName(),
                                 straightLine(lengthWorld, 1))});

    cwDecoratedStrokePathSource model;
    model.setSnapshot(seedSnapshot());
    model.setMapScaleRatio(kScale250);
    model.setSketch(&sketch);

    const QRectF worldBounds = finishedBounds(model);
    REQUIRE(worldBounds.width() > 0.0);
    REQUIRE(worldBounds.height() > 0.0);

    constexpr int kSize = 256;
    constexpr double kMargin = 24.0;

    const auto renderScheme = [&](Qt::ColorScheme scheme) {
        hints->setColorScheme(scheme);

        QImage image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(scheme == Qt::ColorScheme::Dark ? Qt::black : Qt::white);

        // Fit the world bounds into the image, Y-flipped (world +Y is up; the
        // live canvas uses the same negative-determinant transform, so glyph
        // chirality is validated against the real render, not a mirror of it).
        const double s = (std::min)((kSize - 2.0 * kMargin) / worldBounds.width(),
                                    (kSize - 2.0 * kMargin) / worldBounds.height());
        QTransform worldToItem;
        worldToItem.translate(kMargin, kSize - kMargin);
        worldToItem.scale(s, -s);
        worldToItem.translate(-worldBounds.left(), -worldBounds.top());
        REQUIRE(worldToItem.determinant() < 0.0);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        cwSketchDrawQPainter draw(&painter);

        cwSketchPainter::PaintContext ctx;
        ctx.worldToItem    = worldToItem;
        ctx.mapScale       = 1.0;
        ctx.strokes        = &model;
        ctx.strokePenScale = cwSketchPainter::paperStrokePenScale(kScale250, 300.0);
        cwSketchPainter::paint(&draw, ctx);
        painter.end();
        return image;
    };

    const QImage light = renderScheme(Qt::ColorScheme::Light);
    const QImage dark  = renderScheme(Qt::ColorScheme::Dark);
    hints->setColorScheme(Qt::ColorScheme::Light);

    QImage blankLight(light.size(), light.format());
    blankLight.fill(Qt::white);
    QImage blankDark(dark.size(), dark.format());
    blankDark.fill(Qt::black);
    CHECK(light != blankLight);     // something rendered in light mode
    CHECK(dark != blankDark);       // something rendered in dark mode
    CHECK(light != dark);           // the ink inverted with the scheme

    QImage composite(light.width() * 2, light.height(), QImage::Format_ARGB32_Premultiplied);
    composite.fill(Qt::transparent);
    {
        QPainter p(&composite);
        p.drawImage(0, 0, light);
        p.drawImage(light.width(), 0, dark);
    }
    QString path = qEnvironmentVariable("CW_DECORATED_STROKE_RENDER_PNG");
    if (path.isEmpty()) {
        path = QDir(QDir::tempPath()).filePath(
            QStringLiteral("cavewhere-decorated-stroke-render.png"));
    }
    REQUIRE(composite.save(path, "PNG"));
    std::cout << "[cwDecoratedStrokePathSource] render written to: "
              << path.toStdString() << std::endl;
}
