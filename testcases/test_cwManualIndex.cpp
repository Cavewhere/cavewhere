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

    SECTION("slugForLink resolves relative .md links to article slugs") {
        //A cross-reference from one article to a sibling directory resolves
        //against the source page's location (settings/ -> scraps/).
        CHECK(index.slugForLink(QStringLiteral("settings-change-settings"),
                                QStringLiteral("../scraps/carpeting.md"))
              == QStringLiteral("scraps-carpeting"));

        //The landing page (empty slug) resolves links against the manual root.
        CHECK(index.slugForLink(QString(),
                                QStringLiteral("scraps/carpeting.md"))
              == QStringLiteral("scraps-carpeting"));

        //A trailing #anchor on an article link is dropped before slugging.
        CHECK(index.slugForLink(QStringLiteral("settings-change-settings"),
                                QStringLiteral("../scraps/carpeting.md#warping"))
              == QStringLiteral("scraps-carpeting"));
    }

    SECTION("slugForLink rejects non-article links") {
        //External links, same-page anchors, and empties are not in-app pages.
        CHECK(index.slugForLink(QString(), QStringLiteral("https://cavewhere.com")).isEmpty());
        CHECK(index.slugForLink(QString(), QStringLiteral("mailto:a@b.com")).isEmpty());
        CHECK(index.slugForLink(QStringLiteral("scraps-carpeting"),
                                QStringLiteral("#warping")).isEmpty());
        CHECK(index.slugForLink(QString(), QString()).isEmpty());
        //A .md path that is not a registered article resolves to nothing.
        CHECK(index.slugForLink(QString(), QStringLiteral("does/not-exist.md")).isEmpty());
        //index.md is the landing, excluded from the article list.
        CHECK(index.slugForLink(QString(), QStringLiteral("index.md")).isEmpty());
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
