/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Attaching an external centerline before the project has a durable
// home. attach() used to reject cwSaveLoad::isTemporaryProject(),
// because the enqueueExternalCenterline* jobs early-returned on the
// same flag. These tests pin the behaviour we want instead: attach
// works on any project, and the attachment survives whichever write
// the user reaches for first (Save on a bundled .cw, Save As on a
// brand-new project).
//
// "Temporary" means two different things here, which is what made the
// original guard wrong. cwSaveLoad::isTemporaryProject() asks "is the
// working tree a QTemporaryDir?" — true for a brand-new project AND
// for a bundled .cw saved this session. cwProject::isTemporaryProject()
// asks "is there somewhere durable to save back to?" — false for a
// bundle, because the archive path counts. The blanket guard therefore
// blocked bundles too, not just unsaved projects. Both layers are
// asserted directly rather than assumed.

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwErrorListModel.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>

// Std
#include <memory>

namespace {

// Headroom for the scan worker plus the save-job queue on busy CI; the
// attach itself finishes in milliseconds.
constexpr int kAttachWaitMs = 10000;

const QString kEntryFile = QStringLiteral("survex_simple.svx");

struct ProjectFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwTrip* trip = nullptr;

    cwSaveLoad* saveLoad() const { return project->saveLoad(); }
    cwExternalSourceSettings* settings() const { return rootData->externalSourceSettings(); }
    QDir attachmentDir() const { return saveLoad()->externalCenterlineDir(trip); }
};

// A project with one cave and one trip, never saved. cwProject's
// constructor runs newProject(), so this is the state the user is in
// immediately after launching CaveWhere.
std::unique_ptr<ProjectFixture> makeUnsavedProject()
{
    auto fixture = std::make_unique<ProjectFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();
    fixture->rootData->account()->setName(QStringLiteral("Attach Temporary Tester"));
    fixture->rootData->account()->setEmail(QStringLiteral("attach.temporary@example.com"));

    auto region = fixture->project->cavingRegion();
    region->addCave();
    cwCave* cave = region->cave(0);
    cave->setName(QStringLiteral("TempCave"));
    cave->addTrip();
    fixture->trip = cave->trip(0);
    fixture->trip->setName(QStringLiteral("TempTrip"));

    return fixture;
}

// Same, then saved out as a bundled .cw so the project is backed by an
// extraction tree rather than a user-visible directory.
std::unique_ptr<ProjectFixture> makeBundledProject(const QString& fileBase)
{
    auto fixture = makeUnsavedProject();

    const QString bundlePath =
        QDir(fixture->tempDir.path()).filePath(fileBase + QStringLiteral(".cw"));
    REQUIRE(fixture->project->saveAs(bundlePath));
    fixture->project->waitSaveToFinish();
    REQUIRE(fixture->project->fileType() == cwProject::BundledGitFileType);

    // Drain the queued fileSaved delivery so modified() has settled
    // before a test takes a baseline.
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    return fixture;
}

QString datasetPath(const QString& fileName)
{
    return testcasesDatasetSourcePath(
        QStringLiteral("external-centerlines/%1").arg(fileName));
}

Monad::Result<cwExternalCenterlineAttach::AttachReport> runAttach(
    ProjectFixture* fixture, const QString& sourceFile)
{
    auto future = cwExternalCenterlineAttach::attach(
        fixture->trip, sourceFile, fixture->saveLoad(), fixture->settings());
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    fixture->project->waitSaveToFinish();
    const auto result = future.result();
    if (result.hasError()) {
        UNSCOPED_INFO("attach error: " << result.errorMessage().toStdString());
    }
    return result;
}

// Reopens a project file and returns the entry file recorded on its
// first trip, plus whether the copied centerline is present on disk
// with the expected bytes.
struct ReloadedAttachment {
    QString entryFile;
    bool fileExists = false;
    QByteArray contents;
};

// Loads a project artifact where it was written and returns the owning
// cwRootData - keep it alive for as long as you read from the project.
//
// Deliberately NOT fileToProject(): that helper copies the single file
// it is handed into a temp folder, which silently strips a .cwproj of
// everything beside its descriptor. Loading in place is the only way to
// see what actually landed on disk (for a bundle, inside the archive).
std::unique_ptr<cwRootData> openProjectInPlace(const QString& projectPath)
{
    REQUIRE(QFileInfo::exists(projectPath));

    auto rootData = std::make_unique<cwRootData>();
    cwProject* project = rootData->project();
    project->loadOrConvert(projectPath);
    project->waitLoadToFinish();
    REQUIRE(project->errorModel()->size() == 0);
    return rootData;
}

ReloadedAttachment reloadAttachment(const QString& projectPath)
{
    auto rootData = openProjectInPlace(projectPath);
    cwProject* reloaded = rootData->project();
    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    REQUIRE(reloaded->cavingRegion()->cave(0)->tripCount() == 1);

    cwTrip* trip = reloaded->cavingRegion()->cave(0)->trip(0);

    ReloadedAttachment result;
    result.entryFile = trip->externalCenterline().entryFile();

    const QDir dir = reloaded->saveLoad()->externalCenterlineDir(trip);
    const QString path = dir.absoluteFilePath(kEntryFile);
    result.fileExists = QFileInfo::exists(path);
    if (result.fileExists) {
        result.contents = fileContents(path);
    }
    return result;
}

} // namespace

//
// Premise: what "temporary" actually means
//

TEST_CASE("a bundled .cw is temporary at the cwSaveLoad layer and durable at the cwProject layer",
          "[Attach][Temporary]")
{
    // Two layers, two answers, and the difference is easy to get wrong
    // by inspection. Save As to a bundle re-zips the working tree but
    // leaves it a QTemporaryDir, so cwSaveLoad still reports temporary;
    // cwProject masks that via LoadedFromBundledArchive because there is
    // a durable archive path to save back to.
    //
    // Anything gating on "can I write a file that will survive?" must
    // therefore not consult cwSaveLoad::isTemporaryProject() — that was
    // the attach bug.
    auto fixture = makeBundledProject(QStringLiteral("bundle-not-temporary"));
    CHECK(fixture->saveLoad()->isTemporaryProject());
    CHECK_FALSE(fixture->project->isTemporaryProject());
}

TEST_CASE("a bundled .cw disagrees with itself about being temporary across a reopen",
          "[Attach][Temporary]")
{
    // KNOWN DIVERGENCE, pinned rather than fixed. Save As to a bundle
    // leaves cwSaveLoad temporary (the working tree stays a
    // QTemporaryDir; only cwProject masks it via
    // LoadedFromBundledArchive), while loading the same bundle back
    // routes through cwSaveLoad::load() and clears the flag. Identical
    // project on disk, two different answers.
    //
    // Attach no longer depends on this — the enqueueExternalCenterline*
    // jobs run regardless — but the same flag still gates
    // enqueueProjectRenameJobs and the orphan/conflict cleanups, so a
    // rename inside a saved-as bundle takes a different path than the
    // same rename after a reopen. Fixing that means auditing those
    // rename paths, which is out of scope here.
    //
    // This asserts today's behaviour so the suite stays green; flip both
    // sides to equal when the divergence is closed.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString bundlePath = QDir(tempDir.path()).filePath(QStringLiteral("state-parity.cw"));

    bool temporaryAfterSaveAs = false;
    {
        auto fixture = makeUnsavedProject();
        REQUIRE(fixture->project->saveAs(bundlePath));
        fixture->project->waitSaveToFinish();
        temporaryAfterSaveAs = fixture->saveLoad()->isTemporaryProject();
    }

    auto reopenedRoot = openProjectInPlace(bundlePath);
    const bool temporaryAfterReopen =
        reopenedRoot->project()->saveLoad()->isTemporaryProject();

    CHECK(temporaryAfterSaveAs);
    CHECK_FALSE(temporaryAfterReopen);
}

TEST_CASE("an unsaved project is temporary but already materialized on disk",
          "[Attach][Temporary]")
{
    // newProject() creates the temp dir, mkpaths dataRoot, sets the
    // filename and writes the project. Nothing about the temporary
    // state stops a file from being copied in — which is why the attach
    // guard is policy rather than a physical limit.
    auto fixture = makeUnsavedProject();

    REQUIRE(fixture->saveLoad()->isTemporaryProject());

    const QString projectFile = fixture->saveLoad()->fileName();
    REQUIRE_FALSE(projectFile.isEmpty());

    const QDir root(QFileInfo(projectFile).absolutePath());
    CHECK(root.exists());

    // The attachment dir resolves under that root, so the destination
    // path an attach would write to is well-defined before any save.
    const QDir attachDir = fixture->attachmentDir();
    CHECK_FALSE(attachDir.absolutePath().isEmpty());
    CHECK(attachDir.absolutePath().startsWith(root.absolutePath()));
}

//
// Bundled .cw
//

TEST_CASE("attach works on a bundled .cw project", "[Attach][Temporary]")
{
    auto fixture = makeBundledProject(QStringLiteral("bundle-attach"));
    REQUIRE_FALSE(fixture->project->modified());

    const auto result = runAttach(fixture.get(), datasetPath(kEntryFile));

    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().persisted.entryFile() == kEntryFile);
    CHECK(fixture->trip->externalCenterline().entryFile() == kEntryFile);
    CHECK(QFileInfo::exists(fixture->attachmentDir().absoluteFilePath(kEntryFile)));
    CHECK(fixture->project->modified());
}

TEST_CASE("an external centerline attached to a bundled .cw survives save and reopen",
          "[Attach][Temporary]")
{
    auto fixture = makeBundledProject(QStringLiteral("bundle-attach-save"));
    const QString projectPath = fixture->project->filename();
    const QString source = datasetPath(kEntryFile);
    const QByteArray sourceBytes = fileContents(source);

    REQUIRE_FALSE(runAttach(fixture.get(), source).hasError());

    REQUIRE(fixture->project->save());
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // Saving a bundle re-zips the extraction tree. The copied
    // centerline has to be inside the archive, not left behind in the
    // abandoned extraction dir.
    const auto reloaded = reloadAttachment(projectPath);
    CHECK(reloaded.entryFile == kEntryFile);
    CHECK(reloaded.fileExists);
    CHECK(reloaded.contents == sourceBytes);
}

//
// Never-saved project
//

TEST_CASE("attach works on a project that has never been saved", "[Attach][Temporary]")
{
    auto fixture = makeUnsavedProject();
    REQUIRE(fixture->saveLoad()->isTemporaryProject());

    const QString source = datasetPath(kEntryFile);
    const auto result = runAttach(fixture.get(), source);

    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().persisted.entryFile() == kEntryFile);
    CHECK(fixture->trip->externalCenterline().entryFile() == kEntryFile);

    // The copy must actually land — a silently no-op'd enqueue would
    // leave the model claiming an attachment that has no file.
    const QString copied = fixture->attachmentDir().absoluteFilePath(kEntryFile);
    REQUIRE(QFileInfo::exists(copied));
    CHECK(fileContents(copied) == fileContents(source));

    // Source memory is written for a temporary project too, so the
    // Update button works before the first save.
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()) == source);
}

TEST_CASE("attaching to an unsaved project marks it modified", "[Attach][Temporary]")
{
    // Without this the attach is discardable: the user attaches, quits,
    // and gets no save prompt because nothing flipped the dirty bit.
    //
    // A never-saved project with a cave already reads modified, so the
    // baseline can't be cleared here — this pins that the attach does
    // not *clear* it, which is the property that keeps the quit prompt
    // honest. The dirty-bit write itself is covered on a saved project
    // by "[Attach][Orchestrator]".
    auto fixture = makeUnsavedProject();

    REQUIRE_FALSE(runAttach(fixture.get(), datasetPath(kEntryFile)).hasError());

    CHECK(fixture->project->modified());
}

TEST_CASE("an external centerline attached before the first save survives Save As to a bundle",
          "[Attach][Temporary]")
{
    auto fixture = makeUnsavedProject();
    const QString source = datasetPath(kEntryFile);
    const QByteArray sourceBytes = fileContents(source);

    REQUIRE_FALSE(runAttach(fixture.get(), source).hasError());

    // Save As moves the whole project root out of the temp dir, so the
    // attachment should travel with it without special handling.
    const QString bundlePath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("saved-from-temp.cw"));
    REQUIRE(fixture->project->saveAs(bundlePath));
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // cwSaveLoad still reports temporary here (the bundle working tree
    // stays a QTemporaryDir); cwProject is the layer that knows there is
    // a durable archive to save back to.
    CHECK_FALSE(fixture->project->isTemporaryProject());
    CHECK(QFileInfo::exists(fixture->attachmentDir().absoluteFilePath(kEntryFile)));

    const auto reloaded = reloadAttachment(bundlePath);
    CHECK(reloaded.entryFile == kEntryFile);
    CHECK(reloaded.fileExists);
    CHECK(reloaded.contents == sourceBytes);
}

TEST_CASE("an external centerline attached before the first save survives Save As to a .cwproj",
          "[Attach][Temporary]")
{
    // The directory format takes the same transferProjectTo path as the
    // bundle but skips the re-zip, so it can fail independently.
    auto fixture = makeUnsavedProject();
    const QString source = datasetPath(kEntryFile);
    const QByteArray sourceBytes = fileContents(source);

    REQUIRE_FALSE(runAttach(fixture.get(), source).hasError());

    REQUIRE(fixture->project->saveAs(
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("saved-from-temp.cwproj"))));
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // .cwproj is a directory format: the descriptor lands inside the new
    // project dir rather than at the path handed to saveAs, so reopen by
    // the filename the project settled on.
    const auto reloaded = reloadAttachment(fixture->project->filename());
    CHECK(reloaded.entryFile == kEntryFile);
    CHECK(reloaded.fileExists);
    CHECK(reloaded.contents == sourceBytes);
}

TEST_CASE("a multi-file attachment closure survives Save As from an unsaved project",
          "[Attach][Temporary]")
{
    // The single-file case can pass while the dependency closure is
    // dropped — reconcile enqueues one copy job per dependency, and
    // every one of them goes through the temporary-project guard.
    auto fixture = makeUnsavedProject();
    // survex_nested.svx -> entrance.svx -> passage.svx
    const QString source = datasetPath(QStringLiteral("survex_nested.svx"));

    const auto result = runAttach(fixture.get(), source);
    REQUIRE_FALSE(result.hasError());
    REQUIRE(result.value().scan.dependencies.size() == 3);

    // Snapshot each dependency's source bytes now, before Save As, so the
    // reopened copy can be byte-compared rather than merely counted.
    const QStringList dependencies = result.value().scan.dependencies;
    QHash<QString, QByteArray> sourceBytesByName;
    for (const QString& dependency : dependencies) {
        sourceBytesByName.insert(QFileInfo(dependency).fileName(), fileContents(dependency));
    }

    REQUIRE(fixture->project->saveAs(
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("closure-from-temp.cw"))));
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // Check the closure in the REOPENED archive, not fixture->attachmentDir():
    // saveBundledArchive zips the working tree that attach already populated
    // and does not re-derive it, so asserting against the working tree would
    // pass even if the re-zip dropped a dependency. Only a reopen proves the
    // whole closure landed inside the .cw.
    auto reopenedRoot = openProjectInPlace(fixture->project->filename());
    cwProject* reopened = reopenedRoot->project();
    REQUIRE(reopened->cavingRegion()->caveCount() == 1);
    REQUIRE(reopened->cavingRegion()->cave(0)->tripCount() == 1);

    cwTrip* reopenedTrip = reopened->cavingRegion()->cave(0)->trip(0);
    const QDir attachDir = reopened->saveLoad()->externalCenterlineDir(reopenedTrip);
    for (auto it = sourceBytesByName.constBegin(); it != sourceBytesByName.constEnd(); ++it) {
        const QString path = attachDir.absoluteFilePath(it.key());
        INFO("dependency: " << it.key().toStdString());
        REQUIRE(QFileInfo::exists(path));
        CHECK(fileContents(path) == it.value());
    }
}

