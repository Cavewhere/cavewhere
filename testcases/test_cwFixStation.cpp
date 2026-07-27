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
    CHECK(fix.coordinateText().isEmpty());
    CHECK(fix.coordinateTextAxisOrder() == cwCoordinateText::EastingNorthing);
}

TEST_CASE("cwFixStation drops the typed coordinate when a component moves",
          "[FixStation][cwFixStation]") {
    //The invariant U14 rests on: the stored string is a claim about these three
    //numbers, so any write to one of them makes it false. Clearing it in the
    //setters rather than at the call sites is what makes the rule hold for the
    //importers, which write components directly, and for whatever writes them
    //next.
    const auto typed = [] {
        cwFixStation fix;
        fix.setEasting(1.0);
        fix.setNorthing(2.0);
        fix.setElevation(3.0);
        fix.setCoordinateText(QStringLiteral("1, 2, 3m"), cwCoordinateText::EastingNorthing);
        return fix;
    };

    SECTION("the easting") {
        cwFixStation fix = typed();
        fix.setEasting(9.0);
        CHECK(fix.coordinateText().isEmpty());
    }

    SECTION("the northing") {
        cwFixStation fix = typed();
        fix.setNorthing(9.0);
        CHECK(fix.coordinateText().isEmpty());
    }

    SECTION("the elevation") {
        cwFixStation fix = typed();
        fix.setElevation(9.0);
        CHECK(fix.coordinateText().isEmpty());
    }

    SECTION("but not the name, the CS or a variance — none of those is a component") {
        cwFixStation fix = typed();
        fix.setStationName(QStringLiteral("A1"));
        fix.setInputCS(QStringLiteral("EPSG:4326"));
        fix.setHorizontalVariance(0.5);
        fix.setVerticalVariance(1.0);
        CHECK(fix.coordinateText() == QStringLiteral("1, 2, 3m"));
    }

    SECTION("clearing the text puts the axis order back with it") {
        //"No stored text" has to be one state, not two: a leftover order would
        //make two otherwise identical fixes compare unequal.
        cwFixStation fix = typed();
        fix.setCoordinateText(QStringLiteral("46.1, -115.6"), cwCoordinateText::LatitudeLongitude);
        REQUIRE(fix.coordinateTextAxisOrder() == cwCoordinateText::LatitudeLongitude);

        fix.setCoordinateText(QString(), cwCoordinateText::LatitudeLongitude);
        CHECK(fix.coordinateTextAxisOrder() == cwCoordinateText::EastingNorthing);
    }

    SECTION("and so does dropping it by writing a component") {
        //The same single state, reached by the other path. A component setter
        //that cleared only the text would leave the order behind, and two fixes
        //that agree on every number and store no string would compare unequal —
        //visibly so across a save, since the order is only written out
        //alongside a non-empty text and comes back as the default.
        cwFixStation typedGeographic = typed();
        typedGeographic.setCoordinateText(QStringLiteral("46.1, -115.6"),
                                          cwCoordinateText::LatitudeLongitude);
        REQUIRE(typedGeographic.coordinateTextAxisOrder() == cwCoordinateText::LatitudeLongitude);

        typedGeographic.setEasting(9.0);
        CHECK(typedGeographic.coordinateText().isEmpty());
        CHECK(typedGeographic.coordinateTextAxisOrder() == cwCoordinateText::EastingNorthing);

        cwFixStation neverTyped = typed();
        neverTyped.setEasting(9.0);

        //Sharing the id because operator== includes it, and each of these was
        //default-constructed with its own — the axis order is the only thing
        //left that could hold them apart.
        neverTyped.setId(typedGeographic.id());
        CHECK(typedGeographic == neverTyped);
    }
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

    SECTION("including the typed string and the order it was read under") {
        cwFixStation c;
        cwFixStation d;
        d.setId(c.id());
        c.setCoordinateText(QStringLiteral("1, 2, 3m"), cwCoordinateText::EastingNorthing);
        CHECK(c != d);

        d.setCoordinateText(QStringLiteral("1, 2, 3m"), cwCoordinateText::LatitudeLongitude);
        CHECK(c != d);

        d.setCoordinateText(QStringLiteral("1, 2, 3m"), cwCoordinateText::EastingNorthing);
        CHECK(c == d);
    }
}
