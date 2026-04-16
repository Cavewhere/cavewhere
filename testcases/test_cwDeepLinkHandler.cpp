/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#include "cwDeepLinkHandler.h"

#include <catch2/catch_test_macros.hpp>
#include <QSignalSpy>

namespace {

struct DeepLinkFixture {
    cwDeepLinkHandler handler;
    QSignalSpy openSpy{&handler, &cwDeepLinkHandler::openRepoRequested};
    QSignalSpy invalidSpy{&handler, &cwDeepLinkHandler::invalidLink};
};

}

TEST_CASE("cwDeepLinkHandler valid links emit openRepoRequested", "[DeepLink]")
{
    SECTION("Valid github.com link emits openRepoRequested")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://github.com/user/repo"));
        REQUIRE(f.openSpy.count() == 1);
        CHECK(f.invalidSpy.count() == 0);
        CHECK(f.openSpy.first().first().toUrl() == QUrl("https://github.com/user/repo"));
    }

    SECTION("Valid gitlab.com link emits openRepoRequested")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://gitlab.com/user/repo"));
        REQUIRE(f.openSpy.count() == 1);
        CHECK(f.invalidSpy.count() == 0);
    }

    SECTION("Valid bitbucket.org link emits openRepoRequested")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://bitbucket.org/user/repo"));
        REQUIRE(f.openSpy.count() == 1);
        CHECK(f.invalidSpy.count() == 0);
    }
}

TEST_CASE("cwDeepLinkHandler invalid links emit invalidLink", "[DeepLink]")
{
    SECTION("Disallowed host emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://evil.com/bad/repo"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("http:// repo URL rejected (HTTPS only)")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=http://github.com/user/repo"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("Missing repo param emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("Malformed URL emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=not-a-url"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("IP address as host rejected")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://192.168.1.1/user/repo"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("Path traversal in repo URL rejected")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://github.com/../etc/passwd"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }
}

TEST_CASE("cwDeepLinkHandler takePendingUrl", "[DeepLink]")
{
    SECTION("takePendingUrl() returns the validated repo URL")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://github.com/user/repo"));
        CHECK(f.handler.takePendingUrl() == QUrl("https://github.com/user/repo"));
    }

    SECTION("takePendingUrl() clears the pending URL")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://github.com/user/repo"));
        f.handler.takePendingUrl();
        CHECK(f.handler.takePendingUrl().isEmpty());
    }

    SECTION("takePendingUrl() returns empty when no URL was handled")
    {
        DeepLinkFixture f;
        CHECK(f.handler.takePendingUrl().isEmpty());
    }

    SECTION("Second handleUrl() replaces first pending URL")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://github.com/user/first"));
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://github.com/user/second"));
        CHECK(f.handler.takePendingUrl() == QUrl("https://github.com/user/second"));
    }

    SECTION("Invalid URL does not set a pending URL")
    {
        DeepLinkFixture f;
        f.handler.handleUrl(QUrl("cavewhere://open?repo=https://evil.com/bad"));
        CHECK(f.handler.takePendingUrl().isEmpty());
    }
}

TEST_CASE("cwDeepLinkHandler handleShareLink valid links", "[DeepLink]")
{
    SECTION("https://cavewhere.com/open share link emits openRepoRequested")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://cavewhere.com/open?repo=https%3A//github.com/user/repo"));
        REQUIRE(f.openSpy.count() == 1);
        CHECK(f.invalidSpy.count() == 0);
        CHECK(f.openSpy.first().first().toUrl() == QUrl("https://github.com/user/repo"));
    }

    SECTION("cavewhere:// scheme delegates to handleUrl")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("cavewhere://open?repo=https://github.com/user/repo"));
        REQUIRE(f.openSpy.count() == 1);
        CHECK(f.invalidSpy.count() == 0);
        CHECK(f.openSpy.first().first().toUrl() == QUrl("https://github.com/user/repo"));
    }

    SECTION("https://cavewhere.com/open/ with trailing slash works")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://cavewhere.com/open/?repo=https%3A//github.com/user/repo"));
        REQUIRE(f.openSpy.count() == 1);
        CHECK(f.invalidSpy.count() == 0);
        CHECK(f.openSpy.first().first().toUrl() == QUrl("https://github.com/user/repo"));
    }

    SECTION("Percent-encoded repo param decoded correctly")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://cavewhere.com/open?repo=https%3A%2F%2Fgitlab.com%2Forg%2Fproject"));
        REQUIRE(f.openSpy.count() == 1);
        CHECK(f.openSpy.first().first().toUrl() == QUrl("https://gitlab.com/org/project"));
    }

    SECTION("Sets pending URL on success")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://cavewhere.com/open?repo=https%3A//github.com/user/repo"));
        CHECK(f.handler.takePendingUrl() == QUrl("https://github.com/user/repo"));
    }
}

TEST_CASE("cwDeepLinkHandler handleShareLink invalid links", "[DeepLink]")
{
    SECTION("Non-cavewhere.com HTTPS URL emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://example.com/open?repo=https://github.com/user/repo"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("HTTP cavewhere.com URL emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("http://cavewhere.com/open?repo=https://github.com/user/repo"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("Wrong path emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://cavewhere.com/notopen?repo=https://github.com/user/repo"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("Missing repo param emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://cavewhere.com/open"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("Disallowed repo host emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("https://cavewhere.com/open?repo=https%3A//evil.com/bad/repo"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }

    SECTION("Bogus URL emits invalidLink")
    {
        DeepLinkFixture f;
        f.handler.handleShareLink(QUrl("not-a-url-at-all"));
        CHECK(f.openSpy.count() == 0);
        REQUIRE(f.invalidSpy.count() == 1);
    }
}

TEST_CASE("cwDeepLinkHandler collaboratorSettingsUrl", "[DeepLink]")
{
    SECTION("GitHub repo returns /settings/access path")
    {
        const QUrl result = cwDeepLinkHandler::collaboratorSettingsUrl(
            QUrl("https://github.com/User/Repo.git"));
        CHECK(result.toString().toStdString() == "https://github.com/User/Repo/settings/access");
    }

    SECTION("GitLab repo returns /-/project_members path")
    {
        const QUrl result = cwDeepLinkHandler::collaboratorSettingsUrl(
            QUrl("https://gitlab.com/org/project.git"));
        CHECK(result.toString().toStdString() == "https://gitlab.com/org/project/-/project_members");
    }

    SECTION("Bitbucket repo returns /admin/access-keys path")
    {
        const QUrl result = cwDeepLinkHandler::collaboratorSettingsUrl(
            QUrl("https://bitbucket.org/team/repo.git"));
        CHECK(result.toString().toStdString() == "https://bitbucket.org/team/repo/admin/access-keys");
    }

    SECTION("URL without .git suffix works")
    {
        const QUrl result = cwDeepLinkHandler::collaboratorSettingsUrl(
            QUrl("https://github.com/User/Repo"));
        CHECK(result.toString().toStdString() == "https://github.com/User/Repo/settings/access");
    }

    SECTION("Unsupported host returns empty")
    {
        CHECK(cwDeepLinkHandler::collaboratorSettingsUrl(
            QUrl("https://example.com/user/repo")).isEmpty());
    }

    SECTION("Empty URL returns empty")
    {
        CHECK(cwDeepLinkHandler::collaboratorSettingsUrl(QUrl()).isEmpty());
    }
}
