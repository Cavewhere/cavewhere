/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGitHostingProvider.h"

#include <catch2/catch_test_macros.hpp>

#include <QUrl>

TEST_CASE("cwGitHostingProvider::forHost looks up by canonical host", "[GitHostingProvider]")
{
    SECTION("Case-insensitive match against known hosts") {
        CHECK(cwGitHostingProvider::forHost(QStringLiteral("github.com")).displayName
              == QStringLiteral("GitHub"));
        CHECK(cwGitHostingProvider::forHost(QStringLiteral("GITHUB.COM")).displayName
              == QStringLiteral("GitHub"));
        CHECK(cwGitHostingProvider::forHost(QStringLiteral("GitHub.Com")).displayName
              == QStringLiteral("GitHub"));

        CHECK(cwGitHostingProvider::forHost(QStringLiteral("gitlab.com")).displayName
              == QStringLiteral("GitLab"));
        CHECK(cwGitHostingProvider::forHost(QStringLiteral("bitbucket.org")).displayName
              == QStringLiteral("Bitbucket"));
    }

    SECTION("Unknown host returns generic entry") {
        const auto& info = cwGitHostingProvider::forHost(QStringLiteral("unknown.example"));
        CHECK(info.host.isEmpty());
        CHECK(info.displayName.isEmpty());
        CHECK(info.collaboratorPathSuffix.isEmpty());
        CHECK(info.invitationsUrl.isEmpty());
        CHECK(info.invitationsLinkLabel.isEmpty());
        CHECK_FALSE(info.authMessage.isEmpty());
        CHECK_FALSE(info.notFoundMessage.isEmpty());
    }

    SECTION("Empty host returns generic entry") {
        const auto& info = cwGitHostingProvider::forHost(QString());
        CHECK(info.host.isEmpty());
        CHECK(info.displayName.isEmpty());
    }
}

TEST_CASE("cwGitHostingProvider::forUrl delegates to forHost via QUrl::host", "[GitHostingProvider]")
{
    CHECK(cwGitHostingProvider::forUrl(QUrl(QStringLiteral("https://github.com/owner/repo"))).displayName
          == QStringLiteral("GitHub"));
    CHECK(cwGitHostingProvider::forUrl(QUrl(QStringLiteral("HTTPS://GITHUB.COM/owner/repo"))).displayName
          == QStringLiteral("GitHub"));
    CHECK(cwGitHostingProvider::forUrl(QUrl(QStringLiteral("ssh://git@github.com/owner/repo.git"))).displayName
          == QStringLiteral("GitHub"));
    CHECK(cwGitHostingProvider::forUrl(QUrl(QStringLiteral("https://codeberg.org/owner/repo"))).displayName
          .isEmpty());
}

TEST_CASE("cwGitHostingProvider::knownHosts lists every non-generic host", "[GitHostingProvider]")
{
    const QStringList hosts = cwGitHostingProvider::knownHosts();
    CHECK(hosts.contains(QStringLiteral("github.com")));
    CHECK(hosts.contains(QStringLiteral("gitlab.com")));
    CHECK(hosts.contains(QStringLiteral("bitbucket.org")));
    // No empty entries from the generic fallback.
    for (const auto& host : hosts) {
        CHECK_FALSE(host.isEmpty());
    }
}

TEST_CASE("cwGitHostingProvider::repositoryWebUrl normalizes clone URLs", "[GitHostingProvider]")
{
    const auto& github = cwGitHostingProvider::forHost(QStringLiteral("github.com"));

    SECTION("Strips trailing .git from https URL") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            github, QUrl(QStringLiteral("https://github.com/owner/repo.git")));
        CHECK(web.toString() == QStringLiteral("https://github.com/owner/repo"));
    }

    SECTION("Converts ssh URL to https web URL") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            github, QUrl(QStringLiteral("ssh://git@github.com/owner/repo.git")));
        CHECK(web.toString() == QStringLiteral("https://github.com/owner/repo"));
    }

    SECTION("Leaves an https URL without .git unchanged") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            github, QUrl(QStringLiteral("https://github.com/owner/repo")));
        CHECK(web.toString() == QStringLiteral("https://github.com/owner/repo"));
    }

    SECTION("Strips trailing slashes") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            github, QUrl(QStringLiteral("https://github.com/owner/repo/")));
        CHECK(web.toString() == QStringLiteral("https://github.com/owner/repo"));
    }

    SECTION("Defensive: host mismatch returns empty") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            github, QUrl(QStringLiteral("https://gitlab.com/owner/repo")));
        CHECK(web.isEmpty());
    }

    SECTION("Empty path returns empty") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            github, QUrl(QStringLiteral("https://github.com")));
        CHECK(web.isEmpty());
    }
}

TEST_CASE("cwGitHostingProvider::repositoryWebUrl with the generic provider", "[GitHostingProvider]")
{
    const auto& generic = cwGitHostingProvider::forHost(QStringLiteral("codeberg.org"));
    REQUIRE(generic.host.isEmpty());

    SECTION("Builds an https URL when the clone URL is https") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            generic, QUrl(QStringLiteral("https://codeberg.org/owner/repo.git")));
        CHECK(web.toString() == QStringLiteral("https://codeberg.org/owner/repo"));
    }

    SECTION("Returns empty for ssh inputs (cannot infer https host safely)") {
        const QUrl web = cwGitHostingProvider::repositoryWebUrl(
            generic, QUrl(QStringLiteral("ssh://git@codeberg.org/owner/repo.git")));
        CHECK(web.isEmpty());
    }
}

TEST_CASE("cwGitHostingProvider::collaboratorSettingsUrl appends the right suffix", "[GitHostingProvider]")
{
    const auto& github = cwGitHostingProvider::forHost(QStringLiteral("github.com"));
    const auto& gitlab = cwGitHostingProvider::forHost(QStringLiteral("gitlab.com"));
    const auto& bitbucket = cwGitHostingProvider::forHost(QStringLiteral("bitbucket.org"));
    const auto& generic = cwGitHostingProvider::forHost(QStringLiteral("unknown.example"));

    CHECK(cwGitHostingProvider::collaboratorSettingsUrl(
              github, QUrl(QStringLiteral("https://github.com/owner/repo.git"))).toString()
          == QStringLiteral("https://github.com/owner/repo/settings/access"));

    CHECK(cwGitHostingProvider::collaboratorSettingsUrl(
              gitlab, QUrl(QStringLiteral("https://gitlab.com/owner/repo.git"))).toString()
          == QStringLiteral("https://gitlab.com/owner/repo/-/project_members"));

    CHECK(cwGitHostingProvider::collaboratorSettingsUrl(
              bitbucket, QUrl(QStringLiteral("https://bitbucket.org/owner/repo.git"))).toString()
          == QStringLiteral("https://bitbucket.org/owner/repo/admin/access-keys"));

    // Generic provider has no settings path.
    CHECK(cwGitHostingProvider::collaboratorSettingsUrl(
              generic, QUrl(QStringLiteral("https://unknown.example/owner/repo"))).isEmpty());
}

TEST_CASE("cwGitHostingProvider entries use provider-native vocabulary", "[GitHostingProvider]")
{
    const auto& github = cwGitHostingProvider::forHost(QStringLiteral("github.com"));
    CHECK(github.authMessage.contains(QStringLiteral("GitHub")));
    CHECK(github.notFoundMessage.contains(QStringLiteral("GitHub")));
    CHECK(github.notFoundMessage.contains(QStringLiteral("collaborator")));

    const auto& gitlab = cwGitHostingProvider::forHost(QStringLiteral("gitlab.com"));
    CHECK(gitlab.authMessage.contains(QStringLiteral("GitLab")));
    CHECK(gitlab.notFoundMessage.contains(QStringLiteral("GitLab")));
    CHECK(gitlab.notFoundMessage.contains(QStringLiteral("project member")));

    const auto& bitbucket = cwGitHostingProvider::forHost(QStringLiteral("bitbucket.org"));
    CHECK(bitbucket.authMessage.contains(QStringLiteral("Bitbucket")));
    CHECK(bitbucket.notFoundMessage.contains(QStringLiteral("Bitbucket")));
    CHECK(bitbucket.notFoundMessage.contains(QStringLiteral("workspace")));
}

TEST_CASE("cwGitHostingProvider::notFoundOrAccessMessage composes a clickable suffix", "[GitHostingProvider]")
{
    const auto& github = cwGitHostingProvider::forHost(QStringLiteral("github.com"));

    SECTION("GitHub embeds the pending-invitations link, not the repo URL") {
        const QString msg = cwGitHostingProvider::notFoundOrAccessMessage(
            github, QUrl(QStringLiteral("https://github.com/owner/repo.git")));
        CHECK(msg.contains(github.notFoundMessage));
        CHECK(msg.contains(QStringLiteral(
            "<a href=\"https://github.com/notifications?query=reason%3Ainvitation\">")));
        CHECK(msg.contains(github.invitationsLinkLabel));
        // The repo URL must NOT appear — it 404s for the same reason the
        // clone failed, so linking to it would just send the user to another
        // 404 page.
        CHECK_FALSE(msg.contains(QStringLiteral("github.com/owner/repo<")));
        CHECK_FALSE(msg.contains(QStringLiteral("github.com/owner/repo\"")));
    }

    SECTION("GitLab omits the link (no clean invitations page)") {
        const auto& gitlab = cwGitHostingProvider::forHost(QStringLiteral("gitlab.com"));
        REQUIRE(gitlab.invitationsUrl.isEmpty());
        const QString msg = cwGitHostingProvider::notFoundOrAccessMessage(
            gitlab, QUrl(QStringLiteral("https://gitlab.com/owner/repo.git")));
        CHECK(msg == gitlab.notFoundMessage);
        CHECK_FALSE(msg.contains(QStringLiteral("<a href")));
    }

    SECTION("Bitbucket omits the link (no clean invitations page)") {
        const auto& bitbucket = cwGitHostingProvider::forHost(QStringLiteral("bitbucket.org"));
        REQUIRE(bitbucket.invitationsUrl.isEmpty());
        const QString msg = cwGitHostingProvider::notFoundOrAccessMessage(
            bitbucket, QUrl(QStringLiteral("https://bitbucket.org/team/repo.git")));
        CHECK(msg == bitbucket.notFoundMessage);
        CHECK_FALSE(msg.contains(QStringLiteral("<a href")));
    }

    SECTION("Generic provider has no link") {
        const auto& generic = cwGitHostingProvider::forHost(QStringLiteral("unknown.example"));
        const QString msg = cwGitHostingProvider::notFoundOrAccessMessage(
            generic, QUrl(QStringLiteral("https://unknown.example/owner/repo.git")));
        CHECK(msg == generic.notFoundMessage);
        CHECK_FALSE(msg.contains(QStringLiteral("<a href")));
    }
}
