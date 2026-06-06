//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QPainterPath>
#include <QSignalSpy>

//Our includes
#include "cwAbstractSketchPainterPathModel.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwPenStrokeModel.h"
#include "cwSketch.h"
#include "cwSketchPainterPathModel.h"

namespace {
QPainterPath pathAt(const cwSketchPainterPathModel &model, int row) {
    return model.index(row)
        .data(cwAbstractSketchPainterPathModel::PainterPathRole)
        .value<QPainterPath>();
}

double widthAt(const cwSketchPainterPathModel &model, int row) {
    return model.index(row)
        .data(cwAbstractSketchPainterPathModel::StrokeWidthRole)
        .toDouble();
}

QColor colorAt(const cwSketchPainterPathModel &model, int row) {
    return model.index(row)
        .data(cwAbstractSketchPainterPathModel::StrokeColorRole)
        .value<QColor>();
}

// cwSketch defers dataChanged to a queued invocation, so tests must pump
// the event loop before checking active-stroke state.
void flush() {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}
} // namespace

TEST_CASE("cwSketchPainterPathModel defaults", "[cwSketchPainterPathModel]") {
    cwSketchPainterPathModel model;
    CHECK(model.rowCount() == 1);
    CHECK(model.activeStrokeIndex() == -1);
    CHECK(model.strokeModel() == nullptr);
    CHECK(pathAt(model, 0).isEmpty());
}

TEST_CASE("cwSketchPainterPathModel active stroke renders centerline at the uniform width",
          "[cwSketchPainterPathModel]") {
    cwSketch sketch;
    cwSketchPainterPathModel model;
    model.setNonWallStrokeColor(Qt::blue);
    model.setStrokeModel(sketch.strokeModel());

    // Set the predicted next row index before beginStroke so the incoming
    // rowsInserted signal sees it as the active row and skips finished-batch
    // routing.
    model.setActiveStrokeIndex(sketch.strokeModel()->rowCount());
    const int row = sketch.beginStroke(QStringLiteral("feature"));
    REQUIRE(row == model.activeStrokeIndex());

    sketch.appendPoint(row, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.appendPoint(row, cwPenPoint(QPointF(10, 10), 0.5));
    sketch.appendPoint(row, cwPenPoint(QPointF(20, 20), 0.5));
    flush();

    REQUIRE(model.rowCount() == 1);
    const QPainterPath path = pathAt(model, 0);
    REQUIRE(path.elementCount() == 3);
    CHECK(path.elementAt(0).type == QPainterPath::MoveToElement);
    CHECK(path.elementAt(1).type == QPainterPath::LineToElement);
    CHECK(path.elementAt(2).type == QPainterPath::LineToElement);
    CHECK(widthAt(model, 0) == kSketchStrokeRenderWidth);
    // feature is not a scrapOutline brush → non-wall colour.
    CHECK(colorAt(model, 0) == QColor(Qt::blue));
}

TEST_CASE("cwSketchPainterPathModel colours strokes by brush class",
          "[cwSketchPainterPathModel]") {
    cwSketch sketch;
    cwSketchPainterPathModel model;
    model.setWallStrokeColor(Qt::black);
    model.setNonWallStrokeColor(Qt::blue);
    model.setStrokeModel(sketch.strokeModel());

    model.setActiveStrokeIndex(sketch.strokeModel()->rowCount());
    const int row = sketch.beginStroke(QStringLiteral("wall"));
    sketch.appendPoint(row, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.appendPoint(row, cwPenPoint(QPointF(10, 10), 0.5));
    flush();

    // wall is a scrapOutline brush → wall colour.
    CHECK(colorAt(model, 0) == QColor(Qt::black));
}

TEST_CASE("cwSketchPainterPathModel finalizes stroke when active index moves on",
          "[cwSketchPainterPathModel]") {
    cwSketch sketch;
    cwSketchPainterPathModel model;
    model.setStrokeModel(sketch.strokeModel());

    // Stroke A: active, then finalized.
    model.setActiveStrokeIndex(sketch.strokeModel()->rowCount());
    const int a = sketch.beginStroke(QStringLiteral("feature"));
    sketch.appendPoint(a, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.appendPoint(a, cwPenPoint(QPointF(10, 10), 0.5));
    flush();

    // New stroke B becomes active, which should push A into a finished batch.
    model.setActiveStrokeIndex(sketch.strokeModel()->rowCount());
    const int b = sketch.beginStroke(QStringLiteral("feature"));
    sketch.appendPoint(b, cwPenPoint(QPointF(100, 100), 0.5));
    sketch.appendPoint(b, cwPenPoint(QPointF(110, 110), 0.5));
    flush();

    REQUIRE(model.rowCount() == 2);
    CHECK_FALSE(pathAt(model, 0).isEmpty()); // active = B
    CHECK_FALSE(pathAt(model, 1).isEmpty()); // finished batch containing A
}

TEST_CASE("cwSketchPainterPathModel batches finished strokes by colour",
          "[cwSketchPainterPathModel]") {
    cwSketch sketch;
    cwSketchPainterPathModel model;
    model.setWallStrokeColor(Qt::black);
    model.setNonWallStrokeColor(Qt::blue);
    model.setStrokeModel(sketch.strokeModel());

    // Three finished feature strokes: same brush class → same colour → one batch.
    for (int i = 0; i < 3; ++i) {
        const int r = sketch.beginStroke(QStringLiteral("feature"));
        sketch.appendPoint(r, cwPenPoint(QPointF(i * 50.0,  0.0), 0.5));
        sketch.appendPoint(r, cwPenPoint(QPointF(i * 50.0, 10.0), 0.5));
    }
    flush();

    // Active row 0 (empty, no active stroke set) + 1 finished batch = 2 rows.
    REQUIRE(model.rowCount() == 2);
    CHECK(colorAt(model, 1) == QColor(Qt::blue));

    // Add a wall stroke (different brush class → different colour) → new batch.
    const int r = sketch.beginStroke(QStringLiteral("wall"));
    sketch.appendPoint(r, cwPenPoint(QPointF(0, 100), 0.5));
    sketch.appendPoint(r, cwPenPoint(QPointF(10, 110), 0.5));
    flush();

    REQUIRE(model.rowCount() == 3);
    // Either batch can be at row 1 or row 2; check both colours are present.
    const QColor c1 = colorAt(model, 1);
    const QColor c2 = colorAt(model, 2);
    CHECK((c1 == QColor(Qt::blue)  || c2 == QColor(Qt::blue)));
    CHECK((c1 == QColor(Qt::black) || c2 == QColor(Qt::black)));
}

TEST_CASE("cwSketchPainterPathModel setStrokeModel(nullptr) clears state",
          "[cwSketchPainterPathModel]") {
    cwSketch sketch;
    cwSketchPainterPathModel model;
    model.setStrokeModel(sketch.strokeModel());

    const int r = sketch.beginStroke(QStringLiteral("feature"));
    sketch.appendPoint(r, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.appendPoint(r, cwPenPoint(QPointF(10, 10), 0.5));
    flush();

    REQUIRE(model.rowCount() == 2);

    model.setStrokeModel(nullptr);
    CHECK(model.rowCount() == 1);
    CHECK(pathAt(model, 0).isEmpty());
}

TEST_CASE("cwSketchPainterPathModel modelReset rebuilds from source",
          "[cwSketchPainterPathModel]") {
    cwSketch sketch;
    cwSketchPainterPathModel model;
    model.setStrokeModel(sketch.strokeModel());

    const int r = sketch.beginStroke(QStringLiteral("feature"));
    sketch.appendPoint(r, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.appendPoint(r, cwPenPoint(QPointF(10, 10), 0.5));
    flush();

    REQUIRE(model.rowCount() == 2);

    // setStrokes triggers a model reset.
    sketch.setStrokes({});
    CHECK(model.rowCount() == 1);
    CHECK(pathAt(model, 0).isEmpty());
}

TEST_CASE("cwSketchPainterPathModel dataChanged coalescing updates active path",
          "[cwSketchPainterPathModel]") {
    cwSketch sketch;
    cwSketchPainterPathModel model;
    model.setStrokeModel(sketch.strokeModel());

    model.setActiveStrokeIndex(sketch.strokeModel()->rowCount());
    const int r = sketch.beginStroke(QStringLiteral("feature"));

    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

    // Five rapid appends — cwSketch coalesces to one dataChanged on the source.
    for (int i = 0; i < 5; ++i) {
        sketch.appendPoint(r, cwPenPoint(QPointF(i * 2.0, i * 2.0), 0.5));
    }
    flush();

    const QPainterPath path = pathAt(model, 0);
    // 5 points → 1 moveTo + 4 lineTo
    CHECK(path.elementCount() == 5);

    // The path model emits dataChanged once per source dataChanged.
    CHECK(dataSpy.count() >= 1);
}
