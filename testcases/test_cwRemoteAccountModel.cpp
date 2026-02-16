#include <catch2/catch_test_macros.hpp>

#include <QSettings>
#include <QDateTime>

#include "cwRemoteAccountModel.h"

TEST_CASE("cwRemoteAccountModel add/remove/persist", "[cwRemoteAccountModel]")
{
    {
        QSettings settings;
        settings.remove(QStringLiteral("RemoteAccounts"));
    }

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

TEST_CASE("cwRemoteAccountModel persists stateful account fields", "[cwRemoteAccountModel]")
{
    {
        QSettings settings;
        settings.remove(QStringLiteral("RemoteAccounts"));
    }

    QString githubAccountId;
    QString gitlabAccountId;
    const QDateTime expectedLastUsedAt = QDateTime::fromString(
        QStringLiteral("2026-02-15T12:30:00Z"),
        Qt::ISODate);

    {
        cwRemoteAccountModel model;
        githubAccountId = model.upsertAccount(cwRemoteAccountModel::Provider::GitHub, QStringLiteral("octocat"));
        gitlabAccountId = model.upsertAccount(cwRemoteAccountModel::Provider::GitLab, QStringLiteral("gitlab-user"));

        REQUIRE(!githubAccountId.isEmpty());
        REQUIRE(!gitlabAccountId.isEmpty());
        REQUIRE(githubAccountId != gitlabAccountId);

        model.setAuthState(githubAccountId, cwRemoteAccountModel::AuthState::Authorized);
        model.setLastUsedAt(githubAccountId, expectedLastUsedAt);
        model.setActiveAccount(cwRemoteAccountModel::Provider::GitHub, githubAccountId);
        model.setActiveAccount(cwRemoteAccountModel::Provider::GitLab, gitlabAccountId);
    }

    {
        cwRemoteAccountModel reloadedModel;
        REQUIRE(reloadedModel.rowCount() == 2);

        const auto github = reloadedModel.accountById(githubAccountId);
        CHECK(github.accountId == githubAccountId);
        CHECK(github.provider == static_cast<int>(cwRemoteAccountModel::Provider::GitHub));
        CHECK(github.username == QStringLiteral("octocat"));
        CHECK(github.authState == static_cast<int>(cwRemoteAccountModel::AuthState::Authorized));
        CHECK(github.active);
        CHECK(github.lastUsedAt == expectedLastUsedAt);

        const auto gitlab = reloadedModel.accountById(gitlabAccountId);
        CHECK(gitlab.accountId == gitlabAccountId);
        CHECK(gitlab.provider == static_cast<int>(cwRemoteAccountModel::Provider::GitLab));
        CHECK(gitlab.username == QStringLiteral("gitlab-user"));
        CHECK(gitlab.active);

        CHECK(reloadedModel.activeAccountId(cwRemoteAccountModel::Provider::GitHub) == githubAccountId);
        CHECK(reloadedModel.activeAccountId(cwRemoteAccountModel::Provider::GitLab) == gitlabAccountId);

        reloadedModel.clear();
    }
}
