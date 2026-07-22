//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwCavernNaming.h"
#include "cwExternalCenterline.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"
#include "cwStationPositionLookup.h"

//Qt inculdes
#include "cwSignalSpy.h"
#include <QVector3D>

TEST_CASE("cwTrip should strip the datestamp away from date", "[cwTrip]") {
    cwTrip trip;

    cwSignalSpy spy(&trip, &cwTrip::dateChanged);

    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate());

    trip.setDate(QDateTime::currentDateTime().addDays(-1));
    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate().addDays(-1));

    CHECK(spy.count() == 1);
}

TEST_CASE("cwTrip::scopePrefix unifies the three trip kinds", "[cwTrip][scope]")
{
    cwTrip trip;

    SECTION("a flat native trip is unscoped") {
        CHECK(trip.scopePrefix().isEmpty());
        CHECK_FALSE(trip.isScoped());
    }

    SECTION("an externally-attached trip scopes to trip_<hex>.") {
        trip.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
        CHECK(trip.scopePrefix() == cwCavernNaming::tripScopePrefix(trip.id()));
        CHECK(trip.isScoped());
    }

    SECTION("a native-prefixed trip scopes to <stationPrefix>.") {
        trip.setStationPrefix(QStringLiteral("A"));
        CHECK(trip.scopePrefix() == QStringLiteral("A."));
        CHECK(trip.isScoped());
    }

    SECTION("external centerline wins over a station prefix") {
        trip.setStationPrefix(QStringLiteral("A"));
        trip.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
        CHECK(trip.scopePrefix() == cwCavernNaming::tripScopePrefix(trip.id()));
    }
}

TEST_CASE("cwTrip scope changes emit stationPrefixChanged and scopeChanged", "[cwTrip][scope]")
{
    cwTrip trip;
    cwSignalSpy prefixSpy(&trip, &cwTrip::stationPrefixChanged);
    cwSignalSpy scopeSpy(&trip, &cwTrip::scopeChanged);

    trip.setStationPrefix(QStringLiteral("A"));
    CHECK(prefixSpy.count() == 1);
    CHECK(scopeSpy.count() == 1);

    // Setting the same prefix is a no-op.
    trip.setStationPrefix(QStringLiteral("A"));
    CHECK(prefixSpy.count() == 1);
    CHECK(scopeSpy.count() == 1);

    // The external centerline also moves the scope, so it fires scopeChanged.
    trip.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
    CHECK(prefixSpy.count() == 1);
    CHECK(scopeSpy.count() == 2);
}

TEST_CASE("cwTrip solved-* accessors strip a native station prefix", "[cwTrip][scope]")
{
    // Seed a cave lookup as the solver would leave it for a native-prefixed
    // (Scope) trip: the trip's stations keyed <prefix>.<tail>, plus an unrelated
    // native station. The accessors must strip the prefix so bare note/scrap
    // names resolve, while keeping the full cave data as a superset.
    cwCave cave;
    cwTrip* trip = new cwTrip();
    trip->setStationPrefix(QStringLiteral("A"));
    cave.addTrip(trip);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("A.s1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("outside"), QVector3D(7, 8, 9));
    cave.setStationPositionLookup(lookup);

    SECTION("solvedStationPositions aliases each scoped tail to a bare name") {
        const cwStationPositionLookup solved = trip->solvedStationPositions();
        REQUIRE(solved.hasPosition(QStringLiteral("s1")));
        REQUIRE(solved.hasPosition(QStringLiteral("s2")));
        CHECK(solved.position(QStringLiteral("s1")) == QVector3D(1, 2, 3));
        CHECK(solved.position(QStringLiteral("s2")) == QVector3D(4, 5, 6));
        // The full cave data is retained as a superset for cross-trip tie-ins.
        CHECK(solved.hasPosition(QStringLiteral("A.s1")));
        CHECK(solved.hasPosition(QStringLiteral("outside")));
    }

    SECTION("solvedStations enumerates only this trip's scope-relative tails") {
        const QList<QPair<QString, QVector3D>> stations = trip->solvedStations();
        REQUIRE(stations.size() == 2);
        QMap<QString, QVector3D> byName;
        for (const auto& pair : stations) {
            byName.insert(pair.first, pair.second);
        }
        CHECK(byName.value(QStringLiteral("s1")) == QVector3D(1, 2, 3));
        CHECK(byName.value(QStringLiteral("s2")) == QVector3D(4, 5, 6));
        CHECK_FALSE(byName.contains(QStringLiteral("outside")));
        CHECK_FALSE(byName.contains(QStringLiteral("A.s1")));
    }
}

TEST_CASE("cwTrip::linePlotKeywordModel carries Type=Line Plot and extends the trip model",
          "[cwTrip][keyword]")
{
    cwTrip trip;
    trip.setName(QStringLiteral("Trip A"));

    cwKeywordModel* linePlotModel = trip.linePlotKeywordModel();
    REQUIRE(linePlotModel != nullptr);

    SECTION("lazily created and stable across calls") {
        CHECK(trip.linePlotKeywordModel() == linePlotModel);
    }

    SECTION("aggregates Type=Line Plot and the trip's keywords via the extension") {
        // keywords() walks extensions, so the line plot model reports both its
        // own Type=Line Plot and the trip's keywords (Trip name, ...).
        const auto all = linePlotModel->keywords();
        CHECK(all.contains(cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("Line Plot"))));
        CHECK(all.contains(cwKeyword(cwKeywordModel::TripNameKey, QStringLiteral("Trip A"))));
    }

    SECTION("the Type identity stays off the trip's own model") {
        // Scraps/notes/leads extend trip->keywordModel(); putting Type=Line Plot
        // there would make them wrongly inherit "Line Plot".
        CHECK_FALSE(trip.keywordModel()->keywords().contains(
            cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("Line Plot"))));
    }
}

