//Our includes
#include "cwManualIndex.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QString>

namespace {
    const cwManualIndex::Article* findBySlug(const cwManualIndex& index, const QString& slug)
    {
        for (const cwManualIndex::Article& article : index.articleList()) {
            if (article.slug == slug) {
                return &article;
            }
        }
        return nullptr;
    }
}

TEST_CASE("cwManualIndex parses the embedded manual", "[cwManualIndex]") {
    cwManualIndex index;

    SECTION("llms.txt yields the article list") {
        //If this is zero the manual resource almost certainly wasn't compiled in.
        REQUIRE(index.articleList().size() > 10);
        CHECK(index.articles().size() == index.articleList().size());
    }

    SECTION("a known article resolves slug, title, chapter and path") {
        const cwManualIndex::Article* scraps =
                findBySlug(index, QStringLiteral("scraps-carpeting"));
        REQUIRE(scraps != nullptr);
        CHECK(scraps->title == QStringLiteral("Scraps and Carpeting"));
        CHECK(scraps->chapter == QStringLiteral("Scraps and Carpeting"));
        CHECK(scraps->relativePath == QStringLiteral("scraps/carpeting.md"));
    }

    SECTION("root-level entries (index.md, AUTHORING.md) are not articles") {
        CHECK(findBySlug(index, QStringLiteral("index")) == nullptr);
        CHECK(findBySlug(index, QStringLiteral("authoring")) == nullptr);
    }

    SECTION("body is front-matter stripped with image links rewritten to qrc:") {
        const QString body = index.body(QStringLiteral("scraps-carpeting"));
        REQUIRE(!body.isEmpty());
        CHECK(!body.trimmed().startsWith(QStringLiteral("---")));
        CHECK(body.trimmed().startsWith(QStringLiteral("# ")));
        //Relative image links must be resolved to absolute resource URLs so Qt's
        //Text (which has no Markdown base URL) can find them.
        CHECK(!body.contains(QStringLiteral("(../images/")));
        CHECK(body.contains(QStringLiteral("qrc:/manual/images/")));
    }

    SECTION("landing body renders the manual index") {
        const QString landing = index.landingBody();
        REQUIRE(!landing.isEmpty());
        CHECK(landing.contains(QStringLiteral("CaveWhere User Manual")));
    }

    SECTION("an unknown slug returns empty without crashing") {
        CHECK(index.body(QStringLiteral("no-such-page")).isEmpty());
        CHECK(index.title(QStringLiteral("no-such-page")).isEmpty());
    }
}

TEST_CASE("cwManualIndex path helpers mirror the website build", "[cwManualIndex]") {
    SECTION("slug scheme") {
        CHECK(cwManualIndex::slugForPath(QStringLiteral("concepts/why-cavewhere.md"))
              == QStringLiteral("concepts-why-cavewhere"));
        CHECK(cwManualIndex::slugForPath(QStringLiteral("index.md"))
              == QStringLiteral("index"));
    }

    SECTION("chapter grouping") {
        CHECK(cwManualIndex::chapterForPath(QStringLiteral("getting-started/install-cavewhere.md"))
              == QStringLiteral("Getting Started"));
        //import-export/ splits into Import and Export by filename.
        CHECK(cwManualIndex::chapterForPath(QStringLiteral("import-export/export-a-map.md"))
              == QStringLiteral("Export"));
        //A directory not in the table falls back to a title-cased name.
        CHECK(cwManualIndex::chapterForPath(QStringLiteral("point-clouds/add-a-point-cloud.md"))
              == QStringLiteral("Point Clouds"));
    }
}
