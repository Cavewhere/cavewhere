//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QRectF>
#include <QSignalSpy>

//Our includes
#include "cwFixedGridModel.h"
#include "cwGridTextModel.h"
#include "cwLength.h"

using Catch::Approx;

TEST_CASE("cwFixedGridModel default property values", "[cwFixedGridModel]") {
    cwFixedGridModel model;

    REQUIRE(model.paths().size() == 2);

    QMatrix4x4 defaultMatrix;
    REQUIRE(model.mapMatrix() == defaultMatrix);

    REQUIRE(model.gridVisible() == true);
    REQUIRE(model.bindableGridVisible().value() == true);
    REQUIRE(model.lineWidth() == Approx(1.0));
    REQUIRE(model.bindableLineWidth().value() == Approx(1.0));
    QColor defaultLineColor(0xCC, 0xCC, 0xCC);
    REQUIRE(model.lineColor() == defaultLineColor);
    REQUIRE(model.bindableLineColor().value() == defaultLineColor);
    REQUIRE(model.viewport() == QRectF());
    REQUIRE(model.bindableViewport().value() == QRectF());

    REQUIRE(model.labelVisible() == true);
    REQUIRE(model.bindableLabelVisible().value() == true);
    QColor defaultLabelColor(0x88, 0x88, 0x88);
    REQUIRE(model.labelColor() == defaultLabelColor);
    REQUIRE(model.bindableLabelColor().value() == defaultLabelColor);
    QFont defaultFont;
    CHECK(model.labelFont().family() == defaultFont.family());
    CHECK(model.bindableLabelFont().value().family() == defaultFont.family());

    REQUIRE(model.gridInterval() != nullptr);
}

TEST_CASE("cwFixedGridModel setter methods emit notify signals", "[cwFixedGridModel]") {
    cwFixedGridModel model;

    QSignalSpy gridSpy(&model, &cwFixedGridModel::gridVisibleChanged);
    model.setGridVisible(false);
    REQUIRE(gridSpy.count() == 1);
    REQUIRE(model.gridVisible() == false);

    QSignalSpy widthSpy(&model, &cwFixedGridModel::lineWidthChanged);
    model.setLineWidth(2.5);
    REQUIRE(widthSpy.count() == 1);

    QSignalSpy colorSpy(&model, &cwFixedGridModel::lineColorChanged);
    model.setLineColor(Qt::red);
    REQUIRE(colorSpy.count() == 1);

    QSignalSpy viewportSpy(&model, &cwFixedGridModel::viewportChanged);
    model.setViewport(QRectF(1, 2, 3, 4));
    REQUIRE(viewportSpy.count() == 1);

    QSignalSpy labelVisSpy(&model, &cwFixedGridModel::labelVisibleChanged);
    model.setLabelVisible(false);
    REQUIRE(labelVisSpy.count() == 1);

    QSignalSpy labelColorSpy(&model, &cwFixedGridModel::labelColorChanged);
    model.setLabelColor(Qt::blue);
    REQUIRE(labelColorSpy.count() == 1);

    QSignalSpy labelFontSpy(&model, &cwFixedGridModel::labelFontChanged);
    QFont font("Arial", 12, QFont::Bold);
    model.setLabelFont(font);
    REQUIRE(labelFontSpy.count() == 1);
}

TEST_CASE("cwFixedGridModel paths() carries geometry, width and color", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 10, 10));

    const auto paths = model.paths();
    REQUIRE(paths.size() == 2);

    // Element 0 is the grid line path, carrying lineWidth/lineColor.
    CHECK(paths.at(0).painterPath.elementCount() > 0);
    CHECK(paths.at(0).strokeWidth == Approx(model.lineWidth()));
    CHECK(paths.at(0).strokeColor == model.lineColor());

    // Element 1 is the label-background fill (strokeWidth <= 0 flags a fill).
    CHECK(paths.at(1).strokeColor == model.labelBackgroundColor());
    CHECK(paths.at(1).strokeWidth <= 0.0);
}

TEST_CASE("cwFixedGridModel paths() availability toggles with gridVisible", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 10, 10));

    auto paths = model.paths();
    REQUIRE(paths.size() == 2);
    REQUIRE(paths.at(0).painterPath.elementCount() > 0);
    const auto gridCount  = paths.at(0).painterPath.elementCount();
    CHECK(paths.at(1).painterPath.elementCount() > 0);
    const auto labelCount = paths.at(1).painterPath.elementCount();

    QSignalSpy pathsSpy(&model, &cwFixedGridModel::pathsChanged);
    model.setGridVisible(false);
    REQUIRE(pathsSpy.count() >= 1);
    CHECK(model.paths().isEmpty());

    const int afterHide = pathsSpy.count();
    model.setGridVisible(true);
    REQUIRE(pathsSpy.count() > afterHide);

    paths = model.paths();
    REQUIRE(paths.size() == 2);
    CHECK(paths.at(0).painterPath.elementCount() == gridCount);
    CHECK(paths.at(1).painterPath.elementCount() == labelCount);
}

TEST_CASE("cwFixedGridModel paths() availability toggles with labelVisible", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 10, 10));

    auto paths = model.paths();
    REQUIRE(paths.size() == 2);
    const auto gridCount  = paths.at(0).painterPath.elementCount();
    const auto labelCount = paths.at(1).painterPath.elementCount();
    REQUIRE(gridCount > 0);
    CHECK(labelCount > 0);

    QSignalSpy pathsSpy(&model, &cwFixedGridModel::pathsChanged);
    model.setLabelVisible(false);
    REQUIRE(pathsSpy.count() >= 1);
    CHECK(model.paths().size() == 1); // grid only

    model.setLabelVisible(true);
    paths = model.paths();
    REQUIRE(paths.size() == 2);
    CHECK(paths.at(0).painterPath.elementCount() == gridCount);
    CHECK(paths.at(1).painterPath.elementCount() == labelCount);
}

TEST_CASE("cwFixedGridModel pathsChanged fires on property updates", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 20, 20));

    QSignalSpy spy(&model, &cwFixedGridModel::pathsChanged);

    SECTION("lineWidth updates the grid path width") {
        spy.clear();
        model.setLineWidth(2.5);
        REQUIRE(spy.count() == 1);
        CHECK(model.paths().at(0).strokeWidth == Approx(2.5));
    }

    SECTION("lineColor updates the grid path color") {
        spy.clear();
        model.setLineColor(Qt::red);
        REQUIRE(spy.count() == 1);
        CHECK(model.paths().at(0).strokeColor == QColor(Qt::red));
    }

    SECTION("labelColor change repaints") {
        spy.clear();
        model.setLabelColor(Qt::blue);
        REQUIRE(spy.count() == 1);
    }

    SECTION("labelFont change rebuilds the label background path") {
        spy.clear();
        QFont font("Arial", 14);
        model.setLabelFont(font);
        REQUIRE(spy.count() >= 1);
    }

    SECTION("viewport change rebuilds grid and label paths") {
        spy.clear();
        model.setViewport(QRectF(0, 0, 30, 30));
        REQUIRE(spy.count() >= 1);
    }

    SECTION("gridInterval value change rebuilds grid and label paths") {
        spy.clear();
        CHECK(model.gridInterval()->value() == 10.0);
        model.gridInterval()->setValue(20.0);
        REQUIRE(spy.count() >= 1);
    }
}

TEST_CASE("cwFixedGridModel text rows carry bounds for screen-space baseline",
          "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 50, 50));
    QFont font("Arial", 12);
    model.setLabelFont(font);

    auto *textModel = model.textModel();
    REQUIRE(textModel != nullptr);
    const auto rows = textModel->rows();
    REQUIRE(!rows.isEmpty());

    // `bounds` is the label rect in world space; `position` is its top-left.
    // The painter is responsible for mapping bounds through worldToItem and
    // deriving the baseline in screen pixels.
    for (const auto &row : rows) {
        CHECK(row.bounds.isValid());
        CHECK(row.position == row.bounds.topLeft());
    }

    // X-axis labels are pinned to the viewport's smaller-Y edge — which the
    // sketch maps to screen bottom via its Y-flip mapMatrix.
    int topAlignedLabels = 0;
    for (const auto &row : rows) {
        if (std::abs(row.bounds.top() - 0.0) <= 0.5) {
            ++topAlignedLabels;
        }
    }
    CHECK(topAlignedLabels >= 3);
}
