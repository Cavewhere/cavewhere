/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

//Our includes
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwLocalProjection.h"

//Qt includes
#include <QString>
#include <QStringList>

#include <cmath>
#include <limits>
#include <string>

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {
    // A cave area in eastern Kentucky, inside NAD83 UTM zone 16N.
    constexpr double kAnchorLatitude = 37.1832;
    constexpr double kAnchorLongitude = -84.0947;

    // Mid-Pacific, where no plate-fixed frame applies: a WGS84 anchor here keeps
    // WGS84, so it is the fixture for everything about the WGS84 default and
    // about the proj-string spelling. The latitude and the longitude are in
    // bands that can't overlap, so a transposed pair is nowhere on Earth.
    constexpr double kOpenOceanLatitude = 5.0;
    constexpr double kOpenOceanLongitude = -150.0;

    // In the Alps, where ETRS89 is the plate-fixed frame.
    constexpr double kAlpsLatitude = 46.5;
    constexpr double kAlpsLongitude = 8.0;

    // Djebel Zaghouan, a Tunisian cave area: Mediterranean, and on the African
    // plate rather than the European one.
    constexpr double kTunisiaLatitude = 36.4;
    constexpr double kTunisiaLongitude = 10.1;

    // Central Anatolia, on the Anatolian plate.
    constexpr double kAnatoliaLatitude = 38.6;
    constexpr double kAnatoliaLongitude = 34.8;

    // Juneau, in the Alaskan panhandle, and Whitehorse, inland in the Yukon:
    // both east of the 141st meridian, on opposite sides of the border.
    constexpr double kJuneauLatitude = 58.3;
    constexpr double kJuneauLongitude = -134.4;
    constexpr double kWhitehorseLatitude = 60.7;
    constexpr double kWhitehorseLongitude = -135.0;

    constexpr const char* kNad83Utm16N = "EPSG:26916";
    constexpr const char* kWgs84Utm16N = "EPSG:32616";

    constexpr const char* kNad83_2011DatumName = "NAD83 (National Spatial Reference System 2011)";
    constexpr const char* kNad83CsrsDatumName = "NAD83 Canadian Spatial Reference System";
    constexpr const char* kEtrs89DatumName = "European Terrestrial Reference System 1989";
    constexpr const char* kWgs84DatumName = "World Geodetic System 1984";

    //! The datum \a ldp is on, as a std::string the matchers read.
    std::string datumNameOf(const QString& ldp)
    {
        return cwLocalProjection::datumName(ldp).toStdString();
    }
}

TEST_CASE("cwLocalProjection derives a transverse Mercator on the anchor",
          "[cwLocalProjection]")
{
    // On the open ocean, so the frame keeps WGS84 and PROJ spells it as a proj
    // string: these are assertions about the recipe, and the parameters they
    // read are only readable in that spelling. A plate-fixed frame is WKT, and
    // the case below checks the same recipe survives into it.
    const QString ldp = cwLocalProjection::derive(kOpenOceanLatitude, kOpenOceanLongitude,
                                                  cwCoordinateTransform::Wgs84);
    REQUIRE_FALSE(ldp.isEmpty());

    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+proj=tmerc"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+lat_0=5"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+lon_0=-150"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+k=1"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+x_0=0"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+y_0=0"));

    // Without this PROJ reads the string as a transformation pipeline rather
    // than as a CRS, and every transform built from it fails.
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+type=crs"));

    // The result has to be usable as a CS everywhere a CS is taken.
    CHECK(cwCoordinateTransform::isValidCS(ldp));
    CHECK_FALSE(cwCoordinateTransform::isGeographic(ldp));
}

TEST_CASE("cwLocalProjection puts the anchor at the scene origin", "[cwLocalProjection]")
{
    const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                  cwCoordinateTransform::Wgs84);
    REQUIRE_FALSE(ldp.isEmpty());

    cwCoordinateTransform toLocal(cwCoordinateTransform::Wgs84, ldp);
    REQUIRE(toLocal.isValid());

    SECTION("the anchor itself lands on (0, 0)")
    {
        const cwGeoPoint local = toLocal.transform(
            cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 0.0));
        CHECK_THAT(local.x, WithinAbs(0.0, 1e-6));
        CHECK_THAT(local.y, WithinAbs(0.0, 1e-6));
    }

    SECTION("a kilometer north is a kilometer of northing")
    {
        // A degree of latitude, roughly — good to a couple of meters, which is
        // all this needs to tell true distance from a scaled one.
        constexpr double kMetersPerDegreeLatitude = 111132.0;
        constexpr double kOneKilometer = 1000.0;
        const cwGeoPoint local = toLocal.transform(
            cwGeoPoint(kAnchorLongitude,
                       kAnchorLatitude + kOneKilometer / kMetersPerDegreeLatitude,
                       0.0));

        CHECK_THAT(local.x, WithinAbs(0.0, 1e-6));
        // k_0 = 1 on the central meridian, so this is true distance to within
        // the crudeness of the degrees-to-meters figure above.
        CHECK_THAT(local.y, WithinAbs(kOneKilometer, 2.0));
    }

    SECTION("elevation passes through untouched")
    {
        constexpr double kElevation = 312.5;
        const cwGeoPoint local = toLocal.transform(
            cwGeoPoint(kAnchorLongitude, kAnchorLatitude, kElevation));
        CHECK_THAT(local.z, WithinAbs(kElevation, 1e-6));
    }
}

TEST_CASE("cwLocalProjection round-trips back to latitude and longitude",
          "[cwLocalProjection]")
{
    // The single load-bearing property for the future globe morph: any point in
    // the frame is recoverable as a geographic location from the string alone.
    const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                  cwCoordinateTransform::Wgs84);
    REQUIRE_FALSE(ldp.isEmpty());

    cwCoordinateTransform toLocal(cwCoordinateTransform::Wgs84, ldp);
    cwCoordinateTransform toGeographic(ldp, cwCoordinateTransform::Wgs84);
    REQUIRE(toLocal.isValid());
    REQUIRE(toGeographic.isValid());

    // ~30 km east and ~20 km north of the anchor — past where a cave area ends
    // and the LDP's guarantees start to matter.
    const cwGeoPoint source(kAnchorLongitude + 0.35, kAnchorLatitude + 0.18, 285.0);
    const cwGeoPoint roundTripped = toGeographic.transform(toLocal.transform(source));

    CHECK_THAT(roundTripped.x, WithinAbs(source.x, 1e-9));
    CHECK_THAT(roundTripped.y, WithinAbs(source.y, 1e-9));
    CHECK_THAT(roundTripped.z, WithinAbs(source.z, 1e-9));
}

TEST_CASE("cwLocalProjection pins the datum from the anchor's own system",
          "[cwLocalProjection]")
{
    SECTION("a NAD83 anchor gives a NAD83 frame")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString::fromLatin1(kNad83Utm16N));
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(ldp.toStdString(), ContainsSubstring("+datum=NAD83"));
    }

    SECTION("a WGS84 anchor keeps WGS84 where no plate-fixed frame applies")
    {
        const QString ldp = cwLocalProjection::derive(kOpenOceanLatitude, kOpenOceanLongitude,
                                                      cwCoordinateTransform::Wgs84);
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(ldp.toStdString(), ContainsSubstring("+datum=WGS84"));
    }

    SECTION("nothing said means WGS84 — what a typed coordinate means")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString());
        const QString wgs84Ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                           cwCoordinateTransform::Wgs84);
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK(ldp == wgs84Ldp);

        // Whitespace is nothing said too, not an unreadable system.
        CHECK(cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                        QStringLiteral("   ")) == wgs84Ldp);
    }

    SECTION("an unreadable system is refused, not quietly called WGS84")
    {
        // Saying something PROJ can't read is not the same as saying nothing:
        // the data is on a datum we failed to identify, and a WGS84 frame would
        // bake that datum's offset into a string nothing ever re-derives.
        CHECK(cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                        QStringLiteral("not a coordinate system")).isEmpty());

        // Parses, but as a transformation rather than a CRS — so it names no
        // datum for the frame to inherit.
        CHECK(cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                        QStringLiteral("+proj=utm +zone=16")).isEmpty());
    }
}

TEST_CASE("cwLocalProjection takes only the horizontal half of a compound system",
          "[cwLocalProjection]")
{
    // What lidar declares: a horizontal CRS plus a vertical one. The vertical
    // half must not reach the frame — a transform that carries it applies the
    // geoid model and quietly returns ellipsoidal heights on whichever machine
    // has the grids installed.
    const QString compound = QStringLiteral("EPSG:26916+EPSG:5703"); // NAD83 UTM 16N + NAVD88
    const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude, compound);
    REQUIRE_FALSE(ldp.isEmpty());

    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+proj=tmerc"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+datum=NAD83"));
    CHECK_THAT(ldp.toStdString(), !ContainsSubstring("+geoidgrids"));
    CHECK_THAT(ldp.toStdString(), !ContainsSubstring("+vunits"));

    // Same frame as the horizontal half alone would have produced.
    CHECK(ldp == cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                           QString::fromLatin1(kNad83Utm16N)));
}

TEST_CASE("cwLocalProjection refuses a location that isn't one", "[cwLocalProjection]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    CHECK(cwLocalProjection::derive(91.0, 0.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(-90.5, 0.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(0.0, 181.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(nan, 0.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(0.0, nan, cwCoordinateTransform::Wgs84).isEmpty());

    // The poles and the antimeridian are locations, however unlikely.
    CHECK_FALSE(cwLocalProjection::derive(90.0, 180.0, cwCoordinateTransform::Wgs84).isEmpty());
}

TEST_CASE("cwLocalProjection::deriveFrom centers on a projected anchor",
          "[cwLocalProjection]")
{
    // The anchor as a fix station holds it: numbers in its own system.
    cwCoordinateTransform toUtm(cwCoordinateTransform::Wgs84,
                                QString::fromLatin1(kNad83Utm16N));
    REQUIRE(toUtm.isValid());
    const cwGeoPoint utmAnchor = toUtm.transform(
        cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 250.0));

    const QString ldp = cwLocalProjection::deriveFrom(QString::fromLatin1(kNad83Utm16N),
                                                      utmAnchor);
    REQUIRE_FALSE(ldp.isEmpty());

    cwCoordinateTransform utmToLocal(QString::fromLatin1(kNad83Utm16N), ldp);
    REQUIRE(utmToLocal.isValid());
    const cwGeoPoint local = utmToLocal.transform(utmAnchor);

    CHECK_THAT(local.x, WithinAbs(0.0, 1e-6));
    CHECK_THAT(local.y, WithinAbs(0.0, 1e-6));

    // Derived from the anchor's own datum, so it never travels through WGS84.
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+datum=NAD83"));
}

TEST_CASE("cwLocalProjection::deriveFrom reads a geographic anchor longitude-first",
          "[cwLocalProjection]")
{
    // Same convention cwCoordinateTransform hands its callers: x is the
    // easting or the longitude, whatever the CRS's own axis order says.
    const QString ldp = cwLocalProjection::deriveFrom(
        cwCoordinateTransform::Wgs84,
        cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 0.0));

    REQUIRE_FALSE(ldp.isEmpty());
    CHECK(ldp == cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                           cwCoordinateTransform::Wgs84));
}

TEST_CASE("cwLocalProjection::deriveFrom refuses an anchor with no numbers",
          "[cwLocalProjection]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(cwLocalProjection::deriveFrom(cwCoordinateTransform::Wgs84,
                                        cwGeoPoint(nan, kAnchorLatitude, 0.0)).isEmpty());
    CHECK(cwLocalProjection::deriveFrom(QStringLiteral("not a coordinate system"),
                                        cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 0.0))
              .isEmpty());
}

TEST_CASE("cwLocalProjection::datumName names the datum a frame is on",
          "[cwLocalProjection]")
{
    SECTION("a NAD83 frame, spelled for a reader rather than for PROJ")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString::fromLatin1(kNad83Utm16N));
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(cwLocalProjection::datumName(ldp).toStdString(),
                   ContainsSubstring("North American Datum 1983"));
    }

    SECTION("a WGS84 frame — the open ocean, where WGS84 is what a frame keeps")
    {
        const QString ldp = cwLocalProjection::derive(kOpenOceanLatitude, kOpenOceanLongitude,
                                                      cwCoordinateTransform::Wgs84);
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kWgs84DatumName));
    }

    SECTION("an input system answers too, so a datum can be read before a frame exists")
    {
        CHECK_THAT(cwLocalProjection::datumName(QString::fromLatin1(kNad83Utm16N)).toStdString(),
                   ContainsSubstring("North American Datum 1983"));
    }

    SECTION("a compound system answers for its horizontal half")
    {
        CHECK_THAT(cwLocalProjection::datumName(QStringLiteral("EPSG:26916+EPSG:5703")).toStdString(),
                   ContainsSubstring("North American Datum 1983"));
    }

    SECTION("nothing readable names no datum")
    {
        CHECK(cwLocalProjection::datumName(QString()).isEmpty());
        CHECK(cwLocalProjection::datumName(QStringLiteral("   ")).isEmpty());
        CHECK(cwLocalProjection::datumName(QStringLiteral("not a coordinate system")).isEmpty());

        // Parses, but as a transformation rather than a CRS, so it carries no
        // datum — the same case derive() refuses.
        CHECK(cwLocalProjection::datumName(QStringLiteral("+proj=utm +zone=16")).isEmpty());
    }
}

TEST_CASE("cwLocalProjection::origin reads back the point a frame was derived on",
          "[cwLocalProjection]")
{
    SECTION("the anchor comes back out, longitude first")
    {
        // On the open ocean, so the frame is on the datum it was handed and the
        // readout is exact. A frame that adopted a plate-fixed datum answers on
        // that datum instead, which is the loose-tolerance case below.
        const QString ldp = cwLocalProjection::derive(kOpenOceanLatitude, kOpenOceanLongitude,
                                                      cwCoordinateTransform::Wgs84);
        REQUIRE_FALSE(ldp.isEmpty());

        const std::optional<cwGeoPoint> origin = cwLocalProjection::origin(ldp);
        REQUIRE(origin.has_value());
        CHECK_THAT(origin->x, WithinAbs(kOpenOceanLongitude, 1e-9));
        CHECK_THAT(origin->y, WithinAbs(kOpenOceanLatitude, 1e-9));
    }

    SECTION("on the frame's own datum, so no datum shift creeps into the readout")
    {
        // derive() plants its latitude and longitude on the datum it was given,
        // so reading them back on that same datum returns them unchanged. Going
        // through WGS84 instead would move the readout by the NAD83 shift.
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString::fromLatin1(kNad83Utm16N));
        REQUIRE_FALSE(ldp.isEmpty());

        const std::optional<cwGeoPoint> origin = cwLocalProjection::origin(ldp);
        REQUIRE(origin.has_value());
        CHECK_THAT(origin->x, WithinAbs(kAnchorLongitude, 1e-9));
        CHECK_THAT(origin->y, WithinAbs(kAnchorLatitude, 1e-9));
    }

    SECTION("a frame with no data behind it still says where it is")
    {
        // deriveFrom's path: the frame remembers the anchor it was centered on
        // whatever coordinate system that anchor was typed in.
        const QString ldp = cwLocalProjection::deriveFrom(
            cwCoordinateTransform::Wgs84,
            cwGeoPoint(kOpenOceanLongitude, kOpenOceanLatitude, 0.0));
        REQUIRE_FALSE(ldp.isEmpty());

        const std::optional<cwGeoPoint> origin = cwLocalProjection::origin(ldp);
        REQUIRE(origin.has_value());
        CHECK_THAT(origin->x, WithinAbs(kOpenOceanLongitude, 1e-9));
        CHECK_THAT(origin->y, WithinAbs(kOpenOceanLatitude, 1e-9));
    }

    SECTION("nothing readable has no origin")
    {
        CHECK_FALSE(cwLocalProjection::origin(QString()).has_value());
        CHECK_FALSE(cwLocalProjection::origin(QStringLiteral("   ")).has_value());
        CHECK_FALSE(cwLocalProjection::origin(QStringLiteral("not a coordinate system")).has_value());
        CHECK_FALSE(cwLocalProjection::origin(QStringLiteral("+proj=utm +zone=16")).has_value());
    }
}

TEST_CASE("cwLocalProjection puts an unclaimed coordinate on the plate-fixed datum",
          "[cwLocalProjection]")
{
    // A coordinate on the bare WGS84 ensemble claims no datum at all, so the
    // frame gets the one its part of the world holds still against. The trigger
    // is the datum PROJ resolves, never the spelling of the system.
    SECTION("a typed lat/long in Kentucky is NAD83(2011)")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString());
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kNad83_2011DatumName));
    }

    SECTION("a WGS84 UTM zone is the same coordinate, said a different way")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString::fromLatin1(kWgs84Utm16N));
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kNad83_2011DatumName));
    }

    SECTION("a declared NAD83 UTM zone is inherited verbatim")
    {
        // Plain NAD83, not NAD83(2011): a declared datum is what the user said,
        // and the two differ by a meter or two on the same ground.
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString::fromLatin1(kNad83Utm16N));
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK(datumNameOf(ldp) == std::string("North American Datum 1983"));
    }

    SECTION("a plate-fixed frame is still an LDP")
    {
        // The recipe assertions read parameters out of a proj string, and this
        // frame is WKT — so this is the case that says the recipe survived the
        // spelling: it is a usable projected CS centered on the anchor.
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString());
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK(cwCoordinateTransform::isValidCS(ldp));
        CHECK_FALSE(cwCoordinateTransform::isGeographic(ldp));

        // Read back on the frame's own datum, so this lands within the datum
        // shift of the typed point rather than on it.
        constexpr double kDatumShiftDegrees = 3e-5;
        const std::optional<cwGeoPoint> origin = cwLocalProjection::origin(ldp);
        REQUIRE(origin.has_value());
        CHECK_THAT(origin->x, WithinAbs(kAnchorLongitude, kDatumShiftDegrees));
        CHECK_THAT(origin->y, WithinAbs(kAnchorLatitude, kDatumShiftDegrees));
    }
}

TEST_CASE("cwLocalProjection hands out ETRS89 where ETRS89 applies",
          "[cwLocalProjection]")
{
    SECTION("the Alps")
    {
        const QString ldp = cwLocalProjection::derive(kAlpsLatitude, kAlpsLongitude, QString());
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kEtrs89DatumName));
    }

    SECTION("a Tunisian cave area keeps WGS84")
    {
        // Across the Mediterranean and on the African plate, so ETRS89 has
        // nothing to say about it — and a frame labeled ETRS89 there would be a
        // label the data never earned.
        const QString ldp = cwLocalProjection::derive(kTunisiaLatitude, kTunisiaLongitude,
                                                      QString());
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kWgs84DatumName));
    }

    SECTION("Anatolia keeps WGS84")
    {
        const QString ldp = cwLocalProjection::derive(kAnatoliaLatitude, kAnatoliaLongitude,
                                                      QString());
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kWgs84DatumName));
    }
}

TEST_CASE("cwLocalProjection keeps the Alaskan panhandle apart from the Yukon",
          "[cwLocalProjection]")
{
    SECTION("Juneau is on the US frame")
    {
        const QString ldp = cwLocalProjection::derive(kJuneauLatitude, kJuneauLongitude,
                                                      QString());
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kNad83_2011DatumName));
    }

    SECTION("Whitehorse is on the Canadian frame")
    {
        const QString ldp = cwLocalProjection::derive(kWhitehorseLatitude, kWhitehorseLongitude,
                                                      QString());
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kNad83CsrsDatumName));
    }
}

TEST_CASE("cwLocalProjection recenters a stored frame without touching its datum",
          "[cwLocalProjection]")
{
    // What an old project carries: a frame derived before CaveWhere chose plate-
    // fixed datums. Recentering it moves the origin; the datum is the project's
    // own and stays whatever it has always been.
    const QString legacyFrame = QStringLiteral(
        "+proj=tmerc +lat_0=37.1832 +lon_0=-84.0947 +k=1 +x_0=0 +y_0=0 "
        "+datum=WGS84 +units=m +no_defs +type=crs");
    REQUIRE_THAT(datumNameOf(legacyFrame), ContainsSubstring(kWgs84DatumName));

    // A kilometer northeast in the frame's own coordinates, the way the middle
    // of the data or a picked station arrives.
    const cwGeoPoint center(1000.0, 1000.0, 0.0);

    SECTION("the frame's own system keeps the frame's own datum")
    {
        const QString recentered = cwLocalProjection::deriveFrom(
            legacyFrame, center, cwLocalProjection::DatumSource::StoredFrame);
        REQUIRE_FALSE(recentered.isEmpty());
        CHECK_THAT(datumNameOf(recentered), ContainsSubstring(kWgs84DatumName));

        // Centered where it was asked to be, so this is a move and only a move.
        const cwCoordinateTransform toRecentered(legacyFrame, recentered);
        REQUIRE(toRecentered.isValid());
        const cwGeoPoint origin = toRecentered.transform(center);
        CHECK_THAT(origin.x, WithinAbs(0.0, 1e-3));
        CHECK_THAT(origin.y, WithinAbs(0.0, 1e-3));
    }

    SECTION("the same point as a data input is a new frame, and gets a datum chosen")
    {
        const QString derived = cwLocalProjection::deriveFrom(
            legacyFrame, center, cwLocalProjection::DatumSource::DataInput);
        REQUIRE_FALSE(derived.isEmpty());
        CHECK_THAT(datumNameOf(derived), ContainsSubstring(kNad83_2011DatumName));
    }
}

TEST_CASE("cwLocalProjection::plateFixedDatumsFor answers with every frame that reaches a point",
          "[cwLocalProjection]")
{
    SECTION("inland, one frame reaches — the answer is the single one")
    {
        CHECK(cwLocalProjection::plateFixedDatumsFor(kAnchorLatitude, kAnchorLongitude)
              == QStringList{QStringLiteral("EPSG:6318")});
    }

    SECTION("along a shared border, both frames reach, in table order")
    {
        // Northern Wisconsin: inside the conterminous US box and inside Canada's,
        // which start at 41.5N together. A cave here is served by both frames, and
        // the picker offers the user the choice derive() has to make alone.
        CHECK(cwLocalProjection::plateFixedDatumsFor(45.0, -90.0)
              == QStringList({QStringLiteral("EPSG:6318"), QStringLiteral("EPSG:4617")}));
    }

    SECTION("one datum spanning several boxes still answers once")
    {
        // Juneau is in the panhandle box; Canada's box covers it too. NAD83(2011)
        // must appear once whichever of its five boxes matched.
        const QStringList datums =
            cwLocalProjection::plateFixedDatumsFor(kJuneauLatitude, kJuneauLongitude);
        CHECK(datums.count(QStringLiteral("EPSG:6318")) == 1);
        CHECK(datums.contains(QStringLiteral("EPSG:4617")));
    }

    SECTION("out at sea no frame reaches, so nothing is offered")
    {
        CHECK(cwLocalProjection::plateFixedDatumsFor(kOpenOceanLatitude, kOpenOceanLongitude)
              == QStringList());
    }

    SECTION("the single-answer form stays the first match")
    {
        // The two forms agree on what comes first — the frame derive() pins is
        // the frame the picker leads with.
        const QStringList datums = cwLocalProjection::plateFixedDatumsFor(45.0, -90.0);
        REQUIRE_FALSE(datums.isEmpty());
        const QString ldp = cwLocalProjection::derive(45.0, -90.0, cwCoordinateTransform::Wgs84);
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(datumNameOf(ldp), ContainsSubstring(kNad83_2011DatumName));
        CHECK(datums.first() == QStringLiteral("EPSG:6318"));
    }
}
