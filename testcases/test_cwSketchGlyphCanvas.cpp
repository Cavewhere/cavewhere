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
#include "cwSketchGlyphCanvas.h"
#include "cwSketchPathSource.h"
#include "cwDecoratedStrokePathSource.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyGlyph.h"
#include "cwGlyphTessellationCache.h"
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
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include <QUuid>

// Std
#include <algorithm>
#include <iostream>

namespace {

// A two-stroke glyph authored in glyph-local paper-mm. The first stroke uses the
// wall brush (an OffsetCurve that traces a polyline); the points are chosen so
// the paper-mm <-> world-m factor round-trips exactly.
cwSymbologyGlyph makeGlyph()
{
    cwSymbologyGlyph glyph;
    glyph.name = QStringLiteral("test-glyph");
    glyph.displayName = QStringLiteral("Test Glyph");

    cwPenStroke edge;
    edge.brushName = cwSymbologyPaletteSeed::wallBrushName();
    edge.id = QUuid::createUuid();
    edge.points = {
        cwPenPoint(QPointF(0.0, 0.0), 1.0),
        cwPenPoint(QPointF(2.0, 0.0), 1.0),
        cwPenPoint(QPointF(2.0, 1.5), 1.0),
    };
    glyph.strokes.append(edge);

    cwPenStroke feature;
    feature.brushName = cwSymbologyPaletteSeed::featureBrushName();
    feature.id = QUuid::createUuid();
    feature.points = {
        cwPenPoint(QPointF(-1.0, -0.5), 1.0),
        cwPenPoint(QPointF(-1.0, 0.5), 1.0),
    };
    glyph.strokes.append(feature);

    return glyph;
}

} // namespace

TEST_CASE("Glyph canvas defaults to a 10x10 mm paper extent", "[cwSketchGlyphCanvas]")
{
    cwSketchGlyphCanvas canvas;
    CHECK(canvas.paperExtentMm() == QSizeF(10.0, 10.0));
    CHECK(canvas.palette() == nullptr);
}

TEST_CASE("loadGlyph then toGlyph round-trips the strokes exactly", "[cwSketchGlyphCanvas]")
{
    cwSketchGlyphCanvas canvas;
    const cwSymbologyGlyph glyph = makeGlyph();

    canvas.loadGlyph(glyph);
    const cwSymbologyGlyph roundTrip = canvas.toGlyph(glyph.name, glyph.displayName);

    // paper-mm -> world-m -> paper-mm is a multiply by 0.25 then 4.0 at 1:250,
    // both exact in double, so the strokes (including ids and brush names) match.
    CHECK(roundTrip == glyph);
}

TEST_CASE("Injected palette resolves brushes without a trip or region", "[cwSketchGlyphCanvas]")
{
    cwSketchGlyphCanvas canvas;
    const cwSymbologyGlyph glyph = makeGlyph();

    // No trip and no region: the only way the path source can resolve the wall
    // brush is the directly-injected palette snapshot.
    cwSymbologyPalette palette;
    palette.setData(cwSymbologyPaletteSeed::create());

    // Contrast: with no palette injected and no manager singleton the internal
    // sketch's snapshot is empty, so the wall brush resolves to nothing.
    if (cwSymbologyPaletteManager::instance() == nullptr) {
        canvas.loadGlyph(glyph);
        CHECK(canvas.pathModel()->paths().isEmpty());
    }

    canvas.setPalette(&palette);
    canvas.loadGlyph(glyph);

    REQUIRE(canvas.palette() == &palette);
    CHECK_FALSE(canvas.pathModel()->paths().isEmpty());
}

TEST_CASE("Glyph canvas renders a loaded glyph to a reference image", "[cwSketchGlyphCanvas]")
{
    cwSketchGlyphCanvas canvas;
    cwSymbologyPalette palette;
    palette.setData(cwSymbologyPaletteSeed::create());
    canvas.setPalette(&palette);
    canvas.loadGlyph(makeGlyph());

    const QList<cwSketchPathSource::Path> paths = canvas.pathModel()->paths();
    REQUIRE_FALSE(paths.isEmpty());

    // Combined world-metre bounds of the resolved ink and the paper extent.
    const double factor = cwGlyphTessellationCache::paperMmToWorldM(1.0 / 250.0);
    const QSizeF extentWorld = canvas.paperExtentMm() * factor;
    QRectF worldBounds(-extentWorld.width() / 2.0, -extentWorld.height() / 2.0,
                       extentWorld.width(), extentWorld.height());
    for (const auto &path : paths) {
        worldBounds = worldBounds.united(path.painterPath.boundingRect());
    }

    constexpr int kSize = 400;
    constexpr double kMarginPx = 30.0;
    QImage image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    const double span = std::max(worldBounds.width(), worldBounds.height());
    const double scale = span > 0.0 ? (kSize - 2.0 * kMarginPx) / span : 1.0;

    // world -> pixels: center the bounds, flip Y (world +Y is up, pixels +Y down).
    const QTransform worldToPx =
        QTransform()
            .translate(kSize / 2.0, kSize / 2.0)
            .scale(scale, -scale)
            .translate(-worldBounds.center().x(), -worldBounds.center().y());

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);

        // Paper extent rectangle and origin crosshair.
        painter.setPen(QPen(QColor(200, 200, 200), 0));
        const QRectF extentRect(-extentWorld.width() / 2.0, -extentWorld.height() / 2.0,
                                extentWorld.width(), extentWorld.height());
        painter.drawPolygon(worldToPx.map(QPolygonF(extentRect)));
        const QPointF originPx = worldToPx.map(QPointF(0.0, 0.0));
        painter.drawLine(originPx + QPointF(-6, 0), originPx + QPointF(6, 0));
        painter.drawLine(originPx + QPointF(0, -6), originPx + QPointF(0, 6));

        // Resolved glyph ink.
        for (const auto &path : paths) {
            const QPainterPath pixelPath = worldToPx.map(path.painterPath);
            if (path.strokeWidth <= 0.0) {
                painter.fillPath(pixelPath, path.strokeColor.isValid() ? path.strokeColor : QColor(Qt::black));
            } else {
                painter.setPen(QPen(path.strokeColor.isValid() ? path.strokeColor : QColor(Qt::black),
                                    2.0, Qt::SolidLine, path.cap, path.join));
                painter.drawPath(pixelPath);
            }
        }
    }

    QImage blank(image.size(), image.format());
    blank.fill(Qt::white);
    CHECK(image != blank);

    QString path = qEnvironmentVariable("CW_GLYPH_CANVAS_PNG");
    if (path.isEmpty()) {
        path = QDir(QDir::tempPath()).filePath(QStringLiteral("cavewhere-glyph-canvas.png"));
    }
    REQUIRE(image.save(path, "PNG"));
    std::cout << "[cwSketchGlyphCanvas] reference image written to: "
              << path.toStdString() << std::endl;
}
