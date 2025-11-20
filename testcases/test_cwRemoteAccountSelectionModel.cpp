#include <catch2/catch_test_macros.hpp>

#include "cwRemoteAccountModel.h"
#include "cwRemoteAccountSelectionModel.h"

TEST_CASE("cwRemoteAccountSelectionModel rows", "[cwRemoteAccountSelectionModel]")
{

    cwRemoteAccountModel baseModel;
    baseModel.clear();

    cwRemoteAccountSelectionModel selectionModel;
    selectionModel.setSourceModel(&baseModel);

    REQUIRE(selectionModel.rowCount() == 3); // None + seperator + Add

    auto noneIdx = selectionModel.index(0);
    CHECK(selectionModel.data(noneIdx, cwRemoteAccountSelectionModel::EntryTypeRole).toInt() == cwRemoteAccountSelectionModel::NoneEntry);
    CHECK(selectionModel.data(selectionModel.index(1), cwRemoteAccountSelectionModel::EntryTypeRole).toInt() == cwRemoteAccountSelectionModel::SeparatorEntry);
    CHECK(selectionModel.data(selectionModel.index(2), cwRemoteAccountSelectionModel::EntryTypeRole).toInt() == cwRemoteAccountSelectionModel::AddEntry);

    baseModel.addOrUpdateAccount(cwRemoteAccountModel::Provider::GitHub, QStringLiteral("user1"));

    // None + separator + account + separator + add
    REQUIRE(selectionModel.rowCount() == 5);

    auto sepIdx = selectionModel.index(1);
    CHECK(selectionModel.data(sepIdx, cwRemoteAccountSelectionModel::EntryTypeRole).toInt() == cwRemoteAccountSelectionModel::SeparatorEntry);

    auto accountIdx = selectionModel.index(2);
    CHECK(selectionModel.data(accountIdx, cwRemoteAccountSelectionModel::EntryTypeRole).toInt() == cwRemoteAccountSelectionModel::AccountEntry);
    CHECK(selectionModel.data(accountIdx, cwRemoteAccountSelectionModel::ProviderRole).toInt() == static_cast<int>(cwRemoteAccountModel::Provider::GitHub));

    auto addIdx = selectionModel.index(4);
    CHECK(selectionModel.data(addIdx, cwRemoteAccountSelectionModel::EntryTypeRole).toInt() == cwRemoteAccountSelectionModel::AddEntry);
}
