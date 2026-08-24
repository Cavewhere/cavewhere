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
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

// Std
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

//! The trip windowing `stationPrefix`, or null when the cave has none.
cwTrip* tripForPrefix(const cwCave* cave, const QString& stationPrefix)
{
    for (cwTrip* trip : cave->trips()) {
        if (trip->stationPrefix() == stationPrefix) {
            return trip;
        }
    }
    return nullptr;
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

TEST_CASE("cave replace keeps matching Scope trips, adds new ones, and keeps orphans",
          "[Attach][Cave]")
{
    auto fixture = makeProjectWithFreshCave(QStringLiteral("cave-replace"));
    cwCave* cave = freshCaveOf(fixture.get());
    attachCaveThroughManager(fixture.get(), cave, blocksFixture());
    drainPipelines(fixture.get());

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
