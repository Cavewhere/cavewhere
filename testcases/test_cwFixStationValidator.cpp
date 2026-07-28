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
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwFixStationValidator.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwSurveyNetwork.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QUuid>

//Std includes
#include <algorithm>

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
    // All three at once — cases below pass an empty cs, where writing them one
    // at a time would keep only the last (see cwFixStation::setCoordinate).
    fix.setCoordinate(easting, northing, elevation);
    return fix;
}

// Every warning a cave carries, flattened to one string to assert substrings
// against. Joined with a space so a phrase never spans two warnings.
QString warningText(cwCave* cave)
{
    return cave->errorModel()->toStringList().join(QChar(' '));
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

TEST_CASE("currentClassification drops fixes with no usable CS of their own",
          "[cwFixStationValidator]")
{
    // Global CS is UTM 12N. Four good fixes, plus one with an EMPTY inputCS and
    // one with a GARBAGE inputCS placed far away. Both are dropped entirely —
    // never a candidate, so never flagged. The empty one is not judged under the
    // region's CS: that is not a stand-in for a system the row never declared.
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
    // Empty inputCS → no system to read the coordinate under → dropped. Placed
    // among the others, so keeping it would go unnoticed if the count were the
    // only assertion.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("E1"), QString(), 500050.0, 4194050.0, 2703.0));
    // Garbage CS, far away → must be dropped, not flagged as an outlier.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("NOT_A_CS"), 9000000.0, 9000000.0, 0.0));

    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 4);

    // By identity, not by count: E1 sits inside the cluster, so a count alone
    // would not say which four survived.
    const auto kept = [&](int row) {
        const QUuid id = cave->fixStations()->fixStationAt(row).id();
        return std::any_of(result.inliers.begin(), result.inliers.end(),
                           [&](const FixCandidate& c) { return c.fixId == id; });
    };
    CHECK(kept(0));       // A1
    CHECK_FALSE(kept(4)); // E1, no CS at all
    CHECK_FALSE(kept(5)); // Bad, a CS PROJ can't read
}

TEST_CASE("currentClassification drops a fix that has a CS but no coordinate yet",
          "[cwFixStationValidator]")
{
    // "Mark Station as Fixed" makes exactly this row: a coordinate system, and
    // no coordinate until the user types one. Its components are 0 — every
    // state but Valid reads 0 — so admitting it would enter the cluster at
    // WGS84's origin, reprojected into UTM 13N some 5000 km away, flag the row
    // the user just created as an outlier, and drag the world origin with it.
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

    const int blankRow = cave->fixStations()->addFixStation(QStringLiteral("A5"));
    REQUIRE(blankRow == 4);
    const cwFixStation blank = cave->fixStations()->fixStationAt(blankRow);
    REQUIRE(blank.state() == cwFixStation::Empty);
    REQUIRE_FALSE(blank.inputCS().isEmpty());
    REQUIRE(cwCoordinateTransform::isValidCS(blank.inputCS()));

    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 4);

    const QUuid blankId = blank.id();
    CHECK_FALSE(std::any_of(result.inliers.begin(), result.inliers.end(),
                            [&](const FixCandidate& c) { return c.fixId == blankId; }));

    // And the origin stays on the four real fixes rather than being pulled
    // toward the empty row's projected (0, 0).
    const auto origin = region.fixStationValidator()->robustWorldOrigin();
    REQUIRE(origin.has_value());
    CHECK(origin->x > 470000.0);
    CHECK(origin->x < 490000.0);
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
    CHECK(warningText(cave).contains(QStringLiteral("Bad")));
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

    const QString firstMessage = warningText(cave);

    // Moving the outlier to a different far coordinate should update the same
    // warning row (new distance) rather than remove-and-append a new one.
    QSignalSpy warningSpy(cave->errorModel(), &cwErrorModel::warningCountChanged);
    const int badRow = cave->fixStations()->count() - 1;
    cave->fixStations()->setData(cave->fixStations()->index(badRow),
                                 6194000.0,
                                 cwFixStationModel::NorthingRole);

    CHECK(cave->errorModel()->warningCount() == 1);
    CHECK(warningSpy.size() == 0);
    CHECK(warningText(cave) != firstMessage);
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
    // These messages are assembled by choosing between two whole sentences, so
    // the tests pin the exact wording — a wrong choice is invisible to a
    // substring both wordings share. The plural halves are pinned further down.
    CHECK(warningText(badCave).contains(
        QStringLiteral("Fix station \"B\" has a coordinate outside the valid range for its "
                       "coordinate system")));
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
    // One outlier, so the singular verb. The distance is computed, so the
    // assertions straddle it rather than pinning a number.
    const QString clusterMessage = warningText(caveC);
    CHECK(clusterMessage.contains(QStringLiteral("Fix station \"C\" (~")));
    CHECK(clusterMessage.contains(QStringLiteral(") is far from the rest of the survey")));
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

    // Part A flags the bad cave; the good cave is clean. Two offenders in one
    // cave, so the plural wording — one sentence naming both, not one each.
    REQUIRE(badCave->errorModel()->warningCount() == 1);
    CHECK(warningText(badCave).contains(
        QStringLiteral("Fix stations \"B1\", \"B2\" have coordinates outside the valid range for "
                       "their coordinate system")));
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

TEST_CASE("needsOutputCS tracks a project with fixes but no output CS",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;  // no global CS
    auto* validator = region.fixStationValidator();

    // Empty project: nothing to place, no prompt.
    CHECK_FALSE(validator->needsOutputCS());
    CHECK(validator->suggestedOutputCS().isEmpty());

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    QSignalSpy needsSpy(validator, &cwFixStationValidator::needsOutputCSChanged);
    QSignalSpy suggestedSpy(validator, &cwFixStationValidator::suggestedOutputCSChanged);

    // A projected fix with no output CS: prompt turns on, suggestion = the input CS.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));

    CHECK(validator->needsOutputCS());
    CHECK(validator->suggestedOutputCS() == QStringLiteral("EPSG:32612"));
    CHECK(needsSpy.count() >= 1);
    CHECK(suggestedSpy.count() >= 1);
}

TEST_CASE("needsOutputCS ignores a blank fix row until it has an input CS",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;  // no global CS
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    auto* validator = region.fixStationValidator();

    // A scaffold row with no input CS is the "not started yet" state — no prompt.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral(""), QStringLiteral(""), 0.0, 0.0, 0.0));
    CHECK_FALSE(validator->needsOutputCS());
}

TEST_CASE("suggestedOutputCS derives a UTM zone from a geographic fix",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;  // no global CS
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    auto* validator = region.fixStationValidator();

    // A lat/long fix at -110 lon, 40 lat can't be the output CS itself; the
    // suggestion is the WGS84 UTM zone that contains it (12N).
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), cwCoordinateTransform::Wgs84, -110.0, 40.0, 1500.0));

    CHECK(validator->needsOutputCS());
    CHECK(validator->suggestedOutputCS() == QStringLiteral("EPSG:32612"));
}

TEST_CASE("setting the output CS clears the prompt",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;  // no global CS
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    auto* validator = region.fixStationValidator();

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));
    REQUIRE(validator->needsOutputCS());

    // Adopting a system (what the prompt's picker does) resolves the condition.
    region.geoReference()->setGlobalCoordinateSystem(validator->suggestedOutputCS());

    CHECK(region.geoReference()->globalCoordinateSystem() == QStringLiteral("EPSG:32612"));
    CHECK_FALSE(validator->needsOutputCS());
    CHECK(validator->suggestedOutputCS().isEmpty());
}

TEST_CASE("a fix at the origin is treated as not-yet-entered, not invalid",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;  // no global CS
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    auto* validator = region.fixStationValidator();

    // A CS was picked but no coordinate typed yet: the prompt shows, but with no
    // suggestion and no invalid flag — an empty picker the user can fill in.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 0.0, 0.0, 0.0));

    CHECK(validator->needsOutputCS());
    CHECK(validator->suggestedOutputCS().isEmpty());
    CHECK_FALSE(validator->outputCSCoordinateInvalid());
}

TEST_CASE("an out-of-domain fix coordinate flags the output-CS prompt as invalid",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;  // no global CS
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    auto* validator = region.fixStationValidator();

    QSignalSpy invalidSpy(validator, &cwFixStationValidator::outputCSCoordinateInvalidChanged);

    // Easting 1478000 is well outside UTM zone 13's valid domain — a data-entry
    // error. No trustworthy suggestion, so the prompt flags the coordinate.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));

    CHECK(validator->needsOutputCS());
    CHECK(validator->outputCSCoordinateInvalid());
    CHECK(validator->suggestedOutputCS().isEmpty());
    CHECK(invalidSpy.count() >= 1);

    // Correcting the coordinate clears the flag and restores the suggestion.
    cave->fixStations()->setData(cave->fixStations()->index(0),
                                 478000.0, cwFixStationModel::EastingRole);

    CHECK_FALSE(validator->outputCSCoordinateInvalid());
    CHECK(validator->suggestedOutputCS() == QStringLiteral("EPSG:32613"));
}

TEST_CASE("needsOutputCS is false when the project already has an output CS",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));

    CHECK_FALSE(region.fixStationValidator()->needsOutputCS());
}

TEST_CASE("suggestedOutputCS is decided by the first cave's fix, not a later cave's",
          "[cwFixStationValidator][outputCS]")
{
    cwCavingRegion region;  // no global CS
    region.addCave();
    region.addCave();
    auto* first = region.cave(0);
    auto* second = region.cave(1);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    auto* validator = region.fixStationValidator();

    // The scan returns on the first non-origin fix carrying an input CS, walking
    // caves in order — so the first cave's good projected fix decides the
    // suggestion and a later cave's out-of-domain fix must not override it.
    first->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));
    second->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));

    CHECK(validator->needsOutputCS());
    CHECK(validator->suggestedOutputCS() == QStringLiteral("EPSG:32612"));
    CHECK_FALSE(validator->outputCSCoordinateInvalid());
}

TEST_CASE("fixStationErrorTypeIds lists the three fix-station warning kinds",
          "[cwFixStationValidator][reference]")
{
    const QList<int> ids = cwFixStationValidator::fixStationErrorTypeIds();
    CHECK(ids.contains(static_cast<int>(cwErrorTypeId::FixStationOutlier)));
    CHECK(ids.contains(static_cast<int>(cwErrorTypeId::FixStationDomain)));
    CHECK(ids.contains(static_cast<int>(cwErrorTypeId::FixStationReference)));
}

TEST_CASE("revalidate flags a fix whose station name is not in the survey",
          "[cwFixStationValidator][reference]")
{
    // No global CS, so the cluster/domain math sits out — this isolates the
    // reference check, which is independent of any output CS.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    cave->setSurveyNetwork(network);

    // A fix on a real station raises nothing.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    CHECK(cave->errorModel()->warningCount() == 0);

    // A fix on a station that doesn't exist is flagged, and the message names it.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("XYZ"), QStringLiteral("EPSG:32613"), 478100.0, 4430100.0, 1656.0));
    REQUIRE(cave->errorModel()->warningCount() == 1);
    CHECK(warningText(cave).contains(
        QStringLiteral("Fix station \"XYZ\" names a survey station that doesn't exist in this "
                       "cave")));

    // Correcting the name to an existing station clears the warning.
    const int badRow = cave->fixStations()->count() - 1;
    cave->fixStations()->setData(cave->fixStations()->index(badRow),
                                 QStringLiteral("A2"),
                                 cwFixStationModel::StationNameRole);
    CHECK(cave->errorModel()->warningCount() == 0);
}

TEST_CASE("several broken fixes in one cave collapse into one plural sentence per kind",
          "[cwFixStationValidator][reference]")
{
    // Both reference kinds report per cave, not per fix, so a cave with two of
    // each produces one warning carrying two sentences, each in its plural form.
    // The unknown names are listed; the unnamed fixes can only be counted, which
    // is why that sentence is built separately from the other three.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    cave->setSurveyNetwork(network);

    for (const QString& name : {QStringLiteral("XYZ"), QStringLiteral("PDQ"), QString(), QString()}) {
        cave->fixStations()->appendFixStation(
            makeFix(name, QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    }

    REQUIRE(cave->errorModel()->warningCount() == 1);
    const QString message = warningText(cave);
    CHECK(message.contains(QStringLiteral("Fix stations \"XYZ\", \"PDQ\" name survey stations that "
                                          "don't exist in this cave")));
    CHECK(message.contains(QStringLiteral("2 fix stations have no station name")));
}

TEST_CASE("revalidate defers a named fix with no network but flags a blank fix",
          "[cwFixStationValidator][reference]")
{
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    // A named fix but no survey network yet — nothing to check against.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    CHECK(cave->errorModel()->warningCount() == 0);

    // A blank scaffold row names no station; survex drops such a fix, so it is
    // flagged at the cave level.
    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    cave->setSurveyNetwork(network);
    cave->fixStations()->addFixStation();
    REQUIRE(cave->errorModel()->warningCount() == 1);
    CHECK(warningText(cave)
              .contains(QStringLiteral("no station name")));

    // Naming the blank row to an existing station clears the warning.
    const int blankRow = cave->fixStations()->count() - 1;
    cave->fixStations()->setData(cave->fixStations()->index(blankRow),
                                 QStringLiteral("A2"),
                                 cwFixStationModel::StationNameRole);
    CHECK(cave->errorModel()->warningCount() == 0);
}

TEST_CASE("a station appearing in the survey clears the reference warning",
          "[cwFixStationValidator][reference]")
{
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    cave->setSurveyNetwork(network);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Depot"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    REQUIRE(cave->errorModel()->warningCount() == 1);

    // The line plot recomputes and now includes the station — the warning clears
    // without touching the fix (surveyNetworkChanged re-attributes).
    cwSurveyNetwork grown;
    grown.addShot(QStringLiteral("A1"), QStringLiteral("Depot"));
    cave->setSurveyNetwork(grown);
    CHECK(cave->errorModel()->warningCount() == 0);
}
