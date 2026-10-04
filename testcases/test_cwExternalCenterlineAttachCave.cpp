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

// Our
#include "cwAttachedCenterlinesModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwGeoReference.h"
#include "cwLinePlotGeometry.h"
#include "cwLinePlotManager.h"
#include "cwNodeWarningModel.h"
#include "cwNoteLiDAR.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSignalSpy.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>
#include <QVector3D>

// Std
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

namespace {

using cwExternalCenterlineAttach::AttachReport;

// The fixture's block tree, by dotted path. sump has no stations of its
// own, so nothing windows it.
const QString kDoghill = QStringLiteral("doghill");
const QString kBigPassage = QStringLiteral("doghill.big-passage");
const QString kEast = QStringLiteral("doghill.big-passage.east");

std::unique_ptr<SavedProjectFixture> makeProjectWithFreshCave(const QString& projectFileBase)
{
    auto fixture = makeSavedProject(projectFileBase,
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    // The cave the cave-level verbs run on: created the way the Add Cave
    // flow creates it, holding no trips at all.
    addEmptyCave(*fixture->project->cavingRegion(), QStringLiteral("BlocksCave"));
    fixture->project->waitSaveToFinish();
    QCoreApplication::processEvents();
    return fixture;
}

cwCave* freshCaveOf(SavedProjectFixture* fixture)
{
    cwCave* cave = fixture->project->cavingRegion()->cave(1);
    REQUIRE(cave->name() == QStringLiteral("BlocksCave"));
    return cave;
}

QString blocksFixture()
{
    return fixturePath(QStringLiteral("survex_blocks.svx"));
}

QStringList tripNames(const cwCave* cave)
{
    QStringList names;
    for (const cwTrip* trip : cave->trips()) {
        names.append(trip->name());
    }
    return names;
}

QStringList tripPrefixes(const cwCave* cave)
{
    QStringList prefixes;
    for (const cwTrip* trip : cave->trips()) {
        prefixes.append(trip->stationPrefix());
    }
    return prefixes;
}

AttachReport attachCaveThroughManager(SavedProjectFixture* fixture, cwCave* cave,
                                      const QString& sourcePath)
{
    auto future = managerOf(fixture)->attachCenterline(cave, sourcePath);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    return future.result().value();
}

// Wide margin so a planted edit's mtime is unambiguous whatever the
// filesystem's granularity, which can be a full second.
constexpr int kEditIsOlderSeconds = 3600;

// Squared distance under which two solved positions count as the same
// point, absorbing the float rounding a solve and a geometry pass add.
constexpr float kSamePositionToleranceSquared = 1e-6f;

// A one-block source the cave verbs can attach and re-copy. Only the
// tape length varies across an edit, so two equally long lengths keep
// the byte count and only an unconditional overwrite lands the edit.
QByteArray doghillSource(const QByteArray& tapeLength)
{
    const QByteArray header(
        "*begin doghill\n"
        "*fix d1 0 0 0\n"
        "*data normal from to tape compass clino\n");
    return header + "d1 d2 " + tapeLength + " 0 0\n" + "*end doghill\n";
}

//! Writes `content` into `dir` and hands back the absolute path.
QString writeSurvey(const QTemporaryDir& dir, const QString& name, const QByteArray& content)
{
    const QString path = QDir(dir.path()).absoluteFilePath(name);
    overwriteFile(path, content);
    return path;
}

QString ownerKindOf(const cwAttachedCenterlinesModel* model, const QString& ownerName)
{
    for (int i = 0; i < model->rowCount(); ++i) {
        if (roleAt(model, i, cwAttachedCenterlinesModel::OwnerNameRole).toString()
            == ownerName) {
            return roleAt(model, i, cwAttachedCenterlinesModel::OwnerKindRole).toString();
        }
    }
    FAIL("The attached-centerlines model holds no row named " << ownerName.toStdString());
    return QString();
}

//! The cave's whole-cave window: the one chunk-less trip that carries no
//! prefix, which owns every station no block window claims (P3.16).
cwTrip* wholeCaveTripOf(const cwCave* cave)
{
    for (cwTrip* trip : cave->trips()) {
        if (trip->windowsWholeCave()) {
            return trip;
        }
    }
    return nullptr;
}

//! Attaches \a fixture at cave level and waits for the solve it chains, so the
//! cave's solved lookup and the geometry pass below have something to read.
void attachAndSolve(SavedProjectFixture* fixture, cwCave* cave, const QString& source)
{
    attachCaveThroughManager(fixture, cave, source);
    drainPipelines(fixture);
    REQUIRE(tryWait(kAttachWaitMs, [cave]() {
        return !cave->stationPositionLookup().positions().isEmpty();
    }));
}

//! The geometry the line plot would draw for \a cave, generated from the same
//! region snapshot and solved network the renderer's worker pass reads.
cwLinePlotGeometry::Result geometryOf(SavedProjectFixture* fixture)
{
    const auto result =
        cwLinePlotGeometry::generate(fixture->project->cavingRegion()->data(),
                                     fixture->rootData->linePlotManager()->regionNetwork());
    REQUIRE_FALSE(result.hasError());
    return result.value();
}

// Solved positions are floats, so a length assembled from them drifts by
// fractions of a centimeter off the tape totals cavern reports.
constexpr double kSolvedLengthMarginMeters = 0.05;

// survex_blocks.svx cut back to its doghill block: big-passage and east,
// the two windows below it, leave the source together.
const QByteArray kDoghillOnlySource(
    "*begin doghill\n"
    "*fix d1 0 0 0\n"
    "*data normal from to tape compass clino\n"
    "d1 d2 10.0 0 0\n"
    "*end doghill\n");

void addLiDARScan(cwTrip* trip)
{
    trip->notesLiDAR()->addNotes({new cwNoteLiDAR()});
}

//! Replaces a fresh cave's attachment with doghill alone and checks that
//! east, which holds what \a addNativeContent put on it, outlives its block
//! while big-passage, which holds nothing, goes with its own. \a checkKept
//! then inspects east while the project that owns it is still alive.
void checkReplaceKeepsEast(const QString& projectFileBase,
                           const std::function<void(cwTrip*)>& addNativeContent,
                           const std::function<void(cwTrip*)>& checkKept)
{
    auto fixture = makeProjectWithFreshCave(projectFileBase);
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

    cwTrip* east = tripForPrefix(cave, kEast);
    REQUIRE(east != nullptr);
    REQUIRE(east->chunkCount() == 0);
    const QUuid eastId = east->id();
    addNativeContent(east);
    REQUIRE(tripForPrefix(cave, kBigPassage) != nullptr);

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString source =
        writeSurvey(sourceDir, QStringLiteral("doghill-only.svx"), kDoghillOnlySource);

    auto future = managerOf(fixture.get())->replaceCenterline(cave, source);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    cwTrip* kept = tripForPrefix(cave, kEast);
    REQUIRE(kept != nullptr);
    CHECK(kept->id() == eastId);
    CHECK(tripForPrefix(cave, kBigPassage) == nullptr);
    CHECK(tripForPrefix(cave, kDoghill) != nullptr);
    CHECK(cave->tripCount() == 2);
    checkKept(kept);
}

} // namespace

TEST_CASE("cave attach creates one Scope trip per station-bearing block",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-attach-blocks"));
    cwCave* cave = freshCaveOf(fixture.get());

    const AttachReport report =
        attachCaveThroughManager(fixture.get(), cave, blocksFixture());

    CHECK(cave->externalCenterline().entryFile() == QStringLiteral("survex_blocks.svx"));

    // File order, parents before children; the 0-station sump block made
    // no trip.
    CHECK(tripNames(cave) == QStringList({QStringLiteral("doghill"),
                                          QStringLiteral("big-passage"),
                                          QStringLiteral("east")}));
    CHECK(tripPrefixes(cave) == QStringList({kDoghill, kBigPassage, kEast}));
    CHECK(tripForPrefix(cave, QStringLiteral("doghill.big-passage.sump")) == nullptr);

    REQUIRE(report.createdScopeTrips.size() == 3);
    CHECK(report.createdScopeTrips.at(0).name == QStringLiteral("doghill"));
    CHECK(report.createdScopeTrips.at(0).stationPrefix == kDoghill);
    CHECK(report.createdScopeTrips.at(2).stationPrefix == kEast);

    // The attach remembered where the file came from, and the copy landed.
    CHECK(fixture->settings()->breadcrumbPath(cave->id())
          == QFileInfo(blocksFixture()).absoluteFilePath());
    const QString attachmentDir =
        fixture->saveLoad()->externalCenterlineDir(cave).absolutePath();
    CHECK(QFileInfo::exists(
        QDir(attachmentDir).absoluteFilePath(QStringLiteral("survex_blocks.svx"))));

    drainPipelines(fixture.get());
    CHECK(ownerKindOf(managerOf(fixture.get())->attachedCenterlinesModel(),
                      QStringLiteral("BlocksCave"))
          == QStringLiteral("Cave"));
}

TEST_CASE("cave attach dedupes blocks that share a leaf name", "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-attach-dedupe"));
    cwCave* cave = freshCaveOf(fixture.get());

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString source = writeSurvey(sourceDir, QStringLiteral("twins.svx"),
        "*begin a\n"
        "*fix a1 0 0 0\n"
        "*data normal from to tape compass clino\n"
        "a1 east.e1 5.0 0 0\n"
        "*begin east\n"
        "e1 e2 5.0 90 0\n"
        "*end east\n"
        "*end a\n"
        "*begin b\n"
        "*fix b1 100 0 0\n"
        "*data normal from to tape compass clino\n"
        "b1 east.e1 5.0 0 0\n"
        "*begin east\n"
        "e1 e2 5.0 90 0\n"
        "*end east\n"
        "*end b\n");

    attachCaveThroughManager(fixture.get(), cave, source);

    REQUIRE(tripForPrefix(cave, QStringLiteral("a.east")) != nullptr);
    REQUIRE(tripForPrefix(cave, QStringLiteral("b.east")) != nullptr);
    const QString firstName = tripForPrefix(cave, QStringLiteral("a.east"))->name();
    const QString secondName = tripForPrefix(cave, QStringLiteral("b.east"))->name();
    CHECK(firstName == QStringLiteral("east"));
    CHECK(secondName != firstName);
}

TEST_CASE("uniqueCaveName sanitizes and dedupes against existing cave names",
          "[Attach][Cave]")
{
    cwCavingRegion region;
    addEmptyCave(region, QStringLiteral("dusk"));

    //Naming a cave after a file has to survive whatever the filename holds:
    //cwCave::setName silently rejects both an unsanitized name and a collision.
    CHECK(region.uniqueCaveName(QStringLiteral("upper:cave"))
          == QStringLiteral("upper_cave"));
    CHECK(region.uniqueCaveName(QStringLiteral("dusk")) == QStringLiteral("dusk 2"));
    CHECK(region.uniqueCaveName(QStringLiteral("DUSK")) == QStringLiteral("DUSK 2"));
    CHECK(region.uniqueCaveName(QStringLiteral("dawn")) == QStringLiteral("dawn"));
}

TEST_CASE("cave attach refuses a cave that already has trips", "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-attach-not-fresh"));
    cwCave* cave = freshCaveOf(fixture.get());
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("A1"), QStringLiteral("A2"));

    auto future = managerOf(fixture.get())->attachCenterline(cave, blocksFixture());
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE(future.result().hasError());
    CHECK(future.result().errorMessage().contains(QStringLiteral("cave already has trips")));

    CHECK(cave->externalCenterline().isEmpty());
    CHECK(cave->tripCount() == 1);
    CHECK_FALSE(fixture->settings()->hasBreadcrumb(cave->id()));
}

TEST_CASE("cave attach refuses a source inside the project's data folder",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-attach-in-project"));
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(cave)
        .absoluteFilePath(QStringLiteral("survex_blocks.svx"));
    REQUIRE(QFileInfo::exists(copyPath));

    auto future = managerOf(fixture.get())->replaceCenterline(cave, copyPath);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE(future.result().hasError());
    const QString message = future.result().errorMessage();
    INFO("refusal message: " << message.toStdString());
    CHECK(message.contains(QFileInfo(copyPath).absoluteFilePath()));
    CHECK(message.contains(QStringLiteral("Pick the original")));

    // The attachment the cave already had is exactly as it was.
    CHECK(cave->externalCenterline().entryFile() == QStringLiteral("survex_blocks.svx"));
    CHECK(cave->tripCount() == 3);
    drainPipelines(fixture.get());
}

TEST_CASE("cave detach removes the Scope trips, the copies, and the breadcrumb",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-detach"));
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

    const QString attachmentDir =
        fixture->saveLoad()->externalCenterlineDir(cave).absolutePath();
    REQUIRE(QDir(attachmentDir).exists());

    int survivingTripCount = 0;
    QPointer<cwTrip> expectedSurvivor;
    std::function<bool(const cwTrip*)> survivorKeepsContent = [](const cwTrip*) { return true; };

    SECTION("every Scope trip is chunk-less") {
        survivingTripCount = 0;
    }

    SECTION("a Scope trip the user put chunks in survives") {
        cwTrip* east = tripForPrefix(cave, kEast);
        REQUIRE(east != nullptr);
        east->addChunk(new cwSurveyChunk());
        survivingTripCount = 1;
    }

    SECTION("a Scope trip holding a note survives") {
        cwTrip* east = tripForPrefix(cave, kEast);
        REQUIRE(east != nullptr);
        addNoteWithScrap(east, QStringLiteral("e1"));
        survivingTripCount = 1;
        expectedSurvivor = east;
        survivorKeepsContent = [](const cwTrip* trip) { return trip->notes()->hasNotes(); };
    }

    SECTION("a Scope trip holding a LiDAR scan survives") {
        cwTrip* east = tripForPrefix(cave, kEast);
        REQUIRE(east != nullptr);
        addLiDARScan(east);
        survivingTripCount = 1;
        expectedSurvivor = east;
        survivorKeepsContent = [](const cwTrip* trip) { return trip->notesLiDAR()->hasNotes(); };
    }

    auto future = managerOf(fixture.get())->detachCenterline(cave);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK_FALSE(future.result().hasError());

    CHECK(cave->tripCount() == survivingTripCount);
    if (expectedSurvivor) {
        CHECK(cave->trips().contains(expectedSurvivor.data()));
        CHECK(survivorKeepsContent(expectedSurvivor));
    }
    CHECK(cave->externalCenterline().isEmpty());
    CHECK(fixture->settings()->breadcrumbPath(cave->id()).isEmpty());
    CHECK_FALSE(QDir(attachmentDir).exists());

    // The cave itself survives, named, as an ordinary Native cave.
    CHECK(fixture->project->cavingRegion()->caves().contains(cave));
    CHECK(cave->name() == QStringLiteral("BlocksCave"));

    drainPipelines(fixture.get());
}

TEST_CASE("cave attach seeds Scope-trip dates from their blocks", "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-block-dates"));
    cwCave* cave = freshCaveOf(fixture.get());
    const QDate dayOfAttach = QDate::currentDate();
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

    // big-passage writes "*date 2024.01.05-2024.01.07": a range seeds its
    // start day.
    cwTrip* bigPassage = tripForPrefix(cave, kBigPassage);
    REQUIRE(bigPassage != nullptr);
    CHECK(bigPassage->date() == QDateTime(QDate(2024, 1, 5), QTime()));

    // east writes no date of its own, so it takes its nearest dated
    // ancestor's - Survex dates scope downward.
    cwTrip* east = tripForPrefix(cave, kEast);
    REQUIRE(east != nullptr);
    CHECK(east->date() == QDateTime(QDate(2024, 1, 5), QTime()));

    // Nothing above doghill writes a date, so its trip keeps cwTrip's
    // default of the day it was constructed. A run that crosses midnight
    // between the attach and this check sees either side of the boundary.
    cwTrip* doghill = tripForPrefix(cave, kDoghill);
    REQUIRE(doghill != nullptr);
    CHECK((doghill->date() == QDateTime(dayOfAttach, QTime())
           || doghill->date() == QDateTime(QDate::currentDate(), QTime())));
}

TEST_CASE("cave replace keeps matching Scope trips, adds new ones, and drops empty orphans",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-replace"));
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

    // A date the user set is theirs: seeding runs only when a trip is
    // created, so a replace has to leave this alone.
    const QDateTime handSetDate(QDate(1999, 9, 9), QTime());
    tripForPrefix(cave, kDoghill)->setDate(handSetDate);

    const QUuid doghillId = tripForPrefix(cave, kDoghill)->id();
    const QUuid bigPassageId = tripForPrefix(cave, kBigPassage)->id();

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    // Same tree with east replaced by west.
    const QString source = writeSurvey(sourceDir, QStringLiteral("blocks-west.svx"),
        "*begin doghill\n"
        "*fix d1 0 0 0\n"
        "*data normal from to tape compass clino\n"
        "d1 d2 10.0 0 0\n"
        "d2 big-passage.p1 2.0 0 0\n"
        "*begin big-passage\n"
        "p1 p2 5.0 90 0\n"
        "p2 west.w1 1.0 0 0\n"
        "*begin west\n"
        "w1 w2 3.0 270 0\n"
        "*end west\n"
        "*end big-passage\n"
        "*end doghill\n");

    auto future = managerOf(fixture.get())->replaceCenterline(cave, source);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    const AttachReport report = future.result().value();

    // Kept: the blocks that survived still window through their own trips.
    REQUIRE(tripForPrefix(cave, kDoghill) != nullptr);
    REQUIRE(tripForPrefix(cave, kBigPassage) != nullptr);
    CHECK(tripForPrefix(cave, kDoghill)->id() == doghillId);
    CHECK(tripForPrefix(cave, kDoghill)->date() == handSetDate);
    CHECK(tripForPrefix(cave, kBigPassage)->id() == bigPassageId);

    // New: the block the replacement added got a trip of its own.
    cwTrip* west = tripForPrefix(cave, QStringLiteral("doghill.big-passage.west"));
    REQUIRE(west != nullptr);
    REQUIRE(report.createdScopeTrips.size() == 1);
    CHECK(report.createdScopeTrips.at(0).stationPrefix
          == QStringLiteral("doghill.big-passage.west"));
    CHECK(report.createdScopeTrips.at(0).name == west->name());

    // Orphaned: the trip whose block is gone held no chunks, so the reconcile
    // removed it — a window is derived from the file (§3.4, amended by P3.16).
    CHECK(tripForPrefix(cave, kEast) == nullptr);
    CHECK(cave->tripCount() == 3);

    CHECK(cave->externalCenterline().entryFile() == QStringLiteral("blocks-west.svx"));
    drainPipelines(fixture.get());
}

TEST_CASE("cancelAttach before the scan lands leaves the cave untouched",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-attach-cancel"));
    cwCave* cave = freshCaveOf(fixture.get());
    const QUuid ownerId = cave->id();

    auto future = managerOf(fixture.get())->attachCenterline(cave, blocksFixture());
    // Same stack as the call, so the flag is provably set before the
    // attach's single consult point.
    managerOf(fixture.get())->cancelAttach(ownerId);
    CHECK(managerOf(fixture.get())->isOwnerBusy(ownerId));

    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK(future.isCanceled());

    settleEventLoop(kNothingHappensSettleMs);
    CHECK_FALSE(managerOf(fixture.get())->isOwnerBusy(ownerId));
    CHECK(cave->tripCount() == 0);
    CHECK(cave->externalCenterline().isEmpty());
    CHECK(fixture->settings()->breadcrumbPath(ownerId).isEmpty());
}

TEST_CASE("cave reload re-copies the remembered source and re-solves",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-reload"));
    cwCave* cave = freshCaveOf(fixture.get());
    auto manager = managerOf(fixture.get());

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QByteArray sourceBytes = doghillSource("10.0");
    const QByteArray editedBytes = doghillSource("11.0");
    const QString source =
        writeSurvey(sourceDir, QStringLiteral("doghill.svx"), sourceBytes);

    attachCaveThroughManager(fixture.get(), cave, source);
    drainPipelines(fixture.get());

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(cave)
        .absoluteFilePath(QStringLiteral("doghill.svx"));
    REQUIRE(fileContents(copyPath) == sourceBytes);

    // The source moves on after the copy was taken. Same byte count and an
    // mtime the copy already beats, so only an unconditional overwrite
    // lands the edit.
    REQUIRE(editedBytes.size() == sourceBytes.size());
    writeFileWithMtime(source, editedBytes,
                       QDateTime::currentDateTimeUtc().addSecs(-kEditIsOlderSeconds));

    CHECK(manager->canReloadFromSource(cave));

    cwSignalSpy solveSpy(manager, &cwExternalCenterlineManager::solveNeeded);
    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    auto reloadFuture = manager->reloadFromSource(cave);
    REQUIRE(AsyncFuture::waitForFinished(reloadFuture, kAttachWaitMs));
    REQUIRE_FALSE(reloadFuture.result().hasError());
    drainPipelines(fixture.get());

    CHECK(fileContents(copyPath) == editedBytes);
    CHECK(cave->externalCenterline().entryFile() == QStringLiteral("doghill.svx"));
    CHECK(solveSpy.count() > 0);

    // The Scope trip the attach created is reconciled rather than
    // duplicated: the block set did not change.
    CHECK(cave->tripCount() == 1);
    CHECK(tripForPrefix(cave, kDoghill) != nullptr);

    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK(report.success());
    CHECK(report.ownerId() == cave->id());
}

TEST_CASE("cave reload is refused when this machine has no source to copy",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-reload-refusals"));
    cwCave* cave = freshCaveOf(fixture.get());
    auto manager = managerOf(fixture.get());

    // A cave that was never attached has nothing to reload.
    CHECK_FALSE(manager->canReloadFromSource(cave));
    CHECK_FALSE(manager->canReloadFromSource(static_cast<cwCave*>(nullptr)));

    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    auto refused = manager->reloadFromSource(cave);
    REQUIRE(refused.isFinished());
    CHECK(refused.result().hasError());
    CHECK(refused.result().errorMessage().contains(QStringLiteral("no source file")));
    CHECK_FALSE(manager->isOwnerBusy(cave->id()));

    auto refusedNull = manager->reloadFromSource(static_cast<cwCave*>(nullptr));
    REQUIRE(refusedNull.isFinished());
    CHECK(refusedNull.result().hasError());
    CHECK(refusedNull.result().errorMessage()
              .contains(QStringLiteral("reload: cave is null")));

    // Both refusals report through the attach bridge, deferred.
    constexpr int kRefusalReportCount = 2;
    while (attachSpy.count() < kRefusalReportCount && attachSpy.wait(kAttachWaitMs)) {
    }
    REQUIRE(attachSpy.count() == kRefusalReportCount);

    const auto noSourceReport = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK_FALSE(noSourceReport.success());
    CHECK(noSourceReport.ownerId() == cave->id());
    CHECK(noSourceReport.errorMessage().contains(QStringLiteral("no source file")));

    const auto nullReport = attachSpy.at(1).at(0).value<cwExternalCenterlineReport>();
    CHECK_FALSE(nullReport.success());
    CHECK(nullReport.ownerId().isNull());
    CHECK(nullReport.errorMessage().contains(QStringLiteral("reload: cave is null")));

    CHECK(cave->externalCenterline().isEmpty());
    CHECK(cave->tripCount() == 0);

    drainPipelines(fixture.get());
}

TEST_CASE("Cave attach emits line geometry for Scope trips whose prefix carries authored case",
          "[Attach][Cave][Geometry]")
{
    // The whole cave-level chain against a survey file that names its blocks
    // with uppercase (48H-Feng): the Scope trips keep that authored spelling in
    // stationPrefix, while cavern lowercases every Survex label it writes to
    // the .3d (it preserves the case of Compass and Walls ones). The line-plot
    // geometry pass has to window across that difference, or the cave renders
    // station labels with no centerline between them.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-attach-mixed-case"));
    cwCave* cave = freshCaveOf(fixture.get());

    attachCaveThroughManager(fixture.get(), cave,
                             fixturePath(QStringLiteral("survex_mixed_case.svx")));
    drainPipelines(fixture.get());

    cwTrip* feng = tripForPrefix(cave, QStringLiteral("48H-Feng"));
    cwTrip* lower = tripForPrefix(cave, QStringLiteral("48H-Feng.Lower"));
    REQUIRE(feng != nullptr);
    REQUIRE(lower != nullptr);

    // The solve lands through the line-plot pipeline, so give it the attach
    // budget before reading the solved state back.
    REQUIRE(tryWait(kAttachWaitMs, [cave]() {
        return !cave->stationPositionLookup().positions().isEmpty();
    }));

    const auto result =
        cwLinePlotGeometry::generate(fixture->project->cavingRegion()->data(),
                                     fixture->rootData->linePlotManager()->regionNetwork());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();
    REQUIRE(geometry.tripUuids.size() == geometry.tripVertexRanges.size());

    const QList<QVector3D> solvedPositions =
        cave->stationPositionLookup().positions().values();
    const auto isSolvedPosition = [&solvedPositions](const QVector3D& point) {
        return std::any_of(solvedPositions.cbegin(), solvedPositions.cend(),
                           [&point](const QVector3D& solved) {
                               return (solved - point).lengthSquared()
                                      < kSamePositionToleranceSquared;
                           });
    };

    // Both Scope trips draw something, and every vertex they draw resolves
    // through the cave's solved lookup. Vertex totals are covered by the
    // nested-scope ownership and cave length/depth tests in
    // test_cwLinePlotGeometry.cpp and test_cwLinePlotManager_AttachedCenterlines.cpp.
    for (const cwTrip* trip : {feng, lower}) {
        INFO("scope trip: " << trip->stationPrefix().toStdString());
        const qsizetype tripIndex = geometry.tripUuids.indexOf(trip->id());
        REQUIRE(tripIndex >= 0);

        const cwLinePlotGeometry::VertexRange range = geometry.tripVertexRanges.at(tripIndex);
        CHECK(range.count > 0);
        CHECK(range.count % 2 == 0);
        for (int i = range.start; i < range.start + range.count; ++i) {
            CHECK(isSolvedPosition(geometry.points.at(i)));
        }
    }
}

// A cave-level attachment's breadcrumb is keyed on the cave's own id, so
// removing the cave has to forget it the way removing a trip forgets a
// trip-level one (the trip-level siblings live in
// test_cwExternalCenterlineAttach.cpp, tagged [Breadcrumb]). Removing the
// cave on the data page is the only way to drop a cave-level attachment.
TEST_CASE("deleting a cave forgets its own remembered source",
          "[Attach][Cave][Breadcrumb]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-delete-breadcrumb"));
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

    const QUuid caveId = cave->id();
    QList<QUuid> ownerIds({caveId});
    for (const cwTrip* trip : cave->trips()) {
        ownerIds.append(trip->id());
    }
    REQUIRE(ownerIds.size() == 4);

    // What the store actually holds going in: the cave's own breadcrumb, and
    // whatever the Scope trips picked up (they have none of their own unless a
    // source was set on them).
    QList<QUuid> ownersWithBreadcrumb;
    for (const QUuid& ownerId : ownerIds) {
        if (fixture->settings()->hasBreadcrumb(ownerId)) {
            ownersWithBreadcrumb.append(ownerId);
        }
    }
    REQUIRE(ownersWithBreadcrumb.contains(caveId));
    REQUIRE(fixture->settings()->breadcrumbPath(caveId)
            == QFileInfo(blocksFixture()).absoluteFilePath());
    REQUIRE_FALSE(fixture->settings()->fingerprint(caveId).isEmpty());

    //The verb the cave's delete UI calls (DataMainPage). The cave and its trips
    //can be destroyed on the spot, so nothing may touch them after this.
    const int caveIndex = fixture->project->cavingRegion()->indexOf(cave);
    REQUIRE(caveIndex >= 0);
    fixture->project->cavingRegion()->removeCave(caveIndex);
    QCoreApplication::processEvents();

    for (const QUuid& ownerId : ownerIds) {
        INFO("owner: " << ownerId.toString().toStdString());
        CHECK_FALSE(fixture->settings()->hasBreadcrumb(ownerId));
        CHECK(fixture->settings()->breadcrumbPath(ownerId).isEmpty());
        CHECK(fixture->settings()->fingerprint(ownerId).isEmpty());
    }

    drainPipelines(fixture.get());
}

TEST_CASE("closing the project keeps the cave's remembered source",
          "[Attach][Cave][Breadcrumb]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-close-breadcrumb"));
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

    const QUuid caveId = cave->id();
    const QString storedPath = fixture->settings()->breadcrumbPath(caveId);
    const auto storedFingerprint = fixture->settings()->fingerprint(caveId);
    REQUIRE_FALSE(storedPath.isEmpty());
    REQUIRE_FALSE(storedFingerprint.isEmpty());

    //Project close and load-replacing-the-region both run this. The closed
    //project still owns its source, so the breadcrumb must survive.
    fixture->project->cavingRegion()->clearCaves();
    QCoreApplication::processEvents();

    CHECK(fixture->settings()->hasBreadcrumb(caveId));
    CHECK(fixture->settings()->breadcrumbPath(caveId) == storedPath);
    CHECK(fixture->settings()->fingerprint(caveId) == storedFingerprint);

    drainPipelines(fixture.get());
}

TEST_CASE("A Compass cave draws through its one whole-cave window",
          "[Attach][Cave][Geometry]")
{
    // cavern throws a Compass "SURVEY NAME:" away and names each station under
    // the survey it makes for the .DAT file it came from, so the cave itself is
    // the only level the .mak spells. The cave gets exactly one window,
    // carrying no prefix, and it owns every station of both files - the two
    // A2s (one per file, tied by the link station the .mak lists) included:
    // before P3.16 the attach made one prefixed trip per survey, each listing
    // nothing, and the lineplot stayed blank.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-compass-window"));
    cwCave* cave = freshCaveOf(fixture.get());

    attachAndSolve(fixture.get(), cave, fixturePath(QStringLiteral("compass_multi.mak")));

    REQUIRE(cave->tripCount() == 1);
    cwTrip* window = wholeCaveTripOf(cave);
    REQUIRE(window != nullptr);
    CHECK(window->stationPrefix().isEmpty());
    CHECK(window->scopePrefix().isEmpty());
    CHECK(window->name() == QStringLiteral("compass_multi"));
    CHECK(window->knownStations().size() == 6);

    const cwLinePlotGeometry::Result geometry = geometryOf(fixture.get());
    // Four shots, each its own pair of vertices.
    CHECK(vertexCountOf(geometry, window) == 8);

    // 31 ft of tape: a Compass .dat spells distances in decimal feet.
    REQUIRE(geometry.nodeLengthAndDepths.contains(cave->id()));
    const cwLinePlotGeometry::LengthAndDepth measured = geometry.nodeLengthAndDepths.value(cave->id());
    CHECK(measured.length() == Catch::Approx(9.45).margin(kSolvedLengthMarginMeters));
    CHECK(measured.depth() == Catch::Approx(1.07).margin(kSolvedLengthMarginMeters));

    drainPipelines(fixture.get());
}

TEST_CASE("A Walls cave splits between its whole-cave window and its prefix windows",
          "[Attach][Cave][Geometry]")
{
    // walls_prefixed.wpj ties a prefix-less survey (A1..A3) to a "#PREFIX XY"
    // one (P1, P2) through the qualified station XY:P1. Only the prefix is a
    // naming level, so the root's three stations belong to the whole-cave
    // window and the XY block's two to its own.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-walls-window"));
    cwCave* cave = freshCaveOf(fixture.get());

    attachAndSolve(fixture.get(), cave,
                   fixturePath(QStringLiteral("walls_prefixed/walls_prefixed.wpj")));

    REQUIRE(cave->tripCount() == 2);
    cwTrip* window = wholeCaveTripOf(cave);
    cwTrip* xy = tripForPrefix(cave, QStringLiteral("XY"));
    REQUIRE(window != nullptr);
    REQUIRE(xy != nullptr);
    CHECK(window->name() == QStringLiteral("walls_prefixed"));
    CHECK(window->knownStations().size() == 3);
    CHECK(xy->knownStations().size() == 2);

    const cwLinePlotGeometry::Result geometry = geometryOf(fixture.get());
    // Four legs in all, eight vertices: the root's two, the XY survey's one,
    // and the tie between them, drawn once by whichever window reaches it
    // first — here XY, which the reconcile created before the whole-cave
    // window. The cave length below is what holds "once" to a number.
    CHECK(vertexCountOf(geometry, window) == 4);
    CHECK(vertexCountOf(geometry, xy) == 4);

    REQUIRE(geometry.nodeLengthAndDepths.contains(cave->id()));
    const cwLinePlotGeometry::LengthAndDepth measured = geometry.nodeLengthAndDepths.value(cave->id());
    CHECK(measured.length() == Catch::Approx(32.0).margin(kSolvedLengthMarginMeters));
    CHECK(measured.depth() == Catch::Approx(3.0).margin(kSolvedLengthMarginMeters));

    drainPipelines(fixture.get());
}

TEST_CASE("A Survex file with shots outside every block windows the root too",
          "[Attach][Cave][Geometry]")
{
    // survex_root_and_block.svx fixes r1 at file root and ties r3 into
    // "*begin side". The root stations sit in the cave's own namespace, so the
    // whole-cave window owns them while side gets its ordinary Scope trip.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-root-window"));
    cwCave* cave = freshCaveOf(fixture.get());

    attachAndSolve(fixture.get(), cave,
                   fixturePath(QStringLiteral("survex_root_and_block.svx")));

    REQUIRE(cave->tripCount() == 2);
    cwTrip* window = wholeCaveTripOf(cave);
    cwTrip* side = tripForPrefix(cave, QStringLiteral("side"));
    REQUIRE(window != nullptr);
    REQUIRE(side != nullptr);
    CHECK(window->name() == QStringLiteral("survex_root_and_block"));
    CHECK(window->knownStations().size() == 3);
    CHECK(side->knownStations().size() == 2);

    const cwLinePlotGeometry::Result geometry = geometryOf(fixture.get());
    // The root's two legs, side's one, and the tie — drawn once, by side,
    // which the reconcile created before the whole-cave window.
    CHECK(vertexCountOf(geometry, window) == 4);
    CHECK(vertexCountOf(geometry, side) == 4);

    REQUIRE(geometry.nodeLengthAndDepths.contains(cave->id()));
    const cwLinePlotGeometry::LengthAndDepth measured = geometry.nodeLengthAndDepths.value(cave->id());
    CHECK(measured.length() == Catch::Approx(25.0).margin(kSolvedLengthMarginMeters));
    CHECK(measured.depth() == Catch::Approx(2.0).margin(kSolvedLengthMarginMeters));

    drainPipelines(fixture.get());
}

TEST_CASE("A cave whose every station sits under a prefix gets no whole-cave window",
          "[Attach][Cave][Geometry]")
{
    // walls_book_prefix.wpj carries ".OPTIONS prefix=BK" over its only survey,
    // so every station is inside the BK level and the file root holds none. A
    // window with nothing to own would be an empty trip in the table forever.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-book-prefix"));
    cwCave* cave = freshCaveOf(fixture.get());

    attachAndSolve(fixture.get(), cave,
                   fixturePath(QStringLiteral("walls_book_prefix/walls_book_prefix.wpj")));

    REQUIRE(cave->tripCount() == 1);
    CHECK(wholeCaveTripOf(cave) == nullptr);
    cwTrip* book = tripForPrefix(cave, QStringLiteral("BK"));
    REQUIRE(book != nullptr);
    CHECK(book->knownStations().size() == 2);

    const cwLinePlotGeometry::Result geometry = geometryOf(fixture.get());
    CHECK(vertexCountOf(geometry, book) == 2);

    drainPipelines(fixture.get());
}

TEST_CASE("cave replace repairs the windows an older attach left behind",
          "[Attach][Cave]")
{
    // The shape an attachment made before P3.16 is in: one chunk-less trip per
    // Compass survey, prefixed "A", listing nothing. A reconcile removes every
    // chunk-less window the file no longer accounts for and creates the
    // whole-cave one, so Reload or Replace is the repair.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-window-repair"));
    cwCave* cave = freshCaveOf(fixture.get());

    // Attach something else first: the cave-level attach guard refuses a cave
    // that already has trips, so the stale window is planted afterward.
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());
    for (int i = cave->tripCount() - 1; i >= 0; --i) {
        cave->removeTrip(i);
    }
    cwTrip* stale = new cwTrip();
    stale->setName(QStringLiteral("A"));
    stale->setStationPrefix(QStringLiteral("A"));
    cave->addTrip(stale);

    const QUuid staleId = stale->id();

    // The replace, and what it must report whatever the stale window held: the
    // whole-cave window exists, and the report names it with an empty prefix.
    const auto replaceAndCheckWindow = [&]() {
        auto future = managerOf(fixture.get())
                          ->replaceCenterline(cave,
                                              fixturePath(QStringLiteral("compass_multi.mak")));
        REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
        REQUIRE_FALSE(future.result().hasError());
        drainPipelines(fixture.get());

        cwTrip* window = wholeCaveTripOf(cave);
        REQUIRE(window != nullptr);
        const AttachReport report = future.result().value();
        REQUIRE(report.createdScopeTrips.size() == 1);
        CHECK(report.createdScopeTrips.at(0).stationPrefix.isEmpty());
        CHECK(report.createdScopeTrips.at(0).name == window->name());
    };

    SECTION("a chunk-less window the file no longer accounts for is removed") {
        replaceAndCheckWindow();

        CHECK(cave->tripCount() == 1);
        CHECK(tripForPrefix(cave, QStringLiteral("A")) == nullptr);
    }

    SECTION("a window holding chunks survives the reconcile") {
        stale->addChunk(new cwSurveyChunk());
        replaceAndCheckWindow();

        // Survey data the user authored, orphaned prefix and all.
        CHECK(cave->tripCount() == 2);
        cwTrip* survivor = tripForPrefix(cave, QStringLiteral("A"));
        REQUIRE(survivor != nullptr);
        CHECK(survivor->id() == staleId);
    }

    drainPipelines(fixture.get());
}

TEST_CASE("cave replace keeps a window whose prefix the user re-cased",
          "[Attach][Cave]")
{
    // A prefix selects its stations case-insensitively everywhere else — in
    // cwTrip::solvedStations and in the line-plot's scope ownership — so the
    // reconcile has to read "xy" as the window over the scan's "XY" block and
    // keep the user's renamed trip rather than replacing it with a default one.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-window-case"));
    cwCave* cave = freshCaveOf(fixture.get());

    const QString source = fixturePath(QStringLiteral("walls_prefixed/walls_prefixed.wpj"));
    attachAndSolve(fixture.get(), cave, source);

    cwTrip* xy = tripForPrefix(cave, QStringLiteral("XY"));
    REQUIRE(xy != nullptr);
    xy->setName(QStringLiteral("Pit series"));
    xy->setStationPrefix(QStringLiteral("xy"));
    const QUuid xyId = xy->id();
    const int tripCount = cave->tripCount();

    auto future = managerOf(fixture.get())->replaceCenterline(cave, source);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    CHECK(cave->tripCount() == tripCount);
    CHECK(future.result().value().createdScopeTrips.isEmpty());
    cwTrip* survivor = tripForPrefix(cave, QStringLiteral("xy"));
    REQUIRE(survivor != nullptr);
    CHECK(survivor->id() == xyId);
    CHECK(survivor->name() == QStringLiteral("Pit series"));
    CHECK(survivor->knownStations().size() == 2);

    drainPipelines(fixture.get());
}

TEST_CASE("cave detach removes the whole-cave window too", "[Attach][Cave]")
{
    // Under an attached cave every chunk-less trip is a window, the whole-cave
    // one included: detaching leaves nothing for it to window.
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-detach-window"));
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave,
                             fixturePath(QStringLiteral("compass_multi.mak")));
    drainPipelines(fixture.get());
    REQUIRE(wholeCaveTripOf(cave) != nullptr);

    auto future = managerOf(fixture.get())->detachCenterline(cave);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK_FALSE(future.result().hasError());

    CHECK(cave->tripCount() == 0);
    CHECK(cave->externalCenterline().isEmpty());
    CHECK(fixture->project->cavingRegion()->caves().contains(cave));

    drainPipelines(fixture.get());
}

TEST_CASE("cave replace keeps a chunk-less scope trip that holds a note",
          "[Attach][Cave]")
{
    cwNote* note = nullptr;
    checkReplaceKeepsEast(QStringLiteral("cave-replace-keeps-note"),
                          [&note](cwTrip* trip) { note = addNoteWithScrap(trip, QStringLiteral("e1")); },
                          [&note](cwTrip* kept) {
                              REQUIRE(kept->notes()->rowCount() == 1);
                              CHECK(kept->notes()->notes().first() == note);
                          });
}

TEST_CASE("cave replace keeps a chunk-less scope trip that holds a LiDAR scan",
          "[Attach][Cave][NoteLiDAR]")
{
    checkReplaceKeepsEast(QStringLiteral("cave-replace-keeps-lidar"),
                          [](cwTrip* trip) { addLiDARScan(trip); },
                          [](cwTrip* kept) { CHECK(kept->notesLiDAR()->rowCount() == 1); });
}

TEST_CASE("cave reload keeps a scope trip's notes and scraps", "[Attach][Cave][Reload]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-reload-keeps-note"));
    cwCave* cave = freshCaveOf(fixture.get());
    auto manager = managerOf(fixture.get());

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString source = writeSurvey(sourceDir, QStringLiteral("blocks.svx"),
                                       fileContents(blocksFixture()));
    attachCaveThroughManager(fixture.get(), cave, source);
    drainPipelines(fixture.get());

    cwTrip* east = tripForPrefix(cave, kEast);
    REQUIRE(east != nullptr);
    const QUuid eastId = east->id();
    cwNote* note = addNoteWithScrap(east, QStringLiteral("e1"));
    cwScrap* scrap = note->scrap(0);

    // Reload with the block the note windows gone from the source, the one
    // case where a reload decides whether the trip under the note stays.
    overwriteFile(source, kDoghillOnlySource);
    REQUIRE(manager->canReloadFromSource(cave));

    auto future = manager->reloadFromSource(cave);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    cwTrip* kept = tripForPrefix(cave, kEast);
    REQUIRE(kept != nullptr);
    CHECK(kept->id() == eastId);
    REQUIRE(kept->notes()->rowCount() == 1);
    CHECK(kept->notes()->notes().first() == note);
    REQUIRE(note->scraps().size() == 1);
    CHECK(note->scrap(0) == scrap);
    REQUIRE(scrap->numberOfStations() == 1);
    CHECK(scrap->station(0).name() == QStringLiteral("e1"));
}

namespace {

const QString kZoneNoDatumMak = QStringLiteral("compass_zone_no_datum.mak");
const QString kUtm13N = QStringLiteral("EPSG:32613");
// A local transverse Mercator centered near the fixture's fix.
const QString kBoulderFrameCS = QStringLiteral(
    "+proj=tmerc +lat_0=40.0254 +lon_0=-105.2581 +k=1 +x_0=0 +y_0=0 "
    "+datum=WGS84 +units=m +no_defs +type=crs");
// North American 1927, UTM zone 13N: the system the fixture's zone reads in
// under Compass's default datum.
const QString kNad27Utm13N = QStringLiteral("EPSG:26713");
// survex METRES_PER_FOOT.
constexpr double kMetersPerFoot = 0.3048;
// What the fixture's .mak fixes A1 at, in feet.
constexpr double kFixEastingFeet = 1568241.5;
constexpr double kFixNorthingFeet = 14534120.7;
constexpr double kFixElevationFeet = 5429.8;
// Absorbs the float a solved position is stored in.
constexpr double kFixMarginMeters = 5.0;
// The NAD27 -> WGS84 shift near the fixture's fix is ~210 m (PROJ cs2cs:
// 478000 4430000 EPSG:26713 -> 477953.925 4430208.847 EPSG:32613); any shift
// beyond this says the fix went through its datum.
constexpr double kMinimumDatumShiftMeters = 50.0;

QStringList defaultDatumWarnings(const cwSurveyNode* node)
{
    return node->errorModel()->errors()->warningMessagesForTypeIds(
        {static_cast<int>(cwErrorTypeId::AttachedFileDefaultDatum)});
}

//! Where the fixture's A1 belongs in \a frameCS: its .mak coordinate, read in
//! North American 1927 at UTM zone 13N.
cwGeoPoint fixtureA1In(const QString& frameCS)
{
    const auto point = cwCoordinateTransform::transformPoint(
        kNad27Utm13N, frameCS,
        cwGeoPoint(kFixEastingFeet * kMetersPerFoot, kFixNorthingFeet * kMetersPerFoot,
                   kFixElevationFeet * kMetersPerFoot));
    REQUIRE(point.has_value());
    return *point;
}

} // namespace

TEST_CASE("A Compass .mak that names a UTM zone but no datum is read in North American 1927",
          "[Attach][Cave][CS]")
{
    const bool localFrame = GENERATE(false, true);
    const QString frameCS = localFrame ? kBoulderFrameCS : kUtm13N;
    INFO("frame: " << frameCS.toStdString());

    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-compass-default-datum"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    region->geoReference()->restore(cwGeoReference::Frozen, frameCS, {}, QString());
    cwCave* cave = freshCaveOf(fixture.get());

    const QString source = fixturePath(kZoneNoDatumMak);
    const QByteArray sourceBytes = fileContents(source);
    REQUIRE_FALSE(sourceBytes.contains('&'));

    attachAndSolve(fixture.get(), cave, source);
    drainPipelines(fixture.get());

    // The project's copy is the source's bytes; the default datum lives in
    // the driver's translation of it.
    const QString copyPath =
        fixture->saveLoad()->externalCenterlineDir(cave).absoluteFilePath(kZoneNoDatumMak);
    CHECK(fileContents(copyPath) == sourceBytes);
    CHECK(fileContents(source) == sourceBytes);

    // The driver writes A1's fix under North American 1927, UTM zone 13N, so
    // cavern moves it through the datum shift into the frame.
    INFO("driver:\n" << fixture->rootData->linePlotManager()->driverSource().toStdString());
    INFO("cavern log:\n" << fixture->rootData->linePlotManager()->cavernLog().toStdString());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString a1 = QStringLiteral("compass_zone_no_datum.a1");
    REQUIRE(lookup.hasPosition(a1));
    const QVector3D placed = lookup.position(a1);
    const cwGeoPoint expected = fixtureA1In(frameCS);
    CHECK(placed.x() == Catch::Approx(expected.x).margin(kFixMarginMeters));
    CHECK(placed.y() == Catch::Approx(expected.y).margin(kFixMarginMeters));
    CHECK(placed.z() == Catch::Approx(expected.z).margin(kFixMarginMeters));
    if (!localFrame) {
        const double shift = std::hypot(expected.x - kFixEastingFeet * kMetersPerFoot,
                                        expected.y - kFixNorthingFeet * kMetersPerFoot);
        CHECK(shift > kMinimumDatumShiftMeters);
    }

    const QStringList warnings = defaultDatumWarnings(cave);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.first()
          == QStringLiteral("compass_zone_no_datum.mak names UTM zone 13N but no datum; CaveWhere "
                            "used Compass's default, North American 1927 — add a & line to the "
                            "file to choose another."));

    // A click on it opens the Fix Stations page, where the file's row shows
    // the system the default gave it.
    cwNodeWarningModel warningModel;
    warningModel.setNode(cave);
    REQUIRE(warningModel.rowCount() == 1);
    const QModelIndex warningRow = warningModel.index(0);
    CHECK(warningModel.data(warningRow, cwNodeWarningModel::MessageRole).toString()
          == warnings.first());
    CHECK(warningModel.data(warningRow, cwNodeWarningModel::TargetRole).toInt()
          == static_cast<int>(cwNodeWarningModel::Target::FixStationRow));

    SECTION("Reload copies the source byte for byte again")
    {
        auto reloadFuture = managerOf(fixture.get())->reloadFromSource(cave);
        REQUIRE(AsyncFuture::waitForFinished(reloadFuture, kAttachWaitMs));
        REQUIRE_FALSE(reloadFuture.result().hasError());
        drainPipelines(fixture.get());

        CHECK(fileContents(copyPath) == sourceBytes);
        CHECK(fileContents(source) == sourceBytes);
        CHECK(defaultDatumWarnings(cave).size() == 1);
    }
}

TEST_CASE("A Compass .mak is copied byte for byte", "[Attach][Cave][CS]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-compass-explicit-datum"));
    cwCave* cave = freshCaveOf(fixture.get());

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QByteArray datBytes = fileContents(fixturePath(QStringLiteral("compass_zone_no_datum.dat")));
    writeSurvey(sourceDir, QStringLiteral("compass_zone_no_datum.dat"), datBytes);
    const bool explicitDatum = GENERATE(false, true);
    const QByteArray datumLine =
        explicitDatum ? QByteArrayLiteral("&North American 1983;\n") : QByteArray();
    const QByteArray makBytes = datumLine + fileContents(fixturePath(kZoneNoDatumMak));
    const QString source = writeSurvey(sourceDir, kZoneNoDatumMak, makBytes);

    attachAndSolve(fixture.get(), cave, source);
    drainPipelines(fixture.get());

    CHECK(fileContents(fixture->saveLoad()->externalCenterlineDir(cave).absoluteFilePath(
              kZoneNoDatumMak))
          == makBytes);
    CHECK(defaultDatumWarnings(cave).size() == (explicitDatum ? 0 : 1));
}

// The owner's report (2026-10-03): attach, save, quit, reopen, and the
// attached-file header no longer offers Reload. The fresh cwRootData stands
// in for the next launch; the per-machine store is the same one.
TEST_CASE("cave reload stays offered after the project is saved and reopened",
          "[Attach][Cave][Reload]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-reload-reopen"));
    cwCave* cave = freshCaveOf(fixture.get());

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString source = QDir(sourceDir.path()).absoluteFilePath(QStringLiteral("blocks.svx"));
    REQUIRE(QFile::copy(blocksFixture(), source));

    attachCaveThroughManager(fixture.get(), cave, source);
    drainPipelines(fixture.get());
    const QUuid caveId = cave->id();
    REQUIRE(managerOf(fixture.get())->canReloadFromSource(cave));

    QTemporaryDir destinationDir;
    REQUIRE(destinationDir.isValid());
    QString destination;
    SECTION("bundled .cw") {
        destination = QDir(destinationDir.path()).filePath(QStringLiteral("reopened.cw"));
    }
    SECTION(".cwproj directory") {
        destination = QDir(destinationDir.path()).filePath(QStringLiteral("reopened.cwproj"));
    }

    REQUIRE(fixture->project->saveAs(destination));
    fixture->project->waitSaveToFinish();
    drainPipelines(fixture.get());
    const QString savedPath = fixture->project->filename();
    fixture.reset();

    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(savedPath);
    freshRoot->project()->waitLoadToFinish();
    freshRoot->linePlotManager()->waitToFinish();
    freshRoot->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    cwCave* reopened = nullptr;
    for (cwCave* candidate : freshRoot->region()->caves()) {
        if (candidate->id() == caveId) {
            reopened = candidate;
        }
    }
    REQUIRE(reopened != nullptr);
    REQUIRE_FALSE(reopened->externalCenterline().isEmpty());
    CHECK(freshRoot->externalSourceSettings()->breadcrumbPath(caveId)
          == QFileInfo(source).absoluteFilePath());
    REQUIRE(freshRoot->externalCenterlineManager()->canReloadFromSource(reopened));
}
