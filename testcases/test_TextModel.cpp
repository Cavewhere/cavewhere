//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our inculdes
#include "TextModel.h"

using namespace cwSketch;

TEST_CASE("TextModel initial state", "[TextModel]") {
    TextModel model;
    REQUIRE(model.rowCount() == 0);
    auto roles = model.roleNames();
    REQUIRE(roles.contains(TextModel::TextRole));
    REQUIRE(roles.contains(TextModel::PositionRole));
    REQUIRE(roles.contains(TextModel::FontRole));
    REQUIRE(roles.contains(TextModel::FillColorRole));
    REQUIRE(roles.contains(TextModel::StrokeColorRole));
}

TEST_CASE("Adding TextRow entries emits rowsInserted and data is retrievable", "[TextModel]") {
    TextModel model;
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);

    TextModel::TextRow row;
    row.text = "Hello";
    row.position = QPointF(10.0, 20.0);
    row.font = QFont("Arial", 12);
    row.fillColor = QColor(Qt::red);
    row.strokeColor = QColor(Qt::blue);

    model.addText(QModelIndex(), row);
    REQUIRE(insertedSpy.count() == 1);
    REQUIRE(model.rowCount() == 1);

    QModelIndex idx = model.index(0, 0);
    REQUIRE(idx.isValid());
    CHECK(model.data(idx, TextModel::TextRole).toString() == "Hello");
}

TEST_CASE("setData updates values and emits dataChanged", "[TextModel]") {
    TextModel model;
    TextModel::TextRow row{"Orig", QPointF(0,0), QFont(), QColor(), QColor()};
    model.addText(QModelIndex(), row);

    QModelIndex idx = model.index(0, 0);
    QSignalSpy dataSpy(&model, &TextModel::dataChanged);

    bool result = model.setData(idx, QVariant("Updated"), TextModel::TextRole);
    REQUIRE(result);
    REQUIRE(dataSpy.count() == 1);
    CHECK(model.data(idx, TextModel::TextRole).toString() == "Updated");
}

TEST_CASE("Removing, setting rows, and clearing model emit proper signals", "[TextModel]") {
    TextModel model;
    QVector<TextModel::TextRow> rows;
    for (int i = 0; i < 3; ++i) {
        TextModel::TextRow r;
        r.text = QString("Item%1").arg(i);
        rows.append(r);
    }

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setRows(rows);
    REQUIRE(resetSpy.count() == 1);
    REQUIRE(model.rowCount() == 3);

    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeText(model.index(1, 0));
    REQUIRE(removedSpy.count() == 1);
    REQUIRE(model.rowCount() == 2);
    CHECK(model.data(model.index(1, 0), TextModel::TextRole).toString() == "Item2");

    QSignalSpy clearSpy(&model, &QAbstractItemModel::modelReset);
    model.clear();
    REQUIRE(clearSpy.count() == 1);
    REQUIRE(model.rowCount() == 0);
}
