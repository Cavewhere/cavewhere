// Our includes
#include "cwKeywordFilterPipelineModel.h"
#include "cwKeywordFilterOrGroupProxyModel.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt includes
#include <QSignalSpy>

TEST_CASE("cwKeywordFilterOrGroupProxyModel groups rows by OR boundaries", "[cwKeywordFilterOrGroupProxyModel]")
{
    cwKeywordFilterPipelineModel pipeline;
    cwKeywordFilterOrGroupProxyModel proxy;

    proxy.setSourceModel(&pipeline);
    QSignalSpy resetSpy(&proxy, &QAbstractItemModel::modelReset);

    auto groupCount = [&proxy]() { return proxy.rowCount(); };
    auto childCount = [&proxy](int groupRow) {
        const auto groupIndex = proxy.index(groupRow, 0);
        REQUIRE(groupIndex.isValid());
        return proxy.rowCount(groupIndex);
    };

    SECTION("Single AND row")
    {
        CHECK(groupCount() == 1);
        CHECK(childCount(0) == 1);
    }

    SECTION("Two AND rows stay in one group")
    {
        pipeline.addRow();
        CHECK(groupCount() == 1);
        CHECK(childCount(0) == 2);
        CHECK(resetSpy.count() == 0);
    }

    SECTION("OR starts a new group")
    {
        pipeline.addRow();
        pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(groupCount() == 2);
        CHECK(childCount(0) == 1);
        CHECK(childCount(1) == 1);
        CHECK(resetSpy.count() == 0);
    }

    SECTION("Mixed AND/OR maintains grouping")
    {
        pipeline.addRow(); // row 1 (AND)
        pipeline.addRow(); // row 2 (AND)

        pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(groupCount() == 2);
        CHECK(childCount(0) == 1);
        CHECK(childCount(1) == 2);

        pipeline.setData(pipeline.index(2), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(groupCount() == 3);
        CHECK(childCount(0) == 1);
        CHECK(childCount(1) == 1);
        CHECK(childCount(2) == 1);
        CHECK(resetSpy.count() == 0);
    }

    SECTION("Insert extends group without reset")
    {
        pipeline.addRow(); // add AND row; still one group
        CHECK(groupCount() == 1);
        CHECK(childCount(0) == 2);
        CHECK(resetSpy.count() == 0);
    }

    SECTION("Operator change to OR does not reset")
    {
        pipeline.addRow();
        pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(groupCount() == 2);
        CHECK(childCount(0) == 1);
        CHECK(childCount(1) == 1);
        CHECK(resetSpy.count() == 0);
    }

    SECTION("Remove rows updates groups without reset")
    {
        pipeline.addRow();
        pipeline.setData(pipeline.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole);
        CHECK(groupCount() == 2);
        pipeline.removeRow(1);
        CHECK(groupCount() == 1);
        CHECK(childCount(0) == 1);
        CHECK(resetSpy.count() == 0);
    }
}
