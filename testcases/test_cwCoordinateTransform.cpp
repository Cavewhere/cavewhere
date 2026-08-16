/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwLocalProjection.h"

//Std includes
#include <iterator>

using Catch::Matchers::WithinAbs;

TEST_CASE("cwCoordinateTransform short-circuits identical CS", "[cwCoordinateTransform][identity]")
{
    SECTION("Same string is identity")
    {
        cwCoordinateTransform t("EPSG:4326", "EPSG:4326");
        CHECK(t.isValid());
        CHECK(t.isIdentity());

        cwGeoPoint p(-105.27, 40.01, 1655.0);
        cwGeoPoint q = t.transform(p);
        CHECK(q == p);
    }

    SECTION("Whitespace and case differences are still identity")
    {
        cwCoordinateTransform t(" epsg:32612 ", "EPSG:32612");
        CHECK(t.isValid());
        CHECK(t.isIdentity());
    }

    SECTION("transformInPlace is a no-op when identity")
    {
        cwCoordinateTransform t("EPSG:32612", "EPSG:32612");
        cwGeoPoint pts[3] = {
            cwGeoPoint(500000.0, 4400000.0, 1000.0),
            cwGeoPoint(501000.0, 4401000.0, 1100.0),
            cwGeoPoint(502000.0, 4402000.0, 1200.0)
        };
        t.transformInPlace(pts, 3);
        CHECK(pts[0].x == 500000.0);
        CHECK(pts[1].y == 4401000.0);
        CHECK(pts[2].z == 1200.0);
    }
}

TEST_CASE("cwCoordinateTransform reports invalid CS", "[cwCoordinateTransform][invalid]")
{
    SECTION("Garbage CS string")
    {
        cwCoordinateTransform t("EPSG:4326", "NOT_A_REAL_CRS_xxx");
        CHECK_FALSE(t.isValid());
        CHECK_FALSE(t.isIdentity());
        CHECK_FALSE(t.errorMessage().isEmpty());
    }

    SECTION("Empty CS string")
    {
        cwCoordinateTransform t("", "EPSG:4326");
        CHECK_FALSE(t.isValid());
        CHECK_FALSE(t.errorMessage().isEmpty());
    }
}

TEST_CASE("cwCoordinateTransform::isValidCS validates EPSG codes", "[cwCoordinateTransform][isValidCS]")
{
    CHECK(cwCoordinateTransform::isValidCS("EPSG:4326"));
    CHECK(cwCoordinateTransform::isValidCS("EPSG:32612"));
    CHECK(cwCoordinateTransform::isValidCS("EPSG:27700"));

    CHECK_FALSE(cwCoordinateTransform::isValidCS(""));
    CHECK_FALSE(cwCoordinateTransform::isValidCS("   "));
    CHECK_FALSE(cwCoordinateTransform::isValidCS("NOT_A_CRS"));
    CHECK_FALSE(cwCoordinateTransform::isValidCS("EPSG:99999999"));
}

TEST_CASE("cwCoordinateTransform reprojects WGS84 to UTM 12N", "[cwCoordinateTransform][reproject]")
{
    cwCoordinateTransform t("EPSG:4326", "EPSG:32612");
    REQUIRE(t.isValid());
    REQUIRE_FALSE(t.isIdentity());

    // -110 lon, 40 lat is well inside UTM zone 12N. Expected easting/northing
    // verified against an independent proj invocation:
    //   echo "-110 40" | cs2cs +init=epsg:4326 +to +init=epsg:32612
    // gives roughly (500000, 4427757) for -111 +40, and ~585360 / 4428236 for
    // -110 +40. We test round-trip precision rather than hard-coding numbers.
    cwGeoPoint lonLat(-110.0, 40.0, 1500.0);
    cwGeoPoint utm = t.transform(lonLat);

    // Easting must be inside the canonical UTM range (zone-N falsifies to 500k).
    CHECK(utm.x > 100000.0);
    CHECK(utm.x < 900000.0);
    // Northing for 40N should be roughly 4.4M.
    CHECK(utm.y > 4'000'000.0);
    CHECK(utm.y < 5'000'000.0);
    // Elevation passes through unchanged for these horizontal-only EPSG codes.
    CHECK_THAT(utm.z, WithinAbs(1500.0, 1e-3));
}

TEST_CASE("cwCoordinateTransform::transformPoint matches a built transform",
          "[cwCoordinateTransform][transformPoint]")
{
    const cwGeoPoint lonLat(-110.0, 40.0, 1500.0);

    SECTION("agrees with constructing the transform by hand") {
        cwCoordinateTransform t("EPSG:4326", "EPSG:32612");
        REQUIRE(t.isValid());
        const cwGeoPoint expected = t.transform(lonLat);

        const auto point = cwCoordinateTransform::transformPoint("EPSG:4326", "EPSG:32612", lonLat);
        REQUIRE(point.has_value());
        CHECK_THAT(point->x, WithinAbs(expected.x, 1e-6));
        CHECK_THAT(point->y, WithinAbs(expected.y, 1e-6));
        CHECK_THAT(point->z, WithinAbs(expected.z, 1e-6));
    }

    SECTION("a memo hit returns the same answer as the first call") {
        const auto first = cwCoordinateTransform::transformPoint("EPSG:4326", "EPSG:32612", lonLat);
        const auto second = cwCoordinateTransform::transformPoint("EPSG:4326", "EPSG:32612", lonLat);
        REQUIRE(first.has_value());
        REQUIRE(second.has_value());
        CHECK(first->x == second->x);
        CHECK(first->y == second->y);
    }

    SECTION("identical systems pass the point through untouched") {
        const auto point = cwCoordinateTransform::transformPoint("EPSG:32612", " epsg:32612 ",
                                                                 cwGeoPoint(500000.0, 4427757.0, 1500.0));
        REQUIRE(point.has_value());
        CHECK_THAT(point->x, WithinAbs(500000.0, 1e-9));
        CHECK_THAT(point->y, WithinAbs(4427757.0, 1e-9));
        CHECK_THAT(point->z, WithinAbs(1500.0, 1e-9));
    }

    SECTION("an empty system is unanswerable, not an identity") {
        CHECK_FALSE(cwCoordinateTransform::transformPoint("", "EPSG:32612", lonLat).has_value());
        CHECK_FALSE(cwCoordinateTransform::transformPoint("EPSG:4326", "   ", lonLat).has_value());
    }

    SECTION("an unbuildable pair stays empty on the cached second call") {
        CHECK_FALSE(cwCoordinateTransform::transformPoint("EPSG:4326", "NOT-A-CS", lonLat).has_value());
        CHECK_FALSE(cwCoordinateTransform::transformPoint("EPSG:4326", "NOT-A-CS", lonLat).has_value());
    }
}

TEST_CASE("cwCoordinateTransform round-trip preserves mm precision", "[cwCoordinateTransform][precision]")
{
    cwCoordinateTransform forward("EPSG:4326", "EPSG:32612");
    cwCoordinateTransform inverse("EPSG:32612", "EPSG:4326");
    REQUIRE(forward.isValid());
    REQUIRE(inverse.isValid());

    const cwGeoPoint origin(-110.123456, 40.987654, 1500.0);
    const cwGeoPoint utm = forward.transform(origin);
    const cwGeoPoint back = inverse.transform(utm);

    // Round-trip residual: converting 1e-7 degrees to meters at 40N ≈ 1cm.
    // Allow up to 1mm in degrees (~10cm at 40N) to be safe across PROJ versions.
    CHECK_THAT(back.x, WithinAbs(origin.x, 1e-8));
    CHECK_THAT(back.y, WithinAbs(origin.y, 1e-8));
    CHECK_THAT(back.z, WithinAbs(origin.z, 1e-3));
}

TEST_CASE("cwCoordinateTransform::transformInPlace reprojects an array", "[cwCoordinateTransform][batch]")
{
    cwCoordinateTransform t("EPSG:4326", "EPSG:32612");
    REQUIRE(t.isValid());

    cwGeoPoint pts[3] = {
        cwGeoPoint(-110.0, 40.0, 1500.0),
        cwGeoPoint(-110.5, 40.5, 1600.0),
        cwGeoPoint(-111.0, 41.0, 1700.0)
    };

    cwCoordinateTransform single("EPSG:4326", "EPSG:32612");
    const cwGeoPoint expected0 = single.transform(pts[0]);
    const cwGeoPoint expected1 = single.transform(pts[1]);
    const cwGeoPoint expected2 = single.transform(pts[2]);

    t.transformInPlace(pts, 3);

    CHECK_THAT(pts[0].x, WithinAbs(expected0.x, 1e-6));
    CHECK_THAT(pts[0].y, WithinAbs(expected0.y, 1e-6));
    CHECK_THAT(pts[1].x, WithinAbs(expected1.x, 1e-6));
    CHECK_THAT(pts[1].y, WithinAbs(expected1.y, 1e-6));
    CHECK_THAT(pts[2].x, WithinAbs(expected2.x, 1e-6));
    CHECK_THAT(pts[2].y, WithinAbs(expected2.y, 1e-6));
}

TEST_CASE("cwCoordinateTransform::commonProjectedCSList lists valid EPSG codes",
          "[cwCoordinateTransform]")
{
    const QStringList list = cwCoordinateTransform::commonProjectedCSList();
    REQUIRE_FALSE(list.isEmpty());
    for (const QString& cs : list) {
        INFO("CS: " << cs.toStdString());
        CHECK(cwCoordinateTransform::isValidCS(cs));
    }
}

TEST_CASE("cwCoordinateTransform::isGeographic detects WGS84 lat/long",
          "[cwCoordinateTransform][isGeographic]")
{
    CHECK(cwCoordinateTransform::isGeographic("EPSG:4326"));
    CHECK(cwCoordinateTransform::isGeographic("epsg:4326"));
    CHECK_FALSE(cwCoordinateTransform::isGeographic("EPSG:32616"));
    CHECK_FALSE(cwCoordinateTransform::isGeographic("EPSG:27700"));
    CHECK_FALSE(cwCoordinateTransform::isGeographic(""));
    CHECK_FALSE(cwCoordinateTransform::isGeographic("NOT_A_CRS"));
}

TEST_CASE("cwCoordinateTransform::utmZoneToEpsg builds WGS84 UTM codes",
          "[cwCoordinateTransform][utm]")
{
    CHECK(cwCoordinateTransform::utmZoneToEpsg(1, true)  == "EPSG:32601");
    CHECK(cwCoordinateTransform::utmZoneToEpsg(16, true) == "EPSG:32616");
    CHECK(cwCoordinateTransform::utmZoneToEpsg(60, true) == "EPSG:32660");
    CHECK(cwCoordinateTransform::utmZoneToEpsg(1, false)  == "EPSG:32701");
    CHECK(cwCoordinateTransform::utmZoneToEpsg(60, false) == "EPSG:32760");

    // Out-of-range zones return empty.
    CHECK(cwCoordinateTransform::utmZoneToEpsg(0, true).isEmpty());
    CHECK(cwCoordinateTransform::utmZoneToEpsg(61, true).isEmpty());
    CHECK(cwCoordinateTransform::utmZoneToEpsg(-5, false).isEmpty());
}

TEST_CASE("cwCoordinateTransform::nameFor returns the human-readable description",
          "[cwCoordinateTransform][nameFor]")
{
    // Empty / invalid inputs return empty.
    CHECK(cwCoordinateTransform::nameFor("").isEmpty());
    CHECK(cwCoordinateTransform::nameFor("NOT_A_CRS").isEmpty());

    // Known EPSG codes return non-empty strings containing recognizable
    // tokens. We don't hard-code the exact name string because PROJ's
    // labels can differ slightly across versions.
    const QString utmName = cwCoordinateTransform::nameFor("EPSG:32616");
    INFO("UTM name: " << utmName.toStdString());
    CHECK_FALSE(utmName.isEmpty());
    CHECK(utmName.contains("UTM", Qt::CaseInsensitive));
    CHECK(utmName.contains("16"));

    const QString osgbName = cwCoordinateTransform::nameFor("EPSG:27700");
    INFO("OSGB name: " << osgbName.toStdString());
    CHECK_FALSE(osgbName.isEmpty());
    CHECK(osgbName.contains("British", Qt::CaseInsensitive));
}

TEST_CASE("cwCoordinateTransform::deriveProjectedOutputCS suggests a projected CS",
          "[cwCoordinateTransform][deriveOutputCS]")
{
    SECTION("Empty / invalid input yields no suggestion") {
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS("", cwGeoPoint(0, 0, 0)).isEmpty());
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS("   ", cwGeoPoint(0, 0, 0)).isEmpty());
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS("NOT_A_CRS", cwGeoPoint(0, 0, 0)).isEmpty());
    }

    SECTION("Already-projected input passes through unchanged (trimmed)") {
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS("EPSG:32612",
                  cwGeoPoint(585360, 4428236, 1500)) == "EPSG:32612");
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS("  EPSG:27700  ",
                  cwGeoPoint(400000, 300000, 100)) == "EPSG:27700");
    }

    SECTION("Geographic input (WGS84) resolves to the UTM zone at the fix") {
        // -110 lon, 40 lat is zone 12 north: floor((-110+180)/6)+1 = 12.
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS(
                  cwCoordinateTransform::Wgs84, cwGeoPoint(-110.0, 40.0, 1500.0)) == "EPSG:32612");
        // 0 lon, 51.5 lat (London) is zone 31 north.
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS(
                  cwCoordinateTransform::Wgs84, cwGeoPoint(0.0, 51.5, 0.0)) == "EPSG:32631");
        // Southern hemisphere picks a 327xx code: 151 lon, -33.9 lat (Sydney) → zone 56 south.
        CHECK(cwCoordinateTransform::deriveProjectedOutputCS(
                  cwCoordinateTransform::Wgs84, cwGeoPoint(151.2, -33.9, 0.0)) == "EPSG:32756");
    }

    SECTION("A geographic input reprojects to WGS84 before choosing the zone") {
        // NAD83 (EPSG:4269) at -87 lon, 38 lat → WGS84 ≈ same → zone 16 north
        // ((-87+180)/6 = 15.5 → floor 15, +1 = 16). -87 is well inside zone 16,
        // clear of the 16/17 boundary at -84.
        const QString cs = cwCoordinateTransform::deriveProjectedOutputCS(
            "EPSG:4269", cwGeoPoint(-87.0, 38.0, 0.0));
        CHECK(cs == "EPSG:32616");
    }
}

TEST_CASE("cwCoordinateTransform::domainCheck attributes the out-of-domain axis",
          "[cwCoordinateTransform][domainCheck]")
{
    using DomainCheck = cwCoordinateTransform::DomainCheck;

    SECTION("An in-domain UTM coordinate flags neither axis") {
        // 478000 E, 4430000 N in UTM 13N is a real in-zone Colorado location.
        const DomainCheck check =
            cwCoordinateTransform::domainCheck("EPSG:32613", cwGeoPoint(478000.0, 4430000.0, 1655.0));
        CHECK(check.eastingValid);
        CHECK(check.northingValid);
    }

    SECTION("A transposed-digit easting flags only the easting") {
        // 1478000 E is ~1000 km east of zone 13 — the longitude leaves the
        // domain while the latitude (from a normal northing) stays inside it.
        const DomainCheck check =
            cwCoordinateTransform::domainCheck("EPSG:32613", cwGeoPoint(1478000.0, 4430000.0, 1655.0));
        CHECK_FALSE(check.eastingValid);
        CHECK(check.northingValid);
    }

    SECTION("A wildly wrong northing flags only the northing") {
        // A large negative northing in a northern zone inverts to a southern
        // latitude well outside the domain, while the easting stays near the
        // central meridian and remains valid.
        const DomainCheck check =
            cwCoordinateTransform::domainCheck("EPSG:32613", cwGeoPoint(478000.0, -2000000.0, 1655.0));
        CHECK(check.eastingValid);
        CHECK_FALSE(check.northingValid);
    }

    SECTION("A northing past the pole is not blamed on the easting") {
        // 14430000 N inverts to 50N/75E — the latitude still looks in-domain
        // while the longitude has wrapped ~180 degrees past the pole. Attributing
        // by axis here would tint the easting red for a bad northing, so the
        // check must decline to attribute and flag both instead.
        const DomainCheck check =
            cwCoordinateTransform::domainCheck("EPSG:32613", cwGeoPoint(478000.0, 14430000.0, 1655.0));
        CHECK_FALSE(check.eastingValid);
        CHECK_FALSE(check.northingValid);
    }

    SECTION("An empty or unparseable CS defers — both axes valid") {
        CHECK(cwCoordinateTransform::domainCheck("", cwGeoPoint(0, 0, 0)).eastingValid);
        CHECK(cwCoordinateTransform::domainCheck("", cwGeoPoint(0, 0, 0)).northingValid);
        CHECK(cwCoordinateTransform::domainCheck("NOT_A_CRS", cwGeoPoint(9e9, 9e9, 0)).eastingValid);
        CHECK(cwCoordinateTransform::domainCheck("NOT_A_CRS", cwGeoPoint(9e9, 9e9, 0)).northingValid);
    }
}

TEST_CASE("cwCoordinateSystem::modeFor classifies CS strings",
          "[cwCoordinateTransform][modeFor]")
{
    using Mode = cwCoordinateSystem::Mode;

    SECTION("Empty / whitespace → Local") {
        CHECK(cwCoordinateSystem::modeFor("")    == Mode::Local);
        CHECK(cwCoordinateSystem::modeFor("   ") == Mode::Local);
    }

    SECTION("EPSG:4326 → LatLon (case-insensitive)") {
        CHECK(cwCoordinateSystem::modeFor("EPSG:4326") == Mode::LatLon);
        CHECK(cwCoordinateSystem::modeFor("epsg:4326") == Mode::LatLon);
    }

    SECTION("WGS84 UTM north → UTM + zone + north=true") {
        CHECK(cwCoordinateSystem::modeFor("EPSG:32616")    == Mode::UTM);
        CHECK(cwCoordinateSystem::utmZoneFor("EPSG:32616")  == 16);
        CHECK(cwCoordinateSystem::utmNorthFor("EPSG:32616") == true);
    }

    SECTION("WGS84 UTM south → UTM + zone + north=false") {
        CHECK(cwCoordinateSystem::modeFor("EPSG:32760")    == Mode::UTM);
        CHECK(cwCoordinateSystem::utmZoneFor("EPSG:32760")  == 60);
        CHECK(cwCoordinateSystem::utmNorthFor("EPSG:32760") == false);
    }

    SECTION("A datum's own UTM series is UTM, on that datum (ETRS89/UTM 32N)") {
        CHECK(cwCoordinateSystem::modeFor("EPSG:25832")    == Mode::UTM);
        CHECK(cwCoordinateSystem::utmZoneFor("EPSG:25832")  == 32);
        CHECK(cwCoordinateSystem::utmNorthFor("EPSG:25832") == true);
        CHECK(cwCoordinateSystem::datumFor("EPSG:25832")    == "EPSG:4258");
    }

    SECTION("OSGB / Lambert / arbitrary → Custom") {
        CHECK(cwCoordinateSystem::modeFor("EPSG:27700") == Mode::Custom);
        CHECK(cwCoordinateSystem::modeFor("EPSG:2154")  == Mode::Custom);
        CHECK(cwCoordinateSystem::modeFor("ESRI:54030") == Mode::Custom);
    }

    SECTION("Out-of-range UTM EPSG (zone 00 / 99) is Custom, not UTM") {
        CHECK(cwCoordinateSystem::modeFor("EPSG:32600") == Mode::Custom);
        CHECK(cwCoordinateSystem::modeFor("EPSG:32661") == Mode::Custom);
        CHECK(cwCoordinateSystem::modeFor("EPSG:32700") == Mode::Custom);
        CHECK(cwCoordinateSystem::modeFor("EPSG:32761") == Mode::Custom);
    }

    SECTION("utmZoneFor returns -1 for non-UTM modes") {
        CHECK(cwCoordinateSystem::utmZoneFor("")           == -1);
        CHECK(cwCoordinateSystem::utmZoneFor("EPSG:4326")  == -1);
        CHECK(cwCoordinateSystem::utmZoneFor("EPSG:27700") == -1);
    }
}

namespace {
    //! One row of the datum table, restated independently of it: what proj.db is
    //! expected to call the geographic code, and the label CaveWhere shows.
    struct DatumExpectation {
        const char* code;
        const char* projName;
        const char* displayName;
    };

    constexpr DatumExpectation kExpectedDatums[] = {
        { "EPSG:4326", "WGS 84",          "WGS84"           },
        { "EPSG:6318", "NAD83(2011)",     "NAD83(2011)"     },
        { "EPSG:4617", "NAD83(CSRS)",     "NAD83(CSRS)"     },
        { "EPSG:6365", "Mexico ITRF2008", "Mexico ITRF2008" },
        { "EPSG:4258", "ETRS89",          "ETRS89"          },
        { "EPSG:6668", "JGD2011",         "JGD2011"         },
        { "EPSG:7844", "GDA2020",         "GDA2020"         },
        { "EPSG:4167", "NZGD2000",        "NZGD2000"        },
    };

    /**
     * A datum's UTM series as the table is expected to build it. `firstCode`
     * pins the base — a base off by one still produces valid EPSG codes, so
     * only naming the code catches it — and `nameFormat` (with %1 the zone) is
     * what proj.db must call every code the series builds.
     */
    struct UtmSeriesExpectation {
        const char* datumCode;
        bool north;
        int zoneMin;
        int zoneMax;
        const char* firstCode;
        const char* nameFormat;
    };

    constexpr UtmSeriesExpectation kExpectedSeries[] = {
        { "EPSG:4326", true,   1, 60, "EPSG:32601", "WGS 84 / UTM zone %1N"          },
        { "EPSG:4326", false,  1, 60, "EPSG:32701", "WGS 84 / UTM zone %1S"          },
        { "EPSG:6318", true,   1, 19, "EPSG:6330",  "NAD83(2011) / UTM zone %1N"     },
        { "EPSG:6365", true,  11, 16, "EPSG:6366",  "Mexico ITRF2008 / UTM zone %1N" },
        { "EPSG:4258", true,  28, 38, "EPSG:25828", "ETRS89 / UTM zone %1N"          },
        { "EPSG:6668", true,  51, 55, "EPSG:6688",  "JGD2011 / UTM zone %1N"         },
        { "EPSG:7844", false, 46, 59, "EPSG:7846",  "GDA2020 / MGA zone %1"          },
        { "EPSG:4167", false, 58, 60, "EPSG:2133",  "NZGD2000 / UTM zone %1S"        },
    };

    //! The lowest and highest UTM zone numbers any series may name.
    constexpr int kFirstUtmZone = 1;
    constexpr int kLastUtmZone = 60;

    //! The series covering \a datumCode on \a north, or nullptr when the datum
    //! has none there.
    const UtmSeriesExpectation* expectedSeries(const QString& datumCode, bool north)
    {
        for (const UtmSeriesExpectation& series : kExpectedSeries) {
            if (datumCode == QLatin1StringView(series.datumCode) && series.north == north) {
                return &series;
            }
        }
        return nullptr;
    }

    //! Whether \a datumCode's expected series names \a zone on \a north.
    bool seriesCovers(const QString& datumCode, int zone, bool north)
    {
        const UtmSeriesExpectation* series = expectedSeries(datumCode, north);
        return series && zone >= series->zoneMin && zone <= series->zoneMax;
    }

    //! What a lidar tile declares: a projected horizontal CRS and a vertical one,
    //! spelled the WKT1 way a LAS 1.4 header carries.
    const char* const kLazCompoundWkt = R"WKT(COMPD_CS["NAD83(2011) / UTM zone 17N + NAVD88 height",
    PROJCS["NAD83(2011) / UTM zone 17N",
        GEOGCS["NAD83(2011)",
            DATUM["NAD83_National_Spatial_Reference_System_2011",
                SPHEROID["GRS 1980",6378137,298.257222101,
                    AUTHORITY["EPSG","7019"]],
                AUTHORITY["EPSG","1116"]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433,
                AUTHORITY["EPSG","9122"]],
            AUTHORITY["EPSG","6318"]],
        PROJECTION["Transverse_Mercator"],
        PARAMETER["latitude_of_origin",0],
        PARAMETER["central_meridian",-81],
        PARAMETER["scale_factor",0.9996],
        PARAMETER["false_easting",500000],
        PARAMETER["false_northing",0],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Easting",EAST],
        AXIS["Northing",NORTH],
        AUTHORITY["EPSG","6346"]],
    VERT_CS["NAVD88 height",
        VERT_DATUM["North American Vertical Datum 1988",2005,
            AUTHORITY["EPSG","5103"]],
        UNIT["metre",1,
            AUTHORITY["EPSG","9001"]],
        AXIS["Gravity-related height",UP],
        AUTHORITY["EPSG","5703"]]])WKT";
}

TEST_CASE("cwCoordinateSystem's datum table matches proj.db", "[cwCoordinateSystem][datumTable]")
{
    SECTION("Every geographic code resolves to the datum it claims")
    {
        const QStringList datums = cwCoordinateSystem::datumList();
        REQUIRE(datums.size() == std::ssize(kExpectedDatums));
        CHECK(datums.first() == "EPSG:4326");

        for (int i = 0; i < std::ssize(kExpectedDatums); ++i) {
            const DatumExpectation& expected = kExpectedDatums[i];
            const QString code = QString::fromLatin1(expected.code);
            INFO("datum " << expected.code);

            CHECK(datums.at(i) == code);
            CHECK(cwCoordinateTransform::nameFor(code) == QLatin1StringView(expected.projName));
            CHECK(cwCoordinateTransform::isGeographic(code));
            CHECK(cwCoordinateSystem::datumDisplayName(code)
                  == QLatin1StringView(expected.displayName));
        }
    }

    SECTION("Every UTM code the table builds is that datum's zone in proj.db")
    {
        for (const UtmSeriesExpectation& series : kExpectedSeries) {
            const QString datum = QString::fromLatin1(series.datumCode);
            INFO("series " << series.datumCode << (series.north ? " north" : " south"));

            CHECK(cwCoordinateSystem::utmZoneToEpsg(series.zoneMin, series.north, datum)
                  == QLatin1StringView(series.firstCode));

            for (int zone = series.zoneMin; zone <= series.zoneMax; ++zone) {
                const QString cs = cwCoordinateSystem::utmZoneToEpsg(zone, series.north, datum);
                INFO("zone " << zone << " -> " << cs.toStdString());
                REQUIRE_FALSE(cs.isEmpty());
                CHECK(cwCoordinateTransform::nameFor(cs)
                      == QString::fromLatin1(series.nameFormat).arg(zone));
            }
        }
    }

    SECTION("A code is built exactly where a series covers the zone")
    {
        for (const QString& datum : cwCoordinateSystem::datumList()) {
            for (const bool north : {true, false}) {
                for (int zone = kFirstUtmZone; zone <= kLastUtmZone; ++zone) {
                    const bool built =
                        !cwCoordinateSystem::utmZoneToEpsg(zone, north, datum).isEmpty();
                    INFO(datum.toStdString() << (north ? " north" : " south") << " zone " << zone);
                    CHECK(built == seriesCovers(datum, zone, north));
                }
            }
        }
    }

    SECTION("NAD83(CSRS) ships lat/long only")
    {
        // Its UTM zones are scattered across three unrelated EPSG blocks, so no
        // base plus zone reaches them.
        CHECK(cwCoordinateSystem::latLonCS("EPSG:4617") == "EPSG:4617");
        CHECK(expectedSeries(QStringLiteral("EPSG:4617"), true) == nullptr);
        CHECK(expectedSeries(QStringLiteral("EPSG:4617"), false) == nullptr);
    }
}

TEST_CASE("cwCoordinateSystem round-trips every datum's CS strings",
          "[cwCoordinateSystem][parseCS]")
{
    using Mode = cwCoordinateSystem::Mode;

    SECTION("latLonCS parses back to LatLon on the same datum")
    {
        for (const QString& datum : cwCoordinateSystem::datumList()) {
            const QString cs = cwCoordinateSystem::latLonCS(datum);
            INFO("datum " << datum.toStdString());
            CHECK(cs == datum);
            CHECK(cwCoordinateSystem::modeFor(cs) == Mode::LatLon);
            CHECK(cwCoordinateSystem::datumFor(cs) == datum);
            CHECK(cwCoordinateSystem::utmZoneFor(cs) == -1);
        }
    }

    SECTION("utmZoneToEpsg parses back to the same zone, hemisphere and datum")
    {
        for (const UtmSeriesExpectation& series : kExpectedSeries) {
            const QString datum = QString::fromLatin1(series.datumCode);
            for (int zone = series.zoneMin; zone <= series.zoneMax; ++zone) {
                const QString cs = cwCoordinateSystem::utmZoneToEpsg(zone, series.north, datum);
                INFO(cs.toStdString());
                CHECK(cwCoordinateSystem::modeFor(cs) == Mode::UTM);
                CHECK(cwCoordinateSystem::utmZoneFor(cs) == zone);
                CHECK(cwCoordinateSystem::utmNorthFor(cs) == series.north);
                CHECK(cwCoordinateSystem::datumFor(cs) == datum);
            }
        }
    }

    SECTION("Zones past a series edge build nothing")
    {
        for (const UtmSeriesExpectation& series : kExpectedSeries) {
            const QString datum = QString::fromLatin1(series.datumCode);
            INFO("series " << series.datumCode << (series.north ? " north" : " south"));
            CHECK(cwCoordinateSystem::utmZoneToEpsg(series.zoneMin - 1, series.north, datum)
                      .isEmpty());
            CHECK(cwCoordinateSystem::utmZoneToEpsg(series.zoneMax + 1, series.north, datum)
                      .isEmpty());
        }

        CHECK(cwCoordinateSystem::utmZoneToEpsg(0, true).isEmpty());
        CHECK(cwCoordinateSystem::utmZoneToEpsg(61, true).isEmpty());
        CHECK(cwCoordinateSystem::utmZoneToEpsg(0, false).isEmpty());
        CHECK(cwCoordinateSystem::utmZoneToEpsg(61, false).isEmpty());
    }

    SECTION("The two-argument utmZoneToEpsg still means WGS84")
    {
        CHECK(cwCoordinateSystem::utmZoneToEpsg(16, true) == "EPSG:32616");
        CHECK(cwCoordinateSystem::utmZoneToEpsg(16, true)
              == cwCoordinateSystem::utmZoneToEpsg(16, true, cwCoordinateSystem::wgs84()));
        CHECK(cwCoordinateSystem::utmZoneToEpsg(60, false)
              == cwCoordinateSystem::utmZoneToEpsg(60, false, cwCoordinateSystem::wgs84()));
    }

    SECTION("A datum outside the table names nothing")
    {
        CHECK(cwCoordinateSystem::latLonCS("EPSG:26916").isEmpty());
        CHECK(cwCoordinateSystem::latLonCS("").isEmpty());
        CHECK(cwCoordinateSystem::datumDisplayName("EPSG:26916").isEmpty());
        CHECK(cwCoordinateSystem::utmZoneToEpsg(16, true, "EPSG:26916").isEmpty());
    }

    SECTION("A system the table doesn't spell stays Custom with no datum")
    {
        CHECK(cwCoordinateSystem::modeFor("EPSG:26916") == Mode::Custom);
        CHECK(cwCoordinateSystem::datumFor("EPSG:26916").isEmpty());
        CHECK(cwCoordinateSystem::datumFor("EPSG:27700").isEmpty());
        CHECK(cwCoordinateSystem::datumFor("").isEmpty());
        CHECK(cwCoordinateSystem::datumFor("not a coordinate system").isEmpty());
    }

    SECTION("utmDatumList offers exactly the datums whose series reaches the zone")
    {
        for (const bool north : {true, false}) {
            for (int zone = kFirstUtmZone; zone <= kLastUtmZone; ++zone) {
                QStringList expected;
                for (const QString& datum : cwCoordinateSystem::datumList()) {
                    if (seriesCovers(datum, zone, north)) {
                        expected.append(datum);
                    }
                }
                INFO("zone " << zone << (north ? " north" : " south"));
                CHECK(cwCoordinateSystem::utmDatumList(zone, north) == expected);
            }
        }

        // The shape of that, spelled out: zone 16N is North American, zone 32N
        // European, and WGS84 is everywhere.
        CHECK(cwCoordinateSystem::utmDatumList(16, true)
              == QStringList({"EPSG:4326", "EPSG:6318", "EPSG:6365"}));
        CHECK(cwCoordinateSystem::utmDatumList(32, true)
              == QStringList({"EPSG:4326", "EPSG:4258"}));
        CHECK(cwCoordinateSystem::utmDatumList(59, false)
              == QStringList({"EPSG:4326", "EPSG:7844", "EPSG:4167"}));
    }
}

TEST_CASE("cwCoordinateTransform::geographicDatumFor names the datum a system is on",
          "[cwCoordinateSystem][geographicDatumFor]")
{
    SECTION("A lidar tile's compound WKT answers for its horizontal half")
    {
        CHECK(cwCoordinateTransform::geographicDatumFor(QString::fromLatin1(kLazCompoundWkt))
              == "EPSG:6318");
    }

    SECTION("A compound spelled as two authority codes answers the same")
    {
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:6346+EPSG:5703") == "EPSG:6318");
    }

    SECTION("A derived frame's WKT2 answers with the datum it was built over")
    {
        // A plain lat/long in the conterminous US derives a frame on NAD83(2011),
        // and that frame is stored as WKT2 because the datum has no proj-string
        // spelling.
        constexpr double kUsLatitude = 37.0;
        constexpr double kUsLongitude = -84.0;
        const QString frame = cwLocalProjection::derive(kUsLatitude, kUsLongitude, QString());
        REQUIRE_FALSE(frame.isEmpty());
        CHECK(cwCoordinateTransform::geographicDatumFor(frame) == "EPSG:6318");
    }

    SECTION("Plain codes answer for themselves and for their UTM zones")
    {
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:4326") == "EPSG:4326");
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:32616") == "EPSG:4326");
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:6318") == "EPSG:6318");
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:6345") == "EPSG:6318");
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:25832") == "EPSG:4258");
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:7855") == "EPSG:7844");
    }

    SECTION("A datum outside the table has no answer")
    {
        // NAD83 (1986) is a real datum the table doesn't name.
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:26916").isEmpty());
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:27700").isEmpty());
    }

    SECTION("Garbage has no answer")
    {
        CHECK(cwCoordinateTransform::geographicDatumFor("").isEmpty());
        CHECK(cwCoordinateTransform::geographicDatumFor("   ").isEmpty());
        CHECK(cwCoordinateTransform::geographicDatumFor("not a coordinate system").isEmpty());
        CHECK(cwCoordinateTransform::geographicDatumFor("EPSG:999999").isEmpty());
        CHECK(cwCoordinateTransform::geographicDatumFor("COMPD_CS[\"broken").isEmpty());
    }
}
