//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QSignalSpy>

//Our includes
#include "cwMovingAveragePenStrokeProxy.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwPenStrokeModel.h"
#include "cwSketch.h"

TEST_CASE("cwPenStrokeModel exposes stroke rows with expected roles", "[cwPenStrokeModel]") {
    cwSketch sketch;
    auto *model = sketch.strokeModel();
    REQUIRE(model != nullptr);

    const auto roles = model->roleNames();
    CHECK(roles.contains(cwPenStrokeModel::StrokeRole));
    CHECK(roles.contains(cwPenStrokeModel::PointsRole));
    CHECK(roles.contains(cwPenStrokeModel::KindRole));
    CHECK(roles.contains(cwPenStrokeModel::WidthRole));
    CHECK(roles.contains(cwPenStrokeModel::ColorRole));
    CHECK(roles.contains(cwPenStrokeModel::IdRole));

    CHECK(model->rowCount() == 0);

    QSignalSpy inserted(model, &QAbstractItemModel::rowsInserted);
    const int row = sketch.beginStroke(cwPenStroke::Wall, 5.0, QColor("#ff0000"));
    CHECK(row == 0);
    CHECK(inserted.count() == 1);
    CHECK(model->rowCount() == 1);

    const QModelIndex idx = model->index(0);
    CHECK(model->data(idx, cwPenStrokeModel::WidthRole).toDouble() == 5.0);
    CHECK(model->data(idx, cwPenStrokeModel::KindRole).toInt() ==
          static_cast<int>(cwPenStroke::Wall));
    CHECK(model->data(idx, cwPenStrokeModel::ColorRole).value<QColor>() == QColor("#ff0000"));
}

TEST_CASE("cwPenStrokeModel coalesces dataChanged across appendPoint calls",
          "[cwPenStrokeModel]") {
    cwSketch sketch;
    auto *model = sketch.strokeModel();

    const int row = sketch.beginStroke(cwPenStroke::Feature, 2.0);

    QSignalSpy changed(model, &QAbstractItemModel::dataChanged);

    for (int i = 0; i < 10; ++i) {
        sketch.appendPoint(row, cwPenPoint(QPointF(i, i), 0.5));
    }

    CHECK(changed.count() == 0); // not yet flushed

    // Drain queued events.
    QCoreApplication::processEvents();

    CHECK(changed.count() == 1);
    CHECK(sketch.strokes().at(row).points.size() == 10);

    // Another burst in the next iteration — again one emit.
    for (int i = 0; i < 5; ++i) {
        sketch.appendPoint(row, cwPenPoint(QPointF(i, -i), 0.5));
    }
    QCoreApplication::processEvents();
    CHECK(changed.count() == 2);
    CHECK(sketch.strokes().at(row).points.size() == 15);
}

TEST_CASE("cwMovingAveragePenStrokeProxy returns raw for short strokes and "
          "window 0", "[cwPenStrokeModel][cwMovingAveragePenStrokeProxy]") {
    cwSketch sketch;
    cwMovingAveragePenStrokeProxy proxy;
    proxy.setSourceModel(sketch.strokeModel());

    const int row = sketch.beginStroke(cwPenStroke::Feature, 1.0);
    sketch.appendPoint(row, cwPenPoint(QPointF(0, 0), 0.5));
    sketch.appendPoint(row, cwPenPoint(QPointF(10, 0), 0.5));
    QCoreApplication::processEvents();

    const QModelIndex idx = proxy.index(0, 0);
    auto pts = proxy.data(idx, cwPenStrokeModel::PointsRole)
                   .value<QVector<cwPenPoint>>();
    // Fewer than 3 points → raw passthrough.
    REQUIRE(pts.size() == 2);
    CHECK(pts.first().position == QPointF(0, 0));
    CHECK(pts.last().position == QPointF(10, 0));
}

TEST_CASE("cwMovingAveragePenStrokeProxy smooths a 5-point stroke",
          "[cwPenStrokeModel][cwMovingAveragePenStrokeProxy]") {
    cwSketch sketch;
    cwMovingAveragePenStrokeProxy proxy;
    proxy.setSourceModel(sketch.strokeModel());
    proxy.setWindowSize(1);

    const int row = sketch.beginStroke(cwPenStroke::Feature, 1.0);
    for (int i = 0; i < 5; ++i) {
        sketch.appendPoint(row, cwPenPoint(QPointF(i, 0), 0.5));
    }
    QCoreApplication::processEvents();

    const QModelIndex idx = proxy.index(0, 0);
    auto pts = proxy.data(idx, cwPenStrokeModel::PointsRole)
                   .value<QVector<cwPenPoint>>();
    REQUIRE(pts.size() == 5);
    // Window 1 means each output averages itself with the prior sample
    // (clamped at the start). Point 0 is raw; point 1 is (0+1)/2 = 0.5.
    CHECK(pts.at(0).position == QPointF(0, 0));
    CHECK(pts.at(1).position == QPointF(0.5, 0));
    CHECK(pts.at(4).position == QPointF(3.5, 0));
}
