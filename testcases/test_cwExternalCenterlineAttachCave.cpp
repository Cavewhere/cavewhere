/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwAttachedCenterlinesModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotGeometry.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSignalSpy.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVector3D>

// Std
#include <algorithm>
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

    SECTION("every Scope trip is chunk-less") {
        survivingTripCount = 0;
    }

    SECTION("a Scope trip the user put chunks in survives") {
        cwTrip* east = tripForPrefix(cave, kEast);
        REQUIRE(east != nullptr);
        east->addChunk(new cwSurveyChunk());
        survivingTripCount = 1;
    }

    auto future = managerOf(fixture.get())->detachCenterline(cave);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK_FALSE(future.result().hasError());

    CHECK(cave->tripCount() == survivingTripCount);
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

TEST_CASE("cave replace keeps matching Scope trips, adds new ones, and keeps orphans",
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
    const QUuid eastId = tripForPrefix(cave, kEast)->id();

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

    // Orphaned: the trip whose block is gone stays, silently (§5 q5).
    cwTrip* east = tripForPrefix(cave, kEast);
    REQUIRE(east != nullptr);
    CHECK(east->id() == eastId);
    CHECK(cave->tripCount() == 4);

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
    // stationPrefix, while cavern lowercases every label it writes to the .3d.
    // The line-plot geometry pass has to window across that difference, or the
    // cave renders station labels with no centerline between them.
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
