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
    const auto rowFlagged = [](cwCave* cave) {
        cwFixStationDiagnosticsModel* diagnostics = cave->fixStationDiagnostics();
        return !diagnostics->data(diagnostics->index(0, 0),
                                  cwFixStationDiagnosticsModel::DomainErrorRole)
                    .toString().isEmpty();
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
