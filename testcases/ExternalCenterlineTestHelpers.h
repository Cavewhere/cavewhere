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
#include "cwAttachedFixModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotGeometry.h"
#include "cwLinePlotManager.h"
#include "cwNote.h"
#include "cwNoteStation.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwScrap.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"

// Test helpers
#include "LoadProjectHelper.h"

// AsyncFuture
#include <asyncfuture.h>

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
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QVariant>

// Std
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>

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

// The shot readings and file names writeSurvexTree() generates.
namespace generatedSurvexTree {

inline constexpr double kTapeBase = 5.0;
inline constexpr int kTapeSpread = 7;
inline constexpr int kSideTapeSpread = 5;
inline constexpr int kBearingStep = 37;
inline constexpr int kSideBearingStep = 53;
inline constexpr int kFileBearingStep = 11;
inline constexpr int kDegreesPerTurn = 360;
inline constexpr int kClinoSpread = 9;
inline constexpr int kClinoCenter = 4;

inline const QString kFilePrefix = QStringLiteral("f");

} // namespace generatedSurvexTree

//! Writes a tree of \a files Survex files under \a dir and returns the entry
//! file. File k opens `*begin fk`, holds \a shots legs split between its own
//! block and a nested `side` block, and includes and equates its \a children
//! files c*k+1 .. c*k+c.
inline QString writeSurvexTree(const QString& dir, int files, int shots, int children)
{
    using namespace generatedSurvexTree;
    const int half = shots / 2;
    for (int k = 0; k < files; ++k) {
        QString text;
        QTextStream out(&text);
        out << "*begin " << kFilePrefix << k << "\n";
        out << "*data normal from to tape compass clino\n";
        for (int i = 0; i < half; ++i) {
            out << i << " " << (i + 1) << " " << (kTapeBase + (i % kTapeSpread)) << " "
                << ((i * kBearingStep + k * kFileBearingStep) % kDegreesPerTurn) << " "
                << ((i % kClinoSpread) - kClinoCenter) << "\n";
        }
        out << "*begin side\n*data normal from to tape compass clino\n";
        for (int i = 0; i < shots - half; ++i) {
            out << i << " " << (i + 1) << " " << (kTapeBase + (i % kSideTapeSpread)) << " "
                << ((i * kSideBearingStep + k * kFileBearingStep) % kDegreesPerTurn) << " "
                << ((i % kClinoSpread) - kClinoCenter) << "\n";
        }
        out << "*end side\n";
        out << "*equate " << half / 2 << " side.0\n";
        for (int child = children * k + 1; child <= children * k + children && child < files;
             ++child) {
            out << "*include \"" << kFilePrefix << child << ".svx\"\n";
            out << "*equate " << half << " " << kFilePrefix << child << ".0\n";
        }
        out << "*end " << kFilePrefix << k << "\n";
        out.flush();
        overwriteFile(QDir(dir).filePath(QStringLiteral("%1%2.svx").arg(kFilePrefix).arg(k)),
                      text.toUtf8());
    }
    const QString entry = QDir(dir).filePath(QStringLiteral("tree.svx"));
    overwriteFile(entry, QStringLiteral("*include \"%1%2.svx\"\n").arg(kFilePrefix).arg(0).toUtf8());
    return entry;
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

// Plenty of headroom for the scan worker + save-job queue under
// valgrind / busy CI; the actual attach finishes in milliseconds.
inline constexpr int kAttachWaitMs = 10000;

inline cwExternalCenterlineManager* managerOf(SavedProjectFixture* fixture)
{
    return fixture->rootData->externalCenterlineManager();
}

// Drains the scan-then-solve pipeline plus the save queue so the maps,
// rows, and watch set reflect every queued recompute.
inline void drainPipelines(SavedProjectFixture* fixture)
{
    fixture->project->waitSaveToFinish();
    fixture->rootData->linePlotManager()->waitToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
}

// The "give me an attached trip" prologue for tests about something
// else. Tests about attach itself drive the future directly so they can
// observe it mid-flight.
inline void attachThroughManager(SavedProjectFixture* fixture, cwTrip* trip,
                                 const QString& sourcePath)
{
    auto future = managerOf(fixture)->attachCenterline(trip, sourcePath);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
}

// The source twin of the prologue above: attaches sourcePath as a sourced
// subtree under target and returns the root node the attach built.
inline cwSurveyNode* attachSourceThroughManager(SavedProjectFixture* fixture,
                                                cwSurveyNode* target,
                                                const QString& sourcePath)
{
    auto future = managerOf(fixture)->attachSource(target, sourcePath);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    cwSurveyNode* root = future.result().value().root.data();
    REQUIRE(root != nullptr);
    return root;
}

//! Attaches sourcePath as a source root under the region's root. Every node
//! below the root is a cwCave, the type cwSaveLoad derives the copy's dir from.
inline cwCave* attachSourceUnderRegion(SavedProjectFixture* fixture, const QString& sourcePath)
{
    auto* root = qobject_cast<cwCave*>(attachSourceThroughManager(
        fixture, fixture->project->cavingRegion()->rootNode(), sourcePath));
    REQUIRE(root != nullptr);
    return root;
}

//! The attached row of \a node that names \a station.
inline cwAttachedFix attachedFixFor(const cwSurveyNode* node, const QString& station)
{
    for (const cwAttachedFix& fix : node->attachedFixes()->attachedFixes()) {
        if (fix.station == station) {
            return fix;
        }
    }
    FAIL("No attached row names " << station.toStdString());
    return cwAttachedFix();
}

//! The attached rows \a node lists on its Fix Stations page, by station.
inline QStringList attachedFixStations(const cwSurveyNode* node)
{
    QStringList stations;
    for (const cwAttachedFix& fix : node->attachedFixes()->attachedFixes()) {
        stations.append(fix.station);
    }
    return stations;
}

//! \a node, then every node under it, pre-order.
inline QList<const cwSurveyNode*> nodeAndDescendants(const cwSurveyNode* node)
{
    QList<const cwSurveyNode*> nodes;
    node->walk([&nodes](const cwSurveyNode* current) { nodes.append(current); });
    return nodes;
}

//! The attached rows \a node lists, then those its trips list.
inline QList<cwAttachedFix> nodeAndTripAttachedFixes(const cwSurveyNode* node)
{
    QList<cwAttachedFix> fixes = node->attachedFixes()->attachedFixes();
    for (const cwTrip* trip : node->trips()) {
        fixes.append(trip->attachedFixes()->attachedFixes());
    }
    return fixes;
}

//! The attached row that names \a station on \a node, a node under it, or a
//! trip of either: a file's fix is listed on the derived trip whose leaf
//! Survex block holds it, else on the derived node whose file holds it.
inline cwAttachedFix subtreeAttachedFixFor(const cwSurveyNode* node, const QString& station)
{
    for (const cwSurveyNode* holder : nodeAndDescendants(node)) {
        for (const cwAttachedFix& fix : nodeAndTripAttachedFixes(holder)) {
            if (fix.station == station) {
                return fix;
            }
        }
    }
    FAIL("No attached row in the subtree names " << station.toStdString());
    return cwAttachedFix();
}

//! \a node's AttachedFileFixWithoutCS warnings.
inline QStringList bareFixWarnings(const cwSurveyNode* node)
{
    return node->errorModel()->errors()->warningMessagesForTypeIds(
        {static_cast<int>(cwErrorTypeId::AttachedFileFixWithoutCS)});
}

inline cwSurveyNode* childNodeNamed(const cwSurveyNode* parent, const QString& name)
{
    for (cwSurveyNode* child : parent->childNodes()) {
        if (child->name() == name) {
            return child;
        }
    }
    return nullptr;
}

inline QStringList childNodeNames(const cwSurveyNode* node)
{
    QStringList names;
    for (const cwSurveyNode* child : node->childNodes()) {
        names.append(child->name());
    }
    return names;
}

inline QStringList tripNames(const cwSurveyNode* node)
{
    QStringList names;
    for (const cwTrip* trip : node->trips()) {
        names.append(trip->name());
    }
    return names;
}

inline cwCave* addEmptyCave(cwCavingRegion& region, const QString& name)
{
    cwCave* cave = new cwCave();
    cave->setName(name);
    region.addCave(cave);
    return cave;
}

//! Puts a \a kind node named \a name under \a parent, below the region's caves.
inline cwCave* addChildNode(cwSurveyNode* parent, const QString& name, cwSurveyNode::Kind kind)
{
    cwCave* child = new cwCave();
    child->setName(name);
    child->setKind(kind);
    parent->addNode(child);
    return child;
}

inline cwTrip* addEmptyTrip(cwCave* cave, const QString& name)
{
    cwTrip* trip = new cwTrip();
    trip->setName(name);
    cave->addTrip(trip);
    return trip;
}

//! Puts a one-scrap note on \a trip, the scrap anchored at \a stationName.
inline cwNote* addNoteWithScrap(cwTrip* trip, const QString& stationName)
{
    // A saved note names its image by path, so the project can write it.
    cwNote* note = new cwNote();
    note->setName(QStringLiteral("page.png"));
    cwImage image;
    image.setPath(QStringLiteral("page.png"));
    note->setImage(image);
    trip->notes()->addNotes({note});
    cwScrap* scrap = new cwScrap();
    note->addScrap(scrap);
    cwNoteStation noteStation;
    noteStation.setName(stationName);
    noteStation.setPositionOnNote(QPointF(0.5, 0.5));
    scrap->addStation(noteStation);
    return note;
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
                                    const QString& toName,
                                    const cwDistanceReading& distance = cwDistanceReading(QStringLiteral("10.0")),
                                    const cwClinoReading& clino = cwClinoReading(QStringLiteral("0.0")))
{
    cwTrip* trip = addEmptyTrip(cave, name);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(distance);
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(clino);
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

//! How many vertices the geometry pass gave \a trip.
inline int vertexCountOf(const cwLinePlotGeometry::Result& geometry, const cwTrip* trip)
{
    const qsizetype tripIndex = geometry.tripUuids.indexOf(trip->id());
    REQUIRE(tripIndex >= 0);
    return geometry.tripVertexRanges.at(tripIndex).count;
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

//! The UnconnectedStations entry in \a model's own list, if the last solve
//! wrote one.
inline std::optional<cwError> unconnectedStationsEntry(const cwErrorModel* model)
{
    const QList<cwError> errors = model->errors()->toList();
    for (const cwError& error : errors) {
        if (error.errorTypeId() == static_cast<int>(cwErrorTypeId::UnconnectedStations)) {
            return error;
        }
    }
    return std::nullopt;
}

namespace mainThreadStall {
inline constexpr int kFrameIntervalMs = 16;
inline constexpr int kLeadInTicks = 3;
inline constexpr int kTrailingTicks = 3;
inline constexpr int kStallWaitMs = 600000;
} // namespace mainThreadStall

//! The longest gap between two ticks of a 16 ms timer on the main thread while
//! one queued call runs \a work: how long \a work holds the event loop.
inline qint64 longestMainThreadStall(const std::function<void()>& work)
{
    using namespace mainThreadStall;
    QTimer frame;
    frame.setTimerType(Qt::PreciseTimer);
    frame.setInterval(kFrameIntervalMs);

    QElapsedTimer clock;
    qint64 lastTick = 0;
    qint64 longest = 0;
    int ticks = 0;
    int ticksAfterWork = -1;
    QObject::connect(&frame, &QTimer::timeout, [&]() {
        const qint64 now = clock.elapsed();
        longest = std::max(longest, now - lastTick);
        lastTick = now;
        ++ticks;
        if (ticksAfterWork >= 0) {
            ++ticksAfterWork;
        }
        if (ticks == kLeadInTicks) {
            longest = 0;
            QTimer::singleShot(0, [&]() {
                work();
                ticksAfterWork = 0;
            });
        }
    });

    clock.start();
    frame.start();
    REQUIRE(tryWait(kStallWaitMs, [&]() { return ticksAfterWork >= kTrailingTicks; }));
    frame.stop();
    return longest;
}

#endif // EXTERNALCENTERLINETESTHELPERS_H
