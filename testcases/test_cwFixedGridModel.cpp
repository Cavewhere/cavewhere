//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QColor>
#include <QFont>
#include <QRectF>
#include <QSignalSpy>

//Our includes
#include "cwFixedGridModel.h"
#include "cwLength.h"

using Catch::Approx;

TEST_CASE("cwFixedGridModel default property values", "[cwFixedGridModel]") {
    cwFixedGridModel model;

    REQUIRE(model.rowCount() == 2);

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

TEST_CASE("cwFixedGridModel data() returns correct QVariant per role", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 10, 10));

    const QModelIndex gridIdx = model.index(0, 0);

    QVariant pathVar = model.data(gridIdx, cwAbstractSketchPainterPathModel::PainterPathRole);
    REQUIRE(pathVar.canConvert<QPainterPath>());
    const auto painterPath = pathVar.value<QPainterPath>();
    CHECK(painterPath.elementCount() > 0);

    QVariant widthVar = model.data(gridIdx, cwAbstractSketchPainterPathModel::StrokeWidthRole);
    CHECK(widthVar.toDouble() == Approx(model.lineWidth()));

    QVariant colorVar = model.data(gridIdx, cwAbstractSketchPainterPathModel::StrokeColorRole);
    REQUIRE(colorVar.canConvert<QColor>());
    CHECK(colorVar.value<QColor>() == model.lineColor());

    // Invalid row returns null QVariant.
    const QModelIndex invalidIdx = model.index(2, 0);
    QVariant inv = model.data(invalidIdx, cwAbstractSketchPainterPathModel::PainterPathRole);
    CHECK(!inv.isValid());
}

TEST_CASE("cwFixedGridModel row availability toggles with gridVisible", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 10, 10));

    REQUIRE(model.rowCount() == 2);
    const QModelIndex idx0 = model.index(0, 0);
    auto p0 = model.data(idx0, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    REQUIRE(p0.elementCount() > 0);
    const auto gridCount = p0.elementCount();

    QModelIndex idx1 = model.index(1, 0);
    auto p1 = model.data(idx1, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() > 0);
    const auto labelCount = p1.elementCount();

    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.setGridVisible(false);
    REQUIRE(removedSpy.count() == 1);
    auto remArgs = removedSpy.takeFirst();
    CHECK(remArgs.at(1).toInt() == 0);
    CHECK(remArgs.at(2).toInt() == 1);

    REQUIRE(model.rowCount() == 0);

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    model.setGridVisible(true);
    REQUIRE(insertedSpy.count() == 1);
    auto insArgs = insertedSpy.takeFirst();
    CHECK(insArgs.at(1).toInt() == 0);
    CHECK(insArgs.at(2).toInt() == 1);

    REQUIRE(model.rowCount() == 2);
    const QModelIndex backIdx0 = model.index(0, 0);
    auto pBack0 = model.data(backIdx0, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(pBack0.elementCount() == gridCount);

    idx1 = model.index(1, 0);
    p1 = model.data(idx1, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() == labelCount);
}

TEST_CASE("cwFixedGridModel row availability toggles with labelVisible", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 10, 10));

    REQUIRE(model.rowCount() == 2);
    QModelIndex idx0 = model.index(0, 0);
    auto p0 = model.data(idx0, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    REQUIRE(p0.elementCount() > 0);
    const auto gridCount = p0.elementCount();

    QModelIndex idx1 = model.index(1, 0);
    auto p1 = model.data(idx1, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() > 0);
    const auto labelCount = p1.elementCount();

    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.setLabelVisible(false);
    REQUIRE(removedSpy.count() == 1);
    auto remArgs = removedSpy.takeFirst();
    CHECK(remArgs.at(1).toInt() == 1);
    CHECK(remArgs.at(2).toInt() == 1);
    REQUIRE(model.rowCount() == 1);

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    model.setLabelVisible(true);
    REQUIRE(insertedSpy.count() == 1);
    auto insArgs = insertedSpy.takeFirst();
    CHECK(insArgs.at(1).toInt() == 1);
    CHECK(insArgs.at(2).toInt() == 1);
    REQUIRE(model.rowCount() == 2);

    idx0 = model.index(0, 0);
    p0 = model.data(idx0, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    REQUIRE(p0.elementCount() == gridCount);

    idx1 = model.index(1, 0);
    p1 = model.data(idx1, cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() == labelCount);
}

TEST_CASE("cwFixedGridModel dataChanged signals on property updates", "[cwFixedGridModel]") {
    cwFixedGridModel model;
    model.setViewport(QRectF(0, 0, 20, 20));

    QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

    SECTION("lineWidth emits StrokeWidthRole for grid row") {
        spy.clear();
        model.setLineWidth(2.5);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(0, 0));
        REQUIRE(args.at(1).value<QModelIndex>() == model.index(0, 0));
        const auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(cwAbstractSketchPainterPathModel::StrokeWidthRole));
        REQUIRE(roles.size() == 1);
    }

    SECTION("lineColor emits StrokeColorRole for grid row") {
        spy.clear();
        model.setLineColor(Qt::red);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(0, 0));
        const auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(cwAbstractSketchPainterPathModel::StrokeColorRole));
    }

    SECTION("labelColor emits StrokeColorRole for label row") {
        spy.clear();
        model.setLabelColor(Qt::blue);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(1, 0));
        const auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(cwAbstractSketchPainterPathModel::StrokeColorRole));
    }

    SECTION("labelFont emits PainterPathRole for label row") {
        spy.clear();
        QFont font("Arial", 14);
        model.setLabelFont(font);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(1, 0));
        const auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(cwAbstractSketchPainterPathModel::PainterPathRole));
    }

    SECTION("viewport change emits PainterPathRole for each row separately") {
        spy.clear();
        model.setViewport(QRectF(0, 0, 30, 30));
        REQUIRE(spy.count() == 2);

        auto args0 = spy.takeFirst();
        REQUIRE(args0.at(0).value<QModelIndex>() == model.index(0, 0));
        REQUIRE(args0.at(1).value<QModelIndex>() == model.index(0, 0));
        const auto roles0 = args0.at(2).value<QVector<int>>();
        REQUIRE(roles0.contains(cwAbstractSketchPainterPathModel::PainterPathRole));

        auto args1 = spy.takeFirst();
        REQUIRE(args1.at(0).value<QModelIndex>() == model.index(1, 0));
        REQUIRE(args1.at(1).value<QModelIndex>() == model.index(1, 0));
        const auto roles1 = args1.at(2).value<QVector<int>>();
        REQUIRE(roles1.contains(cwAbstractSketchPainterPathModel::PainterPathRole));
    }

    SECTION("mapMatrix change emits PainterPathRole for each row separately") {
        spy.clear();
        QMatrix4x4 mat;
        mat.scale(2.0f, 2.0f, 1.0f);
        model.setMapMatrix(mat);
        REQUIRE(spy.count() == 2);

        auto margs0 = spy.takeFirst();
        REQUIRE(margs0.at(0).value<QModelIndex>() == model.index(0, 0));
        REQUIRE(margs0.at(1).value<QModelIndex>() == model.index(0, 0));
        const auto mroles0 = margs0.at(2).value<QVector<int>>();
        REQUIRE(mroles0.contains(cwAbstractSketchPainterPathModel::PainterPathRole));

        auto margs1 = spy.takeFirst();
        REQUIRE(margs1.at(0).value<QModelIndex>() == model.index(1, 0));
        REQUIRE(margs1.at(1).value<QModelIndex>() == model.index(1, 0));
        const auto mroles1 = margs1.at(2).value<QVector<int>>();
        REQUIRE(mroles1.contains(cwAbstractSketchPainterPathModel::PainterPathRole));
    }

    SECTION("gridInterval value change emits PainterPathRole for both rows") {
        spy.clear();

        CHECK(model.gridInterval()->value() == 10.0);
        model.gridInterval()->setValue(20.0);

        REQUIRE(spy.count() == 2);
        auto margs0 = spy.takeFirst();
        REQUIRE(margs0.at(0).value<QModelIndex>() == model.index(0, 0));
        REQUIRE(margs0.at(1).value<QModelIndex>() == model.index(0, 0));
        const auto mroles0 = margs0.at(2).value<QVector<int>>();
        REQUIRE(mroles0.contains(cwAbstractSketchPainterPathModel::PainterPathRole));

        auto margs1 = spy.takeFirst();
        REQUIRE(margs1.at(0).value<QModelIndex>() == model.index(1, 0));
        REQUIRE(margs1.at(1).value<QModelIndex>() == model.index(1, 0));
        const auto mroles1 = margs1.at(2).value<QVector<int>>();
        REQUIRE(mroles1.contains(cwAbstractSketchPainterPathModel::PainterPathRole));
    }
}
