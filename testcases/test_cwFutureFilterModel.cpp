//Catch includes
#include <catch.hpp>

//Our includes
#include "cwFutureFilterModel.h"
#include "cwFutureManagerModel.h"

//Qt includes
#include <QStandardItemModel>

TEST_CASE("cwFutureFilterModel should filter rows correctly", "[cwFutureFilterModel]") {
    QStandardItemModel model;

    QStandardItem* row1 = new QStandardItem();
    QStandardItem* row2 = new QStandardItem();

    row1->setData(0, cwFutureManagerModel::RunTimeRole);
    row1->setData("row1", cwFutureManagerModel::NameRole);
    row2->setData(0, cwFutureManagerModel::RunTimeRole);
    row2->setData("row2", cwFutureManagerModel::NameRole);

    model.appendRow(row1);
    model.appendRow(row2);

    REQUIRE(model.rowCount() == 2);

    cwFutureFilterModel filterModel;
    filterModel.setSourceModel(&model);

    CHECK(filterModel.rowCount() == 0);

    row1->setData(1500, cwFutureManagerModel::RunTimeRole);
    CHECK(filterModel.rowCount() == 0);
    row1->setData(1501, cwFutureManagerModel::RunTimeRole);
    CHECK(filterModel.rowCount() == 1);
    CHECK(filterModel.index(0, 0).data(cwFutureManagerModel::NameRole).toString().toStdString() == "row1");

    row2->setData(2000, cwFutureManagerModel::RunTimeRole);
    CHECK(filterModel.rowCount() == 2);
    CHECK(filterModel.index(1, 0).data(cwFutureManagerModel::NameRole).toString().toStdString() == "row2");

    SECTION("Change the delay time") {
        filterModel.setDelayTime(2000);
        CHECK(filterModel.rowCount() == 0);

        filterModel.setDelayTime(1600);
        CHECK(filterModel.rowCount() == 1);
        CHECK(filterModel.index(0, 0).data(cwFutureManagerModel::NameRole).toString().toStdString() == "row2");

        row2->setData(1600, cwFutureManagerModel::RunTimeRole);
        CHECK(filterModel.rowCount() == 0);
    }
}
