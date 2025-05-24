//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//qt includes
#include <QCoreApplication>
#include <QRectF>
#include <QColor>
#include <QFont>
#include <QtTest/QSignalSpy>

//Our inculdes
#include "InfiniteGridModel.h"

//CaveWhere includes
#include "cwLength.h"

using namespace cwSketch;


TEST_CASE("Default property values", "[InfiniteGridModel]") {
    InfiniteGridModel model;

    // Basic model structure
    REQUIRE(model.rowCount() == 2);

    // Grid defaults
    REQUIRE(model.gridVisible() == true);
    REQUIRE(model.bindableGridVisible().value() == true);
    REQUIRE(model.lineWidth() == Catch::Approx(1.0));
    REQUIRE(model.bindableLineWidth().value() == Catch::Approx(1.0));
    QColor defaultLineColor(0xCC, 0xCC, 0xCC);
    REQUIRE(model.lineColor() == defaultLineColor);
    REQUIRE(model.bindableLineColor().value() == defaultLineColor);
    REQUIRE(model.viewport() == QRectF());
    REQUIRE(model.bindableViewport().value() == QRectF());

    // Label defaults
    REQUIRE(model.labelVisible() == true);
    REQUIRE(model.bindableLabelVisible().value() == true);
    QColor defaultLabelColor(0x88, 0x88, 0x88);
    REQUIRE(model.labelColor() == defaultLabelColor);
    REQUIRE(model.bindableLabelColor().value() == defaultLabelColor);
    QFont defaultFont;
    CHECK(model.labelFont().family() == defaultFont.family());
    CHECK(model.bindableLabelFont().value().family() == defaultFont.family());

    // Grid interval pointer should be valid
    REQUIRE(model.gridInterval() != nullptr);
}

TEST_CASE("Setter methods and signals", "[InfiniteGridModel]") {
    InfiniteGridModel model;

    QSignalSpy gridSpy(&model, &InfiniteGridModel::gridVisibleChanged);
    model.setGridVisible(false);
    REQUIRE(gridSpy.count() == 1);
    REQUIRE(model.gridVisible() == false);

    QSignalSpy widthSpy(&model, &InfiniteGridModel::lineWidthChanged);
    model.setLineWidth(2.5);
    REQUIRE(widthSpy.count() == 1);

    QSignalSpy colorSpy(&model, &InfiniteGridModel::lineColorChanged);
    QColor red(Qt::red);
    model.setLineColor(red);
    REQUIRE(colorSpy.count() == 1);

    // QSignalSpy scaleSpy(&model, &InfiniteGridModel::mapScaleChanged);
    // model.setMapScale(10.0);
    // REQUIRE(scaleSpy.count() == 1);
    // CHECK(scaleSpy.takeFirst().at(0).toDouble() == Catch::Approx(10.0));

    QSignalSpy viewportSpy(&model, &InfiniteGridModel::viewportChanged);
    QRectF rect(1,2,3,4);
    model.setViewport(rect);
    REQUIRE(viewportSpy.count() == 1);

    QSignalSpy labelVisSpy(&model, &InfiniteGridModel::labelVisibleChanged);
    model.setLabelVisible(false);
    REQUIRE(labelVisSpy.count() == 1);

    QSignalSpy labelColorSpy(&model, &InfiniteGridModel::labelColorChanged);
    QColor blue(Qt::blue);
    model.setLabelColor(blue);
    REQUIRE(labelColorSpy.count() == 1);

    QSignalSpy labelFontSpy(&model, &InfiniteGridModel::labelFontChanged);
    QFont font("Arial", 12, QFont::Bold);
    model.setLabelFont(font);
    REQUIRE(labelFontSpy.count() == 1);
}

TEST_CASE("data() provides correct QVariant per role", "[InfiniteGridModel]") {
    InfiniteGridModel model;
    model.setViewport(QRectF(0,0,10,10));
    // model.setMapScale(1.0);

    QModelIndex gridIdx = model.index(0,0);
    // PainterPathRole
    QVariant pathVar = model.data(gridIdx, AbstractPainterPathModel::PainterPathRole);
    REQUIRE(pathVar.canConvert<QPainterPath>());
    auto painterPath = pathVar.value<QPainterPath>();
    CHECK(painterPath.elementCount() > 0);

    // StrokeWidthRole
    QVariant widthVar = model.data(gridIdx, AbstractPainterPathModel::StrokeWidthRole);
    // REQUIRE(widthVar.type() == QVariant::Double);
    CHECK(widthVar.toDouble() == Catch::Approx(model.lineWidth()));

    // StrokeColorRole
    QVariant colorVar = model.data(gridIdx, AbstractPainterPathModel::StrokeColorRole);
    REQUIRE(colorVar.canConvert<QColor>());
    CHECK(colorVar.value<QColor>() == model.lineColor());

    // Invalid row returns null QVariant
    QModelIndex invalidIdx = model.index(2,0);
    QVariant inv = model.data(invalidIdx, AbstractPainterPathModel::PainterPathRole);
    CHECK(!inv.isValid());
}

TEST_CASE("Grid row availability toggles with gridVisible", "[InfiniteGridModel]") {
    InfiniteGridModel model;
    model.setViewport(QRectF(0,0,10,10));
    // model.setMapScale(1.0);

    // Initially gridVisible=true: grid in row 0, label row 1 empty
    REQUIRE(model.rowCount() == 2);
    QModelIndex idx0 = model.index(0,0);
    auto p0 = model.data(idx0, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    REQUIRE(p0.elementCount() > 0);
    auto gridCount = p0.elementCount();

    QModelIndex idx1 = model.index(1,0);
    auto p1 = model.data(idx1, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() > 0);
    auto labelCount = p1.elementCount();

    // Toggle grid off: grid row removed, label becomes row 0
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.setGridVisible(false);
    REQUIRE(removedSpy.count() == 1);
    QList<QVariant> remArgs = removedSpy.takeFirst();
    QCOMPARE(remArgs.at(1).toInt(), 0); // first row removed
    QCOMPARE(remArgs.at(2).toInt(), 1); // last row removed

    REQUIRE(model.rowCount() == 0);

    // Toggle grid back on: row inserted
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    model.setGridVisible(true);
    REQUIRE(insertedSpy.count() == 1);
    QList<QVariant> insArgs = insertedSpy.takeFirst();
    QCOMPARE(insArgs.at(1).toInt(), 0); // first row inserted
    QCOMPARE(insArgs.at(2).toInt(), 1); // last row inserted

    REQUIRE(model.rowCount() == 2);
    QModelIndex backIdx0 = model.index(0,0);
    auto pBack0 = model.data(backIdx0, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(pBack0.elementCount() == gridCount); // grid present again

    idx1 = model.index(1,0);
    p1 = model.data(idx1, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() == labelCount);
}

TEST_CASE("Grid row availability toggles with labelVisible", "[InfiniteGridModel]") {
    InfiniteGridModel model;
    model.setViewport(QRectF(0,0,10,10));
    // model.setMapScale(1.0);

    // Initially gridVisible=true: grid in row 0, label row 1 empty
    REQUIRE(model.rowCount() == 2);
    QModelIndex idx0 = model.index(0,0);
    auto p0 = model.data(idx0, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    REQUIRE(p0.elementCount() > 0);
    auto gridCount = p0.elementCount();

    QModelIndex idx1 = model.index(1,0);
    auto p1 = model.data(idx1, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() > 0);
    auto labelCount = p1.elementCount();


    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.setLabelVisible(false);
    REQUIRE(removedSpy.count() == 1);
    auto remArgs = removedSpy.takeFirst();
    QCOMPARE(remArgs.at(1).toInt(), 1);
    QCOMPARE(remArgs.at(2).toInt(), 1);
    REQUIRE(model.rowCount() == 1);

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    model.setLabelVisible(true);
    REQUIRE(insertedSpy.count() == 1);
    auto insArgs = insertedSpy.takeFirst();
    QCOMPARE(insArgs.at(1).toInt(), 1);
    QCOMPARE(insArgs.at(2).toInt(), 1);
    REQUIRE(model.rowCount() == 2);

    idx0 = model.index(0,0);
    p0 = model.data(idx0, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    REQUIRE(p0.elementCount() == gridCount);

    idx1 = model.index(1,0);
    p1 = model.data(idx1, AbstractPainterPathModel::PainterPathRole).value<QPainterPath>();
    CHECK(p1.elementCount() == labelCount );
}
