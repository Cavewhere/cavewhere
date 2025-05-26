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
#include "FixedGridModel.h"

//CaveWhere includes
#include "cwLength.h"

using namespace cwSketch;


TEST_CASE("Default property values", "[InfiniteGridModel]") {
    FixedGridModel model;

    // Basic model structure
    REQUIRE(model.rowCount() == 2);

    QMatrix4x4 defaultMatrix;
    REQUIRE(model.mapMatrix() == defaultMatrix);

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
    FixedGridModel model;

    QSignalSpy gridSpy(&model, &FixedGridModel::gridVisibleChanged);
    model.setGridVisible(false);
    REQUIRE(gridSpy.count() == 1);
    REQUIRE(model.gridVisible() == false);

    QSignalSpy widthSpy(&model, &FixedGridModel::lineWidthChanged);
    model.setLineWidth(2.5);
    REQUIRE(widthSpy.count() == 1);

    QSignalSpy colorSpy(&model, &FixedGridModel::lineColorChanged);
    QColor red(Qt::red);
    model.setLineColor(red);
    REQUIRE(colorSpy.count() == 1);

    // QSignalSpy scaleSpy(&model, &FixedGridModel::mapScaleChanged);
    // model.setMapScale(10.0);
    // REQUIRE(scaleSpy.count() == 1);
    // CHECK(scaleSpy.takeFirst().at(0).toDouble() == Catch::Approx(10.0));

    QSignalSpy viewportSpy(&model, &FixedGridModel::viewportChanged);
    QRectF rect(1,2,3,4);
    model.setViewport(rect);
    REQUIRE(viewportSpy.count() == 1);

    QSignalSpy labelVisSpy(&model, &FixedGridModel::labelVisibleChanged);
    model.setLabelVisible(false);
    REQUIRE(labelVisSpy.count() == 1);

    QSignalSpy labelColorSpy(&model, &FixedGridModel::labelColorChanged);
    QColor blue(Qt::blue);
    model.setLabelColor(blue);
    REQUIRE(labelColorSpy.count() == 1);

    QSignalSpy labelFontSpy(&model, &FixedGridModel::labelFontChanged);
    QFont font("Arial", 12, QFont::Bold);
    model.setLabelFont(font);
    REQUIRE(labelFontSpy.count() == 1);
}

TEST_CASE("data() provides correct QVariant per role", "[InfiniteGridModel]") {
    FixedGridModel model;
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
    FixedGridModel model;
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
    FixedGridModel model;
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

TEST_CASE("dataChanged signals on property updates", "[InfiniteGridModel]") {
    FixedGridModel model;
    model.setViewport(QRectF(0,0,20,20));

    QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

    SECTION("lineWidth emits StrokeWidthRole for grid row") {
        spy.clear();
        model.setLineWidth(2.5);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(0,0));
        REQUIRE(args.at(1).value<QModelIndex>() == model.index(0,0));
        auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(AbstractPainterPathModel::StrokeWidthRole));
        REQUIRE(roles.size() == 1);
    }

    SECTION("lineColor emits StrokeColorRole for grid row") {
        spy.clear();
        model.setLineColor(Qt::red);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(0,0));
        auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(AbstractPainterPathModel::StrokeColorRole));
    }

    SECTION("labelColor emits StrokeColorRole for label row") {
        spy.clear();
        model.setLabelColor(Qt::blue);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(1,0));
        auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(AbstractPainterPathModel::StrokeColorRole));
    }

    SECTION("labelFont emits PainterPathRole for label row") {
        spy.clear();
        QFont font("Arial",14);
        model.setLabelFont(font);
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).value<QModelIndex>() == model.index(1,0));
        auto roles = args.at(2).value<QVector<int>>();
        REQUIRE(roles.contains(AbstractPainterPathModel::PainterPathRole));
    }

    SECTION("viewport change emits PainterPathRole for each row separately") {
        spy.clear();
        model.setViewport(QRectF(0,0,30,30));
        // two separate signals: one per row
        REQUIRE(spy.count() == 2);
        // First signal for grid
        auto args0 = spy.takeFirst();
        REQUIRE(args0.at(0).value<QModelIndex>() == model.index(0,0));
        REQUIRE(args0.at(1).value<QModelIndex>() == model.index(0,0));
        auto roles0 = args0.at(2).value<QVector<int>>();
        REQUIRE(roles0.contains(AbstractPainterPathModel::PainterPathRole));
        // Second signal for label
        auto args1 = spy.takeFirst();
        REQUIRE(args1.at(0).value<QModelIndex>() == model.index(1,0));
        REQUIRE(args1.at(1).value<QModelIndex>() == model.index(1,0));
        auto roles1 = args1.at(2).value<QVector<int>>();
        REQUIRE(roles1.contains(AbstractPainterPathModel::PainterPathRole));
    }

    SECTION("mapMatrix change emits PainterPathRole for each row separately") {
        spy.clear();
        QMatrix4x4 mat;
        mat.scale(2.0f, 2.0f, 1.0f);
        model.setMapMatrix(mat);
        REQUIRE(spy.count() == 2);
        // grid row
        auto margs0 = spy.takeFirst();
        REQUIRE(margs0.at(0).value<QModelIndex>() == model.index(0,0));
        REQUIRE(margs0.at(1).value<QModelIndex>() == model.index(0,0));
        auto mroles0 = margs0.at(2).value<QVector<int>>();
        REQUIRE(mroles0.contains(AbstractPainterPathModel::PainterPathRole));
        // label row
        auto margs1 = spy.takeFirst();
        REQUIRE(margs1.at(0).value<QModelIndex>() == model.index(1,0));
        REQUIRE(margs1.at(1).value<QModelIndex>() == model.index(1,0));
        auto mroles1 = margs1.at(2).value<QVector<int>>();
        REQUIRE(mroles1.contains(AbstractPainterPathModel::PainterPathRole));
    }


}
