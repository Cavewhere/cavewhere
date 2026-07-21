//Our includes
#include "cwManualSearchModel.h"
#include "cwManualIndex.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QString>
#include <QStringList>

namespace {
    //The slugs currently visible in the model, in row order.
    QStringList visibleSlugs(const cwManualSearchModel& model)
    {
        QStringList slugs;
        for (int row = 0; row < model.rowCount(); ++row) {
            slugs.append(model.data(model.index(row, 0),
                                    cwManualSearchModel::SlugRole).toString());
        }
        return slugs;
    }
}

TEST_CASE("cwManualSearchModel filters the manual", "[cwManualSearchModel]") {
    cwManualIndex index;
    REQUIRE(index.articleList().size() > 10);

    cwManualSearchModel model;

    SECTION("without an index the model is empty") {
        CHECK(model.rowCount() == 0);
    }

    model.setManualIndex(&index);

    SECTION("an empty query lists every article in index order") {
        REQUIRE(model.rowCount() == index.articleList().size());
        //Row order mirrors cwManualIndex, so the ListView's chapter sections
        //come out grouped without any sorting in QML.
        CHECK(model.data(model.index(0, 0), cwManualSearchModel::SlugRole).toString()
              == index.articleList().first().slug);
    }

    SECTION("a query narrows to matching articles and keeps them addressable") {
        model.setQuery(QStringLiteral("declination"));
        const QStringList slugs = visibleSlugs(model);
        REQUIRE(!slugs.isEmpty());
        CHECK(slugs.size() < index.articleList().size());
        CHECK(slugs.contains(QStringLiteral("survey-data-declination")));
        //Every surviving row exposes its display roles.
        const QModelIndex first = model.index(0, 0);
        CHECK(!model.data(first, cwManualSearchModel::TitleRole).toString().isEmpty());
        CHECK(!model.data(first, cwManualSearchModel::ChapterRole).toString().isEmpty());
    }

    SECTION("query matches keywords, not just the visible title") {
        //"triangulation" appears only in the scrap article's front-matter
        //keywords — a title-only filter would miss it.
        model.setQuery(QStringLiteral("triangulation"));
        CHECK(visibleSlugs(model).contains(QStringLiteral("scraps-carpeting")));
    }

    SECTION("multiple terms are AND-ed together") {
        model.setQuery(QStringLiteral("point cloud"));
        const QStringList both = visibleSlugs(model);
        REQUIRE(!both.isEmpty());
        for (const QString& slug : both) {
            const QString haystack = index.searchText(slug);
            CHECK(haystack.contains(QStringLiteral("point")));
            CHECK(haystack.contains(QStringLiteral("cloud")));
        }
    }

    SECTION("the query is case-insensitive") {
        model.setQuery(QStringLiteral("DECLINATION"));
        CHECK(visibleSlugs(model).contains(QStringLiteral("survey-data-declination")));
    }

    SECTION("a non-matching query yields no rows") {
        model.setQuery(QStringLiteral("zzzznotarealtopic"));
        CHECK(model.rowCount() == 0);
    }

    SECTION("clearing the query restores the full list") {
        model.setQuery(QStringLiteral("declination"));
        REQUIRE(model.rowCount() < index.articleList().size());
        model.setQuery(QString());
        CHECK(model.rowCount() == index.articleList().size());
    }

    SECTION("changing the query resets the model and reports the new count") {
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        QSignalSpy countSpy(&model, &cwManualSearchModel::countChanged);
        model.setQuery(QStringLiteral("declination"));
        CHECK(resetSpy.count() == 1);
        CHECK(countSpy.count() == 1);
    }
}
