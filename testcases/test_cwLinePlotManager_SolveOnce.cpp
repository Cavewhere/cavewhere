/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineManager.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cwUpdatable.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

// Std
#include <algorithm>
#include <memory>

namespace {

constexpr int kTreeFiles = 8;
constexpr int kShotsPerFile = 40;
constexpr int kBinaryTreeChildren = 2;
constexpr int kTripTreeFiles = 3;
constexpr int kTripOwners = 2;
constexpr int kSettleWaitMs = 120000;
constexpr int kWallsBooks = 10;
constexpr int kWallsSurveysPerBook = 100;
constexpr int kWallsShots = 3;
constexpr int kWallsFixEvery = 25;
constexpr int kCachedOwners = 3;
//! How long a scan that reads every owner from the harvest cache gets to
//! finish while cavern is held. Generous for a loaded machine; a scan that
//! reaches cavern never finishes inside it.
constexpr int kCachedScanWaitMs = 30000;

const QString kLinePlotJob = QStringLiteral("Line plot");
const QString kWatchedEvent = QStringLiteral("watchedFilesChanged");
const QString kSolveNeededEvent = QStringLiteral("solveNeeded");
const QString kLinePlotJobEvent = QStringLiteral("Line plot job");
const QString kPublishedEvent = QStringLiteral("cavernOutputChanged");
const QString kScanJob = QStringLiteral("External centerline scan");
const QString kTreeEntryName = QStringLiteral("tree.svx");
const QString kAddedStation = QStringLiteral("added.1");

//! The anchor the driver writes for a source root whose entry file is the
//! generated tree's: its first file's first station, fixed at the origin.
QString originAnchor()
{
    return QStringLiteral("*fix %1%2.0 0 0 0").arg(generatedSurvexTree::kFilePrefix).arg(0);
}

//! What the line plot pipeline of one cwRootData published, in order: the
//! scan's apply signals, each "Line plot" job the future manager listed, and
//! each cavern output with its driver and station count.
struct SolveLog {
    QStringList events;
    QStringList drivers;
    QList<int> stationCounts;
    int linePlotJobs = 0;

    qsizetype firstIndexOf(const QString& event) const { return events.indexOf(event); }

    QString describe() const
    {
        QString text = events.join(QStringLiteral(", "));
        for (qsizetype i = 0; i < drivers.size(); ++i) {
            text += QStringLiteral("\n-- published %1: %2 stations, driver:\n%3")
                        .arg(i)
                        .arg(stationCounts.at(i))
                        .arg(drivers.at(i));
        }
        return text;
    }
};

void recordSolves(SolveLog* log, cwLinePlotManager* linePlot, QObject* context)
{
    cwExternalCenterlineManager* external = linePlot->externalCenterlineManager();
    QObject::connect(external, &cwExternalCenterlineManager::watchedFilesChanged, context,
                     [log]() { log->events.append(kWatchedEvent); });
    QObject::connect(external, &cwExternalCenterlineManager::solveNeeded, context,
                     [log]() { log->events.append(kSolveNeededEvent); });
    QObject::connect(linePlot, &cwLinePlotManager::cavernOutputChanged, context, [log, linePlot]() {
        log->events.append(kPublishedEvent);
        log->drivers.append(linePlot->driverSource());
        log->stationCounts.append(linePlot->lastSolveStationCount());
    });
}

void recordSolves(SolveLog* log, cwRootData* root, QObject* context)
{
    recordSolves(log, root->linePlotManager(), context);
    cwFutureManagerModel* futures = root->futureManagerModel();
    QObject::connect(futures, &QAbstractItemModel::rowsInserted, context,
                     [log, futures](const QModelIndex&, int first, int last) {
        for (int row = first; row <= last; ++row) {
            if (futures->index(row).data(cwFutureManagerModel::NameRole).toString() == kLinePlotJob) {
                ++log->linePlotJobs;
                log->events.append(kLinePlotJobEvent);
            }
        }
    });
}

//! Drains every pipeline of \a root until the line plot is Clean, no job is
//! listed, and a quiet spell of the event loop starts nothing new.
void settle(cwRootData* root)
{
    const auto idle = [root]() {
        return root->linePlotManager()->updateState() == cwUpdatable::State::Clean
               && root->futureManagerModel()->rowCount() == 0;
    };
    REQUIRE(tryWait(kSettleWaitMs, [&]() {
        root->project()->waitSaveToFinish();
        root->linePlotManager()->waitToFinish();
        root->futureManagerModel()->waitForFinished();
        if (!idle()) {
            return false;
        }
        if (!waitForQueuedEvents()) {
            return false;
        }
        return idle();
    }));
}

//! Whether \a futures lists the scan job reading \a progress of \a steps.
bool scanRowReads(const cwFutureManagerModel* futures, int steps, int progress)
{
    for (int row = 0; row < futures->rowCount(); ++row) {
        const QModelIndex index = futures->index(row);
        if (index.data(cwFutureManagerModel::NameRole).toString() == kScanJob
            && index.data(cwFutureManagerModel::NumberOfStepRole).toInt() == steps
            && index.data(cwFutureManagerModel::ProgressRole).toInt() == progress) {
            return true;
        }
    }
    return false;
}

//! Records into \a maxSteps the largest step count the scan job's rows show
//! whenever \a futures inserts or changes a row.
void recordScanSteps(int* maxSteps, cwFutureManagerModel* futures, QObject* context)
{
    const auto readRows = [maxSteps, futures](int first, int last) {
        for (int row = first; row <= last; ++row) {
            const QModelIndex index = futures->index(row);
            if (index.data(cwFutureManagerModel::NameRole).toString() == kScanJob) {
                *maxSteps = std::max(*maxSteps,
                                     index.data(cwFutureManagerModel::NumberOfStepRole).toInt());
            }
        }
    };
    QObject::connect(futures, &QAbstractItemModel::rowsInserted, context,
                     [readRows](const QModelIndex&, int first, int last) { readRows(first, last); });
    QObject::connect(futures, &QAbstractItemModel::dataChanged, context,
                     [readRows](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
        readRows(topLeft.row(), bottomRight.row());
    });
}

//! The generated tree's entry, plus a survey tied to its first file that
//! adds kAddedStation.
QByteArray treeEntryWithAddedSurvey()
{
    return QStringLiteral("*include \"%1%2.svx\"\n"
                          "*begin added\n"
                          "*data normal from to tape compass clino\n"
                          "0 1 5.0 0 0\n"
                          "*end added\n"
                          "*equate %1%2.0 added.0\n")
        .arg(generatedSurvexTree::kFilePrefix)
        .arg(0)
        .toUtf8();
}

std::unique_ptr<SavedProjectFixture> makeProject(const QString& name)
{
    auto fixture = makeSavedProject(name, QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    settle(fixture->rootData.get());
    return fixture;
}

QString writeTree(const QTemporaryDir& root, const QString& name, int files)
{
    return writeSurvexTree(tempSubdir(root, name), files, kShotsPerFile, kBinaryTreeChildren);
}

//! Saves \a fixture to a fresh directory, drops it, reopens the copy in a new
//! cwRootData recorded into \a log, and drains it.
std::unique_ptr<cwRootData> saveAndReopen(std::unique_ptr<SavedProjectFixture> fixture,
                                          const QTemporaryDir& destination,
                                          SolveLog* log,
                                          QObject* context)
{
    REQUIRE(fixture->project->saveAs(QDir(destination.path()).filePath(QStringLiteral("reopened.cwproj"))));
    settle(fixture->rootData.get());
    const QString savedPath = fixture->project->filename();
    fixture.reset();

    auto reopened = std::make_unique<cwRootData>();
    recordSolves(log, reopened.get(), context);
    reopened->project()->loadFile(savedPath);
    reopened->project()->waitLoadToFinish();
    settle(reopened.get());
    return reopened;
}

//! A region whose native trip holds A1-A2 and whose attached trip's copy
//! sits in a dir handed to the manager before the region, so the scan that
//! fills its inputs is still pending when setRegion() returns.
struct PendingScanRegion {
    QTemporaryDir tempRoot;
    cwCavingRegion region;
    AttachedSetup setup;
    cwTrip* native = nullptr;
};

std::unique_ptr<PendingScanRegion> makePendingScanRegion()
{
    auto pending = std::make_unique<PendingScanRegion>();
    REQUIRE(pending->tempRoot.isValid());
    pending->setup = setupNativeAndAttached(pending->tempRoot, pending->region,
                                            QStringLiteral("survex_simple.svx"));
    pending->native = pending->setup.cave->trip(0);
    REQUIRE(pending->native->name() == QStringLiteral("Native"));
    return pending;
}

void appendNativeShot(cwTrip* trip, const QString& from, const QString& to)
{
    REQUIRE(trip->chunkCount() > 0);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("90.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    trip->chunk(0)->appendShot(cwStation(from), cwStation(to), shot);
}

} // namespace

TEST_CASE("Attaching a source tree solves the region once, after the scan applies",
          "[LinePlotManager][SolveOnce]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeTree(sourceDir, QStringLiteral("tree"), kTreeFiles);

    auto fixture = makeProject(QStringLiteral("solve-once-attach"));
    QObject context;
    SolveLog log;
    recordSolves(&log, fixture->rootData.get(), &context);
    QSignalSpy published(fixture->rootData->linePlotManager(), &cwLinePlotManager::cavernOutputChanged);

    attachSourceThroughManager(fixture.get(), fixture->project->cavingRegion()->rootNode(), entry);
    settle(fixture->rootData.get());

    INFO(log.describe().toStdString());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

    CHECK(log.linePlotJobs == 1);
    CHECK(published.count() == 1);

    // The scan's apply precedes the job that runs cavern.
    const qsizetype firstJob = log.firstIndexOf(kLinePlotJobEvent);
    REQUIRE(firstJob >= 0);
    const qsizetype watched = log.firstIndexOf(kWatchedEvent);
    const qsizetype solveNeeded = log.firstIndexOf(kSolveNeededEvent);
    REQUIRE(watched >= 0);
    REQUIRE(solveNeeded >= 0);
    CHECK(watched < firstJob);
    CHECK(solveNeeded < firstJob);
    CHECK(solveNeeded < log.firstIndexOf(kPublishedEvent));

    // The first published driver anchors the source root at the origin.
    REQUIRE_FALSE(log.drivers.isEmpty());
    CHECK(log.drivers.first().contains(originAnchor()));
}

TEST_CASE("Attaching a Walls project that fills in over several turns solves the region once",
          "[LinePlotManager][SolveOnce][AttachFill]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWallsProject(tempSubdir(sourceDir, QStringLiteral("walls")),
                                            kWallsBooks, kWallsSurveysPerBook, kWallsShots,
                                            kWallsFixEvery);

    auto fixture = makeProject(QStringLiteral("solve-once-walls-fill"));
    QObject context;
    SolveLog log;
    recordSolves(&log, fixture->rootData.get(), &context);
    QSignalSpy published(fixture->rootData->linePlotManager(), &cwLinePlotManager::cavernOutputChanged);

    auto future = managerOf(fixture.get())->attachSource(
        fixture->project->cavingRegion()->rootNode(), entry);
    REQUIRE(AsyncFuture::waitForFinished(future, kSettleWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    settle(fixture->rootData.get());

    INFO(log.describe().toStdString());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    CHECK(log.linePlotJobs == 1);
    CHECK(published.count() == 1);
    const qsizetype solveNeeded = log.firstIndexOf(kSolveNeededEvent);
    REQUIRE(solveNeeded >= 0);
    CHECK(solveNeeded < log.firstIndexOf(kLinePlotJobEvent));
}

TEST_CASE("Reopening a sourced project solves the region once, after the scan applies",
          "[LinePlotManager][SolveOnce]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeTree(sourceDir, QStringLiteral("tree"), kTreeFiles);

    auto fixture = makeProject(QStringLiteral("solve-once-reopen"));
    attachSourceThroughManager(fixture.get(), fixture->project->cavingRegion()->rootNode(), entry);
    settle(fixture->rootData.get());

    QTemporaryDir destination;
    REQUIRE(destination.isValid());
    QObject context;
    SolveLog log;
    auto reopened = saveAndReopen(std::move(fixture), destination, &log, &context);

    INFO(log.describe().toStdString());
    REQUIRE_FALSE(reopened->linePlotManager()->hasSolveError());

    CHECK(log.linePlotJobs == 1);
    CHECK(log.drivers.size() == 1);

    // The scan's apply precedes the job that runs cavern.
    const qsizetype firstJob = log.firstIndexOf(kLinePlotJobEvent);
    REQUIRE(firstJob >= 0);
    const qsizetype solveNeeded = log.firstIndexOf(kSolveNeededEvent);
    REQUIRE(solveNeeded >= 0);
    CHECK(solveNeeded < firstJob);

    // The first published driver anchors the source root at the origin.
    REQUIRE_FALSE(log.drivers.isEmpty());
    CHECK(log.drivers.first().contains(originAnchor()));
}

TEST_CASE("Reopening trip owners beside a source root publishes the whole plot first",
          "[LinePlotManager][SolveOnce]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());

    auto fixture = makeProject(QStringLiteral("solve-once-owners"));
    attachSourceThroughManager(fixture.get(), fixture->project->cavingRegion()->rootNode(),
                               writeTree(sourceDir, QStringLiteral("root"), kTreeFiles));
    for (int owner = 0; owner < kTripOwners; ++owner) {
        const QString name = QStringLiteral("Owner%1").arg(owner);
        cwTrip* trip = addEmptyTrip(fixture->cave, name);
        attachThroughManager(fixture.get(), trip, writeTree(sourceDir, name, kTripTreeFiles));
    }
    settle(fixture->rootData.get());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

    QTemporaryDir destination;
    REQUIRE(destination.isValid());
    QObject context;
    SolveLog log;
    auto reopened = saveAndReopen(std::move(fixture), destination, &log, &context);

    INFO(log.describe().toStdString());
    REQUIRE_FALSE(reopened->linePlotManager()->hasSolveError());
    REQUIRE_FALSE(log.stationCounts.isEmpty());
    const int finalCount = reopened->linePlotManager()->lastSolveStationCount();
    REQUIRE(finalCount > 0);
    CHECK(log.stationCounts.first() == finalCount);
    CHECK(log.linePlotJobs == 1);
}

TEST_CASE("A native edit made while the scan is pending joins the solve after it",
          "[LinePlotManager][SolveOnce]")
{
    auto pending = makePendingScanRegion();

    cwLinePlotManager manager;
    QObject context;
    SolveLog log;
    recordSolves(&log, &manager, &context);

    manager.externalCenterlineManager()->setTripAttachmentDirs(pending->setup.tripDirs);
    manager.setRegion(&pending->region);
    REQUIRE(manager.externalCenterlineManager()->watchedFiles().isEmpty());

    appendNativeShot(pending->native, QStringLiteral("A2"), QStringLiteral("A3"));

    REQUIRE(tryWait(kSettleWaitMs, [&manager]() {
        manager.waitToFinish();
        return manager.updateState() == cwUpdatable::State::Clean;
    }));
    REQUIRE(waitForQueuedEvents());

    INFO(log.describe().toStdString());
    CHECK(manager.updateState() == cwUpdatable::State::Clean);
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE_FALSE(log.drivers.isEmpty());
    // One solve covers both the edit and the scan's inputs.
    CHECK(log.drivers.size() == 1);
    CHECK(log.drivers.first().contains(QStringLiteral("A3"), Qt::CaseInsensitive));
    CHECK(log.drivers.first().contains(QStringLiteral("survex_simple.svx")));
    CHECK(manager.driverSource().contains(QStringLiteral("A3"), Qt::CaseInsensitive));
}

TEST_CASE("Dropping the region while its scan is pending leaves the line plot Clean",
          "[LinePlotManager][SolveOnce]")
{
    auto pending = makePendingScanRegion();

    cwLinePlotManager manager;
    QObject context;
    SolveLog log;
    recordSolves(&log, &manager, &context);

    manager.externalCenterlineManager()->setTripAttachmentDirs(pending->setup.tripDirs);
    manager.setRegion(&pending->region);
    REQUIRE(manager.externalCenterlineManager()->watchedFiles().isEmpty());
    appendNativeShot(pending->native, QStringLiteral("A2"), QStringLiteral("A3"));
    REQUIRE(manager.updateState() == cwUpdatable::State::Working);

    manager.setRegion(nullptr);

    CHECK(tryWait(kSettleWaitMs, [&manager]() {
        return manager.updateState() == cwUpdatable::State::Clean;
    }));
    REQUIRE(waitForQueuedEvents());
    INFO(log.describe().toStdString());
    CHECK(manager.updateState() == cwUpdatable::State::Clean);
    CHECK(manager.driverSource().isEmpty());
}

TEST_CASE("Emptying the region while its scan is pending starts no solve after it",
          "[LinePlotManager][SolveOnce]")
{
    auto pending = makePendingScanRegion();

    cwLinePlotManager manager;
    QObject context;
    SolveLog log;
    recordSolves(&log, &manager, &context);

    manager.externalCenterlineManager()->setTripAttachmentDirs(pending->setup.tripDirs);
    manager.setRegion(&pending->region);
    REQUIRE(manager.externalCenterlineManager()->isScanning());
    appendNativeShot(pending->native, QStringLiteral("A2"), QStringLiteral("A3"));
    REQUIRE(manager.updateState() == cwUpdatable::State::Working);

    // Removing the attached trip and then the native one leaves nothing to
    // solve, which ends the run before the scan applies.
    cwCave* cave = pending->setup.cave;
    REQUIRE(cave->tripCount() == 2);
    cave->removeTrip(1);
    cave->removeTrip(0);
    REQUIRE(cave->tripCount() == 0);
    REQUIRE(manager.updateState() == cwUpdatable::State::Clean);

    REQUIRE(tryWait(kSettleWaitMs, [&manager]() {
        manager.waitToFinish();
        return !manager.externalCenterlineManager()->isScanning()
               && manager.updateState() == cwUpdatable::State::Clean;
    }));
    REQUIRE(waitForQueuedEvents());

    INFO(log.describe().toStdString());
    CHECK(manager.updateState() == cwUpdatable::State::Clean);
    CHECK_FALSE(manager.hasSolveError());
    CHECK(manager.driverSource().isEmpty());
    for (const QString& driver : std::as_const(log.drivers)) {
        CHECK(driver.isEmpty());
    }
}

TEST_CASE("A restart while a harvest waits on the cavern mutex applies one harvest and solves once",
          "[LinePlotManager][SolveOnce][CancelHarvest]")
{
    if (!CavernMutexHolder::kSupported) {
        SKIP("The cavern mutex holder needs a POSIX FIFO");
    }

    // A pin on the outcome, which holds today too: the restarter drops the
    // superseded scan's result. That the superseded harvest skips cavern is
    // pinned in [ExternalStationHarvest][CancelHarvest].
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeTree(sourceDir, QStringLiteral("tree"), kTreeFiles);

    auto fixture = makeProject(QStringLiteral("solve-once-restart-harvest"));
    cwExternalCenterlineManager* external = managerOf(fixture.get());
    QObject context;
    SolveLog log;
    recordSolves(&log, fixture->rootData.get(), &context);
    QSignalSpy published(fixture->rootData->linePlotManager(), &cwLinePlotManager::cavernOutputChanged);

    CavernMutexHolder holder;
    REQUIRE(holder.isHolding());
    cwSurveyNode* sourceRoot =
        attachSourceThroughManager(fixture.get(), fixture->project->cavingRegion()->rootNode(), entry);
    const bool scanStarted = tryWait(kSettleWaitMs, [external]() { return external->isScanning(); });

    // The scan is running; run the continuations queued behind it.
    REQUIRE(waitForQueuedEvents());
    external->rescanAttachments();
    holder.release();
    settle(fixture->rootData.get());
    REQUIRE(scanStarted);

    INFO(log.describe().toStdString());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    CHECK_FALSE(sourceRoot->externalStations().isEmpty());
    CHECK(log.events.count(kSolveNeededEvent) == 1);
    CHECK(log.linePlotJobs == 1);
    CHECK(published.count() == 1);
}

TEST_CASE("Save As of an unsaved project with an attached source runs no harvest and solves once",
          "[LinePlotManager][SolveOnce][HarvestCache]")
{
    if (!CavernMutexHolder::kSupported) {
        SKIP("The cavern mutex holder needs a POSIX FIFO");
    }

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeTree(sourceDir, QStringLiteral("tree"), kTreeFiles);

    // Unsaved, so the Save As moves the project root and then renames its
    // data root: two moves of every copy, with the content unchanged.
    auto fixture = makeNewProject(QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    settle(fixture->rootData.get());
    cwExternalCenterlineManager* external = managerOf(fixture.get());
    cwSurveyNode* sourceRoot =
        attachSourceThroughManager(fixture.get(), fixture->project->cavingRegion()->rootNode(), entry);
    settle(fixture->rootData.get());
    const QStringList stationsBefore = sourceRoot->externalStations();
    REQUIRE_FALSE(stationsBefore.isEmpty());

    QTemporaryDir destination;
    REQUIRE(destination.isValid());
    QObject context;
    SolveLog log;
    recordSolves(&log, fixture->rootData.get(), &context);
    QSignalSpy published(fixture->rootData->linePlotManager(), &cwLinePlotManager::cavernOutputChanged);
    QSignalSpy scanning(external, &cwExternalCenterlineManager::scanningChanged);

    CavernMutexHolder holder;
    REQUIRE(holder.isHolding());
    REQUIRE(fixture->project->saveAs(QDir(destination.path()).filePath(QStringLiteral("copy.cwproj"))));

    // Every scan the Save As starts finishes while cavern is held, so none
    // of them ran a harvest: the copy's content is unchanged.
    const bool scansFinishedWhileHeld = tryWait(kCachedScanWaitMs, [&]() {
        return !scanning.isEmpty() && !external->isScanning();
    });

    // A later restart (the data-root rename, a save job's path) starts
    // another scan. It finishes only if it skips cavern, since the mutex is
    // still held.
    REQUIRE(waitForQueuedEvents());
    const bool quietWhileHeld = tryWait(kCachedScanWaitMs, [external]() {
        return !external->isScanning();
    });

    // Asserted once nothing runs cavern: a cavern run redirects stdout into
    // its own log, so a failure reported during one is lost.
    holder.release();
    settle(fixture->rootData.get());
    CHECK(scansFinishedWhileHeld);
    CHECK(quietWhileHeld);

    INFO(log.describe().toStdString());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    CHECK(sourceRoot->externalStations() == stationsBefore);
    CHECK(log.linePlotJobs == 1);
    CHECK(published.count() == 1);
}

TEST_CASE("Editing one copied file re-harvests only that owner",
          "[LinePlotManager][SolveOnce][HarvestCache]")
{
    if (!CavernMutexHolder::kSupported) {
        SKIP("The cavern mutex holder needs a POSIX FIFO");
    }

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());

    auto fixture = makeProject(QStringLiteral("solve-once-edit-copy"));
    cwExternalCenterlineManager* external = managerOf(fixture.get());
    QList<cwTrip*> owners;
    for (int owner = 0; owner < kCachedOwners; ++owner) {
        const QString name = QStringLiteral("Owner%1").arg(owner);
        cwTrip* trip = addEmptyTrip(fixture->cave, name);
        attachThroughManager(fixture.get(), trip, writeTree(sourceDir, name, kTripTreeFiles));
        owners.append(trip);
    }
    settle(fixture->rootData.get());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

    // The last owner the scan reads, so the owners before it are done when
    // its harvest waits on cavern.
    cwTrip* edited = owners.last();
    REQUIRE_FALSE(edited->externalStations().contains(kAddedStation));
    const QString copyPath =
        fixture->saveLoad()->externalCenterlineDir(edited).absoluteFilePath(kTreeEntryName);
    REQUIRE(QFileInfo::exists(copyPath));

    QObject context;
    SolveLog log;
    recordSolves(&log, fixture->rootData.get(), &context);
    QSignalSpy published(fixture->rootData->linePlotManager(), &cwLinePlotManager::cavernOutputChanged);

    CavernMutexHolder holder;
    REQUIRE(holder.isHolding());
    overwriteFile(copyPath, treeEntryWithAddedSurvey());
    external->rescanAttachments();

    // The unchanged owners come from the cache, so the scan job reads one
    // short of every owner while the edited one waits on cavern.
    const int ownerCount = static_cast<int>(owners.size());
    const bool cachedOwnersCounted = tryWait(kCachedScanWaitMs, [&]() {
        return scanRowReads(fixture->rootData->futureManagerModel(), ownerCount, ownerCount - 1);
    });

    holder.release();
    settle(fixture->rootData.get());
    CHECK(cachedOwnersCounted);

    INFO(log.describe().toStdString());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    CHECK(edited->externalStations().contains(kAddedStation));
    CHECK(log.linePlotJobs == 1);
    CHECK(published.count() == 1);
}

TEST_CASE("The external centerline scan job counts its owners",
          "[LinePlotManager][SolveOnce][cwFutureManagerModel]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());

    auto fixture = makeProject(QStringLiteral("solve-once-scan-steps"));
    QObject context;
    int maxSteps = 0;
    recordScanSteps(&maxSteps, fixture->rootData->futureManagerModel(), &context);

    for (int owner = 0; owner < kCachedOwners; ++owner) {
        const QString name = QStringLiteral("Owner%1").arg(owner);
        cwTrip* trip = addEmptyTrip(fixture->cave, name);
        attachThroughManager(fixture.get(), trip, writeTree(sourceDir, name, kTripTreeFiles));
    }
    settle(fixture->rootData.get());

    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    CHECK(maxSteps == kCachedOwners);
}
