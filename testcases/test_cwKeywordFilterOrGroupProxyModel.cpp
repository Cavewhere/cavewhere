// Our includes
#include "cwKeywordFilterPipelineModel.h"
#include "cwKeywordFilterOrGroupProxyModel.h"
#include "cwKeywordFilterGroupProxyModel.h"
#include "SignalSpyChecker.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

TEST_CASE("cwKeywordFilterOrGroupProxyModel exposes groups via OR boundaries", "[cwKeywordFilterOrGroupProxyModel]")
{
    cwKeywordFilterPipelineModel pipeline;
    cwKeywordFilterOrGroupProxyModel orProxy;
    cwKeywordFilterGroupProxyModel groupProxy;

    orProxy.setSourceModel(&pipeline);
    groupProxy.setSourceModel(&pipeline);

    auto orSpyChecker = SignalSpyChecker::Constant::makeChecker(&orProxy);
    auto groupSpyChecker = SignalSpyChecker::Constant::makeChecker(&groupProxy);

    // auto orSpyChecker = SignalSpyChecker::Constant::makeChecker(&orProxy);
    auto or_dataChangedSpy = orSpyChecker.findSpy(&QAbstractItemModel::dataChanged);
    auto or_rowsInsertedSpy = orSpyChecker.findSpy(&QAbstractItemModel::rowsInserted);
    auto or_rowsAboutToBeInsertedSpy = orSpyChecker.findSpy(&QAbstractItemModel::rowsAboutToBeInserted);
    auto or_rowsRemovedSpy = orSpyChecker.findSpy(&QAbstractItemModel::rowsRemoved);
    auto or_rowsAboutToBeRemovedSpy = orSpyChecker.findSpy(&QAbstractItemModel::rowsAboutToBeRemoved);

    auto group_dataChangedSpy = groupSpyChecker.findSpy(&QAbstractItemModel::dataChanged);
    auto group_rowsInsertedSpy = groupSpyChecker.findSpy(&QAbstractItemModel::rowsInserted);
    auto group_rowsAboutToBeInsertedSpy = groupSpyChecker.findSpy(&QAbstractItemModel::rowsAboutToBeInserted);
    auto group_rowsRemovedSpy = groupSpyChecker.findSpy(&QAbstractItemModel::rowsRemoved);
    auto group_rowsAboutToBeRemovedSpy = groupSpyChecker.findSpy(&QAbstractItemModel::rowsAboutToBeRemoved);


    SECTION("Single AND row")
    {
        CHECK(orProxy.rowCount() == 1);
        CHECK(groupProxy.rowCount() == 1);
    }

    SECTION("Two AND rows stay in one group with only dataChanged")
    {
        pipeline.addRow();

        CHECK(orProxy.rowCount() == 1);

        orSpyChecker.checkSpies();

        groupSpyChecker[group_rowsAboutToBeInsertedSpy]++;
        groupSpyChecker[group_rowsInsertedSpy]++;
        groupSpyChecker.checkSpies();
    }

    SECTION("OR starts a new group and inserts a proxy row")
    {
        pipeline.addRow();

        orSpyChecker.checkSpies();

        groupSpyChecker[group_rowsAboutToBeInsertedSpy]++;
        groupSpyChecker[group_rowsInsertedSpy]++;
        groupSpyChecker.checkSpies();

        CHECK(pipeline.rowCount() == 2);
        CHECK(orProxy.rowCount() == 1);
        CHECK(groupProxy.rowCount() == 2);

        bool okay = pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(okay);

        orSpyChecker[or_rowsInsertedSpy]++;
        orSpyChecker[or_rowsAboutToBeInsertedSpy]++;

        groupSpyChecker[group_rowsAboutToBeRemovedSpy]++;
        groupSpyChecker[group_rowsRemovedSpy]++;

        CHECK(group_rowsRemovedSpy->last().at(1).toInt() == 1);
        CHECK(group_rowsRemovedSpy->last().at(2).toInt() == 1);

        CHECK(orProxy.rowCount() == 2);
        CHECK(groupProxy.rowCount() == 1);
    }

    SECTION("Group proxy filters rows by group index")
    {
        pipeline.addRow();
        pipeline.addRow();

        CHECK(orProxy.rowCount() == 1);
        CHECK(groupProxy.rowCount() == 3);
        REQUIRE(pipeline.rowCount() == 3);

        bool okay = pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(okay);

        CHECK(orProxy.rowCount() == 2);
        REQUIRE(pipeline.rowCount() == 3);

        qDebug() << "---- Set group 0! ---";

        // groupProxy.setGroupIndex(0);
        qDebug() << "Group rowCount:" << groupProxy.rowCount();
        CHECK(groupProxy.rowCount() == 1);
        CHECK(groupProxy.index(0, 0).data(cwKeywordFilterGroupProxyModel::SourceRowRole).toInt() == 0);

        qDebug() << "---- Set group 1! ---";

        groupProxy.setGroupIndex(1);
        CHECK(groupProxy.rowCount() == 2);
        CHECK(groupProxy.index(0, 0).data(cwKeywordFilterGroupProxyModel::SourceRowRole).toInt() == 1);
        CHECK(groupProxy.index(1, 0).data(cwKeywordFilterGroupProxyModel::SourceRowRole).toInt() == 2);
    }

    SECTION("Add and remove a row keeps single group stable")
    {
        pipeline.addRow();
        CHECK(pipeline.rowCount() == 2);
        CHECK(orProxy.rowCount() == 1);
        CHECK(groupProxy.rowCount() == 2);

        groupSpyChecker[group_rowsAboutToBeInsertedSpy]++;
        groupSpyChecker[group_rowsInsertedSpy]++;
        groupSpyChecker.checkSpies();

        // QObject::connect(&groupProxy, &QAbstractItemModel::rowsInserted, &groupProxy, [](const QModelIndex& parent, int begin, int end){
        //     qDebug() << "I get here!";
        // });

        pipeline.removeRow(1);

        CHECK(pipeline.rowCount() == 1);
        CHECK(orProxy.rowCount() == 1);
        CHECK(groupProxy.rowCount() == 1);

        groupSpyChecker[group_rowsAboutToBeRemovedSpy]++;
        groupSpyChecker[group_rowsRemovedSpy]++;
        groupSpyChecker.checkSpies();
    }

    SECTION("Promote to OR then remove last row")
    {
        pipeline.addRow(); // row 1 (AND)
        orSpyChecker.checkSpies();
        groupSpyChecker[group_rowsAboutToBeInsertedSpy]++;
        groupSpyChecker[group_rowsInsertedSpy]++;
        groupSpyChecker.checkSpies();

        // Promote the second row to OR, creating a new group
        bool okay = pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(okay);

        orSpyChecker[or_rowsInsertedSpy]++;
        orSpyChecker[or_rowsAboutToBeInsertedSpy]++;
        groupSpyChecker[group_rowsAboutToBeRemovedSpy]++;
        groupSpyChecker[group_rowsRemovedSpy]++;

        CHECK(orProxy.rowCount() == 2);
        CHECK(groupProxy.rowCount() == 1); // default groupIndex 0

        // Remove the OR row
        pipeline.removeRow(1);

        orSpyChecker[or_rowsAboutToBeRemovedSpy]++;
        orSpyChecker[or_rowsRemovedSpy]++;

        CHECK(pipeline.rowCount() == 1);
        CHECK(orProxy.rowCount() == 1);
        CHECK(groupProxy.rowCount() == 1);

        orSpyChecker.checkSpies();
        groupSpyChecker.checkSpies();
    }
}
