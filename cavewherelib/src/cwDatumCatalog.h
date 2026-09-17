/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDATUMCATALOG_H
#define CWDATUMCATALOG_H

//Std includes
#include <span>

//
// The one place a datum CaveWhere offers is described: what to call it, which
// UTM series belongs to it, and where on Earth it is the customary frame.
// cwCoordinateTransform.cpp reads the naming and UTM columns; cwLocalProjection.cpp
// reads the regions. A datum is added by adding its region array and one kDatums
// row here, plus its expected row in test_cwCoordinateTransform.cpp, where proj.db
// vouches for the numbers.
//

namespace cwDatumCatalog {

    //! A latitude/longitude box one datum covers. The box is coarse on purpose:
    //! it decides a datum, and neighboring national frames agree to within
    //! centimeters along the borders they share.
    struct Region {
        double minLatitude;
        double maxLatitude;
        double minLongitude;
        double maxLongitude;
    };

    /**
     * A geodetic datum CaveWhere can spell a coordinate on, with the UTM series
     * belonging to it. `regionName` is the part of the world the datum serves,
     * which is how the picker sorts one short acronym from another.
     * `utmNorthBase`/`utmSouthBase` are the EPSG code a zone
     * number is added to; kNoUtmSeries means the datum has no series on that
     * hemisphere. `utmZoneMin`/`utmZoneMax` bound the zones the series covers,
     * because most series run only across the datum's own part of the world and
     * the codes past the end belong to something else entirely. `regions` are the
     * boxes where this datum is the plate-fixed frame a local projection adopts;
     * WGS84 has none, because it is the answer where no plate-fixed frame reaches.
     */
    struct Datum {
        const char* geographicCode;
        const char* displayName;
        const char* regionName;
        int utmNorthBase;
        int utmSouthBase;
        int utmZoneMin;
        int utmZoneMax;
        std::span<const Region> regions;
    };

    inline constexpr int kNoUtmSeries = 0;

    /**
     * Where each national plate-fixed frame applies, the datums in kDatums order
     * and each datum's own boxes in order, first match winning.
     *
     * These frames are tied to their own plate, so a cave keeps the coordinates
     * it was surveyed on; WGS84 is tied to the whole Earth and slides under North
     * America by about 2 cm a year.
     *
     * The United States comes first, so the strips it shares with Canada and
     * Mexico resolve to NAD83(2011) — the two answers there differ by a few
     * centimeters, and a box drawn along the real border would still be a guess
     * about which side of it a cave is on. Alaska takes two boxes so that its
     * main box stops at the 141st meridian, the border it shares with the
     * Yukon, and the panhandle is the strip east of it: one box out to -129
     * would reach Whitehorse and most of the Yukon, which is inland Canada
     * rather than a shared border.
     *
     * Europe takes four boxes because ETRS89 is tied to the stable part of the
     * Eurasian plate and stops where Europe does: the boxes are drawn to leave
     * out North Africa (Morocco reaches 35.9N, Algeria 37.1N and Tunisia 37.4N)
     * and Anatolia, which are on plates of their own and have national frames
     * of their own. The southern Spanish coast and the Aegean islands fall
     * outside them and keep WGS84, which is the modest answer rather than a
     * wrong one.
     *
     * These frames get replaced on decade scales (NAD83 → NATRF2022 is coming).
     * A changed entry only reaches frames derived after it changed, because a
     * stored frame is never re-derived.
     */
    inline constexpr Region kUnitedStatesRegions[] = {
        {  24.5,  49.5, -125.0,  -66.5 },  // Conterminous US
        {  51.0,  72.0, -173.0, -141.0 },  // Alaska west of the Yukon border
        {  54.5,  60.5, -141.0, -129.5 },  // The Alaskan panhandle
        {  18.0,  23.0, -161.0, -154.0 },  // Hawaii
        {  17.5,  18.6,  -68.0,  -64.5 },  // Puerto Rico and the Virgin Islands
    };

    inline constexpr Region kCanadaRegions[] = {
        {  41.5,  84.0, -141.0,  -52.0 },
    };

    inline constexpr Region kMexicoRegions[] = {
        {  14.0,  33.0, -118.0,  -86.0 },
    };

    inline constexpr Region kEuropeRegions[] = {
        {  36.0,  72.0,  -12.0,   -1.0 },  // Iberia and the British Isles
        {  37.5,  72.0,   -1.0,   12.0 },  // France to western Italy and Scandinavia
        {  34.0,  42.0,   12.0,   26.0 },  // Sicily, the Adriatic and Greece
        {  42.0,  72.0,   12.0,   40.0 },  // Central and eastern Europe
    };

    inline constexpr Region kJapanRegions[] = {
        {  24.0,  46.0,  122.0,  154.0 },
    };

    inline constexpr Region kAustraliaRegions[] = {
        { -44.0,  -9.0,  112.0,  154.0 },
    };

    inline constexpr Region kNewZealandRegions[] = {
        { -48.0, -33.0,  166.0,  179.0 },
    };

    /**
     * WGS84 first, then the eight datums a local projection can adopt, so a fix
     * can be typed on the same datum the frame and the lidar tiles hold still
     * against.
     *
     * A static table rather than a proj.db query, for two reasons. Curation:
     * proj.db knows thousands of datums, and this table states which ones
     * CaveWhere offers, what to call them, and which UTM series the picker
     * exposes — a product decision proj.db can't answer. Cost: parseCS runs in
     * QML binding paths per fix-station row, and the table keeps it at pure
     * string and integer matching. Every code here is checked against the
     * bundled proj.db by test_cwCoordinateTransform's datum table cases — that
     * test is what makes the numbers trustworthy, so a row that disagrees with
     * proj.db is a wrong row, never a wrong test.
     *
     * NAD83(CSRS) ships lat/long only: its UTM zones are scattered across three
     * unrelated EPSG blocks, so no base plus zone reaches them.
     */
    inline constexpr Datum kDatums[] = {
        { "EPSG:4326", "WGS84",           "World (GPS)",          32600,        32700,  1, 60, {}                   },
        { "EPSG:6318", "NAD83(2011)",     "North America (USA)",   6329, kNoUtmSeries,  1, 19, kUnitedStatesRegions },
        { "EPSG:4617", "NAD83(CSRS)",     "Canada",         kNoUtmSeries, kNoUtmSeries,  0,  0, kCanadaRegions       },
        { "EPSG:6365", "Mexico ITRF2008", "Mexico",                6355, kNoUtmSeries, 11, 16, kMexicoRegions       },
        { "EPSG:4258", "ETRS89",          "Europe",               25800, kNoUtmSeries, 28, 38, kEuropeRegions       },
        { "EPSG:6668", "JGD2011",         "Japan",                 6637, kNoUtmSeries, 51, 55, kJapanRegions        },
        { "EPSG:7844", "GDA2020",         "Australia",      kNoUtmSeries,         7800, 46, 59, kAustraliaRegions    },
        { "EPSG:4167", "NZGD2000",        "New Zealand",    kNoUtmSeries,         2075, 58, 60, kNewZealandRegions   },
    };
}

#endif // CWDATUMCATALOG_H
