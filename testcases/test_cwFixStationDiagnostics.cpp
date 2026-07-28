/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwErrorModel.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnostics.h"
#include "cwFixStationDiagnosticsModel.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwSurveyNetwork.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QModelIndex>
#include <QStringList>

namespace {

using StationReference = cwFixStationDiagnostics::StationReference;

// An in-zone Colorado location in UTM 13N, and the same point with a transposed
// leading digit on the easting — ~1000 km east of the zone, so its longitude
// leaves the CS's area of use while the latitude stays inside it.
constexpr double kGoodEasting = 478000.0;
constexpr double kBadEasting = 1478000.0;
constexpr double kNorthing = 4430000.0;
constexpr double kElevation = 1655.0;

cwFixStation makeFix(const QString& cs, double easting, double northing = kNorthing)
{
    cwFixStation fix;
    fix.setStationName(QStringLiteral("A"));
    fix.setInputCS(cs);
    fix.setEasting(easting);
    fix.setNorthing(northing);
    fix.setElevation(kElevation);
    return fix;
}

//! Whether the first row of \a cave's diagnostics carries a domain complaint —
//! what FixStationPage tints the cell on.
bool rowFlagged(cwCave* cave)
{
    cwFixStationDiagnosticsModel* diagnostics = cave->fixStationDiagnostics();
    return !diagnostics->data(diagnostics->index(0, 0),
                              cwFixStationDiagnosticsModel::DomainErrorRole)
                .toString().isEmpty();
}

} // namespace

TEST_CASE("cwFixStationDiagnostics::domainCheck resolves the CS it judges under",
          "[FixStation][cwFixStationDiagnostics]")
{
    SECTION("A fix's own input CS wins over the region's global CS") {
        // The fix is fine for UTM 13N; the global CS is a zone it would fail in,
        // and must be ignored while the fix carries its own.
        const cwFixStation fix = makeFix(QStringLiteral("EPSG:32613"), kGoodEasting);
        CHECK(cwFixStationDiagnostics::isDomainValid(fix, QStringLiteral("EPSG:32601")));
    }

    SECTION("A fix with no input CS is judged under the global CS") {
        const cwFixStation fix = makeFix(QString(), kBadEasting);
        CHECK(cwFixStationDiagnostics::isDomainValid(fix, QString()));
        CHECK_FALSE(cwFixStationDiagnostics::isDomainValid(fix, QStringLiteral("EPSG:32613")));
    }

    SECTION("Whitespace-only CS strings count as absent") {
        const cwFixStation fix = makeFix(QStringLiteral("   "), kBadEasting);
        CHECK(cwFixStationDiagnostics::isDomainValid(fix, QStringLiteral("  ")));
    }

    SECTION("A fix still on the origin has no coordinate to judge") {
        // "Mark Station as Fixed" creates the row before the user types anything,
        // so a default fix must not open its popup on a red warning. The CS is a
        // southern-hemisphere zone, where the origin lands near the pole — the
        // deferral has to come from the fix, not from the coordinate happening to
        // land somewhere harmless.
        const QString cs = QStringLiteral("EPSG:28355");

        // The premise, asserted rather than assumed: without the guard this fix
        // really would be flagged, so the check below cannot pass for free.
        REQUIRE_FALSE(cwCoordinateTransform::domainCheck(cs, cwGeoPoint(0.0, 0.0, 0.0))
                          .northingValid);

        cwFixStation untouched;
        untouched.setStationName(QStringLiteral("A"));
        untouched.setInputCS(cs);
        CHECK(cwFixStationDiagnostics::isDomainValid(untouched, QString()));

        // One zero is not two. A real easting with the northing still at zero is
        // a half-entered coordinate, not an unentered one, and stays flagged.
        cwFixStation halfEntered = untouched;
        halfEntered.setEasting(kGoodEasting);
        CHECK_FALSE(cwFixStationDiagnostics::isDomainValid(halfEntered, QString()));

        // And a coordinate someone actually wrote at the origin is judged like
        // any other. Some local grids really do put a station there; deferring
        // on the numbers couldn't tell it from the untouched row above.
        cwFixStation atOrigin = untouched;
        atOrigin.setCoordinate(QStringLiteral("0, 0, 0m"));
        REQUIRE(atOrigin.state() == cwFixStation::Valid);
        CHECK_FALSE(cwFixStationDiagnostics::isDomainValid(atOrigin, QString()));

        // A row whose text can't be read has no coordinate to judge either, and
        // must not be reported as one sitting out of domain at 0, 0 — the text
        // is what's wrong with it, and that is a different complaint.
        cwFixStation unreadable = untouched;
        unreadable.setCoordinate(QStringLiteral("N 46 07 16 W 115 35 56"));
        REQUIRE(unreadable.state() == cwFixStation::Unreadable);
        CHECK(cwFixStationDiagnostics::isDomainValid(unreadable, QString()));
    }

    SECTION("Per-axis attribution survives the fix-level wrapper") {
        // The page tints only the offending cell, so domainCheck() must not
        // collapse to one bool the way isDomainValid() does.
        const cwFixStation fix = makeFix(QStringLiteral("EPSG:32613"), kBadEasting);
        const cwCoordinateTransform::DomainCheck check =
            cwFixStationDiagnostics::domainCheck(fix, QString());
        CHECK_FALSE(check.eastingValid);
        CHECK(check.northingValid);
    }
}

TEST_CASE("cwFixStationDiagnostics::classifyStationReference measures a name against the network",
          "[FixStation][cwFixStationDiagnostics]")
{
    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));

    SECTION("A name in the network is Ok") {
        CHECK(cwFixStationDiagnostics::classifyStationReference(QStringLiteral("A1"), network)
              == StationReference::Ok);
    }

    SECTION("Matching is case-insensitive and ignores surrounding whitespace") {
        CHECK(cwFixStationDiagnostics::classifyStationReference(QStringLiteral(" a1 "), network)
              == StationReference::Ok);
    }

    SECTION("A name no station matches is Unknown") {
        CHECK(cwFixStationDiagnostics::classifyStationReference(QStringLiteral("Z9"), network)
              == StationReference::Unknown);
    }

    SECTION("An empty or whitespace-only name is Empty") {
        CHECK(cwFixStationDiagnostics::classifyStationReference(QString(), network)
              == StationReference::Empty);
        CHECK(cwFixStationDiagnostics::classifyStationReference(QStringLiteral("  "), network)
              == StationReference::Empty);
    }

    SECTION("An empty network defers a named fix but still flags a blank one") {
        const cwSurveyNetwork empty;
        CHECK(cwFixStationDiagnostics::classifyStationReference(QStringLiteral("Z9"), empty)
              == StationReference::Ok);
        CHECK(cwFixStationDiagnostics::classifyStationReference(QString(), empty)
              == StationReference::Empty);
    }
}

TEST_CASE("the inline row flag and the cave warning reach the same verdict",
          "[FixStation][cwFixStationDiagnostics]")
{
    // R4's invariant, made executable: cwFixStationDiagnosticsModel tints a cell
    // on FixStationPage while cwFixStationValidator raises a cave-level banner,
    // both from the same fix. Sourcing both from cwFixStationDiagnostics is what
    // keeps them from disagreeing — a tinted cell with no banner, or the reverse,
    // would read to the user as one of the two being wrong.
    const auto domainWarned = [](cwCave* cave) {
        return cave->errorModel()->toStringList().join(QChar(' '))
            .contains(QStringLiteral("outside the valid range"));
    };
    // Three caves under one staging: a clean fix, a bad one under its own CS, and
    // a bad one relying on the region's global CS. The fallback case is the one
    // most likely to drift apart again, since it is where the two paths used to
    // resolve the CS independently.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    const auto addCaveWithFix = [&](const cwFixStation& fix) {
        region.addCave();
        cwCave* cave = region.cave(region.caveCount() - 1);
        REQUIRE(cave != nullptr);
        cave->fixStations()->appendFixStation(fix);
        return cave;
    };

    cwCave* goodCave = addCaveWithFix(makeFix(QStringLiteral("EPSG:32613"), kGoodEasting));
    cwCave* badOwnCS = addCaveWithFix(makeFix(QStringLiteral("EPSG:32613"), kBadEasting));
    cwCave* badFallback = addCaveWithFix(makeFix(QString(), kBadEasting));

    CHECK(domainWarned(badOwnCS));
    CHECK(rowFlagged(badOwnCS));

    CHECK(domainWarned(badFallback));
    CHECK(rowFlagged(badFallback));

    // The negative half matters as much: a rule that always flagged would satisfy
    // every assertion above.
    CHECK_FALSE(domainWarned(goodCave));
    CHECK_FALSE(rowFlagged(goodCave));
}

TEST_CASE("correcting a fix's coordinate system clears a coordinate read the wrong way round",
          "[FixStation][cwFixStationDiagnostics]")
{
    // End to end: a UTM pair pasted into a row whose CS says geographic is read
    // latitude first and lands transposed, where the domain check catches it.
    // Correcting the CS is the whole fix — the coordinate is the string, and the
    // string was right all along; it was only ever being read wrong.
    cwCavingRegion region;
    region.addCave();
    cwCave* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwFixStationModel* fixes = cave->fixStations();
    fixes->addFixStation();
    const QModelIndex idx = fixes->index(0);
    fixes->setData(idx, QStringLiteral("EPSG:4326"), cwFixStationModel::InputCSRole);

    const QString typed = QStringLiteral("610016.792, 5615117.075, 304m");
    REQUIRE(fixes->setCoordinateText(0, typed, cwUnits::Metric) == QString());
    REQUIRE(fixes->fixStationAt(0).northing() == 610016.792);
    CHECK(rowFlagged(cave));

    fixes->setData(idx, QStringLiteral("EPSG:32611"), cwFixStationModel::InputCSRole);
    CHECK(fixes->fixStationAt(0).easting() == 610016.792);
    CHECK(fixes->fixStationAt(0).northing() == 5615117.075);
    // Re-read, not rewritten: the user's own words are still what the row holds.
    CHECK(fixes->fixStationAt(0).coordinate() == typed);
    CHECK_FALSE(rowFlagged(cave));
}
