/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "SurveyTreeTestHelper.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwExternalCenterline.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnosticsModel.h"
#include "cwFixStationModel.h"
#include "cwFixStationTreeModel.h"
#include "cwFixStationValidator.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwSurveyNetwork.h"
#include "ExternalCenterlineTestHelpers.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QSignalSpy>
#include <QUuid>

//Std includes
#include <algorithm>
#include <limits>

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

// A bare shot raises data warnings of its own, so trip-backed tests look for
// the fix's warning by its type.
bool hasReferenceWarning(cwSurveyNode* node)
{
    const QList<cwError> errors = node->errorModel()->errors()->toList();
    return std::any_of(errors.cbegin(), errors.cend(), [](const cwError& error) {
        return error.errorTypeId() == static_cast<int>(cwErrorTypeId::FixStationReference);
    });
}

// The stations appendGoodFixes() fixes, and those plus the outlier tests' typo.
const QStringList kGoodStations = {QStringLiteral("A1"), QStringLiteral("A2"),
                                   QStringLiteral("A3"), QStringLiteral("A4")};
const QStringList kGoodAndBadStations = kGoodStations + QStringList{QStringLiteral("Bad")};

// Append four good fixes close together in EPSG:32612. The first anchors the
// project's frame, so all four sit within a few hundred meters of its origin.
void appendGoodFixes(cwCave* cave)
{
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"), 500000.0, 4194000.0, 2700.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32612"), 500200.0, 4194100.0, 2710.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A3"), QStringLiteral("EPSG:32612"), 499900.0, 4193900.0, 2705.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A4"), QStringLiteral("EPSG:32612"), 500100.0, 4193950.0, 2708.0));
    // The frame the classification reads settles on the next event-loop turn.
    QCoreApplication::processEvents();
}

} // namespace

TEST_CASE("classifyCandidates flags an isolated outlier",
          "[cwFixStationValidator]")
{
    // Five fixes on the project's frame origin plus one ~500 km away.
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

TEST_CASE("classifyCandidates keeps fixes near the frame origin clean",
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
    // Two real groups of caves ~200 km apart. A karst region this wide is
    // ordinary, and every fix is still inside the threshold, so the project
    // must stay quiet — the false-alarm case the threshold is bounded by.
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

TEST_CASE("classifyCandidates flags a distant fix in a two-fix project",
          "[cwFixStationValidator]")
{
    // Two caves, one fix each, the second ~500 km out — the commonest shape a
    // project takes, and the one no cross-cave cluster rule can judge: with two
    // points there is no majority to say which of them is wrong. Measuring each
    // against the frame origin needs no majority, so this is caught.
    const QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(500000.0, 0.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    REQUIRE(result.outliers.size() == 1);
    CHECK(result.outliers.first().global.x == 500000.0);
    CHECK(result.inliers.size() == 1);
}

TEST_CASE("classifyCandidates flags a lone distant fix with nothing to compare against",
          "[cwFixStationValidator]")
{
    // One fix, and it is nowhere near the project's frame — which happens when
    // the frame is Frozen (its anchor deleted) and the survivor carries the
    // typo. A single fix is judged on the same terms as any other.
    const QList<FixCandidate> candidates = {
        candidateAt(500000.0, 0.0),
    };

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    REQUIRE(result.outliers.size() == 1);
    CHECK(result.inliers.isEmpty());
}

TEST_CASE("classifyCandidates judges each fix independently of how many there are",
          "[cwFixStationValidator]")
{
    // The same distant point, alone and among neighbors, must get the same
    // verdict — there is no count at which detection switches on or off.
    const FixCandidate far = candidateAt(500000.0, 0.0);

    CHECK(cwFixStationValidator::classifyCandidates({far}).outliers.size() == 1);
    CHECK(cwFixStationValidator::classifyCandidates(
              {candidateAt(0.0, 0.0), far}).outliers.size() == 1);
    CHECK(cwFixStationValidator::classifyCandidates(
              {candidateAt(0.0, 0.0), candidateAt(10.0, 5.0), far}).outliers.size() == 1);
}

TEST_CASE("classifyCandidates reports a domain-bad fix as domain-bad, not distant",
          "[cwFixStationValidator]")
{
    // A fix marked domain-invalid (Part A) sits right next to the frame origin,
    // so only Part A has anything to say about it. It must land in
    // domainOutliers, leaving the good one as the sole inlier.
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

TEST_CASE("classifyCandidates raises one warning per bad fix, not two",
          "[cwFixStationValidator]")
{
    // A domain-bad point that is ALSO far from the frame — the usual shape,
    // since a coordinate outside its own CS's range is rarely nearby. It must
    // be reported once, by Part A, which says what is wrong with it; Part B
    // must leave it alone rather than add a second warning about the same fix.
    QList<FixCandidate> candidates = {
        candidateAt(0.0, 0.0),
        candidateAt(10.0, 5.0),
        candidateAt(-8.0, 12.0),
        candidateAt(3.0, 3.0),
        candidateAt(500000.0, 0.0),    // in-domain but distant → Part B
        candidateAt(9000000.0, 0.0),   // domain-bad → Part A, and only Part A
    };
    candidates[5].domainValid = false;

    const auto result = cwFixStationValidator::classifyCandidates(candidates);

    REQUIRE(result.domainOutliers.size() == 1);
    CHECK(result.domainOutliers.first().global.x == 9000000.0);
    REQUIRE(result.outliers.size() == 1);
    CHECK(result.outliers.first().global.x == 500000.0);
    CHECK(result.inliers.size() == 4);
}

TEST_CASE("classifyCandidates leaves a point inside the threshold alone",
          "[cwFixStationValidator]")
{
    // 50 km out: far enough from its neighbors that a cluster rule would have
    // called it a straggler, but an ordinary distance for one project, so it is
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

    // Four good fixes spread across two caves, all near one spot.
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

    QCoreApplication::processEvents();
    const auto result = region.fixStationValidator()->currentClassification();

    REQUIRE(result.outliers.size() == 1);
    CHECK(result.inliers.size() == 3);
    CHECK(result.outliers.first().node == caveB);
    CHECK(result.outliers.first().fixId == badId);
}

TEST_CASE("currentClassification is empty on a region with no fixes",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;

    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.inliers.isEmpty());
    CHECK(result.outliers.isEmpty());
}

TEST_CASE("currentClassification reprojects a fix entered in a different CS",
          "[cwFixStationValidator]")
{
    // The global CS is UTM 12N; four good fixes are entered directly in it, plus
    // one good fix for the SAME real-world spot entered in WGS84 lon/lat. Raw, the
    // WGS84 degrees sit ~4000 km from the UTM fixes and would be flagged — so
    // classifying it as an inlier proves gatherCandidates() actually ran the
    // cwCoordinateTransform. Exercises the reproject branch the same-CS tests skip.
    cwCavingRegion region;

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

    QCoreApplication::processEvents();
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

    QCoreApplication::processEvents();
    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 4);

    // By identity, not by count: E1 sits near the frame origin, so a count alone
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
    // state but Valid reads 0 — so admitting it would judge it at
    // WGS84's origin, thousands of km from the project, and flag the row the
    // user just created as an outlier.
    cwCavingRegion region;

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

    QCoreApplication::processEvents();
    const auto result = region.fixStationValidator()->currentClassification();

    CHECK(result.outliers.isEmpty());
    CHECK(result.inliers.size() == 4);

    const QUuid blankId = blank.id();
    CHECK_FALSE(std::any_of(result.inliers.begin(), result.inliers.end(),
                            [&](const FixCandidate& c) { return c.fixId == blankId; }));

    // And the four real fixes, which sit close together, are all within a few
    // hundred meters of the frame's origin — the empty row never pulled it.
    for (const FixCandidate& candidate : result.inliers) {
        CHECK(std::abs(candidate.global.x) < 1000.0);
        CHECK(std::abs(candidate.global.y) < 1000.0);
    }
}

TEST_CASE("revalidate attributes an outlier warning to its cave",
          "[cwFixStationValidator]")
{
    cwCavingRegion region;

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    SurveyTreeTestHelper::surveyStations(cave, kGoodAndBadStations);
    appendGoodFixes(cave);

    // No outlier yet — all four fixes are clean.
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

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    SurveyTreeTestHelper::surveyStations(cave, kGoodAndBadStations);
    appendGoodFixes(cave);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));
    REQUIRE(cave->errorModel()->warningCount() == 1);

    // Correct the typo in place — the northing back beside the other fixes.
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

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    SurveyTreeTestHelper::surveyStations(cave, kGoodAndBadStations);
    appendGoodFixes(cave);
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

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    SurveyTreeTestHelper::surveyStations(cave, kGoodAndBadStations);
    appendGoodFixes(cave);
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

    // Cave 0 stays clean; cave 1 carries an outlier warning.
    region.addCave();
    auto* keepCave = region.cave(0);
    REQUIRE(keepCave != nullptr);
    SurveyTreeTestHelper::surveyStations(keepCave, kGoodStations);
    appendGoodFixes(keepCave);

    region.addCave();
    auto* doomedCave = region.cave(1);
    REQUIRE(doomedCave != nullptr);
    SurveyTreeTestHelper::surveyStations(doomedCave, kGoodAndBadStations);
    appendGoodFixes(doomedCave);
    doomedCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), 500150.0, 5194000.0, 2708.0));
    REQUIRE(doomedCave->errorModel()->warningCount() == 1);

    // Removing the offending cave must tear down its fix-station connection and
    // clear its warning while the cave is still alive (it outlives removal via the
    // undo command's deleteLater), without disturbing the surviving cave.
    region.removeCave(1);
    QCoreApplication::processEvents();

    CHECK(doomedCave->errorModel()->warningCount() == 0);
    CHECK(keepCave->errorModel()->warningCount() == 0);
}

TEST_CASE("revalidate raises no warning across caves entered in different CSs",
          "[cwFixStationValidator]")
{
    // Two caves at the same real-world spot, entered in different input CSs.
    // Compared as raw coordinates they look ~4000 km apart; reprojecting both
    // into the project's frame first is what keeps a legitimate station from
    // being flagged as an outlier.
    cwCavingRegion region;

    region.addCave();
    auto* utmCave = region.cave(0);
    REQUIRE(utmCave != nullptr);
    SurveyTreeTestHelper::surveyStations(utmCave, kGoodStations);
    appendGoodFixes(utmCave);

    region.addCave();
    auto* wgsCave = region.cave(1);
    REQUIRE(wgsCave != nullptr);
    SurveyTreeTestHelper::surveyStations(wgsCave, {QStringLiteral("W1")});
    // Raw WGS84 degrees sit ~4000 km from the UTM eastings/northings above.
    wgsCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("W1"), QStringLiteral("EPSG:4326"), -111.0, 37.871, 2700.0));
    QCoreApplication::processEvents();

    CHECK(utmCave->errorModel()->warningCount() == 0);
    CHECK(wgsCave->errorModel()->warningCount() == 0);
}

TEST_CASE("classifyCandidates flags an elevation-only outlier",
          "[cwFixStationValidator]")
{
    // Fixes at ground level plus one displaced only in z — a transposed
    // elevation. The frame has no z origin, so this rides entirely on
    // distanceFromFrameOrigin() keeping its z term; dropping it to a horizontal
    // hypot would leave this uncaught.
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

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Deep Hole"));
    SurveyTreeTestHelper::surveyStations(cave, kGoodAndBadStations);
    appendGoodFixes(cave);

    auto* validator = region.fixStationValidator();
    REQUIRE(validator != nullptr);

    // Clean fixes leave the summary empty so the overlay stays hidden.
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

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Deep Hole"));
    SurveyTreeTestHelper::surveyStations(cave, kGoodAndBadStations);
    appendGoodFixes(cave);
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

TEST_CASE("revalidate flags a fix outside its CS's valid domain",
          "[cwFixStationValidator]")
{
    // The realistic 2-cave typo: one good cave, one whose single fix has a
    // transposed leading digit (1478000 easting in UTM 13N — ~1000 km east of
    // the zone). Part A names what is actually wrong with it, which is more
    // than the distance alone would say.
    cwCavingRegion region;

    region.addCave();
    auto* goodCave = region.cave(0);
    REQUIRE(goodCave != nullptr);
    SurveyTreeTestHelper::surveyStations(goodCave, {QStringLiteral("G")});
    goodCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("G"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));

    region.addCave();
    auto* badCave = region.cave(1);
    REQUIRE(badCave != nullptr);
    SurveyTreeTestHelper::surveyStations(badCave, {QStringLiteral("B")});
    badCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
    QCoreApplication::processEvents();

    REQUIRE(badCave->errorModel()->warningCount() == 1);
    // These messages are assembled by choosing between two whole sentences, so
    // the tests pin the exact wording — a wrong choice is invisible to a
    // substring both wordings share. The plural halves are pinned further down.
    CHECK(warningText(badCave).contains(
        QStringLiteral("Fix station \"B\" has a coordinate outside the valid range for its "
                       "coordinate system")));
    CHECK(goodCave->errorModel()->warningCount() == 0);
}

TEST_CASE("revalidate flags a lone domain-bad fix on a project with no frame",
          "[cwFixStationValidator][issue660]")
{
    // A fix its own CS can't hold may not anchor the local frame (#660), so a
    // project whose only fix is a typo stays ungeoreferenced. The survex export
    // drops the fix as well, so Part A is the one place that says why; it needs
    // no frame to say it. Nothing is off-screen, though, so the render-view
    // summary stays quiet.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    SurveyTreeTestHelper::surveyStations(cave, {QStringLiteral("B")});
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
    QCoreApplication::processEvents();
    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);

    REQUIRE(cave->errorModel()->warningCount() == 1);
    CHECK(warningText(cave).contains(
        QStringLiteral("Fix station \"B\" has a coordinate outside the valid range for its "
                       "coordinate system")));
    CHECK(region.fixStationValidator()->warningMessage().isEmpty());
    CHECK(region.fixStationValidator()->outlierCount() == 0);

    // Correcting it anchors the frame and clears the warning.
    cave->fixStations()->setData(cave->fixStations()->index(0), 478000.0,
                                 cwFixStationModel::EastingRole);
    QCoreApplication::processEvents();
    CHECK(region.geoReference()->state() == cwGeoReference::Anchored);
    CHECK(cave->errorModel()->warningCount() == 0);
    CHECK(region.fixStationValidator()->warningMessage().isEmpty());
    CHECK(region.fixStationValidator()->outlierCount() == 0);
}

TEST_CASE("revalidate flags a distant in-domain cave",
          "[cwFixStationValidator]")
{
    // Three caves, one fix each, all valid UTM 13N coordinates — but one sits
    // ~1000 km north of the other two. The domain check passes (it is a real
    // in-zone location), so Part B's distance from the frame is the only thing
    // that catches it.
    cwCavingRegion region;

    region.addCave();
    auto* caveA = region.cave(0);
    REQUIRE(caveA != nullptr);
    SurveyTreeTestHelper::surveyStations(caveA, {QStringLiteral("A")});
    caveA->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));

    region.addCave();
    auto* caveB = region.cave(1);
    REQUIRE(caveB != nullptr);
    SurveyTreeTestHelper::surveyStations(caveB, {QStringLiteral("B")});
    caveB->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 478100.0, 4430100.0, 1656.0));

    region.addCave();
    auto* caveC = region.cave(2);
    REQUIRE(caveC != nullptr);
    SurveyTreeTestHelper::surveyStations(caveC, {QStringLiteral("C")});
    caveC->fixStations()->appendFixStation(
        makeFix(QStringLiteral("C"), QStringLiteral("EPSG:32613"), 478000.0, 5430000.0, 1655.0));
    QCoreApplication::processEvents();

    REQUIRE(caveC->errorModel()->warningCount() == 1);
    // One outlier, so the singular verb. The distance is computed, so the
    // assertions straddle it rather than pinning a number.
    const QString distanceMessage = warningText(caveC);
    CHECK(distanceMessage.contains(QStringLiteral("Fix station \"C\" (~")));
    CHECK(distanceMessage.contains(QStringLiteral(") is far from the rest of the survey")));
    CHECK(caveA->errorModel()->warningCount() == 0);
    CHECK(caveB->errorModel()->warningCount() == 0);
}

TEST_CASE("revalidate attributes a whole bad cave to Part A, not Part B",
          "[cwFixStationValidator]")
{
    // Two caves, two fixes each. One cave's fixes both carry a transposed-digit
    // easting (out of the zone's domain), so the bad cave is flagged for what is
    // actually wrong with it — the coordinates, not their distance — even though
    // both fixes are also far from the frame.
    cwCavingRegion region;

    region.addCave();
    auto* goodCave = region.cave(0);
    REQUIRE(goodCave != nullptr);
    SurveyTreeTestHelper::surveyStations(goodCave, {QStringLiteral("G1"), QStringLiteral("G2")});
    goodCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("G1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    goodCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("G2"), QStringLiteral("EPSG:32613"), 478100.0, 4430100.0, 1656.0));

    region.addCave();
    auto* badCave = region.cave(1);
    REQUIRE(badCave != nullptr);
    SurveyTreeTestHelper::surveyStations(badCave, {QStringLiteral("B1"), QStringLiteral("B2")});
    badCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
    badCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B2"), QStringLiteral("EPSG:32613"), 1478100.0, 4430100.0, 1656.0));
    QCoreApplication::processEvents();

    // Part A flags the bad cave; the good cave is clean. Two offenders in one
    // cave, so the plural wording — one sentence naming both, not one each.
    REQUIRE(badCave->errorModel()->warningCount() == 1);
    CHECK(warningText(badCave).contains(
        QStringLiteral("Fix stations \"B1\", \"B2\" have coordinates outside the valid range for "
                       "their coordinate system")));
    CHECK(goodCave->errorModel()->warningCount() == 0);

    // And exactly one warning, not two: the same fixes are far from the frame as
    // well, so a Part B that did not defer would flag this cave a second time.
    const auto classification = region.fixStationValidator()->currentClassification();
    CHECK(classification.domainOutliers.size() == 2);
    CHECK(classification.outliers.isEmpty());
}

TEST_CASE("revalidate flags a two-cave project whose second fix is a continent away",
          "[cwFixStationValidator]")
{
    // The reported shape (#627): two caves, one WGS84 fix each, the second
    // ~2000 km from the first — a real coordinate for somewhere the project is
    // not. Both are legal lat/lon, so Part A passes them; with only two fixes
    // there is no majority for a cluster rule to find, which is exactly why
    // this went unreported until Part B started measuring from the frame.
    cwCavingRegion region;

    region.addCave();
    auto* bcCave = region.cave(0);
    REQUIRE(bcCave != nullptr);
    bcCave->setName(QStringLiteral("Cave 1"));
    SurveyTreeTestHelper::surveyStations(bcCave, {QStringLiteral("a1")});
    bcCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("a1"), QStringLiteral("EPSG:4326"), -121.83058365, 51.11408015, 2198.03));

    region.addCave();
    auto* strayCave = region.cave(1);
    REQUIRE(strayCave != nullptr);
    strayCave->setName(QStringLiteral("Cave 2"));
    SurveyTreeTestHelper::surveyStations(strayCave, {QStringLiteral("b2")});
    strayCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("b2"), QStringLiteral("EPSG:4326"), -98.370455, 41.936678, 0.0));
    QCoreApplication::processEvents();

    REQUIRE(bcCave->errorModel()->warningCount() == 0);
    REQUIRE(strayCave->errorModel()->warningCount() == 1);
    CHECK(warningText(strayCave).contains(QStringLiteral("Fix station \"b2\" (~")));
    CHECK(warningText(strayCave).contains(QStringLiteral(") is far from the rest of the survey")));

    // And the render-view banner names the offending cave, so an empty-looking
    // 3D view has a cause the user can act on.
    auto* validator = region.fixStationValidator();
    CHECK(validator->property("outlierCount").toInt() == 1);
    CHECK(validator->property("firstOutlierCave").value<cwCave*>() == strayCave);
}

TEST_CASE("revalidate does not flag two legitimately distant caves",
          "[cwFixStationValidator]")
{
    // Two caves ~300 km apart, every fix a valid in-zone UTM 13N coordinate — a
    // karst region, not a typo. This is the lower bound on the distance
    // threshold: a legitimately spread-out survey must not cry wolf.
    cwCavingRegion region;

    region.addCave();
    auto* westCave = region.cave(0);
    REQUIRE(westCave != nullptr);
    SurveyTreeTestHelper::surveyStations(westCave, {QStringLiteral("W1"), QStringLiteral("W2")});
    westCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("W1"), QStringLiteral("EPSG:32613"), 400000.0, 4430000.0, 1655.0));
    westCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("W2"), QStringLiteral("EPSG:32613"), 400100.0, 4430100.0, 1656.0));

    region.addCave();
    auto* eastCave = region.cave(1);
    REQUIRE(eastCave != nullptr);
    SurveyTreeTestHelper::surveyStations(eastCave, {QStringLiteral("E1"), QStringLiteral("E2")});
    eastCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("E1"), QStringLiteral("EPSG:32613"), 700000.0, 4430000.0, 1655.0));
    eastCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("E2"), QStringLiteral("EPSG:32613"), 700100.0, 4430100.0, 1656.0));
    QCoreApplication::processEvents();

    CHECK(westCave->errorModel()->warningCount() == 0);
    CHECK(eastCave->errorModel()->warningCount() == 0);
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
    // No project frame, so the distance/domain math sits out — this isolates
    // the reference check, which is independent of any frame.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    SurveyTreeTestHelper::setScopedNetwork(cave, network);

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

TEST_CASE("a fix in a cave with no survey data is flagged until the network names it",
          "[cwFixStationValidator][reference]")
{
    // Issue #727: a cave with no named station in any trip has nothing to
    // solve, so its network stays empty and the fix names a station that
    // doesn't exist.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("a1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    REQUIRE(cave->errorModel()->warningCount() == 1);
    CHECK(warningText(cave).contains(
        QStringLiteral("Fix station \"a1\" names a survey station that doesn't exist in this "
                       "cave")));

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("a1"), QStringLiteral("a2"));
    SurveyTreeTestHelper::setScopedNetwork(cave, network);
    CHECK(cave->errorModel()->warningCount() == 0);
}

TEST_CASE("a fix in a cave whose trips name stations defers until the first solve",
          "[cwFixStationValidator][reference]")
{
    // The network is left empty, as before the first solve. The trip promises
    // one, so no named fix is judged yet, whether or not the trip holds it.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    SurveyTreeTestHelper::addTripWithStations(cave, {QStringLiteral("a1"), QStringLiteral("a2")});
    REQUIRE(cave->scopedNetwork().isEmpty());

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("zzz"), QStringLiteral("EPSG:32613"), 478010.0, 4430010.0, 1655.0));
    CHECK_FALSE(hasReferenceWarning(cave));
    CHECK_FALSE(warningText(cave).contains(QStringLiteral("Fix station")));
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
    SurveyTreeTestHelper::setScopedNetwork(cave, network);

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

TEST_CASE("revalidate flags a blank fix",
          "[cwFixStationValidator][reference]")
{
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    SurveyTreeTestHelper::setScopedNetwork(cave, network);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    CHECK(cave->errorModel()->warningCount() == 0);

    // A blank scaffold row names no station; survex drops such a fix, so it is
    // flagged at the cave level.
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
    SurveyTreeTestHelper::setScopedNetwork(cave, network);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Depot"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    REQUIRE(cave->errorModel()->warningCount() == 1);

    // The line plot recomputes and now includes the station — the warning clears
    // without touching the fix (surveyNetworkChanged re-attributes).
    cwSurveyNetwork grown;
    grown.addShot(QStringLiteral("A1"), QStringLiteral("Depot"));
    SurveyTreeTestHelper::setScopedNetwork(cave, grown);
    CHECK(cave->errorModel()->warningCount() == 0);
}

TEST_CASE("each fix-station warning targets the first fix it names",
          "[cwFixStationValidator]")
{
    // A click on a warning opens the fix it names, so each kind carries that
    // fix's id: the domain-bad one, and the first of the broken references.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    network.addShot(QStringLiteral("A2"), QStringLiteral("A3"));
    SurveyTreeTestHelper::setScopedNetwork(cave, network);

    const auto targetOf = [cave](cwErrorTypeId errorTypeId) {
        const QList<cwError> errors = cave->errorModel()->errors()->toList();
        for (const cwError& error : errors) {
            if (error.errorTypeId() == static_cast<int>(errorTypeId)) {
                return error.targetId();
            }
        }
        return QUuid();
    };
    const auto fixIdAt = [cave](int row) {
        return cave->fixStations()->fixStationAt(row).id();
    };

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("XYZ"), QStringLiteral("EPSG:32613"), 478100.0, 4430100.0, 1656.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("PDQ"), QStringLiteral("EPSG:32613"), 478200.0, 4430200.0, 1657.0));

    CHECK(targetOf(cwErrorTypeId::FixStationDomain) == fixIdAt(1));
    CHECK(targetOf(cwErrorTypeId::FixStationReference) == fixIdAt(2));

    // Correcting the first broken name moves the target to the next one.
    cave->fixStations()->setData(cave->fixStations()->index(2),
                                 QStringLiteral("A3"),
                                 cwFixStationModel::StationNameRole);
    CHECK(targetOf(cwErrorTypeId::FixStationReference) == fixIdAt(3));
}

TEST_CASE("a second fix on one station flags the later row",
          "[cwFixStationValidator][reference]")
{
    // The export keeps the first of two fixes on a station and drops the
    // second, so the second is the row the page and the node warning name.
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    SurveyTreeTestHelper::setScopedNetwork(cave, network);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    CHECK(cave->errorModel()->warningCount() == 0);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("a1 "), QStringLiteral("EPSG:32613"), 478010.0, 4430000.0, 1655.0));

    REQUIRE(cave->errorModel()->warningCount() == 1);
    CHECK(warningText(cave).contains(QStringLiteral("Station \"a1\" is fixed twice")));
    const QList<cwError> errors = cave->errorModel()->errors()->toList();
    REQUIRE(errors.size() == 1);
    CHECK(errors.first().targetId() == cave->fixStations()->fixStationAt(1).id());

    const cwFixStationDiagnosticsModel* diagnostics = cave->fixStationDiagnostics();
    const auto stationError = [diagnostics](int row) {
        return diagnostics->data(diagnostics->index(row),
                                 cwFixStationDiagnosticsModel::StationErrorRole).toString();
    };
    CHECK(stationError(0).isEmpty());
    CHECK(stationError(1).contains(QStringLiteral("already fixes \"a1\"")));

    // Removing the first row leaves the second the only fix on the station.
    cave->fixStations()->removeAt(0);
    CHECK(cave->errorModel()->warningCount() == 0);
    CHECK(stationError(0).isEmpty());
}

TEST_CASE("a fix on a station an attached file fixes itself is flagged",
          "[cwFixStationValidator][reference]")
{
    cwCavingRegion region;
    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_blocks.svx")));

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("doghill.d1"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    CHECK(cave->errorModel()->warningCount() == 0);

    // As the attach scan's harvest reports the file's own *fix.
    cave->setFileFixedStations({{cave->id(), {QStringLiteral("doghill.d1")}}});
    QCoreApplication::processEvents();

    const QString collision = QStringLiteral(
        "survex_blocks.svx already fixes doghill.d1; remove that fix from the file or fix "
        "another station");
    REQUIRE(cave->errorModel()->warningCount() == 1);
    CHECK(warningText(cave).contains(collision));
    const cwFixStationDiagnosticsModel* diagnostics = cave->fixStationDiagnostics();
    CHECK(diagnostics->data(diagnostics->index(0), cwFixStationDiagnosticsModel::StationErrorRole)
              .toString() == collision);

    // A file that stops fixing the station frees it for the node's fix.
    cave->setFileFixedStations({});
    QCoreApplication::processEvents();
    CHECK(cave->errorModel()->warningCount() == 0);
}

namespace {

constexpr int kAttachedTreeFiles = 6;
constexpr int kAttachedTreeShots = 20;
constexpr int kBinaryTreeChildren = 2;

// Few legs per file, so the fan-out's cost is the nodes it visits rather than
// the stations each member view holds.
constexpr int kRelayTreeShots = 8;
constexpr int kRelaySmallTreeFiles = 32;
constexpr int kRelayLargeTreeFiles = 128;
constexpr int kRelayStallRepeats = 3;

// Four times the files is about four times the nodes, so one validator pass
// per fan-out grows about fourfold; one pass per relayed node grows about
// sixteenfold.
constexpr double kMaxRelayStallGrowth = 6.0;

bool hasReferenceWarning(const cwSurveyNode* node)
{
    const QList<cwError> errors = node->errorModel()->errors()->toList();
    return std::any_of(errors.cbegin(), errors.cend(), [](const cwError& error) {
        return error.errorTypeId() == static_cast<int>(cwErrorTypeId::FixStationReference);
    });
}

//! A derived node below \a root whose own sourced trip names stations, and the
//! first of them as the node names it.
struct DerivedMember
{
    cwSurveyNode* node = nullptr;
    QString station;
};

DerivedMember firstDerivedMember(cwSurveyNode* root)
{
    DerivedMember member;
    root->walk([&member, root](const cwSurveyNode* node) {
        if (member.node != nullptr || node == root || node->isSourceRoot()) {
            return;
        }
        for (const cwTrip* trip : node->trips()) {
            if (!trip->sourcePath().isEmpty() && !trip->externalStations().isEmpty()) {
                member.node = const_cast<cwSurveyNode*>(node);
                member.station = cwStation::canonicalKey(trip->externalStations().first());
                return;
            }
        }
    });
    return member;
}

//! \a network without \a station and every shot that touches it.
cwSurveyNetwork withoutStation(const cwSurveyNetwork& network, const QString& station)
{
    cwSurveyNetwork result;
    for (const QString& from : network.stations()) {
        if (from == station) {
            continue;
        }
        for (const QString& to : network.neighbors(from)) {
            if (to != station) {
                result.addShot(from, to);
            }
        }
        if (network.hasPosition(from)) {
            result.setPosition(from, network.position(from));
        }
    }
    return result;
}

//! Attaches a generated tree of \a files Survex files under the region's root
//! and waits for the scan and the solve. Returns the source root.
cwSurveyNode* attachGeneratedTree(SavedProjectFixture* fixture,
                                  const QTemporaryDir& sourceDir,
                                  int files,
                                  int shots)
{
    const QString entry = writeSurvexTree(sourceDir.path(), files, shots, kBinaryTreeChildren);
    cwSurveyNode* root = attachSourceThroughManager(
        fixture, fixture->project->cavingRegion()->rootNode(), entry);
    drainPipelines(fixture);
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    REQUIRE(root->scopedNetwork().stationCount() > 0);
    return root;
}

struct RelayStall
{
    qint64 stallMs = 0;
    qsizetype nodes = 0;
};

//! The shortest of kRelayStallRepeats main-thread stalls of one queued
//! invalidateMemberViews() on a generated tree of \a files fix-less files.
RelayStall measureRelayStall(int files)
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    auto fixture = makeSavedProject(QStringLiteral("validator-relay-%1").arg(files),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    attachGeneratedTree(fixture.get(), sourceDir, files, kRelayTreeShots);

    cwCavingRegion* region = fixture->project->cavingRegion();
    const QList<cwSurveyNode*> nodes = region->rootNode()->allNodes();
    for (const cwSurveyNode* node : nodes) {
        REQUIRE(node->fixStations()->count() == 0);
    }

    RelayStall result;
    result.nodes = nodes.size();
    result.stallMs = std::numeric_limits<qint64>::max();
    for (int repeat = 0; repeat < kRelayStallRepeats; ++repeat) {
        const qint64 stall = longestMainThreadStall([region]() { region->invalidateMemberViews(); });
        result.stallMs = std::min(result.stallMs, stall);
    }
    return result;
}

} // namespace

TEST_CASE("revalidate checks a derived node's fix against its member network",
          "[cwFixStationValidator][reference]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    auto fixture = makeSavedProject(QStringLiteral("validator-derived-fix"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachGeneratedTree(fixture.get(), sourceDir,
                                             kAttachedTreeFiles, kAttachedTreeShots);
    cwCavingRegion* region = fixture->project->cavingRegion();

    const DerivedMember member = firstDerivedMember(root);
    REQUIRE(member.node != nullptr);
    REQUIRE(member.node->scopedNetwork().hasStation(member.station));

    const QString absent = QStringLiteral("Absent");
    REQUIRE_FALSE(member.node->scopedNetwork().hasStation(absent));
    member.node->fixStations()->appendFixStation(
        makeFix(absent, QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
    drainPipelines(fixture.get());

    CHECK(hasReferenceWarning(member.node));
    CHECK(warningText(qobject_cast<cwCave*>(member.node)).contains(
        QStringLiteral("Fix station \"Absent\" names a survey station that doesn't exist")));
    for (const cwSurveyNode* node : region->rootNode()->allNodes()) {
        if (node != member.node) {
            INFO("node " << node->name().toStdString());
            CHECK_FALSE(hasReferenceWarning(node));
        }
    }

    member.node->fixStations()->setData(member.node->fixStations()->index(0),
                                        member.station,
                                        cwFixStationModel::StationNameRole);
    drainPipelines(fixture.get());
    CHECK_FALSE(hasReferenceWarning(member.node));

    // The region's key for the fixed station: the source root's prefix, then
    // the node's path below it, then the station.
    const QString regionKey = cwStation::canonicalKey(root->scopePrefix())
                              + cwStation::canonicalKey(member.node->cavernPath())
                              + member.station;
    REQUIRE(region->network().hasStation(regionKey));
    const cwSurveyNetwork dropped = withoutStation(region->network(), regionKey);

    // Records, as each reference warning lands on the node, whether the region
    // already held the network without the station.
    QList<bool> warningsAfterRegionChange;
    cwErrorListModel* errors = member.node->errorModel()->errors();
    QObject::connect(errors, &QAbstractItemModel::rowsInserted, errors,
                     [&](const QModelIndex&, int first, int last) {
        for (int row = first; row <= last; ++row) {
            if (errors->at(row).errorTypeId() == static_cast<int>(cwErrorTypeId::FixStationReference)) {
                warningsAfterRegionChange.append(!region->network().hasStation(regionKey));
            }
        }
    });

    SECTION("a solve that drops the fixed station") {
        region->setNetwork(dropped);
        REQUIRE(member.node->scopedNetwork().stationCount() > 0);
        REQUIRE_FALSE(member.node->scopedNetwork().hasStation(member.station));

        CHECK(hasReferenceWarning(member.node));
        CHECK(warningsAfterRegionChange == QList<bool>{true});
    }

    SECTION("a member-view invalidation after the solve changed unheard") {
        region->blockSignals(true);
        region->setNetwork(dropped);
        region->blockSignals(false);
        REQUIRE_FALSE(member.node->scopedNetwork().hasStation(member.station));
        CHECK_FALSE(hasReferenceWarning(member.node));

        region->invalidateMemberViews();
        CHECK(hasReferenceWarning(member.node));
        CHECK(warningsAfterRegionChange == QList<bool>{true});
    }
}

TEST_CASE("one member-view fan-out revalidates in time linear in the nodes",
          "[cwFixStationValidator][reference]")
{
    const RelayStall small = measureRelayStall(kRelaySmallTreeFiles);
    const RelayStall large = measureRelayStall(kRelayLargeTreeFiles);

    INFO("small: " << small.nodes << " nodes, " << small.stallMs << " ms stall");
    INFO("large: " << large.nodes << " nodes, " << large.stallMs << " ms stall");
    REQUIRE(small.stallMs > 0);
    const double growth = double(large.stallMs) / double(small.stallMs);
    INFO("growth " << growth);
    CHECK(growth <= kMaxRelayStallGrowth);
}

namespace {

const QString kDomainBadWording =
    QStringLiteral("Fix station \"B\" has a coordinate outside the valid range for its "
                   "coordinate system");

//! A cave with one good fix, and below it a Folder whose one fix (B) has a
//! transposed leading digit: 1478000 easting in UTM 13N.
struct DomainBadSubNode {
    cwCavingRegion region;
    cwSurveyNode* cave = nullptr;
    cwSurveyNode* upperLevel = nullptr;

    DomainBadSubNode()
    {
        cave = region.addNode(nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Deep Hole"));
        REQUIRE(cave != nullptr);
        SurveyTreeTestHelper::addTripWithStations(cave, {QStringLiteral("G")});
        cave->fixStations()->appendFixStation(
            makeFix(QStringLiteral("G"), QStringLiteral("EPSG:32613"), 478000.0, 4430000.0, 1655.0));
        upperLevel = region.addNode(cave, cwSurveyNode::Kind::Folder, QStringLiteral("Upper Level"));
        REQUIRE(upperLevel != nullptr);
        SurveyTreeTestHelper::addTripWithStations(upperLevel, {QStringLiteral("B")});
        upperLevel->fixStations()->appendFixStation(
            makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
        QCoreApplication::processEvents();
        REQUIRE(upperLevel->errorModel()->errors()->warningMessages().size() == 1);
    }
};

} // namespace

TEST_CASE("the project-wide fix-station warnings name the node each one sits on",
          "[cwFixStationValidator]")
{
    DomainBadSubNode project;
    cwFixStationValidator* validator = project.region.fixStationValidator();
    REQUIRE(validator != nullptr);

    const QStringList messages = validator->fixStationWarningMessages();
    REQUIRE(messages.size() == 1);
    CHECK(messages.at(0).startsWith(QStringLiteral("Upper Level: ")));
    CHECK(messages.at(0).contains(kDomainBadWording));

    SECTION("suppressing the row takes it off the list, in one change") {
        QSignalSpy changed(validator, &cwFixStationValidator::fixStationWarningsChanged);
        cwErrorListModel* errors = project.upperLevel->errorModel()->errors();
        errors->setData(errors->index(0), true,
                        static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole));

        CHECK(validator->fixStationWarningMessages().isEmpty());
        CHECK(changed.count() == 1);

        errors->setData(errors->index(0), false,
                        static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole));
        CHECK(validator->fixStationWarningMessages().size() == 1);
        CHECK(changed.count() == 2);
    }

    SECTION("a rename follows into the prefix") {
        project.upperLevel->setName(QStringLiteral("Lower Level"));

        const QStringList renamed = validator->fixStationWarningMessages();
        REQUIRE(renamed.size() == 1);
        CHECK(renamed.at(0).startsWith(QStringLiteral("Lower Level: ")));
        CHECK(renamed.at(0).contains(kDomainBadWording));
    }

    SECTION("correcting the fix clears the list") {
        project.upperLevel->fixStations()->setData(project.upperLevel->fixStations()->index(0),
                                                   478000.0, cwFixStationModel::EastingRole);
        CHECK(validator->fixStationWarningMessages().isEmpty());
    }

    SECTION("a node leaving the region takes its warning with it") {
        project.cave->removeNode(project.cave->indexOfNode(project.upperLevel));
        QCoreApplication::processEvents();
        CHECK(validator->fixStationWarningMessages().isEmpty());
    }
}

TEST_CASE("the project-wide fix-station warnings list nodes in tree order",
          "[cwFixStationValidator]")
{
    DomainBadSubNode project;
    cwSurveyNode* otherCave =
        project.region.addNode(nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Aardvark Pit"));
    REQUIRE(otherCave != nullptr);
    SurveyTreeTestHelper::addTripWithStations(otherCave, {QStringLiteral("B")});
    otherCave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
    QCoreApplication::processEvents();

    //Pre-order, not name order: Deep Hole's section comes before the cave added
    //after it.
    const QStringList messages = project.region.fixStationValidator()->fixStationWarningMessages();
    REQUIRE(messages.size() == 2);
    CHECK(messages.at(0).startsWith(QStringLiteral("Upper Level: ")));
    CHECK(messages.at(1).startsWith(QStringLiteral("Aardvark Pit: ")));
}

TEST_CASE("the Fix Stations page's add path leaves the region root without fixes",
          "[cwFixStationValidator]")
{
    //The validator walks rootNode()->allNodes(), which leaves the root out, so
    //the page's Add refuses the root rather than create a fix nothing judges.
    cwCavingRegion region;
    cwSurveyNode* cave =
        region.addNode(nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Aardvark Pit"));
    REQUIRE(cave != nullptr);
    cwFixStationTreeModel model;
    model.setRootNode(region.rootNode());
    REQUIRE(model.rowCount() == 1);

    CHECK(model.addFixAt(QModelIndex()) == -1);
    CHECK(model.addFixAt(model.index(0, 0)) == -1);
    CHECK(region.rootNode()->fixStations()->count() == 0);

    CHECK(model.addFixAt(model.nodeRowIndex(cave)) >= 0);
    CHECK(cave->fixStations()->count() == 1);
    CHECK(region.rootNode()->fixStations()->count() == 0);

    QCoreApplication::processEvents();
    for (const QString& message : region.fixStationValidator()->fixStationWarningMessages()) {
        CHECK(message.startsWith(QStringLiteral("Aardvark Pit: ")));
    }
}

TEST_CASE("a node's fix-station warnings cover the node and its descendants",
          "[cwFixStationValidator]")
{
    DomainBadSubNode project;
    cwFixStationValidator* validator = project.region.fixStationValidator();
    REQUIRE(validator != nullptr);
    cwSurveyNode* sibling =
        project.region.addNode(nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Aardvark Pit"));
    REQUIRE(sibling != nullptr);
    SurveyTreeTestHelper::addTripWithStations(sibling, {QStringLiteral("B")});

    const QStringList underCave = validator->fixStationWarningMessagesUnder(project.cave);
    REQUIRE(underCave.size() == 1);
    CHECK(underCave.at(0).startsWith(QStringLiteral("Upper Level: ")));
    CHECK(underCave.at(0).contains(kDomainBadWording));

    CHECK(validator->fixStationWarningMessagesUnder(project.upperLevel) == underCave);
    CHECK(validator->fixStationWarningMessagesUnder(project.region.rootNode()) == underCave);
    CHECK(validator->fixStationWarningMessagesUnder(sibling).isEmpty());
    CHECK(validator->fixStationWarningMessagesUnder(nullptr).isEmpty());

    SECTION("a sibling branch keeps its own warning to itself") {
        sibling->fixStations()->appendFixStation(
            makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"), 1478000.0, 4430000.0, 1655.0));
        QCoreApplication::processEvents();

        const QStringList underSibling = validator->fixStationWarningMessagesUnder(sibling);
        REQUIRE(underSibling.size() == 1);
        CHECK(underSibling.at(0).startsWith(QStringLiteral("Aardvark Pit: ")));
        CHECK(validator->fixStationWarningMessagesUnder(project.cave) == underCave);
        CHECK(validator->fixStationWarningMessagesUnder(project.region.rootNode()).size() == 2);
    }

    SECTION("suppressing the sub-node's row takes it off its ancestor's list") {
        cwErrorListModel* errors = project.upperLevel->errorModel()->errors();
        errors->setData(errors->index(0), true,
                        static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole));
        CHECK(validator->fixStationWarningMessagesUnder(project.cave).isEmpty());

        errors->setData(errors->index(0), false,
                        static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole));
        CHECK(validator->fixStationWarningMessagesUnder(project.cave) == underCave);
    }

    SECTION("a rename follows into the subtree line") {
        project.upperLevel->setName(QStringLiteral("Lower Level"));

        const QStringList renamed = validator->fixStationWarningMessagesUnder(project.cave);
        REQUIRE(renamed.size() == 1);
        CHECK(renamed.at(0).startsWith(QStringLiteral("Lower Level: ")));
    }

    SECTION("moving the sub-node carries its warning to the new ancestor") {
        REQUIRE(sibling->moveHere(project.upperLevel));
        QCoreApplication::processEvents();

        CHECK(validator->fixStationWarningMessagesUnder(project.cave).isEmpty());
        const QStringList underSibling = validator->fixStationWarningMessagesUnder(sibling);
        REQUIRE(underSibling.size() == 1);
        CHECK(underSibling.at(0).startsWith(QStringLiteral("Upper Level: ")));
    }
}

namespace {

constexpr int kBudgetNodeCount = 12;
constexpr int kBudgetSmallNodeCount = 40;
constexpr int kBudgetLargeNodeCount = kBudgetSmallNodeCount * 4;
constexpr int kBudgetStallRepeats = 3;

// Four times the nodes in one turn is one pass over four times the nodes; a
// pass for each node grows about sixteenfold.
constexpr double kMaxBudgetStallGrowth = 4.0;

constexpr double kDomainBadEasting = 1478000.0;
constexpr double kDomainNorthing = 4430000.0;
constexpr double kDomainElevation = 1655.0;
constexpr double kOutlierEasting = 500150.0;
constexpr double kOutlierNorthing = 5194000.0;
constexpr double kOutlierElevation = 2708.0;

//! A detached cave whose one fix (B) has a transposed leading digit, so it
//! raises a domain warning wherever it joins.
cwCave* makeDomainBadCave()
{
    auto* cave = new cwCave();
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B"), QStringLiteral("EPSG:32613"),
                kDomainBadEasting, kDomainNorthing, kDomainElevation));
    return cave;
}

//! A cave appended to \a region whose one fix sits ~1000 km north of the frame
//! appendGoodFixes anchors.
cwCave* addOutlierCave(cwCavingRegion& region, const QString& name)
{
    region.addCave();
    cwCave* cave = region.cave(region.caveCount() - 1);
    REQUIRE(cave != nullptr);
    cave->setName(name);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"), kOutlierEasting, kOutlierNorthing, kOutlierElevation));
    return cave;
}

//! The shortest of kBudgetStallRepeats main-thread stalls of removing \a nodes
//! domain-bad caves in one turn.
qint64 measureRemovalStall(int nodes)
{
    qint64 shortest = std::numeric_limits<qint64>::max();
    for (int repeat = 0; repeat < kBudgetStallRepeats; ++repeat) {
        cwCavingRegion region;
        for (int i = 0; i < nodes; ++i) {
            region.addCave(makeDomainBadCave());
        }
        QCoreApplication::processEvents();
        REQUIRE(region.fixStationValidator()->fixStationWarningMessages().size() == nodes);

        const qint64 stall = longestMainThreadStall([&region]() {
            while (region.caveCount() > 0) {
                region.removeCave(region.caveCount() - 1);
            }
        });
        shortest = std::min(shortest, stall);
        QCoreApplication::processEvents();
        REQUIRE(region.fixStationValidator()->fixStationWarningMessages().isEmpty());
    }
    return shortest;
}

} // namespace

TEST_CASE("nodes joining in one turn revalidate once",
          "[cwFixStationValidator][Budget]")
{
    cwCavingRegion region;
    cwFixStationValidator* validator = region.fixStationValidator();
    REQUIRE(validator != nullptr);
    QSignalSpy changed(validator, &cwFixStationValidator::fixStationWarningsChanged);

    for (int i = 0; i < kBudgetNodeCount; ++i) {
        region.addCave(makeDomainBadCave());
    }
    QCoreApplication::processEvents();

    CHECK(changed.count() == 1);
    CHECK(validator->fixStationWarningMessages().size() == kBudgetNodeCount);
}

TEST_CASE("nodes leaving in one turn revalidate once",
          "[cwFixStationValidator][Budget]")
{
    cwCavingRegion region;
    cwFixStationValidator* validator = region.fixStationValidator();
    REQUIRE(validator != nullptr);
    for (int i = 0; i < kBudgetNodeCount; ++i) {
        region.addCave(makeDomainBadCave());
    }
    QCoreApplication::processEvents();
    REQUIRE(validator->fixStationWarningMessages().size() == kBudgetNodeCount);

    QSignalSpy changed(validator, &cwFixStationValidator::fixStationWarningsChanged);
    while (region.caveCount() > 0) {
        region.removeCave(region.caveCount() - 1);
    }
    QCoreApplication::processEvents();

    CHECK(changed.count() == 1);
    CHECK(validator->fixStationWarningMessages().isEmpty());
}

TEST_CASE("deleting the first outlier's cave leaves no dangling summary",
          "[cwFixStationValidator][Budget]")
{
    cwCavingRegion region;
    region.addCave();
    cwCave* goodCave = region.cave(0);
    REQUIRE(goodCave != nullptr);
    appendGoodFixes(goodCave);

    QPointer<cwCave> firstOutlier = addOutlierCave(region, QStringLiteral("First Sink"));
    QPointer<cwCave> nextOutlier = addOutlierCave(region, QStringLiteral("Next Sink"));
    QCoreApplication::processEvents();

    cwFixStationValidator* validator = region.fixStationValidator();
    REQUIRE(validator != nullptr);
    REQUIRE(validator->firstOutlierCave() == firstOutlier.data());

    // Destroy the cave before the queued pass can run: removal with no undo
    // stack deletes it on the next deferred-delete sweep.
    region.removeCave(region.indexOf(firstOutlier.data()));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    REQUIRE(firstOutlier.isNull());

    CHECK(validator->firstOutlierCave() == nullptr);
    QCoreApplication::processEvents();
    CHECK(validator->firstOutlierCave() == nextOutlier.data());

    // With no outlier left, the summary stays empty through the pass.
    region.removeCave(region.indexOf(nextOutlier.data()));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    REQUIRE(nextOutlier.isNull());

    CHECK(validator->firstOutlierCave() == nullptr);
    QCoreApplication::processEvents();
    CHECK(validator->firstOutlierCave() == nullptr);
    CHECK(validator->outlierCount() == 0);
}

TEST_CASE("removing nodes in one turn stalls in time linear in the nodes",
          "[cwFixStationValidator][Budget]")
{
    const qint64 small = measureRemovalStall(kBudgetSmallNodeCount);
    const qint64 large = measureRemovalStall(kBudgetLargeNodeCount);

    INFO("small: " << kBudgetSmallNodeCount << " nodes, " << small << " ms stall");
    INFO("large: " << kBudgetLargeNodeCount << " nodes, " << large << " ms stall");
    REQUIRE(small > 0);
    const double growth = double(large) / double(small);
    INFO("growth " << growth);
    CHECK(growth <= kMaxBudgetStallGrowth);
}
