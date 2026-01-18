//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwTableStaticColumnModel.h"
#include "cwTableStaticColumn.h"

TEST_CASE("cwTableStaticColumnModel basic behavior", "[cwTableStaticColumnModel]")
{
    cwTableStaticColumnModel model;

    CHECK(model.rowCount() == 0);
    CHECK(model.count() == 0);
    CHECK(model.totalWidth() == 0);

    CHECK(model.flags(QModelIndex()) == Qt::NoItemFlags);
    CHECK_FALSE(model.data(QModelIndex(), cwTableStaticColumnModel::TextRole).isValid());

    QHash<int, QByteArray> roles = model.roleNames();
    CHECK(roles.value(cwTableStaticColumnModel::TextRole) == "text");
    CHECK(roles.value(cwTableStaticColumnModel::ColumnWidthRole) == "columnWidth");
    CHECK(roles.value(cwTableStaticColumnModel::SortRole) == "sortRole");
}

TEST_CASE("cwTableStaticColumnModel columns list property", "[cwTableStaticColumnModel]")
{
    cwTableStaticColumnModel model;
    QSignalSpy countSpy(&model, &cwTableStaticColumnModel::countChanged);
    QSignalSpy totalSpy(&model, &cwTableStaticColumnModel::totalWidthChanged);

    QQmlListProperty<cwTableStaticColumn> list = model.columns();
    REQUIRE(list.append != nullptr);
    REQUIRE(list.count != nullptr);
    REQUIRE(list.at != nullptr);
    REQUIRE(list.clear != nullptr);

    auto* columnA = new cwTableStaticColumn();
    columnA->setText("A");
    columnA->setColumnWidth(10);
    columnA->setSortRole(1);

    list.append(&list, columnA);

    CHECK(model.count() == 1);
    CHECK(model.rowCount() == 1);
    CHECK(model.totalWidth() == 10);
    CHECK(countSpy.count() == 1);
    CHECK(totalSpy.count() == 1);

    auto* columnB = new cwTableStaticColumn();
    columnB->setText("B");
    columnB->setColumnWidth(20);
    columnB->setSortRole(2);

    list.append(&list, columnB);

    CHECK(model.count() == 2);
    CHECK(model.totalWidth() == 30);
    CHECK(countSpy.count() == 2);
    CHECK(totalSpy.count() == 2);

    list.append(&list, nullptr);
    CHECK(model.count() == 2);

    CHECK(list.count(&list) == 2);
    CHECK(list.at(&list, 0) == columnA);
    CHECK(list.at(&list, 1) == columnB);

    QModelIndex parentIndex = model.index(0, 0);
    REQUIRE(parentIndex.isValid());
    CHECK(model.rowCount(parentIndex) == 0);

    list.clear(&list);
    CHECK(model.count() == 0);
    CHECK(model.totalWidth() == 0);
}

TEST_CASE("cwTableStaticColumnModel data and setData", "[cwTableStaticColumnModel]")
{
    cwTableStaticColumnModel model;

    QQmlListProperty<cwTableStaticColumn> list = model.columns();
    auto* column = new cwTableStaticColumn();
    column->setText("Initial");
    column->setColumnWidth(15);
    column->setSortRole(3);
    list.append(&list, column);

    QModelIndex index = model.index(0, 0);
    REQUIRE(index.isValid());

    CHECK(model.data(index, cwTableStaticColumnModel::TextRole).toString() == "Initial");
    CHECK(model.data(index, cwTableStaticColumnModel::ColumnWidthRole).toInt() == 15);
    CHECK(model.data(index, cwTableStaticColumnModel::SortRole).toInt() == 3);

    CHECK_FALSE(model.data(QModelIndex(), cwTableStaticColumnModel::TextRole).isValid());
    CHECK_FALSE(model.setData(QModelIndex(), 10, cwTableStaticColumnModel::ColumnWidthRole));
    CHECK_FALSE(model.setData(index, 10, Qt::UserRole + 100));

    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy totalSpy(&model, &cwTableStaticColumnModel::totalWidthChanged);

    CHECK(model.setData(index, 25, cwTableStaticColumnModel::ColumnWidthRole));
    CHECK(column->columnWidth() == 25);
    CHECK(model.totalWidth() == 25);
    CHECK(dataSpy.count() == 1);
    CHECK(totalSpy.count() == 1);

    CHECK(model.setData(index, QStringLiteral("Updated"), cwTableStaticColumnModel::TextRole));
    CHECK(column->text() == "Updated");
    CHECK(dataSpy.count() == 2);

    CHECK(model.setData(index, 7, cwTableStaticColumnModel::SortRole));
    CHECK(column->sortRole() == 7);
    CHECK(dataSpy.count() == 3);

    CHECK(model.flags(index).testFlag(Qt::ItemIsEditable));
}

TEST_CASE("cwTableStaticColumnModel column changes propagate", "[cwTableStaticColumnModel]")
{
    cwTableStaticColumnModel model;

    QQmlListProperty<cwTableStaticColumn> list = model.columns();
    auto* column = new cwTableStaticColumn();
    column->setText("Column");
    column->setColumnWidth(5);
    column->setSortRole(1);
    list.append(&list, column);

    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy totalSpy(&model, &cwTableStaticColumnModel::totalWidthChanged);

    column->setColumnWidth(8);
    CHECK(model.totalWidth() == 8);
    CHECK(dataSpy.count() == 1);
    CHECK(totalSpy.count() == 1);

    column->setText("Column 2");
    CHECK(dataSpy.count() == 2);

    column->setSortRole(4);
    CHECK(dataSpy.count() == 3);
}
