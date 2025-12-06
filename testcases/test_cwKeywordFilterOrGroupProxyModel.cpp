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

        // qDebug() << "---- Set group 0! ---";

        // groupProxy.setGroupIndex(0);
        // qDebug() << "Group rowCount:" << groupProxy.rowCount();
        CHECK(groupProxy.rowCount() == 1);
        CHECK(groupProxy.index(0, 0).data(cwKeywordFilterGroupProxyModel::SourceRowRole).toInt() == 0);

        // qDebug() << "---- Set group 1! ---";

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
        CHECK(or_rowsRemovedSpy->last().at(1).toInt() == 1);
        CHECK(or_rowsRemovedSpy->last().at(2).toInt() == 1);

        CHECK(pipeline.rowCount() == 1);
        CHECK(orProxy.rowCount() == 1);
        CHECK(groupProxy.rowCount() == 1);

        orSpyChecker.checkSpies();
        groupSpyChecker.checkSpies();
    }

    SECTION("Two OR boundaries then remove the first OR row")
    {
        // Add row 1 (AND)
        pipeline.addRow();
        groupSpyChecker[group_rowsAboutToBeInsertedSpy]++;
        groupSpyChecker[group_rowsInsertedSpy]++;
        groupSpyChecker.checkSpies();

        // Make row 1 an OR (creates group boundary)
        bool ok = pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(ok);
        orSpyChecker[or_rowsAboutToBeInsertedSpy]++;
        orSpyChecker[or_rowsInsertedSpy]++;
        groupSpyChecker[group_rowsAboutToBeRemovedSpy]++;
        groupSpyChecker[group_rowsRemovedSpy]++;

        // Add row 2 (AND)
        pipeline.addRow();
        CHECK(pipeline.rowCount() == 3);
        CHECK(orProxy.rowCount() == 2); // first row + row1(OR)

        cwKeywordFilterGroupProxyModel groupProxy1;
        groupProxy1.setSourceModel(&pipeline);
        groupProxy1.setGroupIndex(1);
        groupProxy1.setObjectName("groupProxy1");

        cwKeywordFilterGroupProxyModel groupProxy2;
        groupProxy2.setSourceModel(&pipeline);
        groupProxy2.setGroupIndex(2);
        groupProxy2.setObjectName("groupProxy2");

        CHECK(groupProxy.rowCount() == 1);
        CHECK(groupProxy1.rowCount() == 2);
        CHECK(groupProxy2.rowCount() == 0);

        // Make row 2 an OR (second boundary)
        ok = pipeline.setData(pipeline.index(2), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(ok);
        orSpyChecker[or_rowsAboutToBeInsertedSpy]++;
        orSpyChecker[or_rowsInsertedSpy]++;

        CHECK(groupProxy.rowCount() == 1);
        CHECK(groupProxy1.rowCount() == 1);
        CHECK(groupProxy2.rowCount() == 1);
        CHECK(orProxy.rowCount() == 3);

        // Remove the first OR row (current source row 1)
        pipeline.removeRow(1);
        CHECK(pipeline.rowCount() == 2);
        CHECK(orProxy.rowCount() == 2);
        CHECK(groupProxy.rowCount() == 1);
        CHECK(groupProxy1.rowCount() == 1); //The group below it should fill in
        CHECK(groupProxy2.rowCount() == 0);

        orSpyChecker[or_rowsAboutToBeRemovedSpy]++;
        orSpyChecker[or_rowsRemovedSpy]++;
        CHECK(or_rowsRemovedSpy->last().at(1).toInt() == 1);
        CHECK(or_rowsRemovedSpy->last().at(2).toInt() == 1);

        orSpyChecker.checkSpies();
        groupSpyChecker.checkSpies();
    }

    SECTION("Two OR boundaries then remove last OR then next OR and add another row")
    {
        // Add row 1 (AND)
        pipeline.addRow();
        groupSpyChecker[group_rowsAboutToBeInsertedSpy]++;
        groupSpyChecker[group_rowsInsertedSpy]++;
        groupSpyChecker.checkSpies();

        // Promote row 1 to OR (first boundary)
        bool ok = pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(ok);
        orSpyChecker[or_rowsAboutToBeInsertedSpy]++;
        orSpyChecker[or_rowsInsertedSpy]++;
        groupSpyChecker[group_rowsAboutToBeRemovedSpy]++;
        groupSpyChecker[group_rowsRemovedSpy]++;

        // Add row 2 (AND)
        pipeline.addRow();
        CHECK(pipeline.rowCount() == 3);
        CHECK(orProxy.rowCount() == 2);

        // Promote row 2 to OR (second boundary)
        ok = pipeline.setData(pipeline.index(2), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(ok);
        orSpyChecker[or_rowsAboutToBeInsertedSpy]++;
        orSpyChecker[or_rowsInsertedSpy]++;

        CHECK(orProxy.rowCount() == 3);

        // Remove the last OR (current source row 2 / proxy index 2)
        pipeline.removeRow(2);
        CHECK(pipeline.rowCount() == 2);
        CHECK(orProxy.rowCount() == 2);
        orSpyChecker[or_rowsAboutToBeRemovedSpy]++;
        orSpyChecker[or_rowsRemovedSpy]++;
        CHECK(or_rowsRemovedSpy->at(0).at(1).toInt() == 2);
        CHECK(or_rowsRemovedSpy->at(0).at(2).toInt() == 2);

        // Remove the remaining OR (current source row 1 / proxy index 1)
        pipeline.removeRow(1);
        CHECK(pipeline.rowCount() == 1);
        CHECK(orProxy.rowCount() == 1);
        orSpyChecker[or_rowsAboutToBeRemovedSpy]++;
        orSpyChecker[or_rowsRemovedSpy]++;
        CHECK(or_rowsRemovedSpy->at(1).at(1).toInt() == 1);
        CHECK(or_rowsRemovedSpy->at(1).at(2).toInt() == 1);

        // Add another row (AND)
        pipeline.addRow();
        CHECK(pipeline.rowCount() == 2);
        CHECK(orProxy.rowCount() == 1);

        orSpyChecker.checkSpies();
    }
}
