//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QBuffer>
#include <QByteArray>

//Our includes
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwPenStrokeModel.h"
#include "cwSketch.h"
#include "cwSketchExporter.h"
#include "cwSketchPainterPathModel.h"

namespace {
void seedSketch(cwSketch &sketch) {
    const int a = sketch.beginStroke(cwPenStroke::Feature, 2.5, Qt::black);
    sketch.appendPoint(a, cwPenPoint(QPointF(0, 0),   0.5));
    sketch.appendPoint(a, cwPenPoint(QPointF(50, 0),  0.5));
    sketch.appendPoint(a, cwPenPoint(QPointF(50, 50), 0.5));

    const int b = sketch.beginStroke(cwPenStroke::Wall, 5.0, Qt::red);
    sketch.appendPoint(b, cwPenPoint(QPointF(0, 50),   0.5));
    sketch.appendPoint(b, cwPenPoint(QPointF(0, 0),    0.5));
}
} // namespace

TEST_CASE("cwSketchExporter exports a non-empty PDF with the %PDF- magic",
          "[cwSketchExporter]") {
    cwSketch sketch;
    seedSketch(sketch);

    cwSketchPainterPathModel pathModel;
    pathModel.setStrokeModel(sketch.strokeModel());

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

    cwSketchPainterPathModel pathModel;
    pathModel.setStrokeModel(sketch.strokeModel());

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

    cwSketchPainterPathModel pathModel;
    pathModel.setStrokeModel(sketch.strokeModel());

    cwSketchExporter exporter;
    exporter.setStrokeModel(&pathModel);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    CHECK(exporter.exportPdf(&buffer));
}
