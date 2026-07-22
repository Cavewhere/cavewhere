/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwFixStationValidator.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QUuid>

namespace {

using FixCandidate = cwFixStationValidator::FixCandidate;

// A candidate at a raw coordinate — the pure classifier only looks at .global,
// so cave/fixId can stay defaulted unless a test asserts on provenance.
FixCandidate candidateAt(double x, double y, double z = 0.0)
{
    return FixCandidate{nullptr, QUuid(), cwGeoPoint{x, y, z}};
}

cwFixStation makeFix(const QString& name,
                     const QString& cs,
                     double easting,
                     double northing,
                     double elevation)
{
    cwFixStation fix;
    fix.setStationName(name);
    fix.setInputCS(cs);
    fix.setEasting(easting);
    fix.setNorthing(northing);
    fix.setElevation(elevation);
    return fix;
}

// Append four fixes in a tight EPSG:32612 cluster — enough to establish a
// cluster so a fifth straggler can be judged an outlier.
void appendGoodCluster(cwCave* cave)
{
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32612"), 500200.0, 4194100.0, 2710.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A3"), QStringLiteral("EPSG:32612"), 499900.0, 4193900.0, 2705.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A4"), QStringLiteral("EPSG:32612"), 500100.0, 4193950.0, 2708.0));
}

} // namespace

TEST_CASE("classifyCandidates flags an isolated outlier",
          "[cwFixStationValidator]")
{
    // A tight cluster of five plus one point ~500 km away.
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(10.0, -5.0),
        candidateAt(-8.0, 12.0),
        candidateAt(3.0, 3.0),
        candidateAt(-4.0, -6.0),
        candidateAt(500000.0, 0.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    REQUIRE(result.outliers.size() == 1);
    CHECK(result.inliers.size() == 5);
    CHECK(result.outliers.first().global.x == 500000.0);
}

TEST_CASE("classifyCandidates keeps a tight cluster clean",
          "[cwFixStationValidator]")
{
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(10.0, -5.0),
        candidateAt(-8.0, 12.0),
        candidateAt(3.0, 3.0),
        candidateAt(-4.0, -6.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 5);
}

TEST_CASE("classifyCandidates ignores a legitimately spread-out survey",
          "[cwFixStationValidator]")
{
    // Two real clusters ~200 km apart: every point is far from the median
    // center, so the cluster-radius term lifts the threshold above the floor
    // and nothing is flagged.
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(1000.0, 500.0),
        candidateAt(-800.0, 300.0),
        candidateAt(200000.0, 0.0),
        candidateAt(201000.0, 400.0),
        candidateAt(199000.0, -600.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 6);
}

TEST_CASE("classifyCandidates does not flag below the minimum fix count",
          "[cwFixStationValidator]")
{
    // Two points, one obviously distant — but too few to establish a cluster,
    // so detection is off and everything is an inlier.
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(500000.0, 0.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 2);
}

TEST_CASE("classifyCandidates flags a straggler once a two-point majority exists",
          "[cwFixStationValidator]")
{
    // Three points: two clustered, one ~500 km away. With the relaxed minimum
    // (3), the component-wise median lands on the majority pair, so the odd one
    // out is judged an outlier — the realistic "cluster + one straggler cave"
    // shape the old four-fix floor would have missed.
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(10.0, 5.0),
        candidateAt(500000.0, 0.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    REQUIRE(result.outliers.size() == 1);
    CHECK(result.inliers.size() == 2);
    CHECK(result.outliers.first().global.x == 500000.0);
}

TEST_CASE("classifyCandidates separates a domain-bad fix without a cluster",
          "[cwFixStationValidator]")
{
    // Two points only — below the cluster minimum — but one is marked
    // domain-invalid (Part A). It must land in domainOutliers, leaving the good
    // one as the sole inlier, even though no cluster could be formed.
    QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(10.0, 5.0),
    };
    candidates[1].domainValid = false;

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    CHECK(result.outliers.isEmpty());
    REQUIRE(result.domainOutliers.size() == 1);
    CHECK(result.domainOutliers.first().global.x == 10.0);
    REQUIRE(result.inliers.size() == 1);
    CHECK(result.inliers.first().global.x == 0.0);
}

TEST_CASE("classifyCandidates drops a domain-bad fix before the cluster median",
          "[cwFixStationValidator]")
{
    // A wild domain-bad point sits far off; if it were left in, the median
    // center would be dragged toward it and could mask a second, subtler
    // straggler. Pulling it out first keeps the cluster judged on real data.
    QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(10.0, 5.0),
        candidateAt(-8.0, 12.0),
        candidateAt(3.0, 3.0),
        candidateAt(500000.0, 0.0),    // in-domain straggler → cluster outlier
        candidateAt(9000000.0, 0.0),   // domain-bad → pulled out first
    };
    candidates[5].domainValid = false;

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    REQUIRE(result.domainOutliers.size() == 1);
    CHECK(result.domainOutliers.first().global.x == 9000000.0);
    REQUIRE(result.outliers.size() == 1);
    CHECK(result.outliers.first().global.x == 500000.0);
    CHECK(result.inliers.size() == 4);
}

TEST_CASE("classifyCandidates leaves a near point below the precision floor alone",
          "[cwFixStationValidator]")
{
    // 50 km from a tight cluster: beyond the cluster radius, but under the
    // ~84 km float-precision floor, so it does not yet break rendering and is
    // not flagged.
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(10.0, -5.0),
        candidateAt(-8.0, 12.0),
        candidateAt(3.0, 3.0),
        candidateAt(-4.0, -6.0),
        candidateAt(50000.0, 0.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 6);
}

TEST_CASE("currentClassification flags a typo'd fix across caves with provenance",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    // Four good fixes spread across two caves, clustered near one spot.
    region.addCave();
    auto* caveA = region.cave(0);
    REQUIRE(caveA != nullptr);
    caveA->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));
    caveA->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32612"), 500200.0, 4194100.0, 2710.0));

    region.addCave();
    auto* caveB = region.cave(1);
    REQUIRE(caveB != nullptr);
    caveB->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), QStringLiteral("EPSG:32612"), 500100.0, 4193900.0, 2705.0));

    // The typo: a transposed northing puts this fix ~1000 km away.
    cwFixStation badFix =
        makeFix(QStringLiteral("B2"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0);
    const QUuid badId = badFix.id();
    caveB->fixStations()->appendFixStation(badFix);

    const auto result = region.fixStationValidator()->currentClassification();

    REQUIRE(result.outliers.size() == 1);
    CHECK(result.inliers.size() == 3);
    CHECK(result.outliers.first().cave == caveB);
    CHECK(result.outliers.first().fixId == badId);
}

TEST_CASE("currentClassification is empty on a region with no fixes",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.inliers.isEmpty());
    CHECK(result.outliers.isEmpty());
}

TEST_CASE("currentClassification reprojects a fix entered in a different CS",
          "[cwFixStationValidator]")
{
    // The global CS is UTM 12N; four good fixes are entered directly in it, plus
    // one good fix for the SAME real-world spot entered in WGS84 lon/lat. Raw, the
    // WGS84 degrees sit ~4000 km from the UTM cluster and would be flagged — so
    // classifying it as an inlier proves gatherCandidates() actually ran the
    // cwCoordinateTransform. Exercises the reproject branch the same-CS tests skip.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32612"), 500200.0, 4194100.0, 2710.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A3"), QStringLiteral("EPSG:32612"), 499900.0, 4193900.0, 2705.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A4"), QStringLiteral("EPSG:32612"), 500100.0, 4193950.0, 2708.0));
    // Same point as ~UTM 12N 500000 / 4193000, but entered as WGS84 lon/lat.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("W1"), QStringLiteral("EPSG:4326"), -111.0, 37.871, 2700.0));

    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 5);
}

TEST_CASE("currentClassification drops invalid-CS fixes and falls back for empty CS",
          "[cwFixStationValidator]")
{
    // Global CS is UTM 12N. Four good fixes, plus one with an EMPTY inputCS (should
    // fall back to the global CS and be kept) and one with a GARBAGE inputCS placed
    // far away (should be dropped entirely — never a candidate, so never flagged).
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32612"), 500200.0, 4194100.0, 2710.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A3"), QStringLiteral("EPSG:32612"), 499900.0, 4193900.0, 2705.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A4"), QStringLiteral("EPSG:32612"), 500100.0, 4193950.0, 2708.0));
    // Empty inputCS → falls back to the region global CS → kept as an inlier.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("E1"), QString(), 500050.0, 4194050.0, 2703.0));
    // Garbage CS, far away → must be dropped, not flagged as an outlier.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("NOT_A_CS"), 9000000.0, 9000000.0, 0.0));

    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 5);
}

TEST_CASE("revalidate attributes an outlier warning to its cave",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    appendGoodCluster(cave);

    // No outlier yet — the four-fix cluster is clean.
    CHECK(cave->errorModel()->warningCount() == 0);

    // Editing the model fires the validator's own connections, so appending the
    // typo re-attributes with no manual revalidate() call.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));

    REQUIRE(cave->errorModel()->warningCount() == 1);
    CHECK(cave->errorModel()->toStringList().join(QChar(' ')).contains(QStringLiteral("Bad")));
}

TEST_CASE("revalidate clears the warning when the outlier is corrected",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    appendGoodCluster(cave);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));
    REQUIRE(cave->errorModel()->warningCount() == 1);

    // Correct the typo in place — the northing back into the cluster.
    const int badRow = cave->fixStations()->count() - 1;
    cave->fixStations()->setData(cave->fixStations()->index(badRow),
                                 4194050.0,
                                 cwFixStationModel::NorthingRole);

    CHECK(cave->errorModel()->warningCount() == 0);
}

TEST_CASE("revalidate updates an existing warning in place without re-adding",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    appendGoodCluster(cave);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));
    REQUIRE(cave->errorModel()->warningCount() == 1);

    const QString firstMessage = cave->errorModel()->toStringList().join(QChar(' '));

    // Moving the outlier to a different far coordinate should update the same
    // warning row (new distance) rather than remove-and-append a new one.
    QSignalSpy warningSpy(cave->errorModel(), &cwErrorModel::warningCountChanged);
    const int badRow = cave->fixStations()->count() - 1;
    cave->fixStations()->setData(cave->fixStations()->index(badRow),
                                 6194000.0,
                                 cwFixStationModel::NorthingRole);

    CHECK(cave->errorModel()->warningCount() == 1);
    CHECK(warningSpy.size() == 0);
    CHECK(cave->errorModel()->toStringList().join(QChar(' ')) != firstMessage);
}

TEST_CASE("revalidate preserves a suppressed outlier warning across edits",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    appendGoodCluster(cave);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));

    cwErrorListModel* errors = cave->errorModel()->errors();
    REQUIRE(errors->size() == 1);

    // The user suppresses the outlier warning.
    errors->setData(errors->index(0), true,
                    static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole));
    REQUIRE(errors->at(0).suppressed());
    REQUIRE(cave->errorModel()->warningCount() == 0);

    // Moving the outlier changes the warning text: the validator must update the
    // SAME (still-suppressed) row, not orphan it and append a fresh unsuppressed
    // duplicate. Locating the row by value would fail once suppressed.
    const int badRow = cave->fixStations()->count() - 1;
    cave->fixStations()->setData(cave->fixStations()->index(badRow),
                                 6194000.0,
                                 cwFixStationModel::NorthingRole);

    CHECK(errors->size() == 1);
    CHECK(errors->at(0).suppressed());
    CHECK(cave->errorModel()->warningCount() == 0);

    // Correcting the outlier clears the suppressed warning rather than leaving it
    // stuck on a row the validator can no longer find.
    cave->fixStations()->setData(cave->fixStations()->index(badRow),
                                 4194050.0,
                                 cwFixStationModel::NorthingRole);
    CHECK(errors->size() == 0);
}

TEST_CASE("revalidate clears the warning when its cave leaves the region",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    // Cave 0 stays clean; cave 1 carries an outlier warning.
    region.addCave();
    auto* keepCave = region.cave(0);
    REQUIRE(keepCave != nullptr);
    appendGoodCluster(keepCave);

    region.addCave();
    auto* doomedCave = region.cave(1);
    REQUIRE(doomedCave != nullptr);
    appendGoodCluster(doomedCave);
    doomedCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));
    REQUIRE(doomedCave->errorModel()->warningCount() == 1);

    // Removing the offending cave must tear down its fix-station connection and
    // clear its warning while the cave is still alive (it outlives removal via the
    // undo command's deleteLater), without disturbing the surviving cave.
    region.removeCave(1);

    CHECK(doomedCave->errorModel()->warningCount() == 0);
    CHECK(keepCave->errorModel()->warningCount() == 0);
}

TEST_CASE("revalidate raises no warning without a region global CS",
          "[cwFixStationValidator]")
{
    // No region global CS: two caves whose fixes are entered in different input
    // CSs would, compared as raw coordinates, look wildly far apart. Skipping
    // classification until a global CS exists keeps a legitimate station from
    // being flagged as an outlier.
    cwCavingRegion region;

    region.addCave();
    auto* utmCave = region.cave(0);
    REQUIRE(utmCave != nullptr);
    appendGoodCluster(utmCave);

    region.addCave();
    auto* wgsCave = region.cave(1);
    REQUIRE(wgsCave != nullptr);
    // Raw WGS84 degrees sit ~4000 km from the UTM eastings/northings above.
    wgsCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("W1"), QStringLiteral("EPSG:4326"), -111.0, 37.871, 2700.0));

    CHECK(utmCave->errorModel()->warningCount() == 0);
    CHECK(wgsCave->errorModel()->warningCount() == 0);
}

TEST_CASE("classifyCandidates flags an elevation-only outlier",
          "[cwFixStationValidator]")
{
    // A tight cluster in x/y at ground level plus one point displaced only in z —
    // a transposed elevation. Only distance()'s dz term can catch it, so a
    // regression dropping dz would leave this uncaught.
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0, 0.0),
        candidateAt(10.0, -5.0, 0.0),
        candidateAt(-8.0, 12.0, 0.0),
        candidateAt(3.0, 3.0, 0.0),
        candidateAt(0.0, 0.0, 500000.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    REQUIRE(result.outliers.size() == 1);
    CHECK(result.inliers.size() == 4);
    CHECK(result.outliers.first().global.z == 500000.0);
}

TEST_CASE("revalidate summarizes outliers for the render-view overlay",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Deep Hole"));
    appendGoodCluster(cave);

    auto* validator = region.fixStationValidator();
    REQUIRE(validator != nullptr);

    // A clean cluster leaves the summary empty so the overlay stays hidden.
    CHECK(validator->property("warningMessage").toString().isEmpty());
    CHECK(validator->property("outlierCount").toInt() == 0);
    CHECK(validator->property("firstOutlierCave").value<cwCave*>() == nullptr);

    QSignalSpy messageSpy(validator, &cwFixStationValidator::warningMessageChanged);
    QSignalSpy countSpy(validator, &cwFixStationValidator::outlierCountChanged);
    QSignalSpy caveSpy(validator, &cwFixStationValidator::firstOutlierCaveChanged);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));

    const QString message = validator->property("warningMessage").toString();
    CHECK_FALSE(message.isEmpty());
    CHECK(message.contains(QStringLiteral("Deep Hole")));
    CHECK(validator->property("outlierCount").toInt() == 1);
    // The routing handle points at the offending cave so the overlay can link to it.
    CHECK(validator->property("firstOutlierCave").value<cwCave*>() == cave);
    CHECK(messageSpy.size() == 1);
    CHECK(countSpy.size() == 1);
    CHECK(caveSpy.size() == 1);

    // Correcting the coordinate empties the summary and fires the change once more.
    const int badRow = cave->fixStations()->count() - 1;
    cave->fixStations()->setData(cave->fixStations()->index(badRow),
                                 4194050.0,
                                 cwFixStationModel::NorthingRole);

    CHECK(validator->property("warningMessage").toString().isEmpty());
    CHECK(validator->property("outlierCount").toInt() == 0);
    CHECK(validator->property("firstOutlierCave").value<cwCave*>() == nullptr);
    CHECK(messageSpy.size() == 2);
    CHECK(countSpy.size() == 2);
    CHECK(caveSpy.size() == 2);
}

TEST_CASE("revalidate refreshes the summary when the offending cave is renamed",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Deep Hole"));
    appendGoodCluster(cave);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));

    auto* validator = region.fixStationValidator();
    REQUIRE(validator != nullptr);
    REQUIRE(validator->property("warningMessage").toString().contains(QStringLiteral("Deep Hole")));

    // Renaming the flagged cave must re-run so the banner names the new cave,
    // not the stale one.
    cave->setName(QStringLiteral("Old Sink"));

    const QString message = validator->property("warningMessage").toString();
    CHECK(message.contains(QStringLiteral("Old Sink")));
    CHECK_FALSE(message.contains(QStringLiteral("Deep Hole")));
}

TEST_CASE("revalidate flags a fix outside its CS's valid domain with no cluster",
          "[cwFixStationValidator]")
{
    // The realistic 2-cave typo: one good cave, one whose single fix has a
    // transposed leading digit (1478000 easting in UTM 13N — ~1000 km east of
    // the zone). Only one fix is domain-valid, so the cluster rule cannot fire;
    // the per-fix domain check (Part A) is what catches it.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    region.addCave();
    auto* goodCave = region.cave(0);
    REQUIRE(goodCave != nullptr);
    goodCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("G"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));

    region.addCave();
    auto* badCave = region.cave(1);
    REQUIRE(badCave != nullptr);
    badCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));

    REQUIRE(badCave->errorModel()->warningCount() == 1);
    CHECK(badCave->errorModel()->toStringList().join(QChar(' '))
              .contains(QStringLiteral("outside the valid range")));
    CHECK(goodCave->errorModel()->warningCount() == 0);
}

TEST_CASE("revalidate flags a distant in-domain cave once a two-cave majority exists",
          "[cwFixStationValidator]")
{
    // Three caves, one fix each, all valid UTM 13N coordinates — but one sits
    // ~1000 km north of the other two. The domain check passes (it is a real
    // in-zone location), so this is caught only by the relaxed cluster rule
    // (Part B): with an odd count the median center lands on the majority pair.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    region.addCave();
    auto* caveA = region.cave(0);
    REQUIRE(caveA != nullptr);
    caveA->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));

    region.addCave();
    auto* caveB = region.cave(1);
    REQUIRE(caveB != nullptr);
    caveB->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 478100.0, 4430100.0, 1656.0));

    region.addCave();
    auto* caveC = region.cave(2);
    REQUIRE(caveC != nullptr);
    caveC->fixStations()->appendFixStation(
        makeFix(QStringLiteral("C"), QStringLiteral("EPSG:32613"), 478000.0, 5430000.0, 1655.0));

    REQUIRE(caveC->errorModel()->warningCount() == 1);
    CHECK(caveC->errorModel()->toStringList().join(QChar(' '))
              .contains(QStringLiteral("far from the rest")));
    CHECK(caveA->errorModel()->warningCount() == 0);
    CHECK(caveB->errorModel()->warningCount() == 0);
}

TEST_CASE("revalidate flags a domain-bad cave in a balanced split the cluster rule cannot",
          "[cwFixStationValidator]")
{
    // Two caves, two fixes each. One cave's fixes both carry a transposed-digit
    // easting (out of the zone's domain). A balanced 2-vs-2 split defeats the
    // cluster rule — the median center sits midway, so neither pair strays — yet
    // Part A still flags the bad cave.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    region.addCave();
    auto* goodCave = region.cave(0);
    REQUIRE(goodCave != nullptr);
    goodCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("G1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    goodCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("G2"), QStringLiteral("EPSG:32613"), 478100.0, 4430100.0, 1656.0));

    region.addCave();
    auto* badCave = region.cave(1);
    REQUIRE(badCave != nullptr);
    badCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
    badCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B2"), QStringLiteral("EPSG:32613"), 1478100.0, 4430100.0, 1656.0));

    // Part A flags the bad cave with the domain message; the good cave is clean.
    REQUIRE(badCave->errorModel()->warningCount() == 1);
    CHECK(badCave->errorModel()->toStringList().join(QChar(' '))
              .contains(QStringLiteral("outside the valid range")));
    CHECK(goodCave->errorModel()->warningCount() == 0);

    // Prove the cluster rule alone could not: the same four points, all treated
    // as in-domain, produce no cluster outlier (median center sits midway).
    const QList<FixCandidate> balanced = {
        candidateAt(478000.0, 4430000.0), candidateAt(478100.0, 4430100.0),
        candidateAt(1478000.0, 4430000.0), candidateAt(1478100.0, 4430100.0),
    };
    CHECK(cwFixStationValidator::classifyCandidates(balanced).outliers.isEmpty());
}

TEST_CASE("revalidate does not flag two legitimately distant caves",
          "[cwFixStationValidator]")
{
    // Two caves ~300 km apart, every fix a valid in-zone UTM 13N coordinate.
    // Neither the domain check nor the balanced cluster split should fire — a
    // legitimately spread-out survey must not cry wolf.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    region.addCave();
    auto* westCave = region.cave(0);
    REQUIRE(westCave != nullptr);
    westCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("W1"), QStringLiteral("EPSG:32613"), 400000.0, 4430000.0, 1655.0));
    westCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("W2"), QStringLiteral("EPSG:32613"), 400100.0, 4430100.0, 1656.0));

    region.addCave();
    auto* eastCave = region.cave(1);
    REQUIRE(eastCave != nullptr);
    eastCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("E1"), QStringLiteral("EPSG:32613"), 700000.0, 4430000.0, 1655.0));
    eastCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("E2"), QStringLiteral("EPSG:32613"), 700100.0, 4430100.0, 1656.0));

    CHECK(westCave->errorModel()->warningCount() == 0);
    CHECK(eastCave->errorModel()->warningCount() == 0);
}

TEST_CASE("robustWorldOrigin ignores a domain-bad fix",
          "[cwFixStationValidator]")
{
    // Four good fixes plus one transposed-digit typo. The typo is domain-bad, so
    // it is excluded from the inlier centroid — otherwise it would still drag the
    // world origin ~200 km east even though it is now flagged.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32613"), 478100.0, 4430100.0, 1656.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A3"), QStringLiteral("EPSG:32613"), 477900.0, 4429900.0, 1654.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A4"), QStringLiteral("EPSG:32613"), 478050.0, 4430050.0, 1655.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));

    const auto origin = region.fixStationValidator()->robustWorldOrigin();
    REQUIRE(origin.has_value());
    // Centroid of the four good fixes (~478012 E); nowhere near the ~678000 it
    // would be if the 1478000 typo were averaged in.
    CHECK(origin->x < 500000.0);
    CHECK(origin->x > 470000.0);
}
