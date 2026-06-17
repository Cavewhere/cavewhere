//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QBuffer>
#include <QByteArray>
#include <QRegularExpression>
#include <QString>

//Our includes
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwSketch.h"
#include "cwSketchExporter.h"
#include "cwDecoratedStrokePathSource.h"

namespace {
void seedSketch(cwSketch &sketch) {
    const int a = sketch.beginStroke(QStringLiteral("feature"));
    sketch.appendPoint(a, cwPenPoint(QPointF(0, 0),   0.5));
    sketch.appendPoint(a, cwPenPoint(QPointF(50, 0),  0.5));
    sketch.appendPoint(a, cwPenPoint(QPointF(50, 50), 0.5));

    const int b = sketch.beginStroke(QStringLiteral("wall"));
    sketch.appendPoint(b, cwPenPoint(QPointF(0, 50),   0.5));
    sketch.appendPoint(b, cwPenPoint(QPointF(0, 0),    0.5));
}
} // namespace

TEST_CASE("cwSketchExporter exports a non-empty PDF with the %PDF- magic",
          "[cwSketchExporter]") {
    cwSketch sketch;
    seedSketch(sketch);

    cwDecoratedStrokePathSource pathModel;
    pathModel.setSketch(&sketch);

    cwSketchExporter exporter;
    exporter.setStrokeModel(&pathModel);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));

    REQUIRE(exporter.exportPdf(&buffer));
    buffer.close();

    CHECK(bytes.size() > 0);
    CHECK(bytes.startsWith("%PDF-"));
}

TEST_CASE("cwSketchExporter exports a non-empty SVG",
          "[cwSketchExporter]") {
    cwSketch sketch;
    seedSketch(sketch);

    cwDecoratedStrokePathSource pathModel;
    pathModel.setSketch(&sketch);

    cwSketchExporter exporter;
    exporter.setStrokeModel(&pathModel);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));

    REQUIRE(exporter.exportSvg(&buffer));
    buffer.close();

    CHECK(bytes.size() > 0);
    // QSvgGenerator emits a UTF-8 XML document starting with <?xml ...
    CHECK(bytes.contains("<?xml"));
    CHECK(bytes.contains("<svg"));
}

TEST_CASE("cwSketchExporter handles an empty sketch without crashing",
          "[cwSketchExporter]") {
    cwSketch sketch;

    cwDecoratedStrokePathSource pathModel;
    pathModel.setSketch(&sketch);

    cwSketchExporter exporter;
    exporter.setStrokeModel(&pathModel);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    CHECK(exporter.exportPdf(&buffer));
}

TEST_CASE("cwSketchExporter Y-flips world-up strokes into paper coordinates",
          "[cwSketchExporter]") {
    cwSketch sketch;
    const int stroke = sketch.beginStroke(QStringLiteral("feature"));
    sketch.appendPoint(stroke, cwPenPoint(QPointF(0.0, 10.0), 0.5));
    sketch.appendPoint(stroke, cwPenPoint(QPointF(1.0, 10.0), 0.5));
    sketch.endStroke();

    cwDecoratedStrokePathSource pathModel;
    pathModel.setSketch(&sketch);

    cwSketchExporter exporter;
    exporter.setStrokeModel(&pathModel);
    exporter.setSketch(&sketch);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    REQUIRE(exporter.exportSvg(&buffer));
    buffer.close();

    const QString svg = QString::fromUtf8(bytes);
    REQUIRE(svg.contains("<path"));

    // QSvgGenerator emits strokes under a transform="matrix(a,b,c,d,e,f)"; the
    // d (Y-scale) coefficient must be negative if the Y-flip made it through.
    const QRegularExpression re(QStringLiteral(
        R"(matrix\(([^,]+),([^,]+),([^,]+),([^,]+),)"));
    auto it = re.globalMatch(svg);
    bool sawYFlip = false;
    while (it.hasNext()) {
        const auto m = it.next();
        const double d = m.captured(4).toDouble();
        if (d < 0.0) {
            sawYFlip = true;
            break;
        }
    }
    CHECK(sawYFlip);
}
