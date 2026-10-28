/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

// Cavewhere
#include "cwAttachedCenterlinesModel.h"
#include "cwAttachedFixModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwLength.h"
#include "cwLinePlotManager.h"
#include "cwLocalProjectionManager.h"
#include "cwNodeWarningModel.h"
#include "cwCavernNaming.h"
#include "cwScopeLabels.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwError.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwRenderLinePlot.h"
#include "cwScopeStationListModel.h"
#include "cwErrorModel.h"
#include "cwStationHandle.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Test helpers
#include "CasePairTestHelpers.h"
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"

// Qt
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QVector3D>
#include <QtMath>

// Std
#include <algorithm>
#include <cmath>
#include <optional>

namespace {

// Solved station positions are floats, so a total assembled from them drifts
// by fractions of a centimeter off the hand-computed tape lengths.
constexpr double kSolvedLengthMarginMeters = 0.05;

cwTrip* addTripWithShot(cwCave* cave, const QString& name)
{
    cwTrip* trip = addEmptyTrip(cave, name);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(QStringLiteral("N1")), cwStation(QStringLiteral("N2")), shot);
    return trip;
}

// A node attached to an external file: the file supplies the node's whole
// body, read through its one *include.
cwCave* addCaveAttachedTo(cwCavingRegion& region,
                          const QString& name,
                          const QString& fixture,
                          const QTemporaryDir& tempRoot,
                          QHash<QUuid, QString>& caveDirs)
{
    cwCave* cave = addEmptyCave(region, name);
    cave->setExternalCenterline(cwExternalCenterline(fixture));

    const QString attachDir = tempSubdir(tempRoot, name);
    seedAttachment(attachDir, fixturePath(fixture));
    caveDirs.insert(cave->id(), attachDir);
    return cave;
}

} // namespace

TEST_CASE("Native region has zero attached rows and a native driver source",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Native"));
    addTripWithShot(cave, QStringLiteral("NativeTrip"));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(manager.externalCenterlineManager()->attachedCenterlinesModel()->rowCount() == 0);

    // The driver text opens the cave under its own survey label, not a
    // synthetic one: the only cave here is "Native", so the label is its name
    // folded to an identifier.
    const QString driver = manager.driverSource();
    REQUIRE_FALSE(driver.isEmpty());
    // Matched to the end of the token, so a stray collision suffix
    // ("*begin native_2" for the only cave in the region) fails here.
    CHECK(driver.contains(QStringLiteral("*begin %1 ;%2")
                          .arg(cwCavernNaming::sanitizeToCavernIdentifier(cave->name()),
                               cave->name())));
    CHECK_FALSE(driver.contains(QStringLiteral("*include")));
}

TEST_CASE("Attaching one trip yields one row and an *include in the driver source",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("one-trip"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    const cwAttachedCenterlinesModel* model = manager.externalCenterlineManager()->attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Attached"));
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerKindRole).toString()
          == QStringLiteral("Trip"));
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::EntryFileRole).toString()
          == QStringLiteral("survex_simple.svx"));
    // survex_simple.svx has no *include chain: the closure is the entry alone.
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::WarningCountRole).toInt() == 0);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::LastSolvedRole)
          .toDateTime().isValid());

    // The solve's driver carries the absolute forward-slash quoted
    // *include line per Phase 1 commit 8's format.
    const QString expectedAbs = QFileInfo(QDir(attachDir)
        .filePath(QStringLiteral("survex_simple.svx"))).absoluteFilePath();
    CHECK(manager.driverSource().contains(QStringLiteral("*include \"%1\"").arg(expectedAbs)));
}

TEST_CASE("Rows sort by cave display name then trip display name",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString zuluDir = tempSubdir(tempRoot, QStringLiteral("zulu"));
    seedAttachment(zuluDir, fixturePath(QStringLiteral("survex_simple.svx")));
    const QString alphaDir = tempSubdir(tempRoot, QStringLiteral("alpha"));
    seedAttachment(alphaDir, fixturePath(QStringLiteral("survex_simple.svx")));

    // Insert the later-sorting cave first so the model's order can't
    // accidentally mirror region insertion order.
    cwCavingRegion region;
    cwCave* zuluCave = addEmptyCave(region, QStringLiteral("Zulu"));
    cwTrip* zuluTrip = addAttachedTrip(zuluCave, QStringLiteral("Alpha trip"));
    cwCave* alphaCave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* alphaTrip = addAttachedTrip(alphaCave, QStringLiteral("Zeta trip"));
    Q_UNUSED(alphaCave);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(zuluTrip->id(), zuluDir);
    tripDirs.insert(alphaTrip->id(), alphaDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    const cwAttachedCenterlinesModel* model = manager.externalCenterlineManager()->attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 2);
    // Cave name is the major key: Alpha's "Zeta trip" precedes Zulu's
    // "Alpha trip" even though the trip names sort the other way.
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Zeta trip"));
    CHECK(roleAt(model, 1, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Alpha trip"));
}

TEST_CASE("Detach removes the owner's row via rowsRemoved with correct indices",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString firstDir = tempSubdir(tempRoot, QStringLiteral("first"));
    seedAttachment(firstDir, fixturePath(QStringLiteral("survex_simple.svx")));
    const QString secondDir = tempSubdir(tempRoot, QStringLiteral("second"));
    seedAttachment(secondDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Detach"));
    cwTrip* first = addAttachedTrip(cave, QStringLiteral("First"));
    cwTrip* second = addAttachedTrip(cave, QStringLiteral("Second"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(first->id(), firstDir);
    tripDirs.insert(second->id(), secondDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    cwAttachedCenterlinesModel* model = manager.externalCenterlineManager()->attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 2);
    REQUIRE(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
            == QStringLiteral("First"));

    QSignalSpy removedSpy(model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);

    // Detach "First": clear its centerline and drop its attachment-dir
    // entry — the setter drives the (async) recompute that rebuilds the
    // rows, so drain it before asserting on the model signals.
    first->setExternalCenterline(cwExternalCenterline());
    QHash<QUuid, QString> remainingDirs;
    remainingDirs.insert(second->id(), secondDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(remainingDirs);
    manager.waitToFinish();

    REQUIRE(removedSpy.count() == 1);
    CHECK(removedSpy.at(0).at(1).toInt() == 0); // first
    CHECK(removedSpy.at(0).at(2).toInt() == 0); // last
    CHECK(resetSpy.count() == 0);
    REQUIRE(model->rowCount() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Second"));
}

TEST_CASE("Attached model answers errorFor by owner",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    const QUuid errorOwner = QUuid::createUuid();
    const QUuid cleanOwner = QUuid::createUuid();
    const QString message = QStringLiteral("reaches outside the project");

    cwAttachedCenterlinesModel model;

    QVector<cwAttachedCenterlinesModel::Row> rows;
    cwAttachedCenterlinesModel::Row errorRow;
    errorRow.ownerId = errorOwner;
    errorRow.caveName = QStringLiteral("Alpha");
    errorRow.ownerName = QStringLiteral("Alpha");
    errorRow.ownerKind = QStringLiteral("Cave");
    errorRow.error = message;
    rows.append(errorRow);

    cwAttachedCenterlinesModel::Row cleanRow;
    cleanRow.ownerId = cleanOwner;
    cleanRow.caveName = QStringLiteral("Beta");
    cleanRow.ownerName = QStringLiteral("Beta Trip");
    cleanRow.ownerKind = QStringLiteral("Trip");
    rows.append(cleanRow);

    model.setRows(rows);
    REQUIRE(model.rowCount() == 2);

    CHECK(model.errorFor(errorOwner) == message);
    CHECK(model.errorFor(cleanOwner).isEmpty());

    // An owner with no row reads as clean.
    CHECK(model.errorFor(QUuid::createUuid()).isEmpty());
}

TEST_CASE("A cave-level Compass attach with an untied survey warns on its node",
          "[LinePlotManager][Attach]")
{
    // compass_untied.dat holds two surveys, S1-S3 and U1-U3, that share no
    // station. Cavern fixes S1 and drops the U survey, naming only U1.
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    QHash<QUuid, QString> caveDirs;
    cwCave* cave = addCaveAttachedTo(region, QStringLiteral("Untied"),
                                     QStringLiteral("compass_untied.dat"),
                                     tempRoot, caveDirs);

    cwLinePlotManager manager;
    manager.externalCenterlineManager()->setCaveAttachmentDirs(caveDirs);
    manager.setRegion(&region);
    manager.waitToFinish();
    INFO("cavern log:\n" << manager.cavernLog().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    const std::optional<cwError> entry = unconnectedStationsEntry(cave->errorModel());
    REQUIRE(entry.has_value());
    CHECK(entry->type() == cwError::Warning);
    CHECK(entry->message() == QStringLiteral("3 stations in compass_untied.dat are not tied to the cave"));
    CHECK(entry->detail() == QStringLiteral("u1, u2, u3"));
    CHECK(entry->targetId() == cave->id());
}

TEST_CASE("Tying an attached file's dropped survey in clears its warning on the next solve",
          "[LinePlotManager][Attach]")
{
    // survex_two_parts.svx holds PartA and PartB with nothing between them.
    // Cavern fixes PartA and drops PartB until an equate joins the two.
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    QHash<QUuid, QString> caveDirs;
    cwCave* cave = addCaveAttachedTo(region, QStringLiteral("Two parts"),
                                     QStringLiteral("survex_two_parts.svx"),
                                     tempRoot, caveDirs);

    cwLinePlotManager manager;
    manager.externalCenterlineManager()->setCaveAttachmentDirs(caveDirs);
    manager.setRegion(&region);
    manager.waitToFinish();
    INFO("cavern log:\n" << manager.cavernLog().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    const std::optional<cwError> entry = unconnectedStationsEntry(cave->errorModel());
    REQUIRE(entry.has_value());
    CHECK(entry->message() == QStringLiteral("3 stations in survex_two_parts.svx are not tied to the cave"));
    CHECK(entry->detail() == QStringLiteral("partb.b1, partb.b2, partb.b3"));

    region.equates()->appendEquate(
        cwEquate({cwStationHandle(cwStationHandle::NativeCave, cave->id(), QStringLiteral("parta.a3")),
                  cwStationHandle(cwStationHandle::NativeCave, cave->id(), QStringLiteral("partb.b1"))}));
    manager.waitToFinish();
    INFO("cavern log after the tie:\n" << manager.cavernLog().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    CHECK(cave->scopedPositions().hasPosition(QStringLiteral("partb.b3")));
    CHECK_FALSE(unconnectedStationsEntry(cave->errorModel()).has_value());
}

namespace {

constexpr double kFeetToMeters = 0.3048;
constexpr double kCompassTapeFeet = 10.0;
// ALPHA's six legs, BRAVO's three, and CHARLIE's two, each one tape.
constexpr int kCompassProjectLegCount = 11;
constexpr int kCompassAlphaLegCount = 6;
constexpr double kCompassNorthing = 4000000.0;
constexpr double kCompassElevation = 100.0;
// PROJECT.MAK's datum and zone, and a free spot east of ALPHA for a fixed
// native trip.
const QString kCompassProjectCS = QStringLiteral("EPSG:32616");
constexpr double kCompassBravoDigEasting = 500100.0;

constexpr double kWallsFixElevation = 100.0;
constexpr double kWallsTapeMeters = 10.0;
// D01 and D02's four legs and N01's three.
constexpr int kWallsProjectLegCount = 7;
constexpr double kWallsWestFixEasting = 500.0;
// E1.SRV's two legs and the West files' four.
constexpr int kWallsTwoCavesLegCount = 6;
// EAST.SRV's two legs and WEST.SRV's two, over five stations.
constexpr int kWallsEmptyDatumLegCount = 4;
constexpr int kWallsEmptyDatumStationCount = 5;
// EAST.SRV's #FIX A1 elevation.
constexpr double kWallsEmptyDatumElevation = 219.0;

constexpr double kSurvexUtmElevation = 100.0;
constexpr double kSurvexLatLongElevation = 200.0;
// The .3d stores centimeters, and positions this near the frame's origin are
// float-exact well below that.
constexpr double kLocalFrameMarginMeters = 0.02;

//! Attaches \a fixture as a source root under the region's root in
//! \a project and drains the pipelines; requireSolved() checks the solve.
//! The (name, position) pairs a trip's solvedStations() yields, by name.
QMap<QString, QVector3D> solvedByName(const cwTrip* trip)
{
    QMap<QString, QVector3D> named;
    for (const auto& station : trip->solvedStations()) {
        named.insert(station.first, station.second);
    }
    return named;
}

cwTrip* tripNamedIn(const cwSurveyNode* node, const QString& name)
{
    for (cwTrip* trip : node->trips()) {
        if (trip->name() == name) {
            return trip;
        }
    }
    return nullptr;
}

cwSurveyNode* attachSourceAndSolve(SavedProjectFixture* project, const QString& fixture)
{
    cwSurveyNode* root = attachSourceThroughManager(
        project, project->project->cavingRegion()->rootNode(), fixturePath(fixture));
    drainPipelines(project);
    return root;
}

void requireSolved(SavedProjectFixture* project)
{
    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE_FALSE(linePlot->hasSolveError());
}

void checkPosition(const cwStationPositionLookup& lookup, const QString& station,
                   double x, double y, double z, double margin = kSolvedLengthMarginMeters)
{
    INFO("station " << station.toStdString());
    REQUIRE(lookup.hasPosition(station));
    const QVector3D position = lookup.position(station);
    CHECK(position.x() == Catch::Approx(x).margin(margin));
    CHECK(position.y() == Catch::Approx(y).margin(margin));
    CHECK(position.z() == Catch::Approx(z).margin(margin));
}

//! Checks that \a cave's fixed \a station solved where PROJ carries its
//! fix's placement into \a solvedCS.
void checkAtPlacement(const cwSurveyNode* cave, const QString& station, const QString& solvedCS,
                      double margin)
{
    INFO("station " << station.toStdString());
    const cwAttachedFix fix = subtreeAttachedFixFor(cave, station);
    INFO("coordinate " << fix.coordinate.toStdString() << ", solve system "
                       << fix.solveSystem().toStdString() << ", solved system "
                       << solvedCS.toStdString());
    const std::optional<cwAttachedFix::Placement> placement = fix.placementIn(fix.solveSystem());
    REQUIRE(placement.has_value());
    const std::optional<cwGeoPoint> solved =
        cwCoordinateTransform::transformPoint(placement->cs, solvedCS, placement->point);
    REQUIRE(solved.has_value());
    checkPosition(cave->scopedPositions(), station, solved->x, solved->y, solved->z, margin);
}

//! Checks that each walls_fix_units fixed station solved where PROJ carries
//! its fix's placement into \a solvedCS. Each .srv is its own component with
//! one fix, so each fixed station sits exactly on its fix's placement.
void checkWallsFixUnitsAtPlacements(const cwSurveyNode* cave, const QString& solvedCS, double margin)
{
    for (const QString& station : {QStringLiteral("M1"), QStringLiteral("F1"), QStringLiteral("L1")}) {
        checkAtPlacement(cave, station, solvedCS, margin);
    }
}

//! Checks that \a project's frame is anchored on \a owner's attachment and
//! that the Data page names it after \a station.
void checkAnchoredOnAttachment(SavedProjectFixture* project, const cwSurveyNode* owner,
                               const QString& station)
{
    cwCavingRegion* region = project->project->cavingRegion();
    const cwGeoReference* geoReference = region->geoReference();
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == owner->id());
    CHECK(region->localProjection()->anchorDescription().toStdString()
          == QStringLiteral("%1 — %2").arg(station, owner->name()).toStdString());
}

//! The horizontal distance between two solved stations of \a lookup.
double solvedDistance(const cwStationPositionLookup& lookup, const QString& from, const QString& to)
{
    INFO("from " << from.toStdString() << " to " << to.toStdString());
    REQUIRE(lookup.hasPosition(from));
    REQUIRE(lookup.hasPosition(to));
    return lookup.position(from).toVector2D().distanceToPoint(lookup.position(to).toVector2D());
}

} // namespace

TEST_CASE("compass_project solves as a source root",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("compass-project-solve"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* cave = attachSourceAndSolve(project.get(), QStringLiteral("compass_project/PROJECT.MAK"));
    requireSolved(project.get());

    // Cavern reads each DAT into a survey named after its file, so the absent
    // DELTA.DAT leaves the three present ones solving.
    const cwStationPositionLookup lookup = cave->scopedPositions();
    for (const QString& station : {QStringLiteral("alpha.A1"), QStringLiteral("alpha.A2"),
                                   QStringLiteral("alpha.A3"), QStringLiteral("alpha.A4"),
                                   QStringLiteral("alpha.A5"), QStringLiteral("alpha.A6"),
                                   QStringLiteral("alpha.A7"), QStringLiteral("bravo.A5"),
                                   QStringLiteral("bravo.B1"), QStringLiteral("bravo.B2"),
                                   QStringLiteral("bravo.B3"), QStringLiteral("charlie.C1"),
                                   QStringLiteral("charlie.C2"), QStringLiteral("charlie.C3")}) {
        INFO("station " << station.toStdString());
        CHECK(lookup.hasPosition(station));
    }
    CHECK(lookup.positions().size() == 14);

    // The .mak's datum and zone give A1's fix a system of its own, so the
    // project with no frame takes one centered on it.
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());
    checkAnchoredOnAttachment(project.get(), cave, QStringLiteral("alpha.A1"));
    const QString frame = project->project->cavingRegion()->geoReference()->localCoordinateSystem();
    const cwStationPositionLookup framed = cave->scopedPositions();
    checkPosition(framed, QStringLiteral("alpha.A1"), 0.0, 0.0, kCompassElevation,
                  kLocalFrameMarginMeters);
    checkAtPlacement(cave, QStringLiteral("charlie.C1"), frame, kLocalFrameMarginMeters);

    // In the frame the driver sets the .mak's location's declination, which
    // turns the legs; distances survive the turn. BRAVO's link station is
    // ALPHA's A5, two tapes east and two north of A1; B3 is two tapes east and
    // one north of it.
    const double tape = kCompassTapeFeet * kFeetToMeters;
    for (const QString& linkStation : {QStringLiteral("alpha.A5"), QStringLiteral("bravo.A5")}) {
        CHECK(solvedDistance(framed, QStringLiteral("alpha.A1"), linkStation)
              == Catch::Approx(std::hypot(2.0 * tape, 2.0 * tape)).margin(kSolvedLengthMarginMeters));
    }
    CHECK(solvedDistance(framed, QStringLiteral("alpha.A1"), QStringLiteral("bravo.B3"))
          == Catch::Approx(std::hypot(4.0 * tape, 3.0 * tape)).margin(kSolvedLengthMarginMeters));

    CHECK(cave->length()->value()
          == Catch::Approx(kCompassProjectLegCount * tape).margin(kSolvedLengthMarginMeters));
    CHECK(childNodeNamed(cave, QStringLiteral("Alpha"))->length()->value()
          == Catch::Approx(kCompassAlphaLegCount * tape).margin(kSolvedLengthMarginMeters));
}

TEST_CASE("walls_project solves as a source root",
          "[LinePlotManager][Attach][SourcedFixtures][BareFixOrigin]")
{
    auto project = makeSavedProject(QStringLiteral("walls-project-solve"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* cave = attachSourceAndSolve(project.get(), QStringLiteral("walls_project/KAUA.WPJ"));
    requireSolved(project.get());

    // K1's #FIX names no system and the project has no frame, so the file
    // is a local grid placed with K1 at the origin; z passes through.
    CHECK(project->project->cavingRegion()->geoReference()->state()
          == cwGeoReference::LocalGrid);

    // Books and files make no level; #PREFIX N does.
    const cwStationPositionLookup lookup = cave->scopedPositions();
    checkPosition(lookup, QStringLiteral("K1"), 0.0, 0.0, kWallsFixElevation);
    checkPosition(lookup, QStringLiteral("D4"),
                  2.0 * kWallsTapeMeters, 2.0 * kWallsTapeMeters, kWallsFixElevation);
    checkPosition(lookup, QStringLiteral("N.3"),
                  3.0 * kWallsTapeMeters, 0.0, kWallsFixElevation);
    CHECK(lookup.positions().size() == 8);
    CHECK(cave->length()->value()
          == Catch::Approx(kWallsProjectLegCount * kWallsTapeMeters).margin(kSolvedLengthMarginMeters));
}

TEST_CASE("walls_two_caves solves as a source root",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("walls-two-caves-solve"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* cave = attachSourceAndSolve(project.get(), QStringLiteral("walls_two_caves/TWO.WPJ"));
    requireSolved(project.get());

    // Each book fixes its own entrance, and W2.SRV joins W1.SRV at W3.
    const cwStationPositionLookup lookup = cave->scopedPositions();
    checkPosition(lookup, QStringLiteral("E1"), 0.0, 0.0, kWallsFixElevation);
    checkPosition(lookup, QStringLiteral("E3"), kWallsTapeMeters, kWallsTapeMeters, kWallsFixElevation);
    checkPosition(lookup, QStringLiteral("W5"),
                  kWallsWestFixEasting - 2.0 * kWallsTapeMeters, 2.0 * kWallsTapeMeters,
                  kWallsFixElevation);
    CHECK(lookup.positions().size() == 8);
    CHECK(cave->length()->value()
          == Catch::Approx(kWallsTwoCavesLegCount * kWallsTapeMeters).margin(kSolvedLengthMarginMeters));
}

TEST_CASE("walls_empty_datum solves as a source root",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("walls-empty-datum-solve"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* cave = attachSourceAndSolve(project.get(),
                                              QStringLiteral("walls_empty_datum/walls_empty_datum.wpj"));
    requireSolved(project.get());

    // The .REF gives A1's fix a system of its own, so the project with no
    // frame takes one centered on it.
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());
    checkAnchoredOnAttachment(project.get(), cave, QStringLiteral("A1"));

    // The frame's grid convergence turns the legs off the grid axes, so
    // compare distances, which survive the turn.
    const cwStationPositionLookup lookup = cave->scopedPositions();
    CHECK(lookup.positions().size() == kWallsEmptyDatumStationCount);
    checkPosition(lookup, QStringLiteral("A1"), 0.0, 0.0, kWallsEmptyDatumElevation,
                  kLocalFrameMarginMeters);
    // EAST.SRV runs north then east to A3; WEST.SRV runs north then west back
    // over A1's easting.
    CHECK(solvedDistance(lookup, QStringLiteral("A1"), QStringLiteral("A3"))
          == Catch::Approx(std::sqrt(2.0) * kWallsTapeMeters).margin(kSolvedLengthMarginMeters));
    CHECK(solvedDistance(lookup, QStringLiteral("A1"), QStringLiteral("B2"))
          == Catch::Approx(2.0 * kWallsTapeMeters).margin(kSolvedLengthMarginMeters));
    CHECK(cave->length()->value()
          == Catch::Approx(kWallsEmptyDatumLegCount * kWallsTapeMeters).margin(kSolvedLengthMarginMeters));
}

TEST_CASE("walls_fix_units solves its fixes where their rows place them",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("walls-fix-units-solve"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* cave = attachSourceAndSolve(project.get(),
                                              QStringLiteral("walls_fix_units/walls_fix_units.wpj"));
    requireSolved(project.get());

    // The .REF gives every fix a system of its own, so the project with no
    // frame takes one centered on the first, M1.
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());
    checkAnchoredOnAttachment(project.get(), cave, QStringLiteral("M1"));
    const QString frame = project->project->cavingRegion()->geoReference()->localCoordinateSystem();
    checkWallsFixUnitsAtPlacements(cave, frame, kLocalFrameMarginMeters);
    const QVector3D m1 = cave->scopedPositions().position(QStringLiteral("M1"));
    CHECK(m1.x() == Catch::Approx(0.0).margin(kLocalFrameMarginMeters));
    CHECK(m1.y() == Catch::Approx(0.0).margin(kLocalFrameMarginMeters));
}

TEST_CASE("walls_detached anchors the frame on the attached survey's fix",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("walls-detached-solve"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* cave = attachSourceAndSolve(project.get(),
                                              QStringLiteral("walls_detached/walls_detached.wpj"));
    requireSolved(project.get());

    // SHEETS' S1, ten kilometers west, comes before CAVE's E1 in the tree, but
    // its book is detached, so cavern places only E1..E3 and E1 anchors the frame.
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());
    checkAnchoredOnAttachment(project.get(), cave, QStringLiteral("E1"));
    const cwStationPositionLookup lookup = cave->scopedPositions();
    CHECK(lookup.positions().size() == 3);
    const QVector3D e1 = lookup.position(QStringLiteral("E1"));
    CHECK(e1.x() == Catch::Approx(0.0).margin(kLocalFrameMarginMeters));
    CHECK(e1.y() == Catch::Approx(0.0).margin(kLocalFrameMarginMeters));
}

TEST_CASE("walls_fix_units solves its fixes where their rows place them in a native fix's frame",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    // A native fix in WGS 84 UTM 16N, within a few kilometers of every
    // walls_fix_units fix, so the project's frame centers near them.
    constexpr double kNativeFixEasting = 521000.0;
    constexpr double kNativeFixNorthing = 4040000.0;
    constexpr double kNativeFixElevation = 100.0;
    auto project = makeSavedProject(QStringLiteral("walls-fix-units-frame"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    addNativeTripWithShot(project->cave, QStringLiteral("Native survey"),
                          QStringLiteral("m1"), QStringLiteral("m2"));
    cwFixStation nativeFix;
    nativeFix.setStationName(QStringLiteral("m1"));
    nativeFix.setInputCS(kCompassProjectCS);
    nativeFix.setCoordinate(kNativeFixEasting, kNativeFixNorthing, kNativeFixElevation);
    project->cave->fixStations()->appendFixStation(nativeFix);

    cwSurveyNode* cave = attachSourceAndSolve(project.get(),
                                              QStringLiteral("walls_fix_units/walls_fix_units.wpj"));
    const cwGeoReference* geoReference = project->project->cavingRegion()->geoReference();
    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    REQUIRE(tryWait(kAttachWaitMs, [geoReference, linePlot]() {
        return geoReference->hasCoordinateSystem()
               && linePlot->driverSource().contains(QStringLiteral("*cs out"));
    }));
    drainPipelines(project.get());
    requireSolved(project.get());

    // M1 and F1 are grid fixes and L1 a lat/long fix, so the frame carries
    // both kinds of Walls #FIX.
    checkWallsFixUnitsAtPlacements(cave, geoReference->localCoordinateSystem(),
                                   kLocalFrameMarginMeters);
}

TEST_CASE("survex_two_cs solves as two source roots in one output system",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("survex-two-cs-solve"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* utmCave = attachSourceAndSolve(project.get(), QStringLiteral("survex_two_cs/utm.svx"));
    cwSurveyNode* latLongCave = attachSourceAndSolve(project.get(), QStringLiteral("survex_two_cs/latlong.svx"));
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());

    // utm.svx is attached first, so its fix anchors the frame.
    checkAnchoredOnAttachment(project.get(), utmCave, QStringLiteral("utm.u1"));
    const QString frame = project->project->cavingRegion()->geoReference()->localCoordinateSystem();
    const cwStationPositionLookup utm = utmCave->scopedPositions();
    checkPosition(utm, QStringLiteral("utm.u1"), 0.0, 0.0, kSurvexUtmElevation,
                  kLocalFrameMarginMeters);
    CHECK(utm.positions().size() == 3);

    // l1 sits on zone 16N's central meridian, u1's too, so it lands south of
    // u1 on the frame's meridian.
    checkAtPlacement(latLongCave, QStringLiteral("latlong.l1"), frame, kLocalFrameMarginMeters);
    const cwStationPositionLookup latLong = latLongCave->scopedPositions();
    const QVector3D l1 = latLong.position(QStringLiteral("latlong.l1"));
    CHECK(l1.x() == Catch::Approx(0.0).margin(kLocalFrameMarginMeters));
    CHECK(l1.y() < 0.0);
    CHECK(l1.z() == Catch::Approx(kSurvexLatLongElevation).margin(kLocalFrameMarginMeters));
    CHECK(latLong.positions().size() == 2);
}

TEST_CASE("compass_project's derived trips yield their stations under local names",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("compass-project-derived"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), QStringLiteral("compass_project/PROJECT.MAK"));
    requireSolved(project.get());

    const cwSurveyNode* alpha = childNodeNamed(root, QStringLiteral("Alpha"));
    const cwSurveyNode* bravo = childNodeNamed(root, QStringLiteral("Bravo"));
    REQUIRE(alpha != nullptr);
    REQUIRE(bravo != nullptr);
    CHECK(alpha->cavernPath() == QStringLiteral("alpha."));

    // ALPHA1 and ALPHA2 share A5, and BRAVO1 starts from its link station.
    // Compass names keep the file's case, as cavern reads them.
    CHECK(solvedByName(tripNamedIn(alpha, QStringLiteral("ALPHA1"))).keys()
          == QStringList({QStringLiteral("A1"), QStringLiteral("A2"), QStringLiteral("A3"),
                          QStringLiteral("A4"), QStringLiteral("A5")}));
    CHECK(solvedByName(tripNamedIn(alpha, QStringLiteral("ALPHA2"))).keys()
          == QStringList({QStringLiteral("A5"), QStringLiteral("A6"), QStringLiteral("A7")}));
    CHECK(solvedByName(tripNamedIn(bravo, QStringLiteral("BRAVO1"))).keys()
          == QStringList({QStringLiteral("A5"), QStringLiteral("B1"), QStringLiteral("B2"),
                          QStringLiteral("B3")}));

    // A derived node's view is its members, named below its cavern path.
    const cwStationPositionLookup alphaView = alpha->scopedPositions();
    CHECK(alphaView.positions().size() == 7);
    CHECK(alphaView.position(QStringLiteral("A5")) == root->scopedPositions().position(QStringLiteral("alpha.A5")));

    // Every station of the root's view belongs to some derived trip's unit,
    // read back through the derived views (§7.3).
    QSet<QString> covered;
    for (const cwTrip* trip : root->allTrips()) {
        const QString cavernPath = trip->parentNode()->cavernPath();
        for (const auto& station : trip->solvedStations()) {
            covered.insert(cwStation::canonicalKey(cavernPath + station.first));
        }
    }
    const QStringList rootKeys = root->scopedPositions().positions().keys();
    CHECK(covered == QSet<QString>(rootKeys.cbegin(), rootKeys.cend()));

    // Decode stays at the root: every name under the root's label resolves to
    // the root or below it, and the root owns it.
    const cwScopeLabels labels(project->project->cavingRegion()->data());
    const QString rootPrefix = root->scopePrefix();
    for (const QString& key : rootKeys) {
        const cwScopeLabels::Resolution resolution = labels.resolve(rootPrefix + key);
        INFO("station " << key.toStdString());
        CHECK(labels.sourceRoot(resolution.nodeId) == root->id());
    }
}

TEST_CASE("walls_project's books take their stations by unit membership",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    auto project = makeSavedProject(QStringLiteral("walls-project-derived"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), QStringLiteral("walls_project/KAUA.WPJ"));
    requireSolved(project.get());

    const cwSurveyNode* surveys = childNodeNamed(root, QStringLiteral("Surveys"));
    REQUIRE(surveys != nullptr);
    const cwSurveyNode* dec2001 = childNodeNamed(surveys, QStringLiteral("Dec 2001 Surveys"));
    REQUIRE(dec2001 != nullptr);

    // Books make no naming level, so their cavern path is empty and the
    // #PREFIX stays on the name.
    CHECK(dec2001->cavernPath().isEmpty());
    CHECK(solvedByName(tripNamedIn(dec2001, QStringLiteral("D Survey 1"))).keys()
          == QStringList({QStringLiteral("D1"), QStringLiteral("D2"), QStringLiteral("K1")}));
    CHECK(solvedByName(tripNamedIn(dec2001, QStringLiteral("N Surveys"))).keys()
          == QStringList({QStringLiteral("D4"), QStringLiteral("N.1"), QStringLiteral("N.2"),
                          QStringLiteral("N.3")}));

    CHECK(dec2001->length()->value()
          == Catch::Approx(kWallsProjectLegCount * kWallsTapeMeters).margin(kSolvedLengthMarginMeters));
    CHECK(childNodeNamed(root, QStringLiteral("Drawings"))->length()->value() == 0.0);

    // Every station of the root's view belongs to some unit (§7.3).
    QSet<QString> covered;
    for (const cwTrip* trip : root->allTrips()) {
        const QString cavernPath = trip->parentNode()->cavernPath();
        for (const auto& station : trip->solvedStations()) {
            covered.insert(cwStation::canonicalKey(cavernPath + station.first));
        }
    }
    const QStringList rootKeys = root->scopedPositions().positions().keys();
    CHECK(covered == QSet<QString>(rootKeys.cbegin(), rootKeys.cend()));
}

TEST_CASE("A native trip under a derived node solves in place once a region equate ties it",
          "[LinePlotManager][Attach][SourcedFixtures][Equate]")
{
    auto project = makeSavedProject(QStringLiteral("compass-project-native"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), QStringLiteral("compass_project/PROJECT.MAK"));
    requireSolved(project.get());

    auto* alpha = qobject_cast<cwCave*>(childNodeNamed(root, QStringLiteral("Alpha")));
    REQUIRE(alpha != nullptr);
    cwTrip* dig = addNativeTripWithShot(alpha, QStringLiteral("Dig"),
                                        QStringLiteral("D1"), QStringLiteral("D2"));
    REQUIRE(dig->scopePrefix() == QStringLiteral("dig."));
    drainPipelines(project.get());

    // Nothing joins it to the rest of the project yet.
    cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    CHECK(linePlot->hasSolveError());

    // The root's block writes the trip after its *include, under its label.
    cwTrip* alpha1 = tripNamedIn(alpha, QStringLiteral("ALPHA1"));
    project->project->cavingRegion()->equates()->appendEquate(
        cwEquate({dig->stationHandle(QStringLiteral("D1")),
                  alpha1->stationHandle(QStringLiteral("A5"))}));
    drainPipelines(project.get());
    requireSolved(project.get());

    const QString driver = linePlot->driverSource();
    INFO("driver:\n" << driver.toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    const qsizetype includeAt = driver.indexOf(QStringLiteral("*include"));
    const qsizetype digAt = driver.indexOf(QStringLiteral("*begin dig ; Dig"));
    const qsizetype rootEndAt = driver.indexOf(QStringLiteral("*end ") + root->scopePrefix().chopped(1));
    CHECK(includeAt >= 0);
    CHECK(digAt > includeAt);
    CHECK(rootEndAt > digAt);
    CHECK(driver.contains(QStringLiteral("*end dig")));
    // The Compass name keeps its case, so the tie is read under *case preserve.
    CHECK(driver.contains(QStringLiteral("*case preserve\n*equate ") + root->scopePrefix()
                          + QStringLiteral("dig.d1 ") + root->scopePrefix() + QStringLiteral("alpha.A5")));

    // The trip's stations solve where the tie puts them, and Alpha's view
    // holds them under the trip's label.
    const QMap<QString, QVector3D> digStations = solvedByName(dig);
    REQUIRE(digStations.contains(QStringLiteral("d1")));
    REQUIRE(digStations.contains(QStringLiteral("d2")));
    CHECK(digStations.value(QStringLiteral("d1"))
          == root->scopedPositions().position(QStringLiteral("alpha.a5")));
    CHECK(alpha->scopedPositions().hasPosition(QStringLiteral("dig.d2")));

    // A like-named trip under Bravo takes the next label of the subtree's one
    // pool, and the root's own fix may name its station: both are written in
    // the root's block.
    auto* bravo = qobject_cast<cwCave*>(childNodeNamed(root, QStringLiteral("Bravo")));
    REQUIRE(bravo != nullptr);
    cwTrip* bravoDig = addNativeTripWithShot(bravo, QStringLiteral("Dig"),
                                             QStringLiteral("E1"), QStringLiteral("E2"));
    REQUIRE(bravoDig->scopePrefix() == QStringLiteral("dig_2."));
    cwFixStation fix;
    fix.setStationName(QStringLiteral("dig_2.e1"));
    fix.setInputCS(kCompassProjectCS);
    fix.setCoordinate(kCompassBravoDigEasting, kCompassNorthing, kCompassElevation);
    root->fixStations()->appendFixStation(fix);
    drainPipelines(project.get());
    requireSolved(project.get());

    const QString fixedDriver = linePlot->driverSource();
    INFO("driver with the fix:\n" << fixedDriver.toStdString());
    CHECK(fixedDriver.count(QStringLiteral("*begin dig ; Dig")) == 1);
    CHECK(fixedDriver.count(QStringLiteral("*begin dig_2 ; Dig")) == 1);
    CHECK(fixedDriver.contains(QStringLiteral("*fix dig_2.e1")));
    const QMap<QString, QVector3D> bravoDigStations = solvedByName(bravoDig);
    REQUIRE(bravoDigStations.contains(QStringLiteral("e1")));
    CHECK(bravoDigStations.contains(QStringLiteral("e2")));
    CHECK(solvedByName(dig).contains(QStringLiteral("d1")));
}

TEST_CASE("A Compass source root draws each member of a case pair with its own legs",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs]")
{
    const QString fixture = GENERATE(casePairs::kFileName, casePairs::kMakFileName);
    INFO("fixture: " << fixture.toStdString());
    // A .mak reads each .dat into a survey named after its file.
    const QString prefix = fixture == casePairs::kMakFileName ? QStringLiteral("compass_case_pairs.")
                                                              : QString();

    // linePlot must outlive the project: its line-plot manager holds a raw
    // pointer to it.
    cwRenderLinePlot linePlot;
    auto project = makeSavedProject(QStringLiteral("case-pairs-source-root"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    project->rootData->linePlotManager()->setRenderLinePlot(&linePlot);
    cwSurveyNode* root = attachSourceAndSolve(project.get(), fixture);
    requireSolved(project.get());

    const cwStationPositionLookup lookup = root->scopedPositions();
    CHECK(casePairs::storedKeys(lookup) == casePairs::joined(casePairs::keys(prefix)));
    casePairs::checkStations(lookup, root->scopedNetwork(), prefix);
    casePairs::checkDrawsShots(linePlot.points(), lookup, prefix);
}

TEST_CASE("A native p2 beside a cave-level Compass file's case pair stays a station of its own",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs][Equate]")
{
    auto project = makeSavedProject(QStringLiteral("case-pairs-mixed"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    const casePairs::MixedCave mixed = casePairs::addMixedCave(project.get());
    const cwCave* root = mixed.root;

    const QString nativeP2 = casePairs::nativeP2Key(mixed);
    const QString fileP2 = QStringLiteral("P2");
    const QString filePairedP2 = QStringLiteral("p2");
    const cwStationPositionLookup lookup = root->scopedPositions();
    const QMap<QString, QVector3D> positions = lookup.positions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    REQUIRE(positions.contains(nativeP2));
    CHECK(positions.contains(fileP2));
    CHECK(positions.contains(filePairedP2));

    // Cavern solved three stations at three points.
    CHECK(positions.value(fileP2).distanceToPoint(positions.value(filePairedP2))
          == Catch::Approx(casePairs::kSeparationMeters).margin(casePairs::kLegMarginMeters));
    CHECK(positions.value(nativeP2).distanceToPoint(positions.value(fileP2)) > casePairs::kLegMarginMeters);
    CHECK(positions.value(nativeP2).distanceToPoint(positions.value(filePairedP2))
          > casePairs::kLegMarginMeters);
    CHECK(casePairs::joined(root->scopedNetwork().neighbors(fileP2))
          == casePairs::joined({QStringLiteral("P1"), QStringLiteral("P3")}));

    // The file's members name the pair by its solved keys; the native trip's
    // stations are members through its scope.
    const cwSurveyNode::SourceMembers members = root->sourceMembers();
    INFO("members: " << casePairs::joined(QStringList(members.stations.cbegin(), members.stations.cend())));
    CHECK(members.stations.contains(fileP2));
    CHECK(members.stations.contains(filePairedP2));
    CHECK_FALSE(members.stations.contains(nativeP2));
    CHECK(members.contains(nativeP2));
}

namespace {

// Where the case-pair tests fix P2: anywhere in UTM 16N does, since the
// project's frame centers on the first fix it sees.
constexpr double kCasePairFixEasting = 500000.0;
constexpr double kCasePairFixNorthing = 4000000.0;
constexpr double kCasePairFixElevation = 100.0;

cwFixStation casePairFix(const QString& station, double northMeters = 0.0)
{
    cwFixStation fix;
    fix.setStationName(station);
    fix.setInputCS(kCompassProjectCS);
    fix.setCoordinate(kCasePairFixEasting, kCasePairFixNorthing + northMeters, kCasePairFixElevation);
    return fix;
}

//! Checks that \a node's \a key solved where PROJ carries \a fix into the
//! project's frame.
void checkAtNodeFix(SavedProjectFixture* project, const cwSurveyNode* node, const QString& key,
                    const cwFixStation& fix)
{
    const QString frame = project->project->cavingRegion()->geoReference()->localCoordinateSystem();
    const std::optional<cwGeoPoint> solved = cwCoordinateTransform::transformPoint(
        fix.inputCS(), frame, cwGeoPoint(fix.easting(), fix.northing(), fix.elevation()));
    REQUIRE(solved.has_value());
    INFO("stored keys: " << casePairs::storedKeys(node->scopedPositions()));
    checkPosition(node->scopedPositions(), key, solved->x, solved->y, solved->z,
                  kLocalFrameMarginMeters);
}

//! Checks that cavern tied every fix of \a project's solve to a station.
void checkFixesTied(SavedProjectFixture* project)
{
    const QString log = project->rootData->linePlotManager()->cavernLog();
    INFO("cavern log:\n" << log.toStdString());
    CHECK_FALSE(casePairs::reportsUnusedFixedPoint(log));
    CHECK_FALSE(log.contains(QStringLiteral("not all connected")));
}

using casePairs::fixReferenceWarnings;
using casePairs::stationErrorOfRow;

} // namespace

TEST_CASE("A fix beside a cave-level Compass file's case pair lands on the station it names",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs][FixStation]")
{
    auto project = makeSavedProject(QStringLiteral("case-pairs-mixed-fix"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    // The native trip is written under its own trip label, so a fix on its P2
    // names that label, in any case.
    const casePairs::MixedCave mixed = casePairs::addMixedCave(project.get());
    cwCave* root = mixed.root;

    const QString nativeScope = mixed.native->scopePrefix();
    REQUIRE(nativeScope == QStringLiteral("native."));
    struct Case {
        QString typed;
        QString key;
    };
    const Case fixCase = GENERATE_COPY(
        Case{QStringLiteral("P2"), QStringLiteral("P2")},
        Case{QStringLiteral("p2"), QStringLiteral("p2")},
        Case{nativeScope + QStringLiteral("P2"), casePairs::nativeP2Key(mixed)},
        Case{QStringLiteral("Native.P2"), casePairs::nativeP2Key(mixed)});
    INFO("the fix names " << fixCase.typed.toStdString() << ", expected on "
                          << fixCase.key.toStdString());

    const cwFixStation fix = casePairFix(fixCase.typed);
    root->fixStations()->appendFixStation(fix);
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());
    INFO("driver:\n" << project->rootData->linePlotManager()->driverSource().toStdString());

    checkFixesTied(project.get());
    CHECK(stationErrorOfRow(root, 0).isEmpty());
    checkAtNodeFix(project.get(), root, fixCase.key, fix);

    // The other two P2 stations stay off the fix.
    const cwStationPositionLookup lookup = root->scopedPositions();
    const QVector3D fixed = lookup.position(fixCase.key);
    for (const QString& other : {QStringLiteral("P2"), QStringLiteral("p2"), casePairs::nativeP2Key(mixed)}) {
        if (other != fixCase.key) {
            INFO("other station " << other.toStdString());
            REQUIRE(lookup.hasPosition(other));
            CHECK(lookup.position(other).distanceToPoint(fixed) > casePairs::kLegMarginMeters);
        }
    }
}

TEST_CASE("A Compass .mak fix on one member of a case pair blocks that member only",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs][FixStation]")
{
    // compass_case_pairs_fixed.mak fixes P2; cavern reads the .dat into a
    // survey named after the file.
    const QString fileP2 = QStringLiteral("compass_case_pairs.P2");
    const QString fileLowerP2 = QStringLiteral("compass_case_pairs.p2");

    auto project = makeSavedProject(QStringLiteral("case-pairs-mak-fix"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(),
                                              QStringLiteral("compass_case_pairs_fixed.mak"));
    REQUIRE(root != nullptr);
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());

    // The Fix Stations page lists the file's fix under the member's own name.
    QStringList listed;
    for (const cwSurveyNode* holder : nodeAndDescendants(root)) {
        for (const cwAttachedFix& attachedFix : holder->listedAttachedFixes()) {
            listed.append(attachedFix.station);
        }
    }
    CHECK(casePairs::joined(listed) == fileP2.toStdString());
    const QHash<QString, QString> fileFixed = root->fileFixedStations();
    CHECK(casePairs::joined(fileFixed.keys()) == fileP2.toStdString());

    SECTION("A node fix on the fixed member is flagged and dropped")
    {
        root->fixStations()->appendFixStation(casePairFix(fileP2));
        drainPipelines(project.get());
        requireSolved(project.get());

        const QString rowError = stationErrorOfRow(root, 0);
        INFO("row error: " << rowError.toStdString());
        CHECK(rowError.contains(QStringLiteral("already fixes %1").arg(fileP2)));
        const QStringList warnings = fixReferenceWarnings(root);
        INFO("warnings: " << warnings.join(QLatin1Char('\n')).toStdString());
        CHECK(std::any_of(warnings.cbegin(), warnings.cend(), [&](const QString& warning) {
            return warning.contains(QStringLiteral("already fixes %1").arg(fileP2));
        }));
    }

    SECTION("A node fix on the other member is kept and lands")
    {
        // p2 is 100 ft north of P2, so the two fixes agree with the shots.
        const cwFixStation fix = casePairFix(fileLowerP2, casePairs::kSeparationMeters);
        root->fixStations()->appendFixStation(fix);
        drainPipelines(project.get());
        requireSolved(project.get());
        INFO("driver:\n" << project->rootData->linePlotManager()->driverSource().toStdString());

        CHECK(stationErrorOfRow(root, 0).isEmpty());
        CHECK(fixReferenceWarnings(root).isEmpty());
        checkFixesTied(project.get());
        checkAtNodeFix(project.get(), root, fileLowerP2, fix);
    }
}

TEST_CASE("A tie to one member of a source root's case pair moves that member only",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs][Equate]")
{
    const QString fixture = GENERATE(casePairs::kFileName, casePairs::kMakFileName);
    // A .mak reads each .dat into a survey named after its file.
    const QString prefix = fixture == casePairs::kMakFileName ? QStringLiteral("compass_case_pairs.")
                                                              : QString();
    const QString tied = GENERATE(QStringLiteral("P2"), QStringLiteral("p2"));
    const QString other = tied == QStringLiteral("P2") ? QStringLiteral("p2") : QStringLiteral("P2");
    INFO("fixture " << fixture.toStdString() << ", the tie names " << tied.toStdString());

    auto project = makeSavedProject(QStringLiteral("case-pairs-tie"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    auto* root = qobject_cast<cwCave*>(attachSourceAndSolve(project.get(), fixture));
    REQUIRE(root != nullptr);
    requireSolved(project.get());
    cwTrip* fileTrip = casePairs::fileTripHolding(root, tied);

    // The native trip is written in the root's block under its own label, so
    // the tie is the only thing that places it.
    cwTrip* native = addNativeTripWithShot(root, QStringLiteral("Native"),
                                           QStringLiteral("N0"), QStringLiteral("N1"));
    project->project->cavingRegion()->equates()->appendEquate(
        cwEquate({native->stationHandle(QStringLiteral("N1")), fileTrip->stationHandle(tied)}));
    drainPipelines(project.get());
    requireSolved(project.get());
    INFO("driver:\n" << project->rootData->linePlotManager()->driverSource().toStdString());

    checkFixesTied(project.get());
    CHECK_FALSE(project->rootData->linePlotManager()->floatingSurveyModel()->isFloating(native->id()));

    const cwStationPositionLookup lookup = root->scopedPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    const QString nativeN1 = cwStation::canonicalKey(native->scopePrefix()) + QStringLiteral("n1");
    REQUIRE(lookup.hasPosition(nativeN1));
    REQUIRE(lookup.hasPosition(prefix + tied));
    CHECK(casePairs::samePoint(lookup.position(nativeN1), lookup.position(prefix + tied)));

    // The other member stays where its own shots put it, and each member keeps
    // its own legs.
    CHECK(lookup.position(prefix + other).distanceToPoint(lookup.position(nativeN1))
          == Catch::Approx(casePairs::kSeparationMeters).margin(casePairs::kLegMarginMeters));
    casePairs::checkStations(lookup, root->scopedNetwork(), prefix);
}

TEST_CASE("A source root's derived trip reads each case-pair member at its own position",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs]")
{
    // The line-plot labels draw solvedStations(), so P2's label sits where
    // this puts P2.
    auto project = makeSavedProject(QStringLiteral("case-pairs-solved-stations"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), casePairs::kFileName);
    REQUIRE(root != nullptr);
    requireSolved(project.get());

    const cwStationPositionLookup lookup = root->scopedPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    const QList<QPair<QString, QVector3D>> stations =
        casePairs::fileTripHolding(root, QStringLiteral("P2"))->solvedStations();
    QStringList names;
    for (const auto& station : stations) {
        INFO("station " << station.first.toStdString());
        names.append(station.first);
        REQUIRE(lookup.hasPosition(station.first));
        CHECK(casePairs::samePoint(station.second, lookup.position(station.first)));
    }
    // Each station once, under the file's spelling.
    CHECK(casePairs::joined(names) == casePairs::joined(casePairs::harvest()));
}

TEST_CASE("A native trip beside a cave-level Compass file's case pair knows its own P2",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs][ScopeStations]")
{
    auto project = makeSavedProject(QStringLiteral("case-pairs-mixed-known"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    const casePairs::MixedCave mixed = casePairs::addMixedCave(project.get());
    const cwStationPositionLookup lookup = mixed.root->scopedPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    const QString nativeP2 = casePairs::nativeP2Key(mixed);
    const QString nativeN1 = cwStation::canonicalKey(mixed.native->scopePrefix()) + QStringLiteral("n1");
    REQUIRE(lookup.hasPosition(nativeP2));
    REQUIRE(lookup.hasPosition(nativeN1));

    // The survey editor names the trip's own stations.
    QStringList known;
    for (const cwTrip::KnownStation& station : mixed.native->knownStations()) {
        INFO("known station " << station.name.toStdString());
        known.append(station.name);
        REQUIRE(station.position.has_value());
        const QString key = cwStation::canonicalKey(mixed.native->scopePrefix() + station.name);
        CHECK(casePairs::samePoint(*station.position, lookup.position(key)));
    }
    CHECK(casePairs::joined(known) == "N1, P2");

    QStringList solved;
    for (const auto& station : mixed.native->solvedStations()) {
        solved.append(station.first);
        if (station.first.compare(QStringLiteral("p2"), Qt::CaseInsensitive) == 0) {
            CHECK(casePairs::samePoint(station.second, lookup.position(nativeP2)));
        }
    }
    CHECK(casePairs::joined(solved) == "n1, p2");

    // The tie at Q1 puts the native N1 on the file's q1.
    CHECK(casePairs::samePoint(lookup.position(nativeN1), lookup.position(QStringLiteral("q1"))));

    // The fix-station autocomplete lists the native station under its trip's
    // label, as typed, beside the file's seven.
    cwScopeStationListModel model;
    model.setNode(mixed.root);
    REQUIRE(waitUntil([&model]() { return model.rowCount() > 0; }));
    QStringList rows;
    for (int row = 0; row < model.rowCount(); ++row) {
        rows.append(model.data(model.index(row, 0), cwScopeStationListModel::StationNameRole).toString());
    }
    CHECK(casePairs::joined(rows) == "P1, P2, P3, Q1, native.N1, native.P2, p1, p2, p3");
    const qsizetype nativeRow = rows.indexOf(QStringLiteral("native.P2"));
    REQUIRE(nativeRow >= 0);
    const QVariant rowPosition = model.data(model.index(int(nativeRow), 0), cwScopeStationListModel::PositionRole);
    REQUIRE(rowPosition.isValid());
    CHECK(casePairs::samePoint(rowPosition.value<QVector3D>(), lookup.position(nativeP2)));
}

namespace {

//! The trip under \a node, or under a node below it, named \a name.
cwTrip* tripNamedBelow(const cwSurveyNode* node, const QString& name)
{
    QStringList names;
    for (const cwSurveyNode* holder : nodeAndDescendants(node)) {
        for (cwTrip* trip : holder->trips()) {
            if (trip->name() == name) {
                return trip;
            }
            names.append(trip->name());
        }
    }
    FAIL("no trip named " << name.toStdString() << " among " << casePairs::joined(names));
    return nullptr;
}

//! Ties a new native trip N0 -> N1 under \a root to \a station of \a fileTrip
//! at N1, re-solves, and returns the native trip.
cwTrip* tieNativeTrip(SavedProjectFixture* project, cwSurveyNode* root, cwTrip* fileTrip,
                      const QString& station)
{
    auto* cave = qobject_cast<cwCave*>(root);
    REQUIRE(cave != nullptr);
    cwTrip* native = addNativeTripWithShot(cave, QStringLiteral("Native"),
                                           QStringLiteral("N0"), QStringLiteral("N1"));
    project->project->cavingRegion()->equates()->appendEquate(
        cwEquate({native->stationHandle(QStringLiteral("N1")), fileTrip->stationHandle(station)}));
    drainPipelines(project);
    return native;
}

//! Checks that cavern read every name of \a project's driver as a station
//! the files and trips hold.
void checkNoPhantomStation(SavedProjectFixture* project)
{
    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    CHECK_FALSE(linePlot->hasSolveError());
    CHECK_FALSE(linePlot->cavernLog().contains(QStringLiteral("referred to just once")));
    CHECK_FALSE(linePlot->cavernLog().contains(QStringLiteral("not all connected")));
}

} // namespace

TEST_CASE("A Survex *case preserve source root's derived trip keeps both members of a case pair",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs][Equate]")
{
    // survex_case_pair_block.svx: pairs.P1/pairs.p1 and pairs.P2/pairs.p2,
    // each pair 30 m apart.
    const QString tied = GENERATE(QStringLiteral("P2"), QStringLiteral("p2"));
    const QString other = tied == QStringLiteral("P2") ? QStringLiteral("p2") : QStringLiteral("P2");
    INFO("the tie names " << tied.toStdString());

    auto project = makeSavedProject(QStringLiteral("case-pairs-preserve-root"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), QStringLiteral("survex_case_pair_block.svx"));
    REQUIRE(root != nullptr);
    requireSolved(project.get());

    cwTrip* pairs = tripNamedBelow(root, QStringLiteral("pairs"));
    CHECK(casePairs::joined(pairs->externalStations()) == "P1, P2, p1, p2");

    const cwStationPositionLookup lookup = root->scopedPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    CHECK(casePairs::storedKeys(lookup) == "pairs.P1, pairs.P2, pairs.p1, pairs.p2");

    QStringList solved;
    for (const auto& station : pairs->solvedStations()) {
        INFO("solved station " << station.first.toStdString());
        solved.append(station.first);
        const QString key = QStringLiteral("pairs.") + station.first;
        REQUIRE(lookup.hasPosition(key));
        CHECK(casePairs::samePoint(station.second, lookup.position(key)));
    }
    CHECK(casePairs::joined(solved) == "P1, P2, p1, p2");

    const cwSurveyNode::SourceMembers members = root->sourceMembers();
    INFO("members: " << casePairs::joined(QStringList(members.stations.cbegin(), members.stations.cend())));
    for (const QString& member : {QStringLiteral("pairs.P1"), QStringLiteral("pairs.P2"),
                                  QStringLiteral("pairs.p1"), QStringLiteral("pairs.p2")}) {
        INFO("member " << member.toStdString());
        CHECK(members.stations.contains(member));
    }

    // A tie to one member lands on that member; the other stays 30 m away.
    cwTrip* native = tieNativeTrip(project.get(), root, pairs, tied);
    INFO("driver:\n" << project->rootData->linePlotManager()->driverSource().toStdString());
    checkNoPhantomStation(project.get());
    const cwStationPositionLookup tiedLookup = root->scopedPositions();
    INFO("stored keys after the tie: " << casePairs::storedKeys(tiedLookup));
    const QString nativeN1 = cwStation::canonicalKey(native->scopePrefix()) + QStringLiteral("n1");
    REQUIRE(tiedLookup.hasPosition(nativeN1));
    REQUIRE(tiedLookup.hasPosition(QStringLiteral("pairs.") + tied));
    REQUIRE(tiedLookup.hasPosition(QStringLiteral("pairs.") + other));
    CHECK(casePairs::samePoint(tiedLookup.position(nativeN1), tiedLookup.position(QStringLiteral("pairs.") + tied)));
    CHECK(tiedLookup.position(QStringLiteral("pairs.") + other).distanceToPoint(tiedLookup.position(nativeN1))
          == Catch::Approx(casePairs::kPairFixtureSeparationMeters).margin(casePairs::kLegMarginMeters));
}

TEST_CASE("A tie into a source root's capitalized Survex block lands on the station cavern labels",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs][Equate]")
{
    // survex_case_preserve_blocks.svx labels its stations Foo.P1 and so on;
    // survex_case_upper.svx labels them FOO.P1 and so on.
    struct Case {
        QString fixture;
        QString trip;
        QString typed;
    };
    const Case tieCase = GENERATE(
        Case{QStringLiteral("survex_case_preserve_blocks.svx"), QStringLiteral("Foo"), QStringLiteral("P2")},
        Case{QStringLiteral("survex_case_upper.svx"), QStringLiteral("foo"), QStringLiteral("p2")});
    INFO("fixture " << tieCase.fixture.toStdString() << ", the tie names " << tieCase.typed.toStdString());

    auto project = makeSavedProject(QStringLiteral("case-pairs-capitalized-tie"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), tieCase.fixture);
    REQUIRE(root != nullptr);
    requireSolved(project.get());

    cwTrip* fileTrip = tripNamedBelow(root, tieCase.trip);
    cwTrip* native = tieNativeTrip(project.get(), root, fileTrip, tieCase.typed);
    INFO("driver:\n" << project->rootData->linePlotManager()->driverSource().toStdString());
    checkNoPhantomStation(project.get());

    const cwStationPositionLookup lookup = root->scopedPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    // The file's three stations and the native two: no station the tie made up.
    CHECK(casePairs::storedKeys(lookup) == "foo.p1, foo.p2, foo.p3, native.n0, native.n1");
    const QString nativeN1 = cwStation::canonicalKey(native->scopePrefix()) + QStringLiteral("n1");
    REQUIRE(lookup.hasPosition(nativeN1));
    REQUIRE(lookup.hasPosition(QStringLiteral("foo.p2")));
    CHECK(casePairs::samePoint(lookup.position(nativeN1), lookup.position(QStringLiteral("foo.p2"))));
}

namespace {

// The leak file's one leg, 5 m north of its fixed s1.
constexpr double kLeakFileTapeMeters = 5.0;
// The native probe leg: 10.5 m on a bearing of 90 degrees, 30 degrees up, so a
// leaked tape, compass or clino setting moves its far end, and a leaked
// decimal separator misreads its tape.
constexpr double kProbeTapeMeters = 10.5;
constexpr double kProbeCompassDegrees = 90.0;
constexpr double kProbeClinoDegrees = 30.0;

//! Writes a Survex file into \a dir whose sub block fixes s1 and runs one leg
//! north from it, followed by \a setting at the file's top level.
QString writeLeakFile(const QTemporaryDir& dir, const QString& setting)
{
    const QString path = QDir(dir.path()).filePath(QStringLiteral("leak.svx"));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QString text = QStringLiteral("*begin sub\n*fix s1 0 0 0\ns1 s2 %1 0 0\n*end sub\n%2\n")
                             .arg(kLeakFileTapeMeters)
                             .arg(setting);
    REQUIRE(file.write(text.toUtf8()) > 0);
    file.close();
    return path;
}

//! Adds a meters trip with no calibration under \a cave: one leg \a from ->
//! \a to along the probe bearing and clino.
cwTrip* addProbeTrip(cwCave* cave, const QString& from, const QString& to)
{
    cwTrip* trip = addEmptyTrip(cave, QStringLiteral("Native"));
    auto* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(kProbeTapeMeters)));
    shot.setCompass(cwCompassReading(QString::number(kProbeCompassDegrees)));
    shot.setClino(cwClinoReading(QString::number(kProbeClinoDegrees)));
    chunk->appendShot(cwStation(from), cwStation(to), shot);
    return trip;
}

//! The keys \a lookup stores under \a scope.
QStringList keysUnder(const cwStationPositionLookup& lookup, const QString& scope)
{
    QStringList keys;
    for (const QString& key : lookup.positions().keys()) {
        if (key.startsWith(scope, Qt::CaseInsensitive)) {
            keys.append(key);
        }
    }
    return keys;
}

} // namespace

TEST_CASE("A Survex source root's *case setting stays out of its native trips",
          "[LinePlotManager][Attach][SourcedFixtures][CasePairs]")
{
    // Both files set *case at their top level. The *include sits in its own
    // block, so a native trip after it is read under cavern's default
    // *case tolower, and its A1 and a1 are one station.
    constexpr double kLeakLegMeters = 10.0;
    constexpr double kEastDegrees = 90.0;
    constexpr double kLevelClinoDegrees = 0.0;
    const QString fixture = GENERATE(QStringLiteral("survex_case_upper.svx"),
                                     QStringLiteral("survex_case_pair_splays.svx"));
    INFO("fixture " << fixture.toStdString());

    auto project = makeSavedProject(QStringLiteral("case-pairs-case-leak"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    auto* root = qobject_cast<cwCave*>(attachSourceAndSolve(project.get(), fixture));
    REQUIRE(root != nullptr);
    requireSolved(project.get());
    cwTrip* fileTrip = casePairs::fileTripHolding(root, QStringLiteral("P1"));

    cwTrip* native = addNativeTripWithShot(root, QStringLiteral("Native"),
                                           QStringLiteral("A1"), QStringLiteral("N1"));
    auto* second = new cwSurveyChunk();
    native->addChunk(second);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(kLeakLegMeters)));
    shot.setCompass(cwCompassReading(QString::number(kEastDegrees)));
    shot.setClino(cwClinoReading(QString::number(kLevelClinoDegrees)));
    second->appendShot(cwStation(QStringLiteral("a1")), cwStation(QStringLiteral("N2")), shot);
    project->project->cavingRegion()->equates()->appendEquate(
        cwEquate({native->stationHandle(QStringLiteral("N1")), fileTrip->stationHandle(QStringLiteral("P1"))}));
    drainPipelines(project.get());
    INFO("driver:\n" << project->rootData->linePlotManager()->driverSource().toStdString());
    checkNoPhantomStation(project.get());

    const QString scope = cwStation::canonicalKey(native->scopePrefix());
    CHECK(casePairs::joined(keysUnder(root->scopedPositions(), scope)) == casePairs::joined({scope + QStringLiteral("a1"),
                                                              scope + QStringLiteral("n1"),
                                                              scope + QStringLiteral("n2")}));
}


TEST_CASE("A Survex source root's top-level settings stay out of its native trips",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    const QString setting = GENERATE(QStringLiteral("*units tape feet"),
                                     QStringLiteral("*units tape 2 metres"),
                                     QStringLiteral("*units compass grads"),
                                     QStringLiteral("*units clino percent"),
                                     QStringLiteral("*units clino grads"),
                                     QStringLiteral("*calibrate tape 1"),
                                     QStringLiteral("*calibrate compass 10"),
                                     QStringLiteral("*calibrate clino 5"),
                                     QStringLiteral("*declination 10 degrees"),
                                     QStringLiteral("*flags surface"),
                                     QStringLiteral("*flags duplicate"),
                                     QStringLiteral("*flags splay"),
                                     QStringLiteral("*truncate 1"),
                                     QStringLiteral("*set decimal ,"));
    INFO("the file ends with " << setting.toStdString());

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    auto project = makeSavedProject(QStringLiteral("survex-settings-leak"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    auto* root = qobject_cast<cwCave*>(attachSourceThroughManager(
        project.get(), project->project->cavingRegion()->rootNode(), writeLeakFile(sourceDir, setting)));
    REQUIRE(root != nullptr);
    drainPipelines(project.get());
    cwTrip* fileTrip = tripNamedBelow(root, QStringLiteral("sub"));

    cwTrip* native = addProbeTrip(root, QStringLiteral("A1"), QStringLiteral("B"));
    project->project->cavingRegion()->equates()->appendEquate(
        cwEquate({native->stationHandle(QStringLiteral("A1")), fileTrip->stationHandle(QStringLiteral("s1"))}));
    drainPipelines(project.get());
    requireSolved(project.get());

    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    const QString driver = linePlot->driverSource();
    INFO("driver:\n" << driver.toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    // The *include's own block ends every setting, *case among them.
    CHECK_FALSE(driver.contains(QStringLiteral("*case tolower")));

    const QString scope = cwStation::canonicalKey(native->scopePrefix());
    const cwStationPositionLookup lookup = root->scopedPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    REQUIRE(casePairs::joined(keysUnder(lookup, scope))
            == casePairs::joined({scope + QStringLiteral("a1"), scope + QStringLiteral("b")}));

    // The probe leg lands where its own readings put it.
    const QVector3D leg = lookup.position(scope + QStringLiteral("b"))
                          - lookup.position(scope + QStringLiteral("a1"));
    const double clino = qDegreesToRadians(kProbeClinoDegrees);
    CHECK(leg.x() == Catch::Approx(kProbeTapeMeters * std::cos(clino)).margin(casePairs::kLegMarginMeters));
    CHECK(leg.y() == Catch::Approx(0.0).margin(casePairs::kLegMarginMeters));
    CHECK(leg.z() == Catch::Approx(kProbeTapeMeters * std::sin(clino)).margin(casePairs::kLegMarginMeters));
    CHECK(casePairs::samePoint(lookup.position(scope + QStringLiteral("a1")),
                               lookup.position(QStringLiteral("sub.s1"))));

    // Cavern leaves a surface, duplicate or splay leg out of its total, so an
    // underground, unflagged probe leg counts beside the file's.
    const QRegularExpression totalLength(
        QStringLiteral("Total length of survey shots =\\s*%1m")
            .arg(kLeakFileTapeMeters + kProbeTapeMeters, 0, 'f', 2));
    CHECK(linePlot->cavernLog().contains(totalLength));
}

TEST_CASE("A Survex source root that uses *export still takes ties and fixes beside another root",
          "[LinePlotManager][Attach][SourcedFixtures][SurvexExport]")
{
    // survex_exports.svx exports sub.s1 alone; survex_cross_level_equate.svx
    // ties x.c to y.c from its top level and exports nothing.
    auto project = makeSavedProject(QStringLiteral("survex-exports"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* regionRoot = project->project->cavingRegion()->rootNode();
    auto* exporting = qobject_cast<cwCave*>(attachSourceThroughManager(
        project.get(), regionRoot, fixturePath(QStringLiteral("survex_exports.svx"))));
    REQUIRE(exporting != nullptr);
    cwSurveyNode* crossLevel = attachSourceThroughManager(
        project.get(), regionRoot, fixturePath(QStringLiteral("survex_cross_level_equate.svx")));
    REQUIRE(crossLevel != nullptr);
    drainPipelines(project.get());

    cwTrip* sub = tripNamedBelow(exporting, QStringLiteral("sub"));
    cwTrip* native = addNativeTripWithShot(exporting, QStringLiteral("Native"),
                                           QStringLiteral("N0"), QStringLiteral("N1"));
    project->project->cavingRegion()->equates()->appendEquate(
        cwEquate({native->stationHandle(QStringLiteral("N1")), sub->stationHandle(QStringLiteral("s1"))}));
    const cwFixStation fix = casePairFix(QStringLiteral("sub.s3"));
    exporting->fixStations()->appendFixStation(fix);
    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());

    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    const QString driver = linePlot->driverSource();
    INFO("driver:\n" << driver.toStdString());
    checkFixesTied(project.get());

    // Once, ahead of every line that names a station.
    CHECK(driver.count(QStringLiteral("*infer exports on")) == 1);
    const qsizetype inferAt = driver.indexOf(QStringLiteral("*infer exports on"));
    REQUIRE(inferAt >= 0);
    for (const QString& keyword : {QStringLiteral("*fix"), QStringLiteral("*equate"),
                                   QStringLiteral("*include")}) {
        INFO("keyword " << keyword.toStdString());
        CHECK(inferAt < driver.indexOf(keyword));
    }

    const cwStationPositionLookup lookup = exporting->scopedPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    const QString nativeN1 = cwStation::canonicalKey(native->scopePrefix()) + QStringLiteral("n1");
    REQUIRE(lookup.hasPosition(nativeN1));
    REQUIRE(lookup.hasPosition(QStringLiteral("sub.s1")));
    CHECK(casePairs::samePoint(lookup.position(nativeN1), lookup.position(QStringLiteral("sub.s1"))));
    checkAtNodeFix(project.get(), exporting, QStringLiteral("sub.s3"), fix);

    const cwStationPositionLookup crossLookup = crossLevel->scopedPositions();
    INFO("cross-level keys: " << casePairs::storedKeys(crossLookup));
    CHECK(casePairs::storedKeys(crossLookup) == "x.b, x.c, y.c, y.d");
    CHECK(casePairs::samePoint(crossLookup.position(QStringLiteral("x.c")),
                               crossLookup.position(QStringLiteral("y.c"))));
}

TEST_CASE("A survey file a trip holds inside a source root loads with a warning that it is not solved",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    // The shape a project saved before such attachments were refused has: a
    // native trip under a .mak source root that holds survex_simple.svx. The
    // held file fixes A1, which the solve never reads, so the root's unfixed
    // stations must still solve on their own.
    const QString heldFile = QStringLiteral("survex_simple.svx");
    const QString prefix = QStringLiteral("compass_case_pairs.");
    auto project = makeSavedProject(QStringLiteral("unsolved-trip-file"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    auto* root = qobject_cast<cwCave*>(attachSourceAndSolve(project.get(), casePairs::kMakFileName));
    REQUIRE(root != nullptr);
    requireSolved(project.get());

    cwTrip* held = addEmptyTrip(root, QStringLiteral("Held"));
    drainPipelines(project.get());
    const QString attachDir = project->project->saveLoad()->externalCenterlineDir(held).absolutePath();
    REQUIRE(QDir().mkpath(attachDir));
    seedAttachment(attachDir, fixturePath(heldFile));
    held->setExternalCenterline(cwExternalCenterline(heldFile));
    const QUuid heldId = held->id();
    const QUuid rootId = root->id();

    auto reopened = reopenFromDisk(project.get());
    const cwLinePlotManager* linePlot = reopened->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    CHECK_FALSE(linePlot->hasSolveError());

    cwSurveyNode* reopenedRoot = nullptr;
    cwTrip* reopenedHeld = nullptr;
    const QList<cwSurveyNode*> nodes = reopened->region()->rootNode()->allNodes();
    for (cwSurveyNode* node : nodes) {
        if (node->id() == rootId) {
            reopenedRoot = node;
        }
        for (cwTrip* trip : node->trips()) {
            if (trip->id() == heldId) {
                reopenedHeld = trip;
            }
        }
    }
    REQUIRE(reopenedRoot != nullptr);
    REQUIRE(reopenedHeld != nullptr);
    REQUIRE(reopenedHeld->parentNode() == reopenedRoot);

    // The source root's own stations still solve.
    const cwStationPositionLookup lookup = reopenedRoot->scopedPositions();
    CHECK(casePairs::storedKeys(lookup) == casePairs::joined(casePairs::keys(prefix)));
    casePairs::checkStations(lookup, reopenedRoot->scopedNetwork(), prefix);

    // Nothing is removed: the trip still holds its file.
    CHECK(reopenedHeld->externalCenterline().entryFile() == heldFile);
    CHECK_FALSE(reopenedHeld->takesSurveyFile());

    const auto unsolvedRows = [](const cwTrip* trip) {
        QStringList messages;
        const QList<cwError> errors = trip->errorModel()->errors()->toList();
        for (const cwError& error : errors) {
            if (error.errorTypeId() == static_cast<int>(cwErrorTypeId::AttachedFileUnsolved)) {
                messages.append(error.message());
            }
        }
        return messages;
    };
    const QStringList rows = unsolvedRows(reopenedHeld);
    INFO("rows: " << casePairs::joined(rows));
    REQUIRE(rows.size() == 1);
    CHECK(rows.first().contains(heldFile));
    CHECK(rows.first() == reopenedHeld->unsolvedFileWarning());

    // The root's page lists it and opens the trip.
    cwNodeWarningModel model;
    model.setNode(reopenedRoot);
    const auto listsHeldTrip = [&model, reopenedHeld]() {
        for (int row = 0; row < model.count(); ++row) {
            const QModelIndex index = model.index(row);
            const auto target = static_cast<cwNodeWarningModel::Target>(
                model.data(index, cwNodeWarningModel::TargetRole).toInt());
            if (target == cwNodeWarningModel::Target::TripPage
                && model.data(index, cwNodeWarningModel::TripRole).value<cwTrip*>() == reopenedHeld
                && model.data(index, cwNodeWarningModel::MessageRole).toString()
                       == reopenedHeld->unsolvedFileWarning()) {
                return true;
            }
        }
        return false;
    };
    CHECK(waitUntil(listsHeldTrip));

    // Only a trip whose file is solved lists the fixes it holds, and a move
    // in or out of the source root relists them on both nodes.
    cwSurveyNode* plainCave = nullptr;
    for (cwSurveyNode* node : nodes) {
        if (node->name() == QStringLiteral("NativeCave")) {
            plainCave = node;
        }
    }
    REQUIRE(plainCave != nullptr);
    const auto listsHeldFix = [heldId](const cwSurveyNode* node) {
        const QList<cwAttachedFix> fixes = node->attachedFixes()->attachedFixes();
        return std::any_of(fixes.cbegin(), fixes.cend(), [heldId](const cwAttachedFix& fix) {
            return fix.ownerId == heldId;
        });
    };
    CHECK_FALSE(listsHeldFix(reopenedRoot));
    plainCave->insertTrip(plainCave->tripCount(), reopenedHeld);
    CHECK(waitUntil([&]() { return listsHeldFix(plainCave); }));
    CHECK(reopenedHeld->unsolvedFileWarning().isEmpty());
    reopenedRoot->insertTrip(reopenedRoot->tripCount(), reopenedHeld);
    CHECK(waitUntil([&]() { return !listsHeldFix(plainCave); }));
    CHECK_FALSE(listsHeldFix(reopenedRoot));
    CHECK(waitUntil(listsHeldTrip));

    // Taking the file away clears the warning.
    reopenedHeld->setExternalCenterline(cwExternalCenterline());
    CHECK(unsolvedRows(reopenedHeld).isEmpty());
    CHECK(waitUntil([&]() { return !listsHeldTrip(); }));
}

TEST_CASE("survex_mixed_case measures its length and depth as a source root",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    // survex_mixed_case.svx is 19 m of passage — 10 m and a 5 m tie in the
    // parent block, a 4 m drop in the nested one — and 4 m deep. Its blocks
    // are named with authored uppercase, so this holds the case matching to a
    // number rather than to "something was drawn".
    auto project = makeSavedProject(QStringLiteral("mixed-case-length"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), QStringLiteral("survex_mixed_case.svx"));
    requireSolved(project.get());

    CHECK(root->length()->value() == Catch::Approx(19.0).margin(kSolvedLengthMarginMeters));
    CHECK(root->depth()->value() == Catch::Approx(4.0).margin(kSolvedLengthMarginMeters));
}

TEST_CASE("Nested blocks count each leg once in a source root's length",
          "[LinePlotManager][Attach][SourcedFixtures]")
{
    // survex_blocks.svx nests three levels deep and totals 34 m. Each block is
    // a derived node, and a parent's fold takes in its children's legs once.
    auto project = makeSavedProject(QStringLiteral("nested-blocks-length"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceAndSolve(project.get(), QStringLiteral("survex_blocks.svx"));
    requireSolved(project.get());

    CHECK(root->length()->value() == Catch::Approx(34.0).margin(kSolvedLengthMarginMeters));
}

TEST_CASE("A Survex file whose *fix names its *cs gives a project with no frame one, centered on the fix",
          "[LinePlotManager][Attach][SourcedFixtures][AttachedFrame]")
{
    // own_cs_fix.svx: *cs UTM48N, then *fix S1 500200 3000000 100 and a 10 m
    // leg due north to S2.
    constexpr double kOwnCsFixElevation = 100.0;
    constexpr double kOwnCsLegMeters = 10.0;
    const QString fixedStation = QStringLiteral("scoped.S1");

    auto project = makeSavedProject(QStringLiteral("own-cs-fix-seeds-frame"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* cave = attachSourceAndSolve(project.get(),
                                              QStringLiteral("survex_native_cs_scope/own_cs_fix.svx"));
    requireSolved(project.get());
    REQUIRE(subtreeListsAttachedFix(cave, fixedStation));

    REQUIRE(waitForFrameAndSolve(project.get()));
    requireSolved(project.get());
    checkAnchoredOnAttachment(project.get(), cave, fixedStation);

    const cwStationPositionLookup lookup = cave->scopedPositions();
    checkPosition(lookup, fixedStation, 0.0, 0.0, kOwnCsFixElevation, kLocalFrameMarginMeters);
    CHECK(solvedDistance(lookup, fixedStation, QStringLiteral("scoped.S2"))
          == Catch::Approx(kOwnCsLegMeters).margin(kSolvedLengthMarginMeters));
}

TEST_CASE("A file whose fixes name their own system leaves a native fix's frame alone",
          "[LinePlotManager][Attach][SourcedFixtures][AttachedFrame]")
{
    // A native fix in WGS 84 UTM 16N, near every fixture's fixes.
    constexpr double kNativeFixEasting = 521000.0;
    constexpr double kNativeFixNorthing = 4040000.0;
    constexpr double kNativeFixElevation = 100.0;

    const QString fixture = GENERATE(QStringLiteral("walls_empty_datum/walls_empty_datum.wpj"),
                                     QStringLiteral("compass_project/PROJECT.MAK"),
                                     QStringLiteral("survex_two_cs/utm.svx"));
    INFO("fixture: " << fixture.toStdString());

    auto project = makeSavedProject(QStringLiteral("own-system-native-frame"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    addNativeTripWithShot(project->cave, QStringLiteral("Native survey"),
                          QStringLiteral("m1"), QStringLiteral("m2"));
    cwFixStation nativeFix;
    nativeFix.setStationName(QStringLiteral("m1"));
    nativeFix.setInputCS(kCompassProjectCS);
    nativeFix.setCoordinate(kNativeFixEasting, kNativeFixNorthing, kNativeFixElevation);
    project->cave->fixStations()->appendFixStation(nativeFix);
    const cwGeoReference* geoReference = project->project->cavingRegion()->geoReference();
    REQUIRE(waitUntil([&]() {
        return geoReference->anchor().kind == cwGeoReference::Anchor::FixStation;
    }));
    const QString before = geoReference->localCoordinateSystem();

    attachSourceAndSolve(project.get(), fixture);
    requireSolved(project.get());

    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::FixStation);
    CHECK(geoReference->localCoordinateSystem() == before);
}

namespace {

// survex_two_pieces' and walls_two_pieces_fix_one's tapes.
constexpr double kPieceTapeMeters = 10.0;
constexpr double kPiecesNativeEasting = 500000.0;

//! A trip A1-A2 in \a project's native cave, fixed on A1 in the Compass
//! fixtures' UTM zone when \a fixed.
cwTrip* addPiecesNativeTrip(SavedProjectFixture* project, bool fixed)
{
    cwTrip* trip = addNativeTripWithShot(project->cave, QStringLiteral("Native"),
                                         QStringLiteral("A1"), QStringLiteral("A2"));
    if (fixed) {
        cwFixStation fix;
        fix.setStationName(QStringLiteral("A1"));
        fix.setInputCS(kCompassProjectCS);
        fix.setCoordinate(kPiecesNativeEasting, kCompassNorthing, kCompassElevation);
        project->cave->fixStations()->appendFixStation(fix);
    }
    return trip;
}

cwTrip* tripNamedUnder(const cwSurveyNode* node, const QString& name)
{
    for (cwTrip* trip : node->allTrips()) {
        if (trip->name() == name) {
            return trip;
        }
    }
    return nullptr;
}

void tieAndSolve(SavedProjectFixture* project, const cwStationHandle& native,
                 const cwStationHandle& attached)
{
    project->project->cavingRegion()->equates()->appendEquate(cwEquate({native, attached}));
    drainPipelines(project);
}

//! Checks that cavern placed both ends of a leg, \a tapeMeters apart.
void checkLeg(const cwStationPositionLookup& lookup, const QString& from, const QString& to,
              double tapeMeters)
{
    INFO("leg " << from.toStdString() << " to " << to.toStdString());
    CHECK(lookup.hasPosition(from));
    CHECK(lookup.hasPosition(to));
    if (lookup.hasPosition(from) && lookup.hasPosition(to)) {
        CHECK(solvedDistance(lookup, from, to)
              == Catch::Approx(tapeMeters).margin(kSolvedLengthMarginMeters));
    }
}

//! Checks that \a station of \a lookup sits on the native cave's A2.
void checkOnNativeA2(SavedProjectFixture* project, const cwStationPositionLookup& lookup,
                     const QString& station)
{
    const cwStationPositionLookup nativePositions = project->cave->scopedPositions();
    REQUIRE(nativePositions.hasPosition(QStringLiteral("a2")));
    const QVector3D a2 = nativePositions.position(QStringLiteral("a2"));
    checkPosition(lookup, station, a2.x(), a2.y(), a2.z());
}

//! A Survex file of two surveys sharing no station, in the two shapes the
//! scan derives trips from: a block per survey, or one block for both.
struct TwoPiecesForm {
    QString fixture;
    //! The derived trips holding each survey.
    QString originTrip;
    QString otherTrip;
    //! The root block's prefix of each survey's stations.
    QString originScope;
    QString otherScope;
};

//! Catch2 keys a generator by its source line, so call this once per test case.
TwoPiecesForm twoPiecesForm()
{
    return GENERATE(
        TwoPiecesForm{QStringLiteral("survex_two_pieces.svx"), QStringLiteral("s"),
                      QStringLiteral("t"), QStringLiteral("s."), QStringLiteral("t.")},
        TwoPiecesForm{QStringLiteral("survex_two_pieces_one_block.svx"),
                      QStringLiteral("survex_two_pieces_one_block"),
                      QStringLiteral("survex_two_pieces_one_block"), QString(), QString()});
}

QString originFixLine(const TwoPiecesForm& form)
{
    return QStringLiteral("*fix %1s1 0 0 0").arg(form.originScope);
}

} // namespace

TEST_CASE("A tie into an attached file's second survey keeps the first survey on its origin fix",
          "[LinePlotManager][Attach][Equate][Anchor][Pieces]")
{
    const TwoPiecesForm form = twoPiecesForm();
    INFO("fixture " << form.fixture.toStdString());
    auto project = makeSavedProject(QStringLiteral("pieces-second"),
                                    QStringLiteral("Fixed"), QStringLiteral("Empty"));
    cwTrip* native = addPiecesNativeTrip(project.get(), true);
    cwSurveyNode* root = attachSourceAndSolve(project.get(), form.fixture);
    cwTrip* other = tripNamedIn(root, form.otherTrip);
    REQUIRE(other != nullptr);

    tieAndSolve(project.get(), native->stationHandle(QStringLiteral("A2")),
                other->stationHandle(QStringLiteral("t1")));
    requireSolved(project.get());

    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    CHECK(linePlot->driverSource().contains(originFixLine(form)));

    const cwStationPositionLookup positions = root->scopedPositions();
    checkLeg(positions, form.originScope + QStringLiteral("s1"),
             form.originScope + QStringLiteral("s2"), kPieceTapeMeters);
    checkLeg(positions, form.otherScope + QStringLiteral("t1"),
             form.otherScope + QStringLiteral("t2"), kPieceTapeMeters);
    checkOnNativeA2(project.get(), positions, form.otherScope + QStringLiteral("t1"));
}

TEST_CASE("A tie into the survey holding an attached file's origin station drops the origin fix",
          "[LinePlotManager][Attach][Equate][Anchor][Pieces]")
{
    const TwoPiecesForm form = twoPiecesForm();
    INFO("fixture " << form.fixture.toStdString());
    auto project = makeSavedProject(QStringLiteral("pieces-origin"),
                                    QStringLiteral("Fixed"), QStringLiteral("Empty"));
    cwTrip* native = addPiecesNativeTrip(project.get(), true);
    cwSurveyNode* root = attachSourceAndSolve(project.get(), form.fixture);
    cwTrip* origin = tripNamedIn(root, form.originTrip);
    REQUIRE(origin != nullptr);

    tieAndSolve(project.get(), native->stationHandle(QStringLiteral("A2")),
                origin->stationHandle(QStringLiteral("s2")));
    requireSolved(project.get());

    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    CHECK_FALSE(linePlot->driverSource().contains(originFixLine(form)));

    const cwStationPositionLookup positions = root->scopedPositions();
    checkLeg(positions, form.originScope + QStringLiteral("s1"),
             form.originScope + QStringLiteral("s2"), kPieceTapeMeters);
    checkOnNativeA2(project.get(), positions, form.originScope + QStringLiteral("s2"));
}

TEST_CASE("An untied attached file of two surveys keeps its origin fix",
          "[LinePlotManager][Attach][Equate][Anchor][Pieces]")
{
    const TwoPiecesForm form = twoPiecesForm();
    INFO("fixture " << form.fixture.toStdString());
    auto project = makeSavedProject(QStringLiteral("pieces-untied"),
                                    QStringLiteral("Fixed"), QStringLiteral("Empty"));
    addPiecesNativeTrip(project.get(), true);
    cwSurveyNode* root = attachSourceAndSolve(project.get(), form.fixture);
    requireSolved(project.get());

    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    CHECK(linePlot->driverSource().contains(originFixLine(form)));
    checkLeg(root->scopedPositions(), form.originScope + QStringLiteral("s1"),
             form.originScope + QStringLiteral("s2"), kPieceTapeMeters);
}

TEST_CASE("An unfixed cave tied into an attached file's unfixed survey keeps its own fallback",
          "[LinePlotManager][Attach][Equate][Anchor][Pieces]")
{
    struct FixOneCase {
        QString fixture;
        //! The derived trip holding the survey without a fix, and its station the tie names.
        QString tiedTrip;
        QString tiedTail;
        //! That survey's leg, named under the root's block.
        QString tiedKey;
        QString otherKey;
        double tapeMeters;
        //! A leg of the survey the file fixes.
        QString fixedKey;
        QString fixedOtherKey;
    };
    const FixOneCase fixOne = GENERATE(
        FixOneCase{QStringLiteral("survex_two_pieces_fix_one.svx"), QStringLiteral("s"),
                   QStringLiteral("s1"), QStringLiteral("s.s1"), QStringLiteral("s.s2"),
                   kPieceTapeMeters, QStringLiteral("t.t1"), QStringLiteral("t.t2")},
        FixOneCase{QStringLiteral("compass_two_pieces_fix_one/two_pieces.mak"), QStringLiteral("U"),
                   QStringLiteral("U1"), QStringLiteral("b.u1"), QStringLiteral("b.u2"),
                   kCompassTapeFeet * kFeetToMeters, QStringLiteral("a.s1"),
                   QStringLiteral("a.s2")},
        FixOneCase{QStringLiteral("walls_two_pieces_fix_one.srv"),
                   QStringLiteral("walls_two_pieces_fix_one"), QStringLiteral("T1"),
                   QStringLiteral("t1"), QStringLiteral("t2"), kPieceTapeMeters,
                   QStringLiteral("s1"), QStringLiteral("s2")});
    INFO("fixture " << fixOne.fixture.toStdString());
    auto project = makeSavedProject(QStringLiteral("pieces-fix-one"),
                                    QStringLiteral("Unfixed"), QStringLiteral("Empty"));
    cwTrip* native = addPiecesNativeTrip(project.get(), false);
    cwSurveyNode* root = attachSourceAndSolve(project.get(), fixOne.fixture);
    cwTrip* tied = tripNamedUnder(root, fixOne.tiedTrip);
    REQUIRE(tied != nullptr);

    tieAndSolve(project.get(), native->stationHandle(QStringLiteral("A2")),
                tied->stationHandle(fixOne.tiedTail));
    requireSolved(project.get());

    const cwLinePlotManager* linePlot = project->rootData->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    checkLeg(project->cave->scopedPositions(), QStringLiteral("a1"), QStringLiteral("a2"),
             kPieceTapeMeters);
    const cwStationPositionLookup positions = root->scopedPositions();
    checkLeg(positions, fixOne.tiedKey, fixOne.otherKey, fixOne.tapeMeters);
    checkLeg(positions, fixOne.fixedKey, fixOne.fixedOtherKey, fixOne.tapeMeters);
    checkOnNativeA2(project.get(), positions, fixOne.tiedKey);
}
