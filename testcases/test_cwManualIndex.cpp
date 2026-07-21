//Our includes
#include "cwManualIndex.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QString>
#include <QVariantMap>

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
        //The one-line summary is captured from llms.txt (the text after "):"),
        //which feeds the search haystack.
        CHECK(scraps->summary.contains(QStringLiteral("carpet")));
        CHECK(!scraps->summary.endsWith(QLatin1Char(' ')));
    }

    SECTION("searchText fuses title, summary and front-matter keywords") {
        //"carpet" is in the summary/title; "triangulation" is only in the
        //article's front-matter keywords — proving keywords are folded in.
        const QString haystack = index.searchText(QStringLiteral("scraps-carpeting"));
        REQUIRE(!haystack.isEmpty());
        CHECK(haystack == haystack.toLower());
        CHECK(haystack.contains(QStringLiteral("scraps and carpeting")));
        CHECK(haystack.contains(QStringLiteral("triangulation")));
        //An unknown slug has no haystack.
        CHECK(index.searchText(QStringLiteral("no-such-page")).isEmpty());
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
        //An empty slug is the landing page, so the viewer can bind body(slug)
        //unconditionally instead of branching on the empty case.
        CHECK(index.body(QString()) == landing);
    }

    SECTION("an unknown slug returns empty without crashing") {
        CHECK(index.body(QStringLiteral("no-such-page")).isEmpty());
    }

    SECTION("resolveLink classifies every way a link can be followed") {
        //An in-app article carries its slug. A cross-reference from one article to
        //a sibling directory resolves against the source page's location
        //(settings/ -> scraps/).
        const QVariantMap article = index.resolveLink(QStringLiteral("settings-change-settings"),
                                                      QStringLiteral("../scraps/carpeting.md"));
        CHECK(article.value(QStringLiteral("kind")).toString() == QStringLiteral("article"));
        CHECK(article.value(QStringLiteral("slug")).toString() == QStringLiteral("scraps-carpeting"));

        //The landing page (empty slug) resolves links against the manual root.
        CHECK(index.resolveLink(QString(), QStringLiteral("scraps/carpeting.md"))
              .value(QStringLiteral("slug")).toString() == QStringLiteral("scraps-carpeting"));

        //A trailing #anchor on an article link is dropped: it still resolves to
        //the target article, not treated as a same-page anchor.
        const QVariantMap articleWithAnchor =
                index.resolveLink(QStringLiteral("settings-change-settings"),
                                  QStringLiteral("../scraps/carpeting.md#warping"));
        CHECK(articleWithAnchor.value(QStringLiteral("kind")).toString() == QStringLiteral("article"));
        CHECK(articleWithAnchor.value(QStringLiteral("slug")).toString() == QStringLiteral("scraps-carpeting"));

        //A same-page anchor is distinct from external, and preserves the fragment
        //so the viewer can scroll to the heading.
        const QVariantMap anchor = index.resolveLink(QStringLiteral("scraps-carpeting"),
                                                     QStringLiteral("#warping"));
        CHECK(anchor.value(QStringLiteral("kind")).toString() == QStringLiteral("anchor"));
        CHECK(anchor.value(QStringLiteral("anchor")).toString() == QStringLiteral("#warping"));

        //Scheme links leave the app; so does an empty href.
        CHECK(index.resolveLink(QString(), QStringLiteral("https://cavewhere.com"))
              .value(QStringLiteral("kind")).toString() == QStringLiteral("external"));
        CHECK(index.resolveLink(QString(), QStringLiteral("mailto:a@b.com"))
              .value(QStringLiteral("kind")).toString() == QStringLiteral("external"));
        CHECK(index.resolveLink(QString(), QString())
              .value(QStringLiteral("kind")).toString() == QStringLiteral("external"));

        //A link resolving inside the manual but not to a registered article is
        //"missing" — not external — so the viewer ignores it instead of handing a
        //bare relative path to the browser.
        CHECK(index.resolveLink(QString(), QStringLiteral("does/not-exist.md"))
              .value(QStringLiteral("kind")).toString() == QStringLiteral("missing"));
        //index.md resolves inside the manual but is the landing, not an article.
        CHECK(index.resolveLink(QString(), QStringLiteral("index.md"))
              .value(QStringLiteral("kind")).toString() == QStringLiteral("missing"));
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
