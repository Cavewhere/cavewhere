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
#include "cwCavernRunner.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwGeoReference.h"
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
#include "TestHelper.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QAbstractItemModel>
#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QVariant>

// Std
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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

// Plenty of headroom for the scan worker + save-job queue under
// valgrind / busy CI; the actual attach finishes in milliseconds.
inline constexpr int kAttachWaitMs = 10000;

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

//! Writes one Survex file, flat.svx, under \a dir and returns its path. The
//! file holds \a blocks sibling leaf blocks b0 .. b(blocks-1) of \a shots legs
//! each and no wrapper block, so every block lists as a trip of the file's
//! root. Each block's first station equates to the previous block's last, so
//! the file solves as one network.
inline QString writeFlatSurvex(const QString& dir, int blocks, int shots)
{
    using namespace generatedSurvexTree;
    const QString blockPrefix = QStringLiteral("b");
    QString text;
    QTextStream out(&text);
    for (int k = 0; k < blocks; ++k) {
        out << "*begin " << blockPrefix << k << "\n";
        out << "*data normal from to tape compass clino\n";
        for (int i = 0; i < shots; ++i) {
            out << i << " " << (i + 1) << " " << (kTapeBase + (i % kTapeSpread)) << " "
                << ((i * kBearingStep + k * kFileBearingStep) % kDegreesPerTurn) << " "
                << ((i % kClinoSpread) - kClinoCenter) << "\n";
        }
        out << "*end " << blockPrefix << k << "\n";
        if (k > 0) {
            out << "*equate " << blockPrefix << (k - 1) << "." << shots << " "
                << blockPrefix << k << ".0\n";
        }
    }
    out.flush();
    const QString entry = QDir(dir).filePath(QStringLiteral("flat.svx"));
    overwriteFile(entry, text.toUtf8());
    return entry;
}

// The names, readings and dates writeWallsProject() generates.
namespace generatedWallsProject {

inline constexpr double kShotLengthMeters = 10.0;
inline constexpr double kOriginEast = 1000.0;
inline constexpr double kOriginNorth = 2000.0;
inline constexpr double kOriginUp = 100.0;

inline const QString kEntryFileName = QStringLiteral("generated.wpj");
inline const QDate kFirstSurveyDate = QDate(2001, 1, 1);

//! The display name and .NAME of book \a book.
inline QString bookTitle(int book) { return QStringLiteral("Book %1").arg(book); }
inline QString bookName(int book) { return QStringLiteral("B%1").arg(book); }

//! The display name and .NAME (the .SRV file's base name) of survey \a survey,
//! counted across the whole project.
inline QString surveyTitle(int survey) { return QStringLiteral("Survey %1").arg(survey); }
inline QString surveyName(int survey) { return QStringLiteral("S%1").arg(survey); }
inline QDate surveyDate(int survey) { return kFirstSurveyDate.addDays(survey); }

//! Station \a index of the one chain every survey continues.
inline QString stationName(int index) { return QStringLiteral("P%1").arg(index); }

} // namespace generatedWallsProject

//! Writes a Walls project under \a dir and returns its entry file,
//! generated.wpj. One top book wraps \a books books B0 .. B(books-1); book b
//! lists \a surveysPerBook surveys, numbered across the project, each a .SRV
//! file beside the .wpj with \a shots legs. Every leg runs 10 m due north, so
//! the surveys continue one chain of stations P0, P1, ... and survey k starts
//! where survey k-1 ends. Every \a fixEvery-th survey (survey 0 among them)
//! fixes its first station where the chain puts it, so the fixes agree.
inline QString writeWallsProject(const QString& dir, int books, int surveysPerBook, int shots,
                                 int fixEvery)
{
    using namespace generatedWallsProject;
    QString project;
    QTextStream wpj(&project);
    wpj << ";WALLS Project file\n";
    wpj << ".BOOK\tGenerated Cave\n.NAME\tGEN\n.STATUS\t8\n";
    for (int book = 0; book < books; ++book) {
        wpj << ".BOOK\t" << bookTitle(book) << "\n.NAME\t" << bookName(book) << "\n.STATUS\t8\n";
        for (int k = 0; k < surveysPerBook; ++k) {
            const int survey = book * surveysPerBook + k;
            wpj << ".SURVEY\t" << surveyTitle(survey) << "\n.NAME\t" << surveyName(survey)
                << "\n.STATUS\t8\n";

            const int firstStation = survey * shots;
            QString text;
            QTextStream srv(&text);
            srv << "#DATE " << surveyDate(survey).toString(Qt::ISODate) << "\n";
            srv << "#UNITS D=Meters A=Degrees V=Degrees DECL=0\n";
            if (survey % fixEvery == 0) {
                srv << "#FIX " << stationName(firstStation) << " " << kOriginEast << " "
                    << (kOriginNorth + firstStation * kShotLengthMeters) << " " << kOriginUp
                    << "\n";
            }
            for (int i = 0; i < shots; ++i) {
                srv << stationName(firstStation + i) << "\t" << stationName(firstStation + i + 1)
                    << "\t" << kShotLengthMeters << "\t0\t0\n";
            }
            srv.flush();
            overwriteFile(QDir(dir).filePath(surveyName(survey) + QStringLiteral(".SRV")),
                          text.toUtf8());
        }
        wpj << ".ENDBOOK\n";
    }
    wpj << ".ENDBOOK\n";
    wpj.flush();
    const QString entry = QDir(dir).filePath(kEntryFileName);
    overwriteFile(entry, project.toUtf8());
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
    // A .cwproj Save commits; without an account the commit fails and the
    // project stays modified.
    fixture->rootData->account()->setName(QStringLiteral("Fixture Tester"));
    fixture->rootData->account()->setEmail(QStringLiteral("fixture.tester@example.com"));
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
    // fileSaved arrives queued, so wait for modified() to settle false before
    // tests take a baseline.
    REQUIRE(tryWait(kAttachWaitMs, [&fixture]() { return !fixture->project->modified(); }));
    return fixture;
}

inline cwExternalCenterlineManager* managerOf(SavedProjectFixture* fixture)
{
    return fixture->rootData->externalCenterlineManager();
}

// Waits until \a manager has no source sweep requested or in flight, so its
// source status rows and source watch set are settled.
inline void waitForSourceSweep(const cwExternalCenterlineManager* manager)
{
    REQUIRE(tryWait(kAttachWaitMs, [manager]() { return !manager->isSweepingSources(); }));
}

// True once nothing is scanning, sweeping, solving, or listed in the task
// model: the quiet point where every queued recompute has landed.
inline bool pipelinesQuiet(const cwRootData* rootData)
{
    const cwExternalCenterlineManager* manager = rootData->externalCenterlineManager();
    return !manager->isScanning()
           && !manager->isSweepingSources()
           && rootData->linePlotManager()->updateState() != cwUpdatable::State::Working
           && rootData->futureManagerModel()->rowCount() == 0;
}

// Waits for the scan-then-solve pipeline and the source sweep behind it to go
// quiet, so the maps, rows, and watch sets reflect every queued recompute.
// The fence runs the continuations a finished operation left queued (an
// owner token's release, a report emission); the second wait covers work
// one of them requested.
inline void waitForQuietPipelines(cwRootData* rootData)
{
    rootData->linePlotManager()->waitToFinish();
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(tryWait(kAttachWaitMs, [rootData]() { return pipelinesQuiet(rootData); }));
    REQUIRE(waitForQueuedEvents());
    REQUIRE(tryWait(kAttachWaitMs, [rootData]() { return pipelinesQuiet(rootData); }));
}

// Drains the scan-then-solve pipeline, the source sweep behind it, and the
// save queue. The save wait runs again after the sweep because the sweep's
// stat refreshes reach the save queue when it installs.
inline void drainPipelines(SavedProjectFixture* fixture)
{
    fixture->project->waitSaveToFinish();
    waitForQuietPipelines(fixture->rootData.get());
    fixture->project->waitSaveToFinish();
}

// Where a project keeps its source breadcrumbs: one record per owner under
// the project root, beside .cw_cache and .cw_trash.
namespace externalSourceStore {
inline const QString kLocalDirName = QStringLiteral(".cw_local");
inline const QString kRecordsDirName = QStringLiteral("external-sources");
inline const QString kRecordSuffix = QStringLiteral(".pb");
//! The QSettings group that held breadcrumbs before they moved into the project.
inline const QString kRetiredSettingsGroup = QStringLiteral("externalCenterlineSources");
} // namespace externalSourceStore

//! The directory holding \a project's .cwproj file, extracted or on disk.
inline QDir projectRootOf(const cwProject* project)
{
    return QFileInfo(project->saveLoad()->fileName()).absoluteDir();
}

//! The directory holding \a project's source records.
inline QString externalSourceRecordsDir(const cwProject* project)
{
    using namespace externalSourceStore;
    return projectRootOf(project).filePath(kLocalDirName + QLatin1Char('/') + kRecordsDirName);
}

//! The file holding \a ownerId's source record in \a project.
inline QString externalSourceRecordPath(const cwProject* project, const QUuid& ownerId)
{
    return QDir(externalSourceRecordsDir(project))
        .filePath(ownerId.toString(QUuid::WithoutBraces) + externalSourceStore::kRecordSuffix);
}

//! How many QSettings keys sit under the retired breadcrumb group.
inline int retiredBreadcrumbSettingsKeyCount()
{
    QSettings settings;
    settings.beginGroup(externalSourceStore::kRetiredSettingsGroup);
    return static_cast<int>(settings.allKeys().size());
}

//! Wipes the retired breadcrumb group, so a reopened project can answer only
//! from its own records.
inline void clearRetiredBreadcrumbSettings()
{
    QSettings settings;
    settings.remove(externalSourceStore::kRetiredSettingsGroup);
}

//! Opens \a projectPath in a new cwRootData, with the retired QSettings group
//! wiped first, and waits for the load and the pipelines behind it.
inline std::unique_ptr<cwRootData> openInFreshRootData(const QString& projectPath)
{
    clearRetiredBreadcrumbSettings();
    auto rootData = std::make_unique<cwRootData>();
    rootData->project()->loadFile(projectPath);
    rootData->project()->waitLoadToFinish();
    waitForQuietPipelines(rootData.get());
    rootData->project()->waitSaveToFinish();
    return rootData;
}

//! Opens \a fixture's project a second time from disk once its queued writes
//! land: what a later session on this machine sees.
inline std::unique_ptr<cwRootData> reopenFromDisk(SavedProjectFixture* fixture)
{
    drainPipelines(fixture);
    return openInFreshRootData(fixture->project->filename());
}

//! Closes \a fixture's project and opens \a projectPath in the same
//! cwRootData, with the retired QSettings group wiped in between.
inline void closeAndOpen(SavedProjectFixture* fixture, const QString& projectPath)
{
    drainPipelines(fixture);
    fixture->project->newProject();
    drainPipelines(fixture);
    clearRetiredBreadcrumbSettings();
    fixture->project->loadFile(projectPath);
    fixture->project->waitLoadToFinish();
    drainPipelines(fixture);
}

// Waits for the project to take a frame and for the solve in it, then drains
// the pipelines. The region writes the frame's *cs out on the driver's second
// line, so a file's own *cs out in a frameless driver leaves the wait running.
inline bool waitForFrameAndSolve(SavedProjectFixture* fixture)
{
    const cwGeoReference* geoReference = fixture->project->cavingRegion()->geoReference();
    const cwLinePlotManager* linePlot = fixture->rootData->linePlotManager();
    const bool solvedInFrame = tryWait(kAttachWaitMs, [geoReference, linePlot]() {
        if (!geoReference->hasCoordinateSystem()) {
            return false;
        }
        const QStringList lines = linePlot->driverSource().split(QLatin1Char('\n'));
        return lines.size() > 1 && lines.at(1).startsWith(QStringLiteral("*cs out "));
    });
    drainPipelines(fixture);
    return solvedInFrame;
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

//! \a Owner is a cwSurveyNode or a cwTrip; both carry an attachment.
template<typename Owner>
void chooseFixSystem(Owner* owner, const QString& system)
{
    auto centerline = owner->externalCenterline();
    centerline.setFixCoordinateSystem(system);
    owner->setExternalCenterline(centerline);
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

//! True when \a node, a node under it, or a trip of either lists an attached
//! row naming \a station.
inline bool subtreeListsAttachedFix(const cwSurveyNode* node, const QString& station)
{
    const QList<const cwSurveyNode*> holders = nodeAndDescendants(node);
    return std::any_of(holders.cbegin(), holders.cend(), [&station](const cwSurveyNode* holder) {
        const QList<cwAttachedFix> fixes = holder->listedAttachedFixes();
        return std::any_of(fixes.cbegin(), fixes.cend(), [&station](const cwAttachedFix& fix) {
            return fix.station == station;
        });
    });
}

//! The attached row that names \a station on \a node, a node under it, or a
//! trip of either: a file's fix is listed on the derived trip whose leaf
//! Survex block holds it, else on the derived node whose file holds it.
inline cwAttachedFix subtreeAttachedFixFor(const cwSurveyNode* node, const QString& station)
{
    for (const cwSurveyNode* holder : nodeAndDescendants(node)) {
        for (const cwAttachedFix& fix : holder->listedAttachedFixes()) {
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

//! The length, in meters, of the legs \a trip draws in \a geometry.
inline double tripLengthOf(const cwLinePlotGeometry::Result& geometry, const cwTrip* trip)
{
    REQUIRE(geometry.tripLengths.size() == geometry.tripUuids.size());
    const qsizetype tripIndex = geometry.tripUuids.indexOf(trip->id());
    REQUIRE(tripIndex >= 0);
    return geometry.tripLengths.at(tripIndex);
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

//! The longest gap between two ticks of a 16 ms timer on the main thread from
//! a queued call to \a start until \a isDone answers true at a tick: how long
//! the event loop is held while asynchronous work started by \a start lands.
inline qint64 longestMainThreadStallUntil(const std::function<void()>& start,
                                          const std::function<bool()>& isDone)
{
    using namespace mainThreadStall;
    QTimer frame;
    frame.setTimerType(Qt::PreciseTimer);
    frame.setInterval(kFrameIntervalMs);

    QElapsedTimer clock;
    qint64 lastTick = 0;
    qint64 longest = 0;
    int ticks = 0;
    bool started = false;
    bool done = false;
    QObject::connect(&frame, &QTimer::timeout, [&]() {
        const qint64 now = clock.elapsed();
        longest = std::max(longest, now - lastTick);
        lastTick = now;
        ++ticks;
        if (started && isDone()) {
            done = true;
        }
        if (ticks == kLeadInTicks) {
            longest = 0;
            QTimer::singleShot(0, [&]() {
                start();
                started = true;
            });
        }
    });

    clock.start();
    frame.start();
    REQUIRE(tryWait(kStallWaitMs, [&]() { return done; }));
    frame.stop();
    return longest;
}

//! Holds cwCavernRunner's cavern mutex from construction until release() or
//! destruction, so a test can tell whether code under test reached cavern:
//! anything that does blocks until the release. A worker runs cavern over a
//! driver that *includes a FIFO; cavern takes the mutex and then blocks
//! reading the FIFO, which stays open and empty until the release closes it.
//! A cavern run redirects stdout into its log, so a test asserts only once
//! nothing runs cavern (after release() and the settle behind it). POSIX
//! only: on other platforms kSupported is false and the holder holds nothing.
class CavernMutexHolder
{
public:
#ifdef Q_OS_UNIX
    static constexpr bool kSupported = true;
#else
    static constexpr bool kSupported = false;
#endif

    CavernMutexHolder()
    {
#ifdef Q_OS_UNIX
        if (!m_dir.isValid()) {
            return;
        }
        m_fifoPath = QDir(m_dir.path()).filePath(QStringLiteral("hold.svx"));
        if (::mkfifo(QFile::encodeName(m_fifoPath).constData(), kFifoMode) != 0) {
            return;
        }
        const QString driver = QDir(m_dir.path()).filePath(QStringLiteral("driver.svx"));
        overwriteFile(driver, QStringLiteral("*include \"%1\"\n").arg(m_fifoPath).toUtf8());
        const QString output = QDir(m_dir.path()).filePath(QStringLiteral("hold.3d"));
        m_worker = std::thread([this, driver, output]() {
            cwCavernRunner::run(driver, output);
            m_finished = true;
        });
        // The FIFO accepts a nonblocking writer only once cavern has opened
        // it for reading, and cavern opens it only under the mutex.
        tryWait(kWatcherWaitMs, [this]() { return openWriter(); });
#endif
    }

    ~CavernMutexHolder() { release(); }

    CavernMutexHolder(const CavernMutexHolder&) = delete;
    CavernMutexHolder& operator=(const CavernMutexHolder&) = delete;

    bool isHolding() const { return m_writer >= 0; }

    //! Lets the held cavern run read an empty file and return, which frees
    //! the mutex.
    void release()
    {
#ifdef Q_OS_UNIX
        if (!m_worker.joinable()) {
            return;
        }
        // A worker still queued on the mutex opens the FIFO later; keep
        // offering a writer until it has, or it never blocks at all.
        while (!isHolding() && !m_finished) {
            openWriter();
            std::this_thread::yield();
        }
        if (isHolding()) {
            ::close(m_writer);
            m_writer = kNoWriter;
        }
        m_worker.join();
#endif
    }

private:
    static constexpr int kNoWriter = -1;
    static constexpr int kFifoMode = 0600;

    bool openWriter()
    {
#ifdef Q_OS_UNIX
        if (!isHolding()) {
            m_writer = ::open(QFile::encodeName(m_fifoPath).constData(), O_WRONLY | O_NONBLOCK);
        }
#endif
        return isHolding();
    }

    QTemporaryDir m_dir;
    QString m_fifoPath;
    int m_writer = kNoWriter;
    std::atomic<bool> m_finished = false;
    std::thread m_worker;
};

//! A FIFO that holds whichever thread first opens it for reading, so a test
//! can keep a source sweep's hash in flight while it does something else.
//! Plant path() in an owner's stored fingerprint with stats that miss, and
//! the sweep reads it to settle the difference. Call waitUntilHeld() right
//! after the trigger: a reader that opened the FIFO before that call can
//! only be the trigger's own thread (the sweep ran inside the trigger), so
//! the hold lets it go at once and waitUntilHeld() answers false. release()
//! unlinks the FIFO and then closes it, so the reader sees an empty file
//! and the path is gone before any watcher could be pointed at it. POSIX
//! only: on other platforms kSupported is false and the hold holds nothing.
class SourceReadHold
{
public:
#ifdef Q_OS_UNIX
    static constexpr bool kSupported = true;
#else
    static constexpr bool kSupported = false;
#endif

    SourceReadHold()
    {
#ifdef Q_OS_UNIX
        if (!m_dir.isValid()) {
            return;
        }
        m_fifoPath = QDir(m_dir.path()).filePath(QStringLiteral("held.svx"));
        if (::mkfifo(QFile::encodeName(m_fifoPath).constData(), kFifoMode) != 0) {
            return;
        }
        m_writerThread = std::thread([this]() { holdFirstReader(); });
#endif
    }

    ~SourceReadHold() { release(); }

    SourceReadHold(const SourceReadHold&) = delete;
    SourceReadHold& operator=(const SourceReadHold&) = delete;

    //! The FIFO to plant in a stored fingerprint.
    QString path() const { return m_fifoPath; }

    //! The stored record that sends a sweep to read path(): its size and
    //! last-modified time match no file.
    cwExternalSourceRecord::SourceFileFingerprint staleRecord() const
    {
        cwExternalSourceRecord::SourceFileFingerprint record;
        record.path = m_fifoPath;
        record.size = kStaleSize;
        record.lastModified = kStaleLastModified;
        return record;
    }

    //! Spins the event loop until a reader other than the calling thread
    //! holds the FIFO open. False when the reader was the calling thread
    //! (it opened before this call) or none came within \a timeoutMs.
    bool waitUntilHeld(int timeoutMs = kWatcherWaitMs)
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_waiting = true;
        }
        return tryWait(timeoutMs, [this]() { return isHolding(); });
    }

    bool isHolding() const { return m_holding; }

    //! Lets the held reader see an empty file, with the path already gone.
    void release()
    {
#ifdef Q_OS_UNIX
        if (!m_writerThread.joinable()) {
            return;
        }
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_released = true;
        }
        m_releaseRequested.notify_all();
        // A writer still waiting for its first reader gets one here, so the
        // thread can see the release and stop.
        const int unblocker =
            ::open(QFile::encodeName(m_fifoPath).constData(), O_RDONLY | O_NONBLOCK);
        m_writerThread.join();
        if (unblocker >= 0) {
            ::close(unblocker);
        }
        QFile::remove(m_fifoPath);
#endif
    }

private:
    static constexpr int kNoWriter = -1;
    static constexpr int kFifoMode = 0600;
    static constexpr qint64 kStaleSize = 1;
    static constexpr qint64 kStaleLastModified = 1;

    void holdFirstReader()
    {
#ifdef Q_OS_UNIX
        // Blocks until a reader opens the FIFO.
        const int writer = ::open(QFile::encodeName(m_fifoPath).constData(), O_WRONLY);
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_waiting && !m_released) {
            m_holding = true;
            m_releaseRequested.wait(lock, [this]() { return m_released; });
        }
        m_holding = false;
        QFile::remove(m_fifoPath);
        if (writer != kNoWriter) {
            ::close(writer);
        }
#endif
    }

    QTemporaryDir m_dir;
    QString m_fifoPath;
    std::thread m_writerThread;
    std::mutex m_mutex;
    std::condition_variable m_releaseRequested;
    bool m_waiting = false;
    bool m_released = false;
    std::atomic<bool> m_holding = false;
};

#endif // EXTERNALCENTERLINETESTHELPERS_H
