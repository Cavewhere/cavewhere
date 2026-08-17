/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef EXTERNALCENTERLINETESTHELPERS_H
#define EXTERNALCENTERLINETESTHELPERS_H

// Shared fixture helpers for the external-centerline suites
// ([Attach][...], [LinePlotManager][AttachedCenterlinesModel],
// [LinePlotManager][AsyncScan]). Header-only, Catch2-based like the
// other testcases/*Helper*.h headers.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Test helpers
#include "LoadProjectHelper.h"

// Qt
#include <QAbstractItemModel>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QVariant>

// Std
#include <functional>
#include <memory>

// Watcher events flow through OS notifications and worker scans settle
// on the next event-loop spins. Tests poll a predicate against this
// budget — picked well above the actual settle time so a busy CI under
// valgrind still has headroom; the watcher itself fires within ~10ms.
inline constexpr int kWatcherWaitMs = 5000;

// Per-iteration event-loop budget inside tryWait. Long enough for one
// AsyncFuture continuation to run; short enough that the predicate
// re-evaluates promptly on flaky-CI scheduling jitter.
inline constexpr int kPollEventsBudgetMs = 50;

// Inside a tryWait predicate tests can pump the loop once more so a
// freshly queued continuation the outer pump just missed can land
// before the predicate re-evaluates. Smaller than kPollEventsBudgetMs
// because it only drains what is already queued.
inline constexpr int kInnerPollEventsMs = 10;

// Bounded wait used to prove nothing happens: long enough for a watcher
// event to have been delivered and any queued reconcile, scan, or solve
// continuation to have run. Shared so tuning it for a slow CI box is one
// edit rather than one per suite.
inline constexpr int kNothingHappensSettleMs = 500;

// Spins the event loop for `ms` wall-clock milliseconds, giving a
// (wrongly) queued continuation every chance to run before the test
// asserts it did not.
inline void settleEventLoop(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
    }
}

inline bool tryWait(int timeoutMs, std::function<bool()> predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        if (timer.elapsed() >= timeoutMs) {
            return predicate(); // one last evaluation under the deadline
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, kPollEventsBudgetMs);
    }
    return true;
}

// Path of a file in the external-centerlines test dataset.
inline QString fixturePath(const QString& name)
{
    return testcasesDatasetSourcePath(QStringLiteral("external-centerlines/%1").arg(name));
}

inline QString tempSubdir(const QTemporaryDir& root, const QString& name)
{
    const QDir baseDir(root.path());
    REQUIRE(baseDir.mkpath(name));
    return baseDir.absoluteFilePath(name);
}

// Copies `source` to <attachDir>/<basename(source)> so the worker's
// *include path resolves at solve time. Returns the destination path
// so the test can later edit it directly.
inline QString seedAttachment(const QString& attachDir, const QString& source)
{
    const QString basename = QFileInfo(source).fileName();
    const QString dest = QDir(attachDir).absoluteFilePath(basename);
    REQUIRE_FALSE(source.isEmpty());
    REQUIRE(QFile::exists(source));
    if (QFile::exists(dest)) {
        QFile::remove(dest);
    }
    REQUIRE(QFile::copy(source, dest));
    return dest;
}

// Overwrite a watched file in place (open-truncate-write) — the same
// shape as a user editing the file in an editor that saves in place,
// which QFileSystemWatcher on macOS treats differently from a
// rename-over.
inline void overwriteFile(const QString& path, const QByteArray& content)
{
    QFile f(path);
    REQUIRE(f.open(QFile::WriteOnly | QFile::Truncate));
    REQUIRE(f.write(content) == content.size());
    f.close();
}

// Writes `content` to `path`, then forces lastModified to `mtime` via
// QFile::setFileTime (Qt 6 API). Returns the path so callers can pipe it
// into copyIfNewer.
inline QString writeFileWithMtime(const QString& path, const QByteArray& content,
                                  const QDateTime& mtime)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    {
        QFile f(path);
        REQUIRE(f.open(QFile::WriteOnly));
        f.write(content);
    }
    QFile f(path);
    REQUIRE(f.open(QFile::ReadWrite));
    REQUIRE(f.setFileTime(mtime, QFileDevice::FileModificationTime));
    return path;
}

inline QByteArray fileContents(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QFile::ReadOnly));
    return file.readAll();
}

struct SavedProjectFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;

    cwSaveLoad* saveLoad() const { return project->saveLoad(); }
    cwExternalSourceSettings* settings() const { return rootData->externalSourceSettings(); }
};

// One cave, one trip, never saved: the state the user is in right after
// launching CaveWhere and adding a cave.
inline std::unique_ptr<SavedProjectFixture> makeNewProject(const QString& caveName,
                                                           const QString& tripName)
{
    auto fixture = std::make_unique<SavedProjectFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();

    auto region = fixture->project->cavingRegion();
    region->addCave();
    fixture->cave = region->cave(0);
    fixture->cave->setName(caveName);
    fixture->cave->addTrip();
    fixture->trip = fixture->cave->trip(0);
    fixture->trip->setName(tripName);

    return fixture;
}

// Builds a saved-to-disk project with one cave + one trip already on disk.
// All paths under tempDir.path(); safe to run in parallel processes because
// QTemporaryDir generates a unique suffix per process.
inline std::unique_ptr<SavedProjectFixture> makeSavedProject(
    const QString& projectFileBase,
    const QString& caveName,
    const QString& tripName,
    const QString& extension = QStringLiteral(".cwproj"))
{
    auto fixture = makeNewProject(caveName, tripName);

    const QString projectPath =
        QDir(fixture->tempDir.path()).filePath(projectFileBase + extension);
    REQUIRE(fixture->project->saveAs(projectPath));
    fixture->project->waitSaveToFinish();
    // Drain the queued fileSaved delivery so modified() is settled false
    // before tests take a baseline (same idiom as test_cwLazLayerSaveLoad).
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    return fixture;
}

inline cwCave* addEmptyCave(cwCavingRegion& region, const QString& name)
{
    cwCave* cave = new cwCave();
    cave->setName(name);
    region.addCave(cave);
    return cave;
}

inline cwTrip* addEmptyTrip(cwCave* cave, const QString& name)
{
    cwTrip* trip = new cwTrip();
    trip->setName(name);
    cave->addTrip(trip);
    return trip;
}

inline cwTrip* addAttachedTrip(cwCave* cave,
                               const QString& name,
                               const QString& fixture = QStringLiteral("survex_simple.svx"))
{
    cwTrip* trip = addEmptyTrip(cave, name);
    trip->setExternalCenterline(cwExternalCenterline(fixture));
    return trip;
}

inline cwTrip* addNativeTripWithShot(cwCave* cave,
                                    const QString& name,
                                    const QString& fromName,
                                    const QString& toName)
{
    cwTrip* trip = addEmptyTrip(cave, name);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(fromName), cwStation(toName), shot);
    return trip;
}

// Copies `fixture` into its own subdirectory of `tempRoot` and registers that
// directory for `trip`, which is what makes the driver's *include resolve.
inline void attachFixture(const QTemporaryDir& tempRoot,
                          const cwTrip* trip,
                          const QString& fixture,
                          QHash<QUuid, QString>& tripDirs)
{
    const QString attachDir = tempSubdir(tempRoot, trip->name());
    seedAttachment(attachDir, fixturePath(fixture));
    tripDirs.insert(trip->id(), attachDir);
}

struct AttachedSetup {
    cwCave* cave = nullptr;
    cwTrip* attached = nullptr;
    QHash<QUuid, QString> tripDirs;
};

// The shape every attachment case starts from: one cave holding a native trip
// that anchors it and one attached centerline that may or may not join.
// `region` is an out-parameter because cwCavingRegion is a QAbstractListModel
// and so cannot be returned by value.
inline AttachedSetup setupNativeAndAttached(const QTemporaryDir& tempRoot,
                                            cwCavingRegion& region,
                                            const QString& fixture)
{
    AttachedSetup setup;
    setup.cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(setup.cave, QStringLiteral("Native"),
                          QStringLiteral("A1"), QStringLiteral("A2"));
    setup.attached = addAttachedTrip(setup.cave, QStringLiteral("Attached"), fixture);
    attachFixture(tempRoot, setup.attached, fixture, setup.tripDirs);
    return setup;
}

// Solves `region` with the given trip attachments and waits for the answer. The
// manager is the caller's, so it can go on to assert on its solve state.
inline void solveRegion(cwLinePlotManager& manager,
                        cwCavingRegion& region,
                        const QHash<QUuid, QString>& tripDirs)
{
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();
}

//! The survey label an externally-attached trip's *begin block carries, which is
//! also the scope its solved stations are keyed under in the cave lookup.
inline QString tripScopeLabel(const cwTrip* trip)
{
    QString prefix = trip->scopePrefix();
    //An unscoped trip has no label, and chop() would quietly hand back "" —
    //which turns every '*begin ' + label assertion into a match on the cave's
    //own block instead of failing.
    REQUIRE_FALSE(prefix.isEmpty());
    prefix.chop(1); //the trailing '.'
    return prefix;
}

inline QVariant roleAt(const QAbstractItemModel* model, int row, int role)
{
    return model->data(model->index(row, 0), role);
}

#endif // EXTERNALCENTERLINETESTHELPERS_H
