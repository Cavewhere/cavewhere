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
