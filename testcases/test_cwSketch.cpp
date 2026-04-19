//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwSketch.h"
#include "cwSketchData.h"
#include "cwPenStroke.h"
#include "cwPenPoint.h"
#include "cwPenStrokeModel.h"
#include "cwScale.h"

TEST_CASE("cwSketch defaults are sane", "[cwSketch]") {
    cwSketch sketch;
    CHECK(sketch.strokes().isEmpty());
    CHECK(sketch.viewType() == cwSketch::Plan);
    CHECK(sketch.strokeModel() != nullptr);
    CHECK(sketch.strokeModel()->rowCount() == 0);
    CHECK(sketch.undoStack() != nullptr);
    CHECK(sketch.undoStack()->canUndo() == false);
    CHECK(sketch.mapScale() != nullptr);
    CHECK(sketch.id().isNull() == false);
}

TEST_CASE("cwSketch beginStroke/endStroke cycles the undo stack", "[cwSketch]") {
    cwSketch sketch;
    const int row = sketch.beginStroke(cwPenStroke::Wall, 3.0);
    REQUIRE(row == 0);
    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().kind == cwPenStroke::Wall);
    CHECK(sketch.strokes().first().width == 3.0);
    CHECK(sketch.strokeModel()->rowCount() == 1);

    sketch.appendPoint(row, cwPenPoint(QPointF(1, 2), 0.5));
    sketch.appendPoint(row, cwPenPoint(QPointF(3, 4), 0.6));
    REQUIRE(sketch.strokes().first().points.size() == 2);

    sketch.endStroke();
    REQUIRE(sketch.undoStack()->canUndo());

    sketch.undoStack()->undo();
    CHECK(sketch.strokes().isEmpty());
    CHECK(sketch.strokeModel()->rowCount() == 0);

    sketch.undoStack()->redo();
    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().points.size() == 2);
}

TEST_CASE("cwSketch zero-point stroke is still undoable", "[cwSketch]") {
    cwSketch sketch;
    sketch.beginStroke(cwPenStroke::Feature, 2.5);
    sketch.endStroke();
    REQUIRE(sketch.strokes().size() == 1);
    REQUIRE(sketch.undoStack()->canUndo());
    sketch.undoStack()->undo();
    CHECK(sketch.strokes().isEmpty());
}

TEST_CASE("cwSketch setData round-trips", "[cwSketch]") {
    cwSketch sketch;
    cwSketchData in;
    in.name          = "Test Sketch";
    in.id            = QUuid::createUuid();
    in.viewType      = cwSketchData::Plan;
    in.iconImage = QByteArray("fake-png-bytes");

    cwPenStroke s;
    s.kind  = cwPenStroke::Wall;
    s.width = 4.0;
    s.color = QColor("#123456");
    s.id    = QUuid::createUuid();
    s.points = { cwPenPoint(QPointF(0, 0), 0.5), cwPenPoint(QPointF(1, 1), 0.7) };
    in.strokes.append(s);

    sketch.setData(in);

    CHECK(sketch.name() == in.name);
    CHECK(sketch.id() == in.id);
    CHECK(sketch.viewType() == cwSketch::Plan);
    CHECK(sketch.iconImage() == in.iconImage);
    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().points.size() == 2);
    CHECK(sketch.undoStack()->canUndo() == false);

    cwSketchData out = sketch.data();
    CHECK(out.name == in.name);
    CHECK(out.strokes.size() == 1);
    CHECK(out.strokes.first().points.size() == 2);
}

TEST_CASE("cwSketch setId regenerates null uuids", "[cwSketch]") {
    cwSketch sketch;
    sketch.setId(QUuid());
    CHECK(sketch.id().isNull() == false);
}

TEST_CASE("cwSketch clearStrokes is undoable", "[cwSketch]") {
    cwSketch sketch;
    const int row = sketch.beginStroke(cwPenStroke::Wall, 2.0);
    sketch.appendPoint(row, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.endStroke();
    REQUIRE(sketch.strokes().size() == 1);

    sketch.clearStrokes();
    CHECK(sketch.strokes().isEmpty());
    REQUIRE(sketch.undoStack()->canUndo());
    sketch.undoStack()->undo();
    CHECK(sketch.strokes().size() == 1);
}
