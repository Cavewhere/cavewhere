#include <catch2/catch_test_macros.hpp>

#include "cwRemoteAccountModel.h"

TEST_CASE("cwRemoteAccountModel add/remove/persist", "[cwRemoteAccountModel]")
{
    {
        cwRemoteAccountModel model;
        REQUIRE(model.rowCount() == 0);

        model.addOrUpdateAccount(cwRemoteAccountModel::Provider::GitHub, QStringLiteral("user1"));
        REQUIRE(model.rowCount() == 1);

        model.addOrUpdateAccount(cwRemoteAccountModel::Provider::GitHub, QStringLiteral("user1"));
        REQUIRE(model.rowCount() == 1);

        model.addOrUpdateAccount(cwRemoteAccountModel::Provider::GitLab, QStringLiteral("user2"));
        REQUIRE(model.rowCount() == 2);

        auto idx = model.index(0, 0);
        CHECK(model.data(idx, cwRemoteAccountModel::LabelRole).toString() == "GitHub (user1)");

        model.removeAccount(cwRemoteAccountModel::Provider::GitHub, QStringLiteral("user1"));
        REQUIRE(model.rowCount() == 1);
    }

    // Ensure entries persist across instances
    {
        cwRemoteAccountModel model;
        REQUIRE(model.rowCount() == 1);
        auto idx = model.index(0, 0);
        CHECK(model.data(idx, cwRemoteAccountModel::ProviderRole).toInt() == static_cast<int>(cwRemoteAccountModel::Provider::GitLab));
        CHECK(model.data(idx, cwRemoteAccountModel::UsernameRole).toString() == "user2");

        model.clear();
        REQUIRE(model.rowCount() == 0);
    }
}
