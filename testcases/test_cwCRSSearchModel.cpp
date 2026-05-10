/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include <catch2/catch_test_macros.hpp>

#include "cwCRSSearchModel.h"
#include "cwCoordinateTransform.h"

#include <QSignalSpy>

namespace {
    bool modelContainsCode(const cwCRSSearchModel& m, const QString& code)
    {
        for (int i = 0; i < m.rowCount(); ++i) {
            const QModelIndex idx = m.index(i, 0);
            if (m.data(idx, cwCRSSearchModel::CodeRole).toString() == code) {
                return true;
            }
        }
        return false;
    }
}

TEST_CASE("cwCRSSearchModel exposes the curated common list when query is empty",
          "[cwCRSSearchModel]")
{
    cwCRSSearchModel model;
    REQUIRE(model.query().isEmpty());

    const int rows = model.rowCount();
    // Every code in commonProjectedCSList() should be present in the empty-
    // query view (proj.db has all of them as EPSG entries).
    const QStringList common = cwCoordinateTransform::commonProjectedCSList();
    REQUIRE(rows > 0);
    REQUIRE(rows <= common.size());

    for (const QString& cs : common) {
        const int colon = cs.indexOf(':');
        REQUIRE(colon > 0);
        const QString code = cs.mid(colon + 1);
        INFO("Looking up " << cs.toStdString() << " (code " << code.toStdString() << ")");
        CHECK(modelContainsCode(model, code));
    }
}

TEST_CASE("cwCRSSearchModel filters by name substring (case-insensitive)",
          "[cwCRSSearchModel]")
{
    cwCRSSearchModel model;

    QSignalSpy spy(&model, &cwCRSSearchModel::queryChanged);
    model.setQuery("British");
    CHECK(spy.count() == 1);

    // EPSG:27700 = "OSGB36 / British National Grid" — must match.
    CHECK(modelContainsCode(model, "27700"));
    // A WGS84 UTM should NOT match "British".
    CHECK_FALSE(modelContainsCode(model, "32616"));

    // Setting the same query string is a no-op.
    model.setQuery("British");
    CHECK(spy.count() == 1);

    // Case-insensitive matching: a different-case query still finds OSGB.
    model.setQuery("BRITISH");
    CHECK(spy.count() == 2);
    CHECK(modelContainsCode(model, "27700"));
}

TEST_CASE("cwCRSSearchModel filters by EPSG code prefix",
          "[cwCRSSearchModel]")
{
    cwCRSSearchModel model;

    SECTION("Numeric prefix matches code") {
        model.setQuery("32616");
        CHECK(modelContainsCode(model, "32616"));
    }

    SECTION("EPSG: prefix is stripped") {
        model.setQuery("EPSG:27700");
        CHECK(modelContainsCode(model, "27700"));
    }

    SECTION("Short numeric prefix matches multiple codes") {
        model.setQuery("326");
        // All WGS84 UTM north zones share this prefix.
        CHECK(modelContainsCode(model, "32601"));
        CHECK(modelContainsCode(model, "32660"));
        // OSGB shouldn't.
        CHECK_FALSE(modelContainsCode(model, "27700"));
    }
}

TEST_CASE("cwCRSSearchModel role names cover the expected fields",
          "[cwCRSSearchModel]")
{
    cwCRSSearchModel model;
    const QHash<int, QByteArray> roles = model.roleNames();
    CHECK(roles.value(cwCRSSearchModel::AuthNameRole)    == "authName");
    CHECK(roles.value(cwCRSSearchModel::CodeRole)        == "code");
    CHECK(roles.value(cwCRSSearchModel::DisplayCodeRole) == "displayCode");
    CHECK(roles.value(cwCRSSearchModel::NameRole)        == "name");
    CHECK(roles.value(cwCRSSearchModel::IsProjectedRole) == "isProjected");
}
