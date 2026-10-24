/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The SB.1 gate (plans/SOURCE_BREADCRUMB_LOCAL_STORE_PLAN.md): an owner's
// breadcrumb and source fingerprint live in one protobuf file per owner under
// <project root>/.cw_local/external-sources/, which git ignores and the
// bundle carries, and QSettings holds none of it.

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"
#include "cavewhere.pb.h"

// libgit2
#include "git2.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtCore/qscopeguard.h>

// Std
#include <memory>
#include <optional>

namespace {

using SourceFingerprint = cwExternalSourceSettings::SourceFingerprint;
using AttachFuture = QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>;

constexpr int kLargeBooks = 10;
constexpr int kLargeSurveysPerBook = 130;
constexpr int kSmallBooks = 2;
constexpr int kSmallSurveysPerBook = 3;
constexpr int kShotsPerSurvey = 3;
constexpr int kFixEvery = 25;
// One record per .SRV file plus one for the .wpj entry.
constexpr int kEntryFileCount = 1;

// Thirteen hundred copies, the scan and the fill under Debug + ASAN on a
// loaded machine.
constexpr int kFillWaitMs = 300000;

constexpr int kGrowthBooks = 2;
constexpr int kGrowthSmallSurveysPerBook = 130;
constexpr int kGrowthLargeSurveysPerBook = 520;
constexpr double kMaxGrowthRatio = 5.0;

constexpr int kAttachedTripCount = 3;

const QString kParentDir = QStringLiteral("..");
const QString kLocalDirPattern = QStringLiteral(".cw_local/");

void setTestAccount(cwRootData* rootData)
{
    rootData->account()->setName(QStringLiteral("Record Store Tester"));
    rootData->account()->setEmail(QStringLiteral("record.store.tester@example.com"));
}

//! A saved .cwproj whose saves commit, so git status and discard have a HEAD.
std::unique_ptr<SavedProjectFixture> makeCommittedProject(const QString& projectFileBase)
{
    auto fixture = makeNewProject(QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    setTestAccount(fixture->rootData.get());
    const QString projectPath =
        QDir(fixture->tempDir.path()).filePath(projectFileBase + QStringLiteral(".cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    drainPipelines(fixture.get());
    return fixture;
}

cwSurveyNode* regionRoot(SavedProjectFixture* fixture)
{
    return fixture->project->cavingRegion()->rootNode();
}

QString writeWalls(const QTemporaryDir& dir, int books, int surveysPerBook)
{
    return writeWallsProject(tempSubdir(dir, QStringLiteral("walls")), books, surveysPerBook,
                             kShotsPerSurvey, kFixEvery);
}

cwSurveyNode* attachWalls(SavedProjectFixture* fixture, const QString& entry)
{
    auto future = managerOf(fixture)->attachSource(regionRoot(fixture), entry);
    REQUIRE(AsyncFuture::waitForFinished(future, kFillWaitMs));
    INFO("attach: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    cwSurveyNode* root = future.result().value().root.data();
    REQUIRE(root != nullptr);
    drainPipelines(fixture);
    return root;
}

cwSurveyNode* nodeWithId(const cwProject* project, const QUuid& id)
{
    for (cwSurveyNode* node : project->cavingRegion()->rootNode()->allNodes()) {
        if (node->id() == id) {
            return node;
        }
    }
    return nullptr;
}

std::optional<CavewhereProto::ExternalSourceRecord> readRecord(const QString& path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    CavewhereProto::ExternalSourceRecord record;
    if (!record.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size()))) {
        return std::nullopt;
    }
    return record;
}

QString fromStd(const std::string& text)
{
    return QString::fromStdString(text);
}

//! Checks \a record against the in-memory \a fingerprint and \a breadcrumb:
//! one absolute canonical base and, per dependency in order, a path relative
//! to it with the same stats and hash.
void checkRecordMatches(const CavewhereProto::ExternalSourceRecord& record,
                        const QString& breadcrumb,
                        const SourceFingerprint& fingerprint)
{
    CHECK(fromStd(record.breadcrumb_path()) == breadcrumb);

    const QString baseDir = fromStd(record.base_dir());
    REQUIRE_FALSE(baseDir.isEmpty());
    CHECK(QDir::isAbsolutePath(baseDir));
    CHECK(QFileInfo(baseDir).canonicalFilePath() == baseDir);

    REQUIRE(record.files_size() == fingerprint.files.size());
    for (int i = 0; i < record.files_size(); ++i) {
        const auto& file = record.files(i);
        const auto& expected = fingerprint.files.at(i);
        const QString relative = fromStd(file.path());
        INFO("record " << i << " path: " << relative.toStdString());
        CHECK_FALSE(QDir::isAbsolutePath(relative));
        CHECK_FALSE(relative.split(QLatin1Char('/')).contains(kParentDir));
        CHECK(QDir(baseDir).filePath(relative) == expected.path);
        CHECK(file.size() == expected.size);
        CHECK(file.last_modified() == expected.lastModified);
        CHECK(fromStd(file.sha256()) == expected.sha256);
    }
}

using RepositoryPtr = std::unique_ptr<git_repository, decltype(&git_repository_free)>;

RepositoryPtr openRepository(const QString& repositoryPath)
{
    git_repository* repo = nullptr;
    REQUIRE(git_repository_open(&repo, repositoryPath.toLocal8Bit().constData()) == GIT_OK);
    return RepositoryPtr(repo, &git_repository_free);
}

//! Whether git ignores \a relativePath in the repository at \a repositoryPath.
bool gitIgnores(const QString& repositoryPath, const QString& relativePath)
{
    const RepositoryPtr repo = openRepository(repositoryPath);
    int ignored = 0;
    REQUIRE(git_ignore_path_is_ignored(&ignored, repo.get(), relativePath.toUtf8().constData())
            == GIT_OK);
    return ignored == 1;
}

//! The working-tree and index paths git status reports, untracked ones included.
QStringList gitStatusPaths(const QString& repositoryPath)
{
    const RepositoryPtr repo = openRepository(repositoryPath);

    git_status_options options = GIT_STATUS_OPTIONS_INIT;
    options.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    options.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
    git_status_list* list = nullptr;
    REQUIRE(git_status_list_new(&list, repo.get(), &options) == GIT_OK);
    auto listGuard = qScopeGuard([list]() { git_status_list_free(list); });

    QStringList paths;
    const size_t count = git_status_list_entrycount(list);
    for (size_t i = 0; i < count; ++i) {
        const git_status_entry* entry = git_status_byindex(list, i);
        const git_diff_delta* delta =
            entry->index_to_workdir != nullptr ? entry->index_to_workdir : entry->head_to_index;
        if (delta != nullptr && delta->new_file.path != nullptr) {
            paths.append(QString::fromUtf8(delta->new_file.path));
        }
    }
    return paths;
}

//! Whether HEAD's tree in the repository at \a repositoryPath holds \a relativePath.
bool headHolds(const QString& repositoryPath, const QString& relativePath)
{
    const RepositoryPtr repo = openRepository(repositoryPath);
    git_object* object = nullptr;
    const QByteArray spec = (QStringLiteral("HEAD:") + relativePath).toUtf8();
    const int found = git_revparse_single(&object, repo.get(), spec.constData());
    git_object_free(object);
    return found == GIT_OK;
}

QString readGitExclude(const QDir& root)
{
    QFile file(root.filePath(QStringLiteral(".git/info/exclude")));
    if (!file.open(QFile::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

//! The max event-loop gap from the attach call until its future finishes.
qint64 attachStall(int surveysPerBook)
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kGrowthBooks, surveysPerBook);
    auto fixture = makeSavedProject(QStringLiteral("record-growth-%1").arg(surveysPerBook),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    drainPipelines(fixture.get());

    AttachFuture future;
    const qint64 stall = longestMainThreadStallUntil(
        [&]() { future = managerOf(fixture.get())->attachSource(regionRoot(fixture.get()), entry); },
        [&]() { return future.isFinished(); });
    INFO("attach: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());
    return stall;
}

} // namespace

TEST_CASE("Attaching a large Walls project records its source under .cw_local and nothing in "
          "QSettings",
          "[Attach][Breadcrumb]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kLargeBooks, kLargeSurveysPerBook);

    auto fixture = makeCommittedProject(QStringLiteral("record-large"));
    clearRetiredBreadcrumbSettings();

    cwSurveyNode* root = attachWalls(fixture.get(), entry);
    const QUuid rootId = root->id();
    REQUIRE(fixture->project->save());
    drainPipelines(fixture.get());

    CHECK(retiredBreadcrumbSettingsKeyCount() == 0);

    const SourceFingerprint fingerprint = fixture->settings()->fingerprint(rootId);
    REQUIRE(fingerprint.files.size() == kLargeBooks * kLargeSurveysPerBook + kEntryFileCount);

    const QString recordPath = externalSourceRecordPath(fixture->project, rootId);

    // Git ignores the store, so it neither shows as a change nor lands in a commit.
    const QDir projectRoot = projectRootOf(fixture->project);
    const QString recordRelative = projectRoot.relativeFilePath(recordPath);
    CHECK(readGitExclude(projectRoot).contains(kLocalDirPattern));
    CHECK(gitIgnores(projectRoot.absolutePath(), recordRelative));
    const QStringList statusPaths = gitStatusPaths(projectRoot.absolutePath());
    INFO("status: " << statusPaths.join(QStringLiteral(", ")).toStdString());
    CHECK(statusPaths.filter(externalSourceStore::kLocalDirName).isEmpty());
    CHECK_FALSE(headHolds(projectRoot.absolutePath(), recordRelative));

    REQUIRE(QFileInfo::exists(recordPath));
    const auto record = readRecord(recordPath);
    REQUIRE(record.has_value());
    checkRecordMatches(*record, fixture->settings()->breadcrumbPath(rootId), fingerprint);
    CHECK(fromStd(record->breadcrumb_path()) == QFileInfo(entry).absoluteFilePath());
}

TEST_CASE("A reopened project reads its breadcrumb and fingerprint from its own record",
          "[Attach][Breadcrumb]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kSmallBooks, kSmallSurveysPerBook);

    auto fixture = makeCommittedProject(QStringLiteral("record-reopen"));
    const QString projectPath = fixture->project->filename();
    const QUuid rootId = attachWalls(fixture.get(), entry)->id();
    const QString breadcrumb = fixture->settings()->breadcrumbPath(rootId);
    const SourceFingerprint fingerprint = fixture->settings()->fingerprint(rootId);
    REQUIRE_FALSE(fingerprint.isEmpty());
    const QString originalRoot = projectRootOf(fixture->project).absolutePath();

    SECTION("a .cwproj directory") {
        closeAndOpen(fixture.get(), projectPath);
        CHECK(projectRootOf(fixture->project).absolutePath() == originalRoot);
    }

    SECTION("a bundled .cw, extracted afresh on open") {
        const QString bundlePath =
            QDir(fixture->tempDir.path()).filePath(QStringLiteral("record-reopen.cw"));
        REQUIRE(fixture->project->saveAs(bundlePath));
        closeAndOpen(fixture.get(), bundlePath);
        REQUIRE(fixture->project->fileType() == cwProject::BundledGitFileType);
        CHECK(projectRootOf(fixture->project).absolutePath() != originalRoot);
    }

    REQUIRE(nodeWithId(fixture->project, rootId) != nullptr);
    CHECK(QFileInfo::exists(externalSourceRecordPath(fixture->project, rootId)));
    CHECK(fixture->settings()->hasBreadcrumb(rootId));
    CHECK(fixture->settings()->breadcrumbPath(rootId) == breadcrumb);
    CHECK(fixture->settings()->fingerprint(rootId) == fingerprint);
}

TEST_CASE("Save As to a new .cwproj carries the record and the original keeps its own",
          "[Attach][Breadcrumb]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kSmallBooks, kSmallSurveysPerBook);

    auto fixture = makeCommittedProject(QStringLiteral("record-original"));
    const QUuid rootId = attachWalls(fixture.get(), entry)->id();
    const QString originalRecord = externalSourceRecordPath(fixture->project, rootId);
    CHECK(QFileInfo::exists(originalRecord));

    const QString copyPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("record-copy.cwproj"));
    REQUIRE(fixture->project->saveAs(copyPath));
    drainPipelines(fixture.get());
    const QString copyRecord = externalSourceRecordPath(fixture->project, rootId);
    REQUIRE(copyRecord != originalRecord);
    CHECK(QFileInfo::exists(copyRecord));
    CHECK(QFileInfo::exists(originalRecord));

    // The copy is now the open project; forgetting its source leaves the
    // original's record alone.
    fixture->settings()->clearBreadcrumb(rootId);
    drainPipelines(fixture.get());
    CHECK_FALSE(QFileInfo::exists(copyRecord));
    CHECK(QFileInfo::exists(originalRecord));
}

TEST_CASE("Attaching in a new unsaved project records the source in its temporary root, and "
          "the first Save As takes it along",
          "[Attach][Breadcrumb]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kSmallBooks, kSmallSurveysPerBook);

    auto fixture = makeNewProject(QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    setTestAccount(fixture->rootData.get());
    REQUIRE(fixture->project->isTemporaryProject());
    const QUuid rootId = attachWalls(fixture.get(), entry)->id();
    const QString breadcrumb = fixture->settings()->breadcrumbPath(rootId);
    const SourceFingerprint fingerprint = fixture->settings()->fingerprint(rootId);
    CHECK(QFileInfo::exists(externalSourceRecordPath(fixture->project, rootId)));

    const QString projectPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("record-first-save.cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    drainPipelines(fixture.get());
    CHECK(QFileInfo::exists(externalSourceRecordPath(fixture->project, rootId)));

    closeAndOpen(fixture.get(), fixture->project->filename());
    CHECK(fixture->settings()->breadcrumbPath(rootId) == breadcrumb);
    CHECK(fixture->settings()->fingerprint(rootId) == fingerprint);
}

TEST_CASE("Deleting a source root removes its record", "[Attach][Breadcrumb]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kSmallBooks, kSmallSurveysPerBook);

    auto fixture = makeCommittedProject(QStringLiteral("record-delete"));
    cwSurveyNode* root = attachWalls(fixture.get(), entry);
    const QUuid rootId = root->id();
    const QString recordPath = externalSourceRecordPath(fixture->project, rootId);
    REQUIRE(QFileInfo::exists(recordPath));

    auto future = managerOf(fixture.get())->deleteSourceRoot(root);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    CHECK_FALSE(QFileInfo::exists(recordPath));
    CHECK_FALSE(fixture->settings()->hasBreadcrumb(rootId));
}

TEST_CASE("clearBreadcrumb removes the record and the owner stays without one after reopen",
          "[Attach][Breadcrumb]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kSmallBooks, kSmallSurveysPerBook);

    auto fixture = makeCommittedProject(QStringLiteral("record-clear"));
    const QString projectPath = fixture->project->filename();
    const QUuid rootId = attachWalls(fixture.get(), entry)->id();
    const QString recordPath = externalSourceRecordPath(fixture->project, rootId);
    REQUIRE(QFileInfo::exists(recordPath));

    fixture->settings()->clearBreadcrumb(rootId);
    drainPipelines(fixture.get());
    CHECK_FALSE(QFileInfo::exists(recordPath));

    closeAndOpen(fixture.get(), projectPath);
    REQUIRE(nodeWithId(fixture->project, rootId) != nullptr);
    CHECK_FALSE(fixture->settings()->hasBreadcrumb(rootId));
    CHECK(fixture->settings()->fingerprint(rootId).isEmpty());
}

TEST_CASE("Removing a cave forgets every attached trip's record in one change",
          "[Attach][Breadcrumb]")
{
    auto fixture = makeCommittedProject(QStringLiteral("record-remove-cave"));
    cwCave* cave = fixture->cave;
    for (int i = 1; i < kAttachedTripCount; ++i) {
        cave->addTrip();
        cave->trip(i)->setName(QStringLiteral("AttachedTrip%1").arg(i));
    }
    REQUIRE(cave->tripCount() == kAttachedTripCount);

    const QString source = fixturePath(QStringLiteral("survex_simple.svx"));
    QStringList recordPaths;
    for (int i = 0; i < kAttachedTripCount; ++i) {
        attachThroughManager(fixture.get(), cave->trip(i), source);
        recordPaths.append(externalSourceRecordPath(fixture->project, cave->trip(i)->id()));
    }
    drainPipelines(fixture.get());
    for (const QString& path : std::as_const(recordPaths)) {
        INFO("record: " << path.toStdString());
        CHECK(QFileInfo::exists(path));
    }

    QSignalSpy changed(fixture->settings(), &cwExternalSourceSettings::breadcrumbsChanged);
    fixture->project->cavingRegion()->removeCave(0);
    drainPipelines(fixture.get());

    // One change for the whole deletion: each emission costs every listener
    // a pass over every owner.
    CHECK(changed.count() == 1);
    for (const QString& path : std::as_const(recordPaths)) {
        INFO("record: " << path.toStdString());
        CHECK_FALSE(QFileInfo::exists(path));
    }
}

TEST_CASE("Discarding changes leaves the source records in place", "[Attach][Breadcrumb]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWalls(sourceDir, kSmallBooks, kSmallSurveysPerBook);

    auto fixture = makeCommittedProject(QStringLiteral("record-discard"));
    const QUuid rootId = attachWalls(fixture.get(), entry)->id();
    REQUIRE(fixture->project->save());
    drainPipelines(fixture.get());
    const QString recordPath = externalSourceRecordPath(fixture->project, rootId);
    REQUIRE(QFileInfo::exists(recordPath));

    fixture->trip->setName(QStringLiteral("EditedTrip"));
    drainPipelines(fixture.get());
    REQUIRE(fixture->project->modified());

    fixture->project->discardChanges();
    fixture->project->waitForDiscardToFinish();
    drainPipelines(fixture.get());

    CHECK(QFileInfo::exists(recordPath));
    CHECK(fixture->settings()->hasBreadcrumb(rootId));
}

TEST_CASE("The attach's longest event-loop stall grows no faster than its source file count",
          "[Attach][Breadcrumb]")
{
    const qint64 small = attachStall(kGrowthSmallSurveysPerBook);
    const qint64 large = attachStall(kGrowthLargeSurveysPerBook);
    INFO("longest stall: " << small << " ms at " << kGrowthSmallSurveysPerBook
         << " surveys per book, " << large << " ms at " << kGrowthLargeSurveysPerBook);
    REQUIRE(small > 0);
    CHECK(static_cast<double>(large) / static_cast<double>(small) <= kMaxGrowthRatio);
}
