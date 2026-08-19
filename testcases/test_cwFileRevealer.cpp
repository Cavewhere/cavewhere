#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QTemporaryDir>

#include "cwFileRevealer.h"

TEST_CASE("cwFileRevealer resolves an entry file against its attachment dir",
          "[ExternalFileReveal]")
{
    QTemporaryDir attachmentDir;
    REQUIRE(attachmentDir.isValid());
    const QString dir = attachmentDir.path();

    SECTION("an entry file in the attachment dir") {
        CHECK(cwFileRevealer::resolvedPath(dir, QStringLiteral("survex_simple.svx"))
              == dir + QStringLiteral("/survex_simple.svx"));
    }

    SECTION("an entry file in a subdirectory of the attachment dir") {
        CHECK(cwFileRevealer::resolvedPath(dir, QStringLiteral("data/caves/deep.svx"))
              == dir + QStringLiteral("/data/caves/deep.svx"));
    }

    SECTION("a trailing separator on the attachment dir") {
        CHECK(cwFileRevealer::resolvedPath(dir + QStringLiteral("/"),
                                           QStringLiteral("survex_simple.svx"))
              == dir + QStringLiteral("/survex_simple.svx"));
    }

    SECTION("dot segments in the entry file are cleaned away") {
        CHECK(cwFileRevealer::resolvedPath(dir, QStringLiteral("./data/../top.svx"))
              == dir + QStringLiteral("/top.svx"));
    }

    SECTION("an already absolute entry file names itself") {
        const QString absolute = dir + QStringLiteral("/elsewhere.svx");
        CHECK(cwFileRevealer::resolvedPath(QString(), absolute) == absolute);
    }

    SECTION("nothing to resolve") {
        CHECK(cwFileRevealer::resolvedPath(dir, QString()).isEmpty());
        CHECK(cwFileRevealer::resolvedPath(QString(),
                                           QStringLiteral("survex_simple.svx")).isEmpty());
    }
}
