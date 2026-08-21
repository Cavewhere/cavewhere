/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwSurvexCS.h"

using cwSurvexCS::fromSurvexCS;

TEST_CASE("fromSurvexCS reads survex's UTM zones", "[cwSurvexCS]")
{
    CHECK(fromSurvexCS(QStringLiteral("UTM16N"))->projCS == QStringLiteral("EPSG:32616"));
    CHECK(fromSurvexCS(QStringLiteral("UTM16S"))->projCS == QStringLiteral("EPSG:32716"));

    //Survex takes a zone with no hemisphere letter as north.
    CHECK(fromSurvexCS(QStringLiteral("UTM16"))->projCS == QStringLiteral("EPSG:32616"));

    CHECK(fromSurvexCS(QStringLiteral("utm1n"))->projCS == QStringLiteral("EPSG:32601"));
    CHECK(fromSurvexCS(QStringLiteral("UTM60S"))->projCS == QStringLiteral("EPSG:32760"));

    //Zone 0 and zone 61 are outside the grid, so they name nothing rather than
    //an EPSG code that would place the cave somewhere arbitrary.
    CHECK_FALSE(fromSurvexCS(QStringLiteral("UTM0N")).has_value());
    CHECK_FALSE(fromSurvexCS(QStringLiteral("UTM61N")).has_value());
}

TEST_CASE("fromSurvexCS passes authority codes through", "[cwSurvexCS]")
{
    CHECK(fromSurvexCS(QStringLiteral("EPSG:32616"))->projCS == QStringLiteral("EPSG:32616"));
    CHECK(fromSurvexCS(QStringLiteral("esri:102008"))->projCS == QStringLiteral("ESRI:102008"));

    //Cavern's own ceiling on a code.
    CHECK_FALSE(fromSurvexCS(QStringLiteral("EPSG:1000000")).has_value());
}

TEST_CASE("fromSurvexCS unwraps a CUSTOM PROJ string", "[cwSurvexCS]")
{
    const QString proj = QStringLiteral("+proj=tmerc +lat_0=37.1 +lon_0=-86.1 +k=1 "
                                        "+x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs");
    const auto parsed = fromSurvexCS(QStringLiteral("CUSTOM \"%1\"").arg(proj));
    REQUIRE(parsed.has_value());
    CHECK(parsed->projCS == proj);
    CHECK_FALSE(parsed->latitudeFirst);
}

TEST_CASE("A PROJ string survives the round trip through survex's syntax", "[cwSurvexCS]")
{
    //Spaces and + signs are what force the CUSTOM quoting, so this is the pair
    //that has to agree: whatever the exporter writes, the importer reads back.
    const QString proj = QStringLiteral("+proj=utm +zone=16 +datum=WGS84 +units=m +no_defs");
    CHECK(fromSurvexCS(cwSurvexCS::toSurvexCS(proj))->projCS == proj);

    const QString epsg = QStringLiteral("EPSG:32616");
    CHECK(fromSurvexCS(cwSurvexCS::toSurvexCS(epsg))->projCS == epsg);
}

TEST_CASE("fromSurvexCS reads the keyword systems", "[cwSurvexCS]")
{
    CHECK(fromSurvexCS(QStringLiteral("S-MERC"))->projCS == QStringLiteral("EPSG:3857"));
    CHECK(fromSurvexCS(QStringLiteral("EUR79Z30"))->projCS.startsWith(
        QStringLiteral("+proj=utm +zone=30 +ellps=intl")));
    CHECK(fromSurvexCS(QStringLiteral("IJTSK"))->projCS.startsWith(QStringLiteral("+proj=krovak")));
    CHECK_FALSE(fromSurvexCS(QStringLiteral("IJTSK"))->projCS.contains(QStringLiteral("+czech")));

    //JTSK is IJTSK plus +czech — cavern refuses it for output, which is an
    //export question; on the way in it is a system like any other.
    CHECK(fromSurvexCS(QStringLiteral("JTSK"))->projCS.contains(QStringLiteral("+czech")));
    CHECK(fromSurvexCS(QStringLiteral("JTSK03"))->projCS.contains(QStringLiteral("+czech")));
    CHECK(fromSurvexCS(QStringLiteral("IJTSK03"))->projCS
          != fromSurvexCS(QStringLiteral("IJTSK"))->projCS);
}

TEST_CASE("fromSurvexCS answers both lat/long orders with EPSG:4326", "[cwSurvexCS]")
{
    const auto longLat = fromSurvexCS(QStringLiteral("LONG-LAT"));
    REQUIRE(longLat.has_value());
    CHECK(longLat->projCS == QStringLiteral("EPSG:4326"));
    CHECK_FALSE(longLat->latitudeFirst);

    //Cavern refuses LAT-LONG outright. CaveWhere reads it, and the whole of the
    //difference is which number comes first in the *fix line.
    const auto latLong = fromSurvexCS(QStringLiteral("LAT-LONG"));
    REQUIRE(latLong.has_value());
    CHECK(latLong->projCS == QStringLiteral("EPSG:4326"));
    CHECK(latLong->latitudeFirst);
}

TEST_CASE("fromSurvexCS shifts an OSGB square's false origin", "[cwSurvexCS]")
{
    //SV is the square the national grid's own origin sits in, so it comes out
    //as EPSG:27700's parameters — which is what makes the letter decoding
    //checkable against something other than itself.
    const QString sv = fromSurvexCS(QStringLiteral("OSGB:SV"))->projCS;
    CHECK(sv == QStringLiteral("+proj=tmerc +lat_0=49 +lon_0=-2 +k=0.9996012717 "
                               "+x_0=400000 +y_0=-100000 +ellps=airy +datum=OSGB36 "
                               "+units=m +no_defs"));

    //The letters are a false origin, not a variant of one system: SD is a
    //different square, so its coordinates mean something else.
    CHECK(fromSurvexCS(QStringLiteral("OSGB:SD"))->projCS != sv);

    //I is missing from the grid, and only H, N, O, S and T start a square.
    CHECK_FALSE(fromSurvexCS(QStringLiteral("OSGB:SI")).has_value());
    CHECK_FALSE(fromSurvexCS(QStringLiteral("OSGB:AA")).has_value());
    CHECK_FALSE(fromSurvexCS(QStringLiteral("OSGB:S")).has_value());
}

TEST_CASE("fromSurvexCS tells LOCAL apart from a spelling it can't map", "[cwSurvexCS]")
{
    //LOCAL says the survey is deliberately ungeoreferenced, so it names no
    //system and there is nothing to warn about.
    const auto local = fromSurvexCS(QStringLiteral("LOCAL"));
    REQUIRE(local.has_value());
    CHECK(local->projCS.isEmpty());

    //A spelling survex doesn't define is absent instead, which is what the
    //importer warns on.
    CHECK_FALSE(fromSurvexCS(QStringLiteral("MADE-UP-GRID")).has_value());
    CHECK_FALSE(fromSurvexCS(QString()).has_value());
}
