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
    const int row = sketch.beginStroke(QStringLiteral("wall"));
    REQUIRE(row == 0);
    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().brushName == QStringLiteral("wall"));
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
    sketch.beginStroke(QStringLiteral("feature"));
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

    cwPenStroke s;
    s.brushName = QStringLiteral("wall");
    s.id        = QUuid::createUuid();
    s.points = { cwPenPoint(QPointF(0, 0), 0.5), cwPenPoint(QPointF(1, 1), 0.7) };
    in.strokes.append(s);

    sketch.setData(in);

    CHECK(sketch.name() == in.name);
    CHECK(sketch.id() == in.id);
    CHECK(sketch.viewType() == cwSketch::Plan);
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
    const int row = sketch.beginStroke(QStringLiteral("wall"));
    sketch.appendPoint(row, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.endStroke();
    REQUIRE(sketch.strokes().size() == 1);

    sketch.clearStrokes();
    CHECK(sketch.strokes().isEmpty());
    REQUIRE(sketch.undoStack()->canUndo());
    sketch.undoStack()->undo();
    CHECK(sketch.strokes().size() == 1);
}

namespace {
int drawHorizontalStroke(cwSketch &sketch, const QString &brushName, double y,
                         int pointCount, double spacing = 1.0)
{
    const int row = sketch.beginStroke(brushName);
    for (int i = 0; i < pointCount; ++i) {
        sketch.appendPoint(row, cwPenPoint(QPointF(i * spacing, y), 0.5));
    }
    sketch.endStroke();
    return row;
}
}

TEST_CASE("cwSketch eraseAlongPath splits a stroke at contact", "[cwSketch]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, QStringLiteral("wall"), 0.0, 10);
    REQUIRE(sketch.strokes().size() == 1);
    const QUuid originalId = sketch.strokes().first().id;
    const int undoBefore = sketch.undoStack()->count();

    // Erase one point (x=5) with a tight radius; splits into [0..4] and [6..9].
    sketch.eraseAlongPath({ QPointF(5.0, 0.0) }, 0.4);

    REQUIRE(sketch.strokes().size() == 2);
    const auto &a = sketch.strokes()[0];
    const auto &b = sketch.strokes()[1];
    CHECK(a.brushName == QStringLiteral("wall"));
    CHECK(a.points.size() == 5);
    CHECK(b.points.size() == 4);
    CHECK(a.id != originalId);
    CHECK(b.id != originalId);
    CHECK(a.id != b.id);
    CHECK(sketch.undoStack()->count() == undoBefore + 1);
}

TEST_CASE("cwSketch eraseAlongPath removes a fully covered stroke", "[cwSketch]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, QStringLiteral("feature"), 0.0, 5);

    // Path sweeps across the entire stroke; large radius wipes it out.
    sketch.eraseAlongPath({ QPointF(0.0, 0.0), QPointF(4.0, 0.0) }, 1.0);

    CHECK(sketch.strokes().isEmpty());
    REQUIRE(sketch.undoStack()->canUndo());
}

TEST_CASE("cwSketch eraseAlongPath leaves non-intersecting strokes alone", "[cwSketch]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, QStringLiteral("wall"), 0.0, 5);
    drawHorizontalStroke(sketch, QStringLiteral("feature"), 100.0, 5);
    REQUIRE(sketch.strokes().size() == 2);
    const QUuid survivorId = sketch.strokes()[1].id;

    sketch.eraseAlongPath({ QPointF(0.0, 0.0), QPointF(4.0, 0.0) }, 1.0);

    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().id == survivorId);
    CHECK(sketch.strokes().first().brushName == QStringLiteral("feature"));
}

TEST_CASE("cwSketch eraseAlongPath is a no-op when the path misses everything",
          "[cwSketch]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, QStringLiteral("wall"), 0.0, 5);
    const int undoBefore = sketch.undoStack()->count();
    const auto snapshot = sketch.strokes();

    sketch.eraseAlongPath({ QPointF(500.0, 500.0) }, 0.5);

    CHECK(sketch.undoStack()->count() == undoBefore);
    REQUIRE(sketch.strokes().size() == snapshot.size());
    CHECK(sketch.strokes().first().id == snapshot.first().id);
}

TEST_CASE("cwSketch eraseAlongPath undo restores pre-erase state", "[cwSketch]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, QStringLiteral("wall"), 0.0, 10);
    const QUuid originalId = sketch.strokes().first().id;

    sketch.eraseAlongPath({ QPointF(5.0, 0.0) }, 0.4);
    REQUIRE(sketch.strokes().size() == 2);

    sketch.undoStack()->undo();
    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().id == originalId);
    CHECK(sketch.strokes().first().points.size() == 10);
}

TEST_CASE("cwSketch eraseAlongPath merges successive live calls into one undo step",
          "[cwSketch]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, QStringLiteral("wall"), 0.0, 10);
    const int undoBefore = sketch.undoStack()->count();

    // Simulate a single drag: per-segment calls, each chipping away one
    // point. No endEraseSession() between them — all merge into one step.
    sketch.eraseAlongPath({ QPointF(2.0, 0.0) }, 0.4);
    sketch.eraseAlongPath({ QPointF(3.0, 0.0) }, 0.4);
    sketch.eraseAlongPath({ QPointF(4.0, 0.0) }, 0.4);
    sketch.endEraseSession();

    CHECK(sketch.undoStack()->count() == undoBefore + 1);

    sketch.undoStack()->undo();
    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().points.size() == 10);
}

TEST_CASE("cwSketch eraseAlongPath does not merge across pen lifts",
          "[cwSketch]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, QStringLiteral("wall"), 0.0, 20);
    const int undoBefore = sketch.undoStack()->count();

    // First pen drag: erase point at x=5.
    sketch.eraseAlongPath({ QPointF(5.0, 0.0) }, 0.4);
    sketch.endEraseSession();

    // Second pen drag: erase point at x=15.
    sketch.eraseAlongPath({ QPointF(15.0, 0.0) }, 0.4);
    sketch.endEraseSession();

    // Two distinct pen drags → two undo entries.
    CHECK(sketch.undoStack()->count() == undoBefore + 2);

    // First undo reverts only the second erase (restoring point at x=15).
    sketch.undoStack()->undo();
    REQUIRE(sketch.strokes().size() == 2);
    const int total = sketch.strokes()[0].points.size()
                      + sketch.strokes()[1].points.size();
    CHECK(total == 19);

    // Second undo reverts the first erase, restoring the full 20-point stroke.
    sketch.undoStack()->undo();
    REQUIRE(sketch.strokes().size() == 1);
    CHECK(sketch.strokes().first().points.size() == 20);
}
