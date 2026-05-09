//Our includes
#include "cwFixStation.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QUuid>

TEST_CASE("cwFixStation defaults are zero/empty", "[FixStation][cwFixStation]") {
    cwFixStation fix;
    CHECK(fix.stationName().isEmpty());
    CHECK(fix.inputCS().isEmpty());
    CHECK(fix.easting() == 0.0);
    CHECK(fix.northing() == 0.0);
    CHECK(fix.elevation() == 0.0);
    CHECK(fix.horizontalVariance() == 0.0);
    CHECK(fix.verticalVariance() == 0.0);
    CHECK(!fix.id().isNull());
}

TEST_CASE("cwFixStation setters round-trip", "[FixStation][cwFixStation]") {
    cwFixStation fix;
    const QUuid id = QUuid::createUuid();
    fix.setId(id);
    fix.setStationName(QStringLiteral("A1"));
    fix.setInputCS(QStringLiteral("EPSG:32612"));
    fix.setEasting(500123.456);
    fix.setNorthing(4194567.89);
    fix.setElevation(2750.5);
    fix.setHorizontalVariance(0.5);
    fix.setVerticalVariance(1.0);

    CHECK(fix.id() == id);
    CHECK(fix.stationName() == QStringLiteral("A1"));
    CHECK(fix.inputCS() == QStringLiteral("EPSG:32612"));
    CHECK(fix.easting() == 500123.456);
    CHECK(fix.northing() == 4194567.89);
    CHECK(fix.elevation() == 2750.5);
    CHECK(fix.horizontalVariance() == 0.5);
    CHECK(fix.verticalVariance() == 1.0);
}

TEST_CASE("cwFixStation copy semantics: COW", "[FixStation][cwFixStation]") {
    cwFixStation a;
    a.setStationName(QStringLiteral("A1"));
    a.setEasting(123.0);

    cwFixStation b = a;
    CHECK(b.stationName() == QStringLiteral("A1"));
    CHECK(b.easting() == 123.0);
    CHECK(a == b);

    b.setEasting(999.0);
    CHECK(a.easting() == 123.0);
    CHECK(b.easting() == 999.0);
    CHECK(a != b);
}

TEST_CASE("cwFixStation equality compares all fields", "[FixStation][cwFixStation]") {
    cwFixStation a;
    cwFixStation b;
    b.setId(a.id());
    CHECK(a == b);

    b.setNorthing(1.0);
    CHECK(a != b);
}
