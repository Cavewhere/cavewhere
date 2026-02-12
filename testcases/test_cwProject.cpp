//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

//Our includes
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "TestHelper.h"
#include "cwImageProvider.h"
#include "cwRootData.h"
#include "cwSurveyNoteModel.h"
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwImageDatabase.h"
#include "cwPDFSettings.h"
#include "cwNote.h"
#include "cwSaveLoad.h"
#include "cwTeam.h"
#include "cwNoteLiDAR.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwImageResolution.h"
#include "cwUnits.h"
#include "cwScrap.h"
#include "cwSignalSpy.h"
#include "cwRemoteRepositoryCloner.h"
#include "cwRepositoryModel.h"
#include "GitRepository.h"
#include "GitFutureWatcher.h"
#include "asyncfuture.h"
#include "LfsStore.h"
#include "LfsServer.h"

//Qt includes
#include <QtCore/qdiriterator.h>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlResult>
#include <QSqlRecord>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QThread>
#include <QtCore/qscopeguard.h>

//libgit2
#include "git2.h"

#include <algorithm>
#include <optional>
#include <google/protobuf/util/json_util.h>
#include "cavewhere.pb.h"

namespace {
cwScrap* firstScrap(cwProject* project) {
    auto region = project->cavingRegion();
    REQUIRE(region != nullptr);
    REQUIRE(region->caveCount() > 0);
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    REQUIRE(cave->tripCount() > 0);
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    auto notes = trip->notes()->notes();
    REQUIRE(!notes.isEmpty());
    auto note = notes.first();
    REQUIRE(note != nullptr);
    REQUIRE(note->scraps().size() > 0);
    return note->scrap(0);
}

QString readGitExclude(const QDir& dir)
{
    QFile file(dir.filePath(QStringLiteral(".git/info/exclude")));
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QByteArray readBlobFromHead(git_repository* repo, const QString& path)
{
    git_reference* head = nullptr;
    if (git_repository_head(&head, repo) != GIT_OK) {
        return QByteArray();
    }
    auto headGuard = qScopeGuard([&head]() {
        if (head) {
            git_reference_free(head);
        }
    });

    const git_oid* headId = git_reference_target(head);
    if (!headId) {
        return QByteArray();
    }

    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, repo, headId) != GIT_OK) {
        return QByteArray();
    }
    auto commitGuard = qScopeGuard([&commit]() {
        if (commit) {
            git_commit_free(commit);
        }
    });

    git_tree* tree = nullptr;
    if (git_commit_tree(&tree, commit) != GIT_OK) {
        return QByteArray();
    }
    auto treeGuard = qScopeGuard([&tree]() {
        if (tree) {
            git_tree_free(tree);
        }
    });

    git_tree_entry* entry = nullptr;
    if (git_tree_entry_bypath(&entry, tree, path.toLocal8Bit().constData()) != GIT_OK) {
        return QByteArray();
    }
    auto entryGuard = qScopeGuard([&entry]() {
        if (entry) {
            git_tree_entry_free(entry);
        }
    });

    git_blob* blob = nullptr;
    if (git_blob_lookup(&blob, repo, git_tree_entry_id(entry)) != GIT_OK) {
        return QByteArray();
    }
    auto blobGuard = qScopeGuard([&blob]() {
        if (blob) {
            git_blob_free(blob);
        }
    });

    const auto* content = static_cast<const char*>(git_blob_rawcontent(blob));
    const size_t size = git_blob_rawsize(blob);
    return QByteArray(content, static_cast<int>(size));
}

QByteArray readBlobFromCommit(git_repository* repo, const git_oid& commitId, const QString& path)
{
    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, repo, &commitId) != GIT_OK) {
        return QByteArray();
    }
    auto commitGuard = qScopeGuard([&commit]() {
        if (commit) {
            git_commit_free(commit);
        }
    });

    git_tree* tree = nullptr;
    if (git_commit_tree(&tree, commit) != GIT_OK) {
        return QByteArray();
    }
    auto treeGuard = qScopeGuard([&tree]() {
        if (tree) {
            git_tree_free(tree);
        }
    });

    git_tree_entry* entry = nullptr;
    if (git_tree_entry_bypath(&entry, tree, path.toLocal8Bit().constData()) != GIT_OK) {
        return QByteArray();
    }
    auto entryGuard = qScopeGuard([&entry]() {
        if (entry) {
            git_tree_entry_free(entry);
        }
    });

    git_blob* blob = nullptr;
    if (git_blob_lookup(&blob, repo, git_tree_entry_id(entry)) != GIT_OK) {
        return QByteArray();
    }
    auto blobGuard = qScopeGuard([&blob]() {
        if (blob) {
            git_blob_free(blob);
        }
    });

    const auto* content = static_cast<const char*>(git_blob_rawcontent(blob));
    const size_t size = git_blob_rawsize(blob);
    return QByteArray(content, static_cast<int>(size));
}

QString gitDirPathFromRepository(git_repository* repo)
{
    if (!repo) {
        return QString();
    }
    const char* gitPath = git_repository_path(repo);
    return gitPath ? QString::fromUtf8(gitPath) : QString();
}

bool setGitConfigString(const QString& workTreePath,
                        const char* key,
                        const QString& value)
{
    git_repository* repo = nullptr;
    if (git_repository_open(&repo, workTreePath.toLocal8Bit().constData()) != GIT_OK || !repo) {
        return false;
    }
    auto repoGuard = qScopeGuard([&repo]() {
        if (repo) {
            git_repository_free(repo);
        }
    });

    git_config* config = nullptr;
    if (git_repository_config(&config, repo) != GIT_OK || !config) {
        return false;
    }
    auto configGuard = qScopeGuard([&config]() {
        if (config) {
            git_config_free(config);
        }
    });

    return git_config_set_string(config,
                                 key,
                                 value.toUtf8().constData()) == GIT_OK;
}

template<typename ProtoT>
ProtoT loadProtoFromJsonFile(const QString& filename)
{
    QFile file(filename);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    file.close();

    ProtoT proto;
    const auto parseStatus = google::protobuf::util::JsonStringToMessage(data.toStdString(), &proto);
    REQUIRE(parseStatus.ok());
    return proto;
}

template<typename ProtoT>
void writeProtoToJsonFile(const QString& filename, const ProtoT& proto)
{
    std::string json;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    const auto toJsonStatus = google::protobuf::util::MessageToJsonString(proto, &json, options);
    REQUIRE(toJsonStatus.ok());

    QFile file(filename);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(json.data(), static_cast<qint64>(json.size())) == static_cast<qint64>(json.size()));
    file.close();
}
} // namespace

// TEST_CASE("cwProject isModified should work correctly", "[cwProject]") {
//     cwProject project;
//     CHECK(project.isModified() == false);

//     cwCave* cave1 = new cwCave();
//     cave1->setName("cave1");
//     project.cavingRegion()->addCave(cave1);

//     CHECK(project.isModified() == true);

//     QString testFile = prependTempFolder("test_cwProject.cw");
//     QFile::remove(testFile);

//     //Fixme: this was calling save as
//     REQUIRE(false);

//     // project.saveAs(testFile);
//     project.waitSaveToFinish();

//     CHECK(project.isModified() == false);

//     project.newProject();
//     CHECK(project.isModified() == false);

//     project.loadOrConvert(testFile);
//     project.waitLoadToFinish();
//     CHECK(project.isModified() == false);

//     REQUIRE(project.cavingRegion()->caveCount() == 1);

//     project.cavingRegion()->cave(0)->addTrip();
//     CHECK(project.isModified() == true);

//     //Fixme: this was calling save
//     REQUIRE(false);

//     // project.save();
//     project.waitSaveToFinish();
//     CHECK(project.isModified() == false);

//     SECTION("Load file") {
//         //If this fails, this is probably because of a version change, or other save changes
//         fileToProject(&project, "://datasets/network.cw");
//         project.waitLoadToFinish();
//         CHECK(project.isModified() == false);

//         REQUIRE(project.cavingRegion()->caveCount() == 1);
//         REQUIRE(project.cavingRegion()->cave(0)->tripCount() == 1);

//         cwTrip* firstTrip = project.cavingRegion()->cave(0)->trip(0);
//         firstTrip->setName("Sauce!");
//         CHECK(project.isModified() == true);
//     }
// }

TEST_CASE("Image data should save and load correctly", "[cwProject]") {

    qRegisterMetaType<QList <cwImage> >("QList<cwImage>");

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    QString filename = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    QImage originalImage(filename);

    trip->notes()->addFromFiles({QUrl("file:/" + filename)});

    rootData->futureManagerModel()->waitForFinished();

    SECTION("Is Modifed shouldn't effect the save and load") {
        //Make sure is modified doesn't modify the underlying file
        CHECK(project->isModified() == true);
    }

    REQUIRE(trip->notes()->notes().size() == 1);

    cwNote* note = trip->notes()->notes().first();
    cwImage image = note->image();
    cwImageProvider provider;
    provider.setDataRootDir(project->dataRootDir());
    const QString noteImagePath = ProjectFilenameTestHelper::absolutePath(note, image.path());
    QImage sqlImage = provider.image(noteImagePath);

    CHECK(!sqlImage.isNull());
    CHECK(originalImage == sqlImage);
}

TEST_CASE("New temporary project detection works", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    REQUIRE(project->isTemporaryProject());
    REQUIRE(project->cavingRegion()->caveCount() == 0);
    CHECK(project->isNewProject() == true);

    project->cavingRegion()->addCave();
    CHECK(project->isNewProject() == false);
}

TEST_CASE("NewProject should not clear objects added after call", "[cwProject][newProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto region = project->cavingRegion();

    // Create a backlog of saves so newProject stays in-flight.
    const int preCaveCount = 10;
    for (int i = 0; i < preCaveCount; ++i) {
        region->addCave();
        auto cave = region->cave(i);
        cave->setName(QStringLiteral("PreCave-%1").arg(i));
        cave->addTrip();
        cave->trip(0)->setName(QStringLiteral("PreTrip-%1").arg(i));
    }

    qDebug() << "Calling newProject!";
    project->newProject();

    region->addCave();
    auto postCave = region->cave(region->caveCount() - 1);
    postCave->setName(QStringLiteral("PostNewProjectCave"));
    postCave->addTrip();
    auto postTrip = postCave->trip(0);
    postTrip->setName(QStringLiteral("PostNewProjectTrip"));

    QPointer<cwCave> postCavePtr = postCave;
    QPointer<cwTrip> postTripPtr = postTrip;

    project->waitSaveToFinish();

    REQUIRE(postCavePtr != nullptr);
    REQUIRE(postTripPtr != nullptr);
    CHECK(region->caveCount() == 1);
    CHECK(region->indexOf(postCavePtr) != -1);
    CHECK(postCavePtr->tripCount() == 1);
    CHECK(postCavePtr->name() == QStringLiteral("PostNewProjectCave"));
    CHECK(postTripPtr->name() == QStringLiteral("PostNewProjectTrip"));
}

TEST_CASE("Rapid renaming", "[cwProject]") {

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto region = project->cavingRegion();

    // Create a backlog of saves so newProject stays in-flight.
    const int preCaveCount = 500;
    for (int i = 0; i < preCaveCount; ++i) {
        region->addCave();
        auto cave = region->cave(i);
        cave->setName(QStringLiteral("PreCave-%1").arg(i));
        cave->addTrip();
        cave->trip(0)->setName(QStringLiteral("PreTrip-%1").arg(i));
    }

    for (int i = 0; i < preCaveCount; ++i) {
        auto cave = region->cave(i);
        cave->setName(QStringLiteral("PreCave-%1-2").arg(i));
        cave->trip(0)->setName(QStringLiteral("PreTrip-%1-2").arg(i));
    }

    project->waitSaveToFinish();

    //Check the file structure and make sure files exist where the should be
    const QDir projectDataRoot = QFileInfo(project->absolutePath(project->dataRoot())).absoluteDir();
    const auto cavePathFor = [&projectDataRoot](const QString& caveName) {
        return projectDataRoot.filePath(QStringLiteral("%1/%1.cwcave").arg(caveName));
    };
    const auto tripPathFor = [&projectDataRoot](const QString& caveName, const QString& tripName) {
        return projectDataRoot.filePath(QStringLiteral("%1/trips/%2/%2.cwtrip").arg(caveName, tripName));
    };

    REQUIRE(region->caveCount() == preCaveCount);
    for (int i = 0; i < preCaveCount; ++i) {
        cwCave* cave = region->cave(i);
        REQUIRE(cave != nullptr);
        REQUIRE(cave->tripCount() == 1);
        cwTrip* trip = cave->trip(0);
        REQUIRE(trip != nullptr);

        const QString expectedCaveName = QStringLiteral("PreCave-%1-2").arg(i);
        const QString expectedTripName = QStringLiteral("PreTrip-%1-2").arg(i);
        const QString oldCaveName = QStringLiteral("PreCave-%1").arg(i);
        const QString oldTripName = QStringLiteral("PreTrip-%1").arg(i);

        CHECK(cave->name() == expectedCaveName);
        CHECK(trip->name() == expectedTripName);

        const QString expectedCavePath = cavePathFor(expectedCaveName);
        const QString expectedTripPath = tripPathFor(expectedCaveName, expectedTripName);
        const QString oldCavePath = cavePathFor(oldCaveName);
        const QString oldTripPath = tripPathFor(oldCaveName, oldTripName);

        INFO("Cave path:" << expectedCavePath.toStdString());
        INFO("Trip path:" << expectedTripPath.toStdString());

        CHECK(QFileInfo::exists(expectedCavePath));
        CHECK(QFileInfo::exists(expectedTripPath));
        CHECK(QFileInfo(expectedCavePath).completeBaseName() == expectedCaveName);
        CHECK(QFileInfo(expectedTripPath).completeBaseName() == expectedTripName);

        CHECK_FALSE(QFileInfo::exists(oldCavePath));
        CHECK_FALSE(QFileInfo::exists(oldTripPath));
    }

}

TEST_CASE("objectPathReady emits for renamed objects", "[cwProject][cwSaveLoad]") {
    qRegisterMetaType<QObject*>("QObject*");

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("PathReadyCave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    trip->setName(QStringLiteral("PathReadyTrip"));

    auto notes = trip->notes();
    REQUIRE(notes != nullptr);

    const QString sourceImage = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(sourceImage));
    notes->addFromFiles({QUrl::fromLocalFile(sourceImage)});

    const QString glbSource = copyToTempFolder("://datasets/test_cwSurveyNotesConcatModel/bones.glb");
    REQUIRE(QFileInfo::exists(glbSource));
    trip->notesLiDAR()->addFromFiles({QUrl::fromLocalFile(glbSource)});

    project->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(notes->notes().size() == 1);
    auto note = notes->notes().first();
    note->setName(QStringLiteral("PathReadyNote"));

    project->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();

    cwSignalSpy spy(project, &cwProject::objectPathReady);

    auto countFor = [&spy](QObject* object) {
        int count = 0;
        for (int i = 0; i < spy.count(); ++i) {
            const auto args = spy.at(i);
            if (!args.isEmpty() && args.at(0).value<QObject*>() == object) {
                ++count;
            }
        }
        return count;
    };

    spy.clear();
    note->setName(QStringLiteral("PathReadyNoteRenamed"));
    project->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();
    CHECK(countFor(note) >= 1);
    CHECK(countFor(trip) == 0);
    CHECK(countFor(cave) == 0);

    spy.clear();
    trip->setName(QStringLiteral("PathReadyTripRenamed"));
    project->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();
    CHECK(countFor(trip) >= 1);
    CHECK(countFor(note) == 0);
    CHECK(countFor(cave) == 0);

    spy.clear();
    cave->setName(QStringLiteral("PathReadyCaveRenamed"));
    project->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();
    CHECK(countFor(cave) >= 1);
    CHECK(countFor(trip) == 0);
    CHECK(countFor(note) == 0);
}

TEST_CASE("Queued copy and rename should preserve note assets", "[cwProject][cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("CopyRenameCave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    trip->setName(QStringLiteral("CopyRenameTrip"));

    auto notes = trip->notes();
    REQUIRE(notes != nullptr);

    const QString sourceImage = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(sourceImage));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const int imageCount = 20;
    QList<QUrl> urls;
    urls.reserve(imageCount);
    for (int i = 0; i < imageCount; ++i) {
        const QString filePath = tempDir.filePath(QStringLiteral("copy-%1.png").arg(i));
        REQUIRE(QFile::copy(sourceImage, filePath));
        urls.append(QUrl::fromLocalFile(filePath));
    }

    const QString originalTripFile = ProjectFilenameTestHelper::absolutePath(trip);
    QFileInfo originalTripInfo(originalTripFile);
    const QString originalTripDirPath = originalTripInfo.absoluteDir().absolutePath();
    const QString originalNotesDirPath = QDir(originalTripDirPath).filePath(QStringLiteral("notes"));

    notes->addFromFiles(urls);
    trip->setName(QStringLiteral("CopyRenameTripAfter"));

    project->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();

    const QString renamedTripFile = ProjectFilenameTestHelper::absolutePath(trip);
    QFileInfo renamedTripInfo(renamedTripFile);
    const QString renamedTripDirPath = renamedTripInfo.absoluteDir().absolutePath();
    const QString renamedNotesDirPath = QDir(renamedTripDirPath).filePath(QStringLiteral("notes"));

    CHECK_FALSE(QFileInfo::exists(originalTripFile));
    CHECK_FALSE(QDir(originalTripDirPath).exists());
    CHECK(QFileInfo::exists(renamedTripFile));
    CHECK(QDir(renamedNotesDirPath).exists());
    CHECK_FALSE(QDir(originalNotesDirPath).exists());

    REQUIRE(notes->rowCount() == imageCount);
    for (cwNote* note : notes->notes()) {
        REQUIRE(note != nullptr);
        const QString imagePath = note->image().path();
        const QString fileName = QFileInfo(imagePath).fileName();
        REQUIRE_FALSE(fileName.isEmpty());
        const QString expectedPath = ProjectFilenameTestHelper::dir(note).absoluteFilePath(fileName);
        CHECK(QFileInfo::exists(expectedPath));
    }
}

TEST_CASE("Saves queued during newProject should target the new project directory", "[cwProject][newProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto region = project->cavingRegion();

    const int preCaveCount = 400;
    for (int i = 0; i < preCaveCount; ++i) {
        region->addCave();
        auto cave = region->cave(i);
        cave->setName(QStringLiteral("PreSaveCave-%1").arg(i));
        cave->addTrip();
        cave->trip(0)->setName(QStringLiteral("PreSaveTrip-%1").arg(i));
    }

    const QString oldProjectFile = project->filename();
    const QDir oldProjectDir = QFileInfo(oldProjectFile).absoluteDir();

    project->newProject();

    region->addCave();
    auto postCave = region->cave(region->caveCount() - 1);
    postCave->setName(QStringLiteral("PostSaveCave"));
    postCave->addTrip();
    postCave->trip(0)->setName(QStringLiteral("PostSaveTrip"));

    QPointer<cwCave> postCavePtr = postCave;

    project->waitSaveToFinish();

    REQUIRE(postCavePtr != nullptr);
    const QString newProjectFile = project->filename();
    const QDir newProjectDir = QFileInfo(newProjectFile).absoluteDir();

    const QString newCavePath = ProjectFilenameTestHelper::absolutePath(postCavePtr);
    const QString oldCavePath = oldProjectDir.absoluteFilePath(ProjectFilenameTestHelper::fileName(postCavePtr));

    CHECK(QFileInfo::exists(newCavePath));
    CHECK(!QFileInfo::exists(oldCavePath));
    CHECK(oldProjectDir.absolutePath() != newProjectDir.absolutePath());
}

TEST_CASE("newProject waits for pending edits before reset", "[cwProject][newProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    cave->setName(QStringLiteral("InitialCave"));
    trip->setName(QStringLiteral("InitialTrip"));
    project->waitSaveToFinish();

    const QString oldProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(oldProjectFile));

    cave->setName(QStringLiteral("EditedCave"));
    trip->setName(QStringLiteral("EditedTrip"));

    project->newProject();
    project->waitSaveToFinish();

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(oldProjectFile);
    reloaded->waitLoadToFinish();

    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    auto reloadedCave = reloaded->cavingRegion()->cave(0);
    REQUIRE(reloadedCave != nullptr);
    CHECK(reloadedCave->name() == QStringLiteral("EditedCave"));
    REQUIRE(reloadedCave->tripCount() == 1);
    CHECK(reloadedCave->trip(0)->name() == QStringLiteral("EditedTrip"));
}

TEST_CASE("addFiles callback is quarantined when newProject retires in-flight import", "[cwProject][newProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString largeFilePath = sourceDir.filePath(QStringLiteral("large-import.bin"));
    {
        QFile largeFile(largeFilePath);
        REQUIRE(largeFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray chunk(1024 * 1024, '\0');
        for (int i = 0; i < 128; ++i) {
            REQUIRE(largeFile.write(chunk) == chunk.size());
        }
        largeFile.close();
    }
    REQUIRE(QFileInfo::exists(largeFilePath));
    REQUIRE(QFileInfo(largeFilePath).size() == 128ll * 1024ll * 1024ll);

    int callbackCount = 0;
    project->addFiles({QUrl::fromLocalFile(largeFilePath)},
                      ProjectFilenameTestHelper::projectDir(project),
                      [&callbackCount](const QList<QString>&) {
                          ++callbackCount;
                      });

    project->newProject();

    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    CHECK(callbackCount == 0);
}

TEST_CASE("Loading a project clears temporary flag", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    REQUIRE(project->isTemporaryProject());

    QString source = copyToTempFolder("://datasets/test_cwProject/v8.cwproj");
    project->loadOrConvert(source);
    rootData->futureManagerModel()->waitForFinished();
    project->waitLoadToFinish();

    CHECK(project->isTemporaryProject() == false);
}

TEST_CASE("Loading a project preserves metadata dataRoot", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());

    const QString projectName = QStringLiteral("Project Name");
    const QString metadataDataRoot = QStringLiteral("custom-root");
    const QString projectFile = projectDir.filePath(QStringLiteral("metadata-root.cwproj"));

    REQUIRE(QDir().mkpath(projectDir.filePath(metadataDataRoot)));

    QFile file(projectFile);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray contents = QStringLiteral(
        "{\n"
        " \"fileVersion\": {\n"
        "  \"version\": 8,\n"
        "  \"cavewhereVersion\": \"2025.2-223-gd949c05f\"\n"
        " },\n"
        " \"name\": \"%1\",\n"
        " \"metadata\": {\n"
        "  \"dataRoot\": \"%2\",\n"
        "  \"gitMode\": \"ManagedNew\",\n"
        "  \"syncEnabled\": true\n"
        " }\n"
        "}\n")
            .arg(projectName, metadataDataRoot)
            .toUtf8();
    REQUIRE(file.write(contents) == contents.size());
    file.close();

    project->loadOrConvert(projectFile);
    rootData->futureManagerModel()->waitForFinished();
    project->waitLoadToFinish();

    CHECK(project->dataRoot().toStdString() == metadataDataRoot.toStdString());
}

TEST_CASE("Loading a project adds .git/info/exclude cache entry", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    QString source = copyToTempFolder("://datasets/test_cwProject/v8.cwproj");
    const QDir projectDir = QFileInfo(source).absoluteDir();
    QFile::remove(projectDir.filePath(QStringLiteral(".git/info/exclude")));

    project->loadOrConvert(source);
    rootData->futureManagerModel()->waitForFinished();
    project->waitLoadToFinish();

    const QString contents = readGitExclude(projectDir);
    CHECK(contents.contains(".cw_cache/"));
}

TEST_CASE("Loading a project repairs and persists missing top-level ids", "[cwProject][repair]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto* region = project->cavingRegion();

    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Repair Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Repair Trip"));

    const QString imagePath = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(imagePath));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(imagePath)});

    const QString glbPath = copyToTempFolder("://datasets/test_cwSurveyNotesConcatModel/bones.glb");
    REQUIRE(QFileInfo::exists(glbPath));
    trip->notesLiDAR()->addFromFiles({QUrl::fromLocalFile(glbPath)});

    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(trip->notes()->rowCount() == 1);
    REQUIRE(trip->notesLiDAR()->rowCount() == 1);
    auto* note = trip->notes()->notes().at(0);
    auto* lidarNote = qobject_cast<cwNoteLiDAR*>(trip->notesLiDAR()->notes().at(0));
    REQUIRE(note != nullptr);
    REQUIRE(lidarNote != nullptr);

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    const QString savePath = QDir(saveDir.path()).filePath(QStringLiteral("repair-missing-ids.cwproj"));
    REQUIRE(project->saveAs(savePath));
    project->waitSaveToFinish();
    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    const QString caveFile = ProjectFilenameTestHelper::absolutePath(cave);
    const QString tripFile = ProjectFilenameTestHelper::absolutePath(trip);
    const QString noteFile = ProjectFilenameTestHelper::absolutePath(note);
    const QString lidarFile = ProjectFilenameTestHelper::absolutePath(lidarNote);

    {
        auto caveProto = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
        auto tripProto = loadProtoFromJsonFile<CavewhereProto::Trip>(tripFile);
        auto noteProto = loadProtoFromJsonFile<CavewhereProto::Note>(noteFile);
        auto lidarProto = loadProtoFromJsonFile<CavewhereProto::NoteLiDAR>(lidarFile);

        caveProto.clear_id();
        tripProto.clear_id();
        noteProto.clear_id();
        lidarProto.clear_id();

        writeProtoToJsonFile(caveFile, caveProto);
        writeProtoToJsonFile(tripFile, tripProto);
        writeProtoToJsonFile(noteFile, noteProto);
        writeProtoToJsonFile(lidarFile, lidarProto);
    }

    {
        const auto clearedCave = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
        const auto clearedTrip = loadProtoFromJsonFile<CavewhereProto::Trip>(tripFile);
        const auto clearedNote = loadProtoFromJsonFile<CavewhereProto::Note>(noteFile);
        const auto clearedLidar = loadProtoFromJsonFile<CavewhereProto::NoteLiDAR>(lidarFile);
        REQUIRE_FALSE(clearedCave.has_id());
        REQUIRE_FALSE(clearedTrip.has_id());
        REQUIRE_FALSE(clearedNote.has_id());
        REQUIRE_FALSE(clearedLidar.has_id());
    }

    rootData.reset();

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();
    reloaded->waitSaveToFinish();

    const auto repairedCaveProto = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
    const auto repairedTripProto = loadProtoFromJsonFile<CavewhereProto::Trip>(tripFile);
    const auto repairedNoteProto = loadProtoFromJsonFile<CavewhereProto::Note>(noteFile);
    const auto repairedLidarProto = loadProtoFromJsonFile<CavewhereProto::NoteLiDAR>(lidarFile);

    REQUIRE(repairedCaveProto.has_id());
    REQUIRE(repairedTripProto.has_id());
    REQUIRE(repairedNoteProto.has_id());
    REQUIRE(repairedLidarProto.has_id());
    CHECK_FALSE(repairedCaveProto.id().empty());
    CHECK_FALSE(repairedTripProto.id().empty());
    CHECK_FALSE(repairedNoteProto.id().empty());
    CHECK_FALSE(repairedLidarProto.id().empty());

    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    auto* loadedCave = reloaded->cavingRegion()->cave(0);
    REQUIRE(loadedCave != nullptr);
    CHECK_FALSE(loadedCave->id().isNull());
    REQUIRE(loadedCave->tripCount() == 1);
    auto* loadedTrip = loadedCave->trip(0);
    REQUIRE(loadedTrip != nullptr);
    CHECK_FALSE(loadedTrip->id().isNull());
    REQUIRE(loadedTrip->notes()->rowCount() == 1);
    REQUIRE(loadedTrip->notesLiDAR()->rowCount() == 1);
    CHECK_FALSE(loadedTrip->notes()->notes().at(0)->id().isNull());
    auto* loadedLiDARNote = qobject_cast<cwNoteLiDAR*>(loadedTrip->notesLiDAR()->notes().at(0));
    REQUIRE(loadedLiDARNote != nullptr);
    CHECK_FALSE(loadedLiDARNote->id().isNull());
}

TEST_CASE("Loading a project repairs duplicate cave ids", "[cwProject][repair]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto* region = project->cavingRegion();

    region->addCave();
    region->addCave();
    auto* caveA = region->cave(0);
    auto* caveB = region->cave(1);
    REQUIRE(caveA != nullptr);
    REQUIRE(caveB != nullptr);
    caveA->setName(QStringLiteral("AA-Cave"));
    caveB->setName(QStringLiteral("BB-Cave"));

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    const QString savePath = QDir(saveDir.path()).filePath(QStringLiteral("repair-duplicate-cave-ids.cwproj"));
    REQUIRE(project->saveAs(savePath));
    project->waitSaveToFinish();
    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    QStringList caveFiles{
        ProjectFilenameTestHelper::absolutePath(caveA),
        ProjectFilenameTestHelper::absolutePath(caveB)
    };
    caveFiles.sort();
    REQUIRE(caveFiles.size() == 2);

    const std::string duplicateId = "11111111-2222-3333-4444-555555555555";
    for (const QString& caveFile : caveFiles) {
        auto caveProto = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
        caveProto.set_id(duplicateId);
        writeProtoToJsonFile(caveFile, caveProto);
    }

    {
        const auto duplicateFirst = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFiles.at(0));
        const auto duplicateSecond = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFiles.at(1));
        REQUIRE(duplicateFirst.has_id());
        REQUIRE(duplicateSecond.has_id());
        REQUIRE(duplicateFirst.id() == duplicateId);
        REQUIRE(duplicateSecond.id() == duplicateId);
    }

    rootData.reset();

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();
    reloaded->waitSaveToFinish();

    const auto repairedFirst = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFiles.at(0));
    const auto repairedSecond = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFiles.at(1));

    REQUIRE(repairedFirst.has_id());
    REQUIRE(repairedSecond.has_id());
    CHECK(repairedFirst.id() == duplicateId);
    CHECK(repairedSecond.id() != duplicateId);
    CHECK(repairedFirst.id() != repairedSecond.id());
}

TEST_CASE("Loading a project surfaces repair save failures", "[cwProject][repair]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto* region = project->cavingRegion();

    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Repair Failure Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Repair Failure Trip"));

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    const QString savePath = QDir(saveDir.path()).filePath(QStringLiteral("repair-save-failure.cwproj"));
    REQUIRE(project->saveAs(savePath));
    project->waitSaveToFinish();
    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    const QString caveFile = ProjectFilenameTestHelper::absolutePath(cave);
    REQUIRE(QFileInfo::exists(caveFile));

    {
        auto caveProto = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
        caveProto.clear_id();
        writeProtoToJsonFile(caveFile, caveProto);
    }

    const auto clearedCave = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
    REQUIRE_FALSE(clearedCave.has_id());

    const QDir caveDir = QFileInfo(caveFile).absoluteDir();
    const QFileDevice::Permissions originalCaveDirPermissions = QFile::permissions(caveDir.absolutePath());
    auto permissionsGuard = qScopeGuard([caveDir, originalCaveDirPermissions]() {
        QFile::setPermissions(caveDir.absolutePath(), originalCaveDirPermissions);
    });

    const QFileDevice::Permissions readOnlyDirPermissions =
        QFileDevice::ReadOwner | QFileDevice::ExeOwner
        | QFileDevice::ReadGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::ExeOther;
    REQUIRE(QFile::setPermissions(caveDir.absolutePath(), readOnlyDirPermissions));

    rootData.reset();

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->errorModel()->clear();
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    REQUIRE(reloaded->errorModel()->count() > 0);
    const auto loadError = reloaded->errorModel()->last();
    CHECK(loadError.type() == cwError::Fatal);
    CHECK(loadError.message().contains(QStringLiteral("Save flush failed")));

    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    auto* loadedCave = reloaded->cavingRegion()->cave(0);
    REQUIRE(loadedCave != nullptr);
    CHECK_FALSE(loadedCave->id().isNull());

    const auto onDiskCave = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
    CHECK_FALSE(onDiskCave.has_id());
}

TEST_CASE("Saving commits local git changes when account is configured", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Commit Tester"));
    rootData->account()->setEmail(QStringLiteral("commit.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Cave A"));
    cave->addTrip();
    cave->trip(0)->setName(QStringLiteral("Trip A"));

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    const QString savePath = QDir(saveDir.path()).filePath(QStringLiteral("commit-on-save.cwproj"));
    REQUIRE(project->saveAs(savePath));
    project->waitSaveToFinish();

    const QDir repoDir = QFileInfo(project->filename()).absoluteDir();
    git_repository* repo = nullptr;
    REQUIRE(git_repository_open(&repo, repoDir.absolutePath().toLocal8Bit().constData()) == GIT_OK);
    auto repoGuard = qScopeGuard([&repo]() {
        if (repo) {
            git_repository_free(repo);
        }
    });

    auto tryHeadOid = [](git_repository* repository) -> std::optional<git_oid> {
        git_reference* head = nullptr;
        const int error = git_repository_head(&head, repository);
        if (error == GIT_EUNBORNBRANCH || error == GIT_ENOTFOUND) {
            return std::nullopt;
        }
        REQUIRE(error == GIT_OK);
        auto headGuard = qScopeGuard([&head]() {
            if (head) {
                git_reference_free(head);
            }
        });
        const git_oid* target = git_reference_target(head);
        REQUIRE(target != nullptr);
        return *target;
    };

    cave->setName(QStringLiteral("Cave B"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    const std::optional<git_oid> beforeSaveCommit = tryHeadOid(repo);

    REQUIRE(project->save());
    project->waitSaveToFinish();

    const std::optional<git_oid> afterSaveCommit = tryHeadOid(repo);
    REQUIRE(afterSaveCommit.has_value());
    if (beforeSaveCommit.has_value()) {
        CHECK(git_oid_cmp(&(*beforeSaveCommit), &(*afterSaveCommit)) != 0);
    }

    git_commit* commit = nullptr;
    REQUIRE(git_commit_lookup(&commit, repo, &(*afterSaveCommit)) == GIT_OK);
    auto commitGuard = qScopeGuard([&commit]() {
        if (commit) {
            git_commit_free(commit);
        }
    });

    const git_signature* committer = git_commit_committer(commit);
    REQUIRE(committer != nullptr);
    CHECK(QString::fromUtf8(committer->name) == QStringLiteral("Commit Tester"));
    CHECK(QString::fromUtf8(committer->email) == QStringLiteral("commit.tester@example.com"));

    const QString commitMessage = QString::fromUtf8(git_commit_message(commit));
    CHECK(commitMessage.startsWith(QStringLiteral("Save from CaveWhere")));
    CHECK(project->isModified() == false);
}

TEST_CASE("commit-on-save is idempotent when no changes exist", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Commit Tester"));
    rootData->account()->setEmail(QStringLiteral("commit.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Cave A"));
    cave->addTrip();
    cave->trip(0)->setName(QStringLiteral("Trip A"));

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    const QString savePath = QDir(saveDir.path()).filePath(QStringLiteral("commit-idempotent-save.cwproj"));
    REQUIRE(project->saveAs(savePath));
    project->waitSaveToFinish();

    const QDir repoDir = QFileInfo(project->filename()).absoluteDir();
    git_repository* repo = nullptr;
    REQUIRE(git_repository_open(&repo, repoDir.absolutePath().toLocal8Bit().constData()) == GIT_OK);
    auto repoGuard = qScopeGuard([&repo]() {
        if (repo) {
            git_repository_free(repo);
        }
    });

    auto headOid = [](git_repository* repository) -> git_oid {
        git_reference* head = nullptr;
        REQUIRE(git_repository_head(&head, repository) == GIT_OK);
        auto headGuard = qScopeGuard([&head]() {
            if (head) {
                git_reference_free(head);
            }
        });
        const git_oid* target = git_reference_target(head);
        REQUIRE(target != nullptr);
        return *target;
    };

    cave->setName(QStringLiteral("Cave B"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    REQUIRE(project->save());
    project->waitSaveToFinish();
    const git_oid firstSaveHead = headOid(repo);
    CHECK(project->isModified() == false);

    REQUIRE(project->save());
    project->waitSaveToFinish();
    const git_oid secondSaveHead = headOid(repo);
    CHECK(git_oid_cmp(&firstSaveHead, &secondSaveHead) == 0);
    CHECK(project->isModified() == false);
}

TEST_CASE("Project repository includes LFS attributes for glb assets", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    const QString attributesPath = QFileInfo(project->filename()).absoluteDir().filePath(QStringLiteral(".gitattributes"));
    QFile attributesFile(attributesPath);
    REQUIRE(attributesFile.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(attributesFile.readAll());
    CHECK(contents.contains(QStringLiteral("*.glb filter=lfs diff=lfs merge=lfs -text")));
}

TEST_CASE("Remote clone open edit save and sync workflow preserves LFS assets",
          "[cwProject][cwRemoteRepositoryCloner][lfs][workflow]") {
    QSettings settings;
    settings.clear();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Workflow Tester"));
    rootData->account()->setEmail(QStringLiteral("workflow.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Seed Cave"));
    cave->addTrip();
    auto seedTrip = cave->trip(0);
    REQUIRE(seedTrip != nullptr);
    seedTrip->setName(QStringLiteral("Seed Trip"));

    QTemporaryDir sourceRoot;
    REQUIRE(sourceRoot.isValid());
    const QString sourceProjectPath = QDir(sourceRoot.path()).filePath(QStringLiteral("seed-project.cwproj"));
    REQUIRE(project->saveAs(sourceProjectPath));
    project->waitSaveToFinish();

    auto* sourceRepository = project->repository();
    REQUIRE(sourceRepository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("seed-remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = sourceRepository->addRemote(QStringLiteral("origin"),
                                                               QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + sourceRepository->headBranchName();
    git_reference* baselineRemoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&baselineRemoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto baselineRemoteBranchGuard = qScopeGuard([&baselineRemoteBranch]() {
        if (baselineRemoteBranch) {
            git_reference_free(baselineRemoteBranch);
        }
    });
    const git_oid* baselineRemoteHead = git_reference_target(baselineRemoteBranch);
    REQUIRE(baselineRemoteHead != nullptr);
    const git_oid baselineRemoteOid = *baselineRemoteHead;

    QTemporaryDir cloneRoot;
    REQUIRE(cloneRoot.isValid());

    auto* repositoryModel = rootData->repositoryModel();
    REQUIRE(repositoryModel != nullptr);
    repositoryModel->setDefaultRepositoryDir(QUrl::fromLocalFile(cloneRoot.path()));
    const int repositoryCountBeforeClone = repositoryModel->rowCount();

    QQuickGit::GitFutureWatcher cloneWatcher;
    cwRemoteRepositoryCloner cloner;
    cloner.setRepositoryModel(repositoryModel);
    cloner.setCloneWatcher(&cloneWatcher);
    cloner.setAccount(rootData->account());

    QSignalSpy stateSpy(&cloneWatcher, &QQuickGit::GitFutureWatcher::stateChanged);
    const QString cloneUrl = QUrl::fromLocalFile(remoteRepoPath).toString();
    cloner.clone(cloneUrl);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&cloneWatcher, &QQuickGit::GitFutureWatcher::stateChanged, &loop, [&cloneWatcher, &loop]() {
        if (cloneWatcher.state() == QQuickGit::GitFutureWatcher::Ready) {
            loop.quit();
        }
    });
    timeout.start(20000);
    loop.exec();

    INFO(cloner.cloneErrorMessage().toStdString());
    REQUIRE(cloneWatcher.state() == QQuickGit::GitFutureWatcher::Ready);
    REQUIRE_FALSE(cloneWatcher.hasError());
    CHECK(cloner.cloneErrorMessage().isEmpty());
    CHECK(cloner.cloneStatusMessage() == QStringLiteral("Clone complete."));
    CHECK(stateSpy.count() > 0);
    CHECK(repositoryModel->rowCount() == repositoryCountBeforeClone + 1);

    const QString clonedRepoName = cloner.repositoryNameFromUrl(cloneUrl);
    const QString clonedRepoPath = QDir(cloneRoot.path()).filePath(clonedRepoName);
    REQUIRE(QDir(clonedRepoPath).exists());

    int clonedIndex = -1;
    for (int i = 0; i < repositoryModel->rowCount(); ++i) {
        const QString path = repositoryModel->data(repositoryModel->index(i, 0), cwRepositoryModel::PathRole).toString();
        if (QDir(path).absolutePath() == QDir(clonedRepoPath).absolutePath()) {
            clonedIndex = i;
            break;
        }
    }
    REQUIRE(clonedIndex >= 0);

    const auto openResult = repositoryModel->openRepository(clonedIndex, project);
    INFO(openResult.errorMessage().toStdString());
    REQUIRE_FALSE(openResult.hasError());
    rootData->futureManagerModel()->waitForFinished();
    project->waitLoadToFinish();
    project->waitSaveToFinish();

    CHECK(QFileInfo(project->filename()).absoluteDir().absolutePath() == QDir(clonedRepoPath).absolutePath());
    CHECK(project->isTemporaryProject() == false);
    auto* loadedRepository = project->repository();
    REQUIRE(loadedRepository != nullptr);
    CHECK(loadedRepository->directory().absolutePath() == QDir(clonedRepoPath).absolutePath());
    CHECK_FALSE(loadedRepository->remotes().isEmpty());
    REQUIRE(loadedRepository->account() != nullptr);
    CHECK(loadedRepository->account()->isValid());

    REQUIRE(project->cavingRegion()->caveCount() > 0);
    auto* loadedCave = project->cavingRegion()->cave(0);
    REQUIRE(loadedCave != nullptr);
    loadedCave->addTrip();
    auto* newTrip = loadedCave->trip(loadedCave->tripCount() - 1);
    REQUIRE(newTrip != nullptr);
    newTrip->setName(QStringLiteral("Regression Trip"));

    newTrip->addNewChunk();
    auto* newChunk = newTrip->chunk(0);
    REQUIRE(newChunk != nullptr);
    newChunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("A1"));
    newChunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("A2"));
    newChunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("12.5"));
    newChunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("90.0"));
    newChunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0.0"));
    newChunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    newTrip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(newTrip->notes()->rowCount() == 1);
    auto* newNote = newTrip->notes()->notes().first();
    REQUIRE(newNote != nullptr);
    const QString noteImagePath = ProjectFilenameTestHelper::absolutePath(newNote, newNote->image().path());
    REQUIRE(QFileInfo::exists(noteImagePath));

    REQUIRE(project->isModified());
    REQUIRE(project->save());
    project->waitSaveToFinish();
    CHECK(project->isModified() == false);

    QStringList syncJobsSeen;
    QMetaObject::Connection rowsInsertedConnection = QObject::connect(
        rootData->futureManagerModel(),
        &QAbstractItemModel::rowsInserted,
        rootData->futureManagerModel(),
        [&](const QModelIndex& parent, int first, int last) {
            Q_UNUSED(parent);
            for (int row = first; row <= last; ++row) {
                syncJobsSeen.append(rootData->futureManagerModel()
                                        ->data(rootData->futureManagerModel()->index(row, 0),
                                               cwFutureManagerModel::NameRole)
                                        .toString());
            }
        });

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    QObject::disconnect(rowsInsertedConnection);

    const bool syncSucceeded = project->errorModel()->count() == 0;
    if (!syncSucceeded) {
        const auto syncError = project->errorModel()->last();
        CHECK(syncError.message() == QStringLiteral("Unsupported LFS remote URL"));
    }
    CHECK(syncJobsSeen.contains(QStringLiteral("Syncing project")));

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    git_reference* localHeadRef = nullptr;
    REQUIRE(git_repository_head(&localHeadRef, localRepo) == GIT_OK);
    auto localHeadRefGuard = qScopeGuard([&localHeadRef]() {
        if (localHeadRef) {
            git_reference_free(localHeadRef);
        }
    });
    const git_oid* localHeadOid = git_reference_target(localHeadRef);
    REQUIRE(localHeadOid != nullptr);

    git_reference* remoteHeadRef = nullptr;
    REQUIRE(git_reference_lookup(&remoteHeadRef, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteHeadRefGuard = qScopeGuard([&remoteHeadRef]() {
        if (remoteHeadRef) {
            git_reference_free(remoteHeadRef);
        }
    });
    const git_oid* remoteHeadOid = git_reference_target(remoteHeadRef);
    REQUIRE(remoteHeadOid != nullptr);

    if (syncSucceeded) {
        CHECK(git_oid_cmp(localHeadOid, remoteHeadOid) == 0);
        CHECK(git_oid_cmp(&baselineRemoteOid, remoteHeadOid) != 0);
    } else {
        CHECK(git_oid_cmp(&baselineRemoteOid, remoteHeadOid) == 0);
    }

    const QString relativeImagePath = QDir(localRepoPath).relativeFilePath(noteImagePath);
    const QByteArray localBlobData = readBlobFromHead(localRepo, relativeImagePath);
    REQUIRE_FALSE(localBlobData.isEmpty());

    QQuickGit::LfsPointer localPointer;
    REQUIRE(QQuickGit::LfsPointer::parse(localBlobData, &localPointer));
    CHECK(localPointer.isValid());
    CHECK(localPointer.size == QFileInfo(noteImagePath).size());

    const QString gitDirPath = gitDirPathFromRepository(localRepo);
    REQUIRE_FALSE(gitDirPath.isEmpty());
    const QString localObjectPath = QQuickGit::LfsStore::objectPath(gitDirPath, localPointer.oid);
    REQUIRE_FALSE(localObjectPath.isEmpty());
    CHECK(QFileInfo::exists(localObjectPath));

    if (syncSucceeded) {
        const QByteArray remoteBlobData = readBlobFromCommit(remoteRepo, *remoteHeadOid, relativeImagePath);
        REQUIRE_FALSE(remoteBlobData.isEmpty());
        QQuickGit::LfsPointer remotePointer;
        REQUIRE(QQuickGit::LfsPointer::parse(remoteBlobData, &remotePointer));
        CHECK(remotePointer.isValid());
        CHECK(remotePointer.oid == localPointer.oid);
        CHECK(remotePointer.size == localPointer.size);
    }
}

TEST_CASE("cwProject sync pushes local changes to remote repository", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Sync Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-project.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    cave->setName(QStringLiteral("Sync Cave Updated"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, QFileInfo(project->filename()).absoluteDir().absolutePath().toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    git_reference* localHead = nullptr;
    REQUIRE(git_repository_head(&localHead, localRepo) == GIT_OK);
    auto localHeadGuard = qScopeGuard([&localHead]() {
        if (localHead) {
            git_reference_free(localHead);
        }
    });
    const git_oid* localHeadOid = git_reference_target(localHead);
    REQUIRE(localHeadOid != nullptr);

    git_commit* localCommit = nullptr;
    REQUIRE(git_commit_lookup(&localCommit, localRepo, localHeadOid) == GIT_OK);
    auto localCommitGuard = qScopeGuard([&localCommit]() {
        if (localCommit) {
            git_commit_free(localCommit);
        }
    });
    const QString localCommitMessage = QString::fromUtf8(git_commit_message(localCommit));
    CHECK(localCommitMessage.startsWith(QStringLiteral("Sync from CaveWhere")));

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&remoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
        if (remoteBranch) {
            git_reference_free(remoteBranch);
        }
    });

    const git_oid* remoteHeadOid = git_reference_target(remoteBranch);
    REQUIRE(remoteHeadOid != nullptr);
    CHECK(git_oid_cmp(localHeadOid, remoteHeadOid) == 0);
    CHECK(project->isModified() == false);
}

TEST_CASE("cwProject sync fails without remotes after saving survey and notes", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("No Remote Tester"));
    rootData->account()->setEmail(QStringLiteral("no.remote.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("No Remote Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("No Remote Trip"));

    trip->addNewChunk();
    auto* chunk = trip->chunk(0);
    REQUIRE(chunk != nullptr);
    chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("S1"));
    chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("S2"));
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("8.5"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("120.0"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("-1.0"));
    chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    QTemporaryDir lowerCaseImageRoot;
    REQUIRE(lowerCaseImageRoot.isValid());
    const QString lowerCasePngPath = QDir(lowerCaseImageRoot.path()).filePath(QStringLiteral("tracked-note.png"));
    REQUIRE(QFile::copy(pngSource, lowerCasePngPath));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(lowerCasePngPath)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    QTemporaryDir saveRoot;
    REQUIRE(saveRoot.isValid());
    const QString projectPath = QDir(saveRoot.path()).filePath(QStringLiteral("no-remote-sync.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    REQUIRE(project->save());
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);
    CHECK(repository->remotes().isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() > 0);
    const auto syncError = project->errorModel()->last();
    CHECK(syncError.type() == cwError::Warning);
    CHECK(syncError.message() == QStringLiteral("No git remote is configured for this project."));
}

TEST_CASE("cwProject sync surfaces save flush failures before git sync", "[cwProject][sync]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Save Flush Tester"));
    rootData->account()->setEmail(QStringLiteral("save.flush.tester@example.com"));

    auto* region = project->cavingRegion();
    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Save Flush Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Save Flush Trip"));

    project->waitSaveToFinish();

    QTemporaryDir saveRoot;
    REQUIRE(saveRoot.isValid());
    const QString projectPath = QDir(saveRoot.path()).filePath(QStringLiteral("save-flush-failure.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    const QString tripFilePath = ProjectFilenameTestHelper::absolutePath(trip);
    REQUIRE(QFileInfo::exists(tripFilePath));
    const QDir tripDir = QFileInfo(tripFilePath).absoluteDir();

    const QFileDevice::Permissions originalTripDirPermissions = QFile::permissions(tripDir.absolutePath());
    auto permissionsGuard = qScopeGuard([tripDir, originalTripDirPermissions]() {
        QFile::setPermissions(tripDir.absolutePath(), originalTripDirPermissions);
    });

    const QFileDevice::Permissions readOnlyDirPermissions =
        QFileDevice::ReadOwner | QFileDevice::ExeOwner
        | QFileDevice::ReadGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::ExeOther;

    REQUIRE(QFile::setPermissions(tripDir.absolutePath(), readOnlyDirPermissions));

    trip->setDate(QDateTime::currentDateTimeUtc().addSecs(60));

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() > 0);
    const auto syncError = project->errorModel()->last();
    CHECK(syncError.type() == cwError::Warning);
    CHECK(syncError.message().contains(QStringLiteral("Save flush failed")));
}

TEST_CASE("cwProject sync succeeds after adding remote through project repository", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Remote Tester"));
    rootData->account()->setEmail(QStringLiteral("remote.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Remote Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Remote Trip"));

    trip->addNewChunk();
    auto* chunk = trip->chunk(0);
    REQUIRE(chunk != nullptr);
    chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("A1"));
    chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("A2"));
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("10.0"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("45.0"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0.5"));
    chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    QTemporaryDir saveRoot;
    REQUIRE(saveRoot.isValid());
    const QString projectPath = QDir(saveRoot.path()).filePath(QStringLiteral("with-remote-sync.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    REQUIRE(project->save());
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());
    CHECK_FALSE(repository->remotes().isEmpty());

    cave->setName(QStringLiteral("Remote Cave Updated"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    CHECK(project->errorModel()->count() == 0);

    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, QFileInfo(project->filename()).absoluteDir().absolutePath().toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    git_reference* localHeadRef = nullptr;
    REQUIRE(git_repository_head(&localHeadRef, localRepo) == GIT_OK);
    auto localHeadRefGuard = qScopeGuard([&localHeadRef]() {
        if (localHeadRef) {
            git_reference_free(localHeadRef);
        }
    });
    const git_oid* localHeadOid = git_reference_target(localHeadRef);
    REQUIRE(localHeadOid != nullptr);

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteHeadRef = nullptr;
    REQUIRE(git_reference_lookup(&remoteHeadRef, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteHeadRefGuard = qScopeGuard([&remoteHeadRef]() {
        if (remoteHeadRef) {
            git_reference_free(remoteHeadRef);
        }
    });
    const git_oid* remoteHeadOid = git_reference_target(remoteHeadRef);
    REQUIRE(remoteHeadOid != nullptr);
    CHECK(git_oid_cmp(localHeadOid, remoteHeadOid) == 0);
}

TEST_CASE("cwProject sync from unborn head creates first commit and pushes branch", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    const QString localFilePath = QDir(localRepoPath).filePath(QStringLiteral("unborn-sync.txt"));

    QFile localFile(localFilePath);
    REQUIRE(localFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(localFile.write("first sync\n") == 11);
    localFile.close();

    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    git_reference* preSyncHead = nullptr;
    const int preSyncHeadError = git_repository_head(&preSyncHead, localRepo);
    if (preSyncHead) {
        git_reference_free(preSyncHead);
        preSyncHead = nullptr;
    }
    CHECK((preSyncHeadError == GIT_EUNBORNBRANCH || preSyncHeadError == GIT_ENOTFOUND));

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    CHECK(project->errorModel()->count() == 0);

    git_reference* localHead = nullptr;
    REQUIRE(git_repository_head(&localHead, localRepo) == GIT_OK);
    auto localHeadGuard = qScopeGuard([&localHead]() {
        if (localHead) {
            git_reference_free(localHead);
        }
    });

    const git_oid* localHeadOid = git_reference_target(localHead);
    REQUIRE(localHeadOid != nullptr);

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&remoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
        if (remoteBranch) {
            git_reference_free(remoteBranch);
        }
    });

    const git_oid* remoteHeadOid = git_reference_target(remoteBranch);
    REQUIRE(remoteHeadOid != nullptr);
    CHECK(git_oid_cmp(localHeadOid, remoteHeadOid) == 0);
}

TEST_CASE("cwProject sync tracks png pdf and glb via LFS", "[cwProject][lfs]") {
    if (!cwProject::supportedImageFormats().contains(QStringLiteral("pdf"))) {
        SUCCEED("PDF support not available in this build");
        return;
    }

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("LFS Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("lfs.sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("LFS Cave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("LFS Trip"));

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    const QString pdfSource = copyToTempFolder("://datasets/test_cwPDFConverter/2page-test.pdf");
    const QString glbSource = copyToTempFolder("://datasets/test_cwSurveyNotesConcatModel/bones.glb");
    REQUIRE(QFileInfo::exists(pngSource));
    REQUIRE(QFileInfo::exists(pdfSource));
    REQUIRE(QFileInfo::exists(glbSource));

    QFile pngSourceFile(pngSource);
    REQUIRE(pngSourceFile.open(QIODevice::ReadOnly));
    const QByteArray pngSourceBytes = pngSourceFile.readAll();
    REQUIRE_FALSE(pngSourceBytes.isEmpty());

    QFile pdfSourceFile(pdfSource);
    REQUIRE(pdfSourceFile.open(QIODevice::ReadOnly));
    const QByteArray pdfSourceBytes = pdfSourceFile.readAll();
    REQUIRE_FALSE(pdfSourceBytes.isEmpty());

    QFile glbSourceFile(glbSource);
    REQUIRE(glbSourceFile.open(QIODevice::ReadOnly));
    const QByteArray glbSourceBytes = glbSourceFile.readAll();
    REQUIRE_FALSE(glbSourceBytes.isEmpty());

    trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pdfSource)});
    trip->notesLiDAR()->addFromFiles({QUrl::fromLocalFile(glbSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QList<cwNote*> notes = trip->notes()->notes();
    REQUIRE(notes.size() >= 3);

    cwNote* pngNote = nullptr;
    cwNote* pdfNote = nullptr;
    for (cwNote* note : notes) {
        REQUIRE(note != nullptr);
        const QString suffix = QFileInfo(note->image().path()).suffix().toLower();
        if (suffix == QStringLiteral("png") && pngNote == nullptr) {
            pngNote = note;
        }
        if (suffix == QStringLiteral("pdf") && pdfNote == nullptr) {
            pdfNote = note;
        }
    }
    REQUIRE(pngNote != nullptr);
    REQUIRE(pdfNote != nullptr);

    REQUIRE(trip->notesLiDAR()->rowCount() == 1);
    auto* glbNote = dynamic_cast<cwNoteLiDAR*>(trip->notesLiDAR()->notes().first());
    REQUIRE(glbNote != nullptr);

    const QString pngPath = ProjectFilenameTestHelper::absolutePath(pngNote, pngNote->image().path());
    const QString pdfPath = ProjectFilenameTestHelper::absolutePath(pdfNote, pdfNote->image().path());
    const QString glbPath = ProjectFilenameTestHelper::absolutePath(glbNote, glbNote->filename());
    REQUIRE(QFileInfo::exists(pngPath));
    REQUIRE(QFileInfo::exists(pdfPath));
    REQUIRE(QFileInfo::exists(glbPath));

    QFile pngProjectFile(pngPath);
    REQUIRE(pngProjectFile.open(QIODevice::ReadOnly));
    CHECK(pngProjectFile.readAll() == pngSourceBytes);

    QFile pdfProjectFile(pdfPath);
    REQUIRE(pdfProjectFile.open(QIODevice::ReadOnly));
    CHECK(pdfProjectFile.readAll() == pdfSourceBytes);

    QFile glbProjectFile(glbPath);
    REQUIRE(glbProjectFile.open(QIODevice::ReadOnly));
    CHECK(glbProjectFile.readAll() == glbSourceBytes);

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() > 0);
    const auto syncError = project->errorModel()->last();
    CHECK(syncError.type() == cwError::Warning);
    CHECK(syncError.message() == QStringLiteral("No git remote is configured for this project."));

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    const QString attributesPath = QDir(localRepoPath).filePath(QStringLiteral(".gitattributes"));
    QFile attributesFile(attributesPath);
    REQUIRE(attributesFile.open(QIODevice::ReadOnly));
    const QString attributes = QString::fromUtf8(attributesFile.readAll());
    CHECK(attributes.contains(QStringLiteral("*.png filter=lfs diff=lfs merge=lfs -text")));
    CHECK(attributes.contains(QStringLiteral("*.pdf filter=lfs diff=lfs merge=lfs -text")));
    CHECK(attributes.contains(QStringLiteral("*.glb filter=lfs diff=lfs merge=lfs -text")));

    git_repository* repo = nullptr;
    REQUIRE(git_repository_open(&repo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto repoGuard = qScopeGuard([&repo]() {
        if (repo) {
            git_repository_free(repo);
        }
    });

    const QString gitDirPath = gitDirPathFromRepository(repo);
    REQUIRE_FALSE(gitDirPath.isEmpty());

    const auto verifyLfsPointer = [&](const QString& absolutePath) {
        const QString relativePath = QDir(localRepoPath).relativeFilePath(absolutePath);
        const QByteArray blobData = readBlobFromHead(repo, relativePath);
        REQUIRE_FALSE(blobData.isEmpty());

        QQuickGit::LfsPointer pointer;
        REQUIRE(QQuickGit::LfsPointer::parse(blobData, &pointer));
        CHECK(pointer.isValid());
        CHECK(pointer.size == QFileInfo(absolutePath).size());

        const QString objectPath = QQuickGit::LfsStore::objectPath(gitDirPath, pointer.oid);
        REQUIRE_FALSE(objectPath.isEmpty());
        CHECK(QFileInfo::exists(objectPath));
    };

    verifyLfsPointer(pngPath);
    verifyLfsPointer(pdfPath);
    verifyLfsPointer(glbPath);
}

TEST_CASE("cwProject sync uploads LFS objects through test LFS server", "[cwProject][lfs][sync]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("LFS E2E Tester"));
    rootData->account()->setEmail(QStringLiteral("lfs.e2e.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("LFS Sync Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("LFS Sync Trip"));

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    QTemporaryDir lowerCaseImageRoot;
    REQUIRE(lowerCaseImageRoot.isValid());
    const QString lowerCasePngPath = QDir(lowerCaseImageRoot.path()).filePath(QStringLiteral("lfs-sync-upload.png"));
    REQUIRE(QFile::copy(pngSource, lowerCasePngPath));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(lowerCasePngPath)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    QTemporaryDir saveRoot;
    REQUIRE(saveRoot.isValid());
    const QString projectPath = QDir(saveRoot.path()).filePath(QStringLiteral("lfs-e2e-sync.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    REQUIRE(project->save());
    project->waitSaveToFinish();

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote-lfs.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);
    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    auto* note = trip->notes()->notes().first();
    REQUIRE(note != nullptr);
    const QString noteImagePath = ProjectFilenameTestHelper::absolutePath(note, note->image().path());
    REQUIRE(QFileInfo::exists(noteImagePath));
    qDebug() << "[cwProject LFS sync test] note image path:" << noteImagePath;
    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    QFile noteImageFile(noteImagePath);
    REQUIRE(noteImageFile.open(QIODevice::ReadOnly));
    const QByteArray noteImageBytes = noteImageFile.readAll();
    REQUIRE_FALSE(noteImageBytes.isEmpty());
    const QString expectedOid = QString::fromLatin1(
        QCryptographicHash::hash(noteImageBytes, QCryptographicHash::Sha256).toHex());
    const qint64 expectedSize = noteImageBytes.size();
    qDebug() << "[cwProject LFS sync test] expected upload oid/size:" << expectedOid << expectedSize;

    const QString relativeImagePath = QDir(localRepoPath).relativeFilePath(noteImagePath);
    const QByteArray localHeadBlob = readBlobFromHead(localRepo, relativeImagePath);
    QQuickGit::LfsPointer committedPointer;
    const bool committedAsPointer = QQuickGit::LfsPointer::parse(localHeadBlob, &committedPointer);
    qDebug() << "[cwProject LFS sync test] head blob bytes:" << localHeadBlob.size()
             << "relativePath=" << relativeImagePath
             << "isPointer=" << committedAsPointer
             << "pointerOid=" << committedPointer.oid
             << "pointerSize=" << committedPointer.size;

    LfsServer lfsServer;
    if (!lfsServer.start()) {
        SKIP("LFS test server could not bind a local TCP port in this environment.");
        return;
    }
    lfsServer.setExpectedUploadObject(expectedOid, expectedSize);

    REQUIRE(setGitConfigString(localRepoPath,
                               "lfs.url",
                               lfsServer.endpoint()));
    qDebug() << "[cwProject LFS sync test] configured lfs.url =" << lfsServer.endpoint();

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    CHECK(lfsServer.uploadBatchRequestCount() > 0);
    CHECK(lfsServer.uploadRequestCount() > 0);

    git_reference* localHeadRef = nullptr;
    REQUIRE(git_repository_head(&localHeadRef, localRepo) == GIT_OK);
    auto localHeadRefGuard = qScopeGuard([&localHeadRef]() {
        if (localHeadRef) {
            git_reference_free(localHeadRef);
        }
    });
    const git_oid* localHeadOid = git_reference_target(localHeadRef);
    REQUIRE(localHeadOid != nullptr);

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteHeadRef = nullptr;
    REQUIRE(git_reference_lookup(&remoteHeadRef, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteHeadRefGuard = qScopeGuard([&remoteHeadRef]() {
        if (remoteHeadRef) {
            git_reference_free(remoteHeadRef);
        }
    });
    const git_oid* remoteHeadOid = git_reference_target(remoteHeadRef);
    REQUIRE(remoteHeadOid != nullptr);
    CHECK(git_oid_cmp(localHeadOid, remoteHeadOid) == 0);
}

TEST_CASE("cwProject sync hydrates pulled LFS objects from test LFS server", "[cwProject][lfs][sync][hydration]") {
    QSettings settings;
    settings.clear();

    auto authorRoot = std::make_unique<cwRootData>();
    auto* authorProject = authorRoot->project();
    authorRoot->account()->setName(QStringLiteral("LFS Hydration Author"));
    authorRoot->account()->setEmail(QStringLiteral("lfs.hydration.author@example.com"));

    auto* region = authorProject->cavingRegion();
    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Hydration Cave"));
    cave->addTrip();
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Hydration Trip"));

    const QString seedPngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(seedPngSource));
    QTemporaryDir authorLowerCaseRoot;
    REQUIRE(authorLowerCaseRoot.isValid());
    const QString seedPngPath = QDir(authorLowerCaseRoot.path()).filePath(QStringLiteral("hydration-seed.png"));
    REQUIRE(QFile::copy(seedPngSource, seedPngPath));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(seedPngPath)});
    authorRoot->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    QTemporaryDir authorSaveRoot;
    REQUIRE(authorSaveRoot.isValid());
    const QString authorProjectPath = QDir(authorSaveRoot.path()).filePath(QStringLiteral("lfs-hydration-author.cwproj"));
    REQUIRE(authorProject->saveAs(authorProjectPath));
    authorProject->waitSaveToFinish();
    REQUIRE(authorProject->save());
    authorProject->waitSaveToFinish();

    const QString authorRepoPath = QFileInfo(authorProject->filename()).absoluteDir().absolutePath();
    auto* authorRepository = authorProject->repository();
    REQUIRE(authorRepository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("hydration-remote.git"));
    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = authorRepository->addRemote(QStringLiteral("origin"),
                                                               QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    auto* seedNote = trip->notes()->notes().first();
    REQUIRE(seedNote != nullptr);
    const QString seedNotePath = ProjectFilenameTestHelper::absolutePath(seedNote, seedNote->image().path());
    REQUIRE(QFileInfo::exists(seedNotePath));
    QFile seedNoteFile(seedNotePath);
    REQUIRE(seedNoteFile.open(QIODevice::ReadOnly));
    const QByteArray seedBytes = seedNoteFile.readAll();
    REQUIRE_FALSE(seedBytes.isEmpty());
    const QString seedOid = QString::fromLatin1(QCryptographicHash::hash(seedBytes, QCryptographicHash::Sha256).toHex());

    LfsServer lfsServer;
    if (!lfsServer.start()) {
        SKIP("LFS test server could not bind a local TCP port in this environment.");
        return;
    }
    lfsServer.setExpectedUploadObject(seedOid, seedBytes.size());
    lfsServer.setDownloadObject(seedOid, seedBytes);
    REQUIRE(setGitConfigString(authorRepoPath, "lfs.url", lfsServer.endpoint()));

    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRoot->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    CHECK(authorProject->errorModel()->count() == 0);
    CHECK(lfsServer.uploadBatchRequestCount() > 0);
    CHECK(lfsServer.uploadRequestCount() > 0);

    auto consumerRoot = std::make_unique<cwRootData>();
    auto* consumerProject = consumerRoot->project();
    consumerRoot->account()->setName(QStringLiteral("LFS Hydration Consumer"));
    consumerRoot->account()->setEmail(QStringLiteral("lfs.hydration.consumer@example.com"));

    QTemporaryDir cloneRoot;
    REQUIRE(cloneRoot.isValid());
    auto* repositoryModel = consumerRoot->repositoryModel();
    REQUIRE(repositoryModel != nullptr);
    repositoryModel->setDefaultRepositoryDir(QUrl::fromLocalFile(cloneRoot.path()));
    const int repositoryCountBeforeClone = repositoryModel->rowCount();

    QQuickGit::GitFutureWatcher cloneWatcher;
    cwRemoteRepositoryCloner cloner;
    cloner.setRepositoryModel(repositoryModel);
    cloner.setCloneWatcher(&cloneWatcher);
    cloner.setAccount(consumerRoot->account());

    const QString cloneUrl = QUrl::fromLocalFile(remoteRepoPath).toString();
    cloner.clone(cloneUrl);

    QEventLoop cloneLoop;
    QTimer cloneTimeout;
    cloneTimeout.setSingleShot(true);
    QObject::connect(&cloneTimeout, &QTimer::timeout, &cloneLoop, &QEventLoop::quit);
    QObject::connect(&cloneWatcher, &QQuickGit::GitFutureWatcher::stateChanged, &cloneLoop, [&cloneWatcher, &cloneLoop]() {
        if (cloneWatcher.state() == QQuickGit::GitFutureWatcher::Ready) {
            cloneLoop.quit();
        }
    });
    cloneTimeout.start(20000);
    cloneLoop.exec();

    REQUIRE(cloneWatcher.state() == QQuickGit::GitFutureWatcher::Ready);
    REQUIRE_FALSE(cloneWatcher.hasError());
    REQUIRE(repositoryModel->rowCount() == repositoryCountBeforeClone + 1);

    const QString clonedRepoName = cloner.repositoryNameFromUrl(cloneUrl);
    const QString clonedRepoPath = QDir(cloneRoot.path()).filePath(clonedRepoName);
    REQUIRE(QDir(clonedRepoPath).exists());
    REQUIRE(setGitConfigString(clonedRepoPath, "lfs.url", lfsServer.endpoint()));

    int clonedIndex = -1;
    for (int i = 0; i < repositoryModel->rowCount(); ++i) {
        const QString path = repositoryModel->data(repositoryModel->index(i, 0), cwRepositoryModel::PathRole).toString();
        if (QDir(path).absolutePath() == QDir(clonedRepoPath).absolutePath()) {
            clonedIndex = i;
            break;
        }
    }
    REQUIRE(clonedIndex >= 0);

    const auto openResult = repositoryModel->openRepository(clonedIndex, consumerProject);
    INFO(openResult.errorMessage().toStdString());
    REQUIRE_FALSE(openResult.hasError());
    consumerRoot->futureManagerModel()->waitForFinished();
    consumerProject->waitLoadToFinish();
    consumerProject->waitSaveToFinish();

    const QString updatePngSource = copyToTempFolder("://datasets/test_cwNote/testpage.png");
    REQUIRE(QFileInfo::exists(updatePngSource));
    const QString updatePngPath = QDir(authorLowerCaseRoot.path()).filePath(QStringLiteral("hydration-update.png"));
    REQUIRE(QFile::copy(updatePngSource, updatePngPath));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(updatePngPath)});
    authorRoot->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 2);

    cwNote* updateNote = nullptr;
    for (cwNote* note : trip->notes()->notes()) {
        if (note != nullptr && note->image().path().endsWith(QStringLiteral("hydration-update.png"), Qt::CaseInsensitive)) {
            updateNote = note;
            break;
        }
    }
    REQUIRE(updateNote != nullptr);
    const QString updateNotePath = ProjectFilenameTestHelper::absolutePath(updateNote, updateNote->image().path());
    REQUIRE(QFileInfo::exists(updateNotePath));
    const QString updateRelativePath = QDir(authorRepoPath).relativeFilePath(updateNotePath);
    QFile updateNoteFile(updateNotePath);
    REQUIRE(updateNoteFile.open(QIODevice::ReadOnly));
    const QByteArray updateBytes = updateNoteFile.readAll();
    REQUIRE_FALSE(updateBytes.isEmpty());
    const QString updateOid = QString::fromLatin1(QCryptographicHash::hash(updateBytes, QCryptographicHash::Sha256).toHex());
    lfsServer.setExpectedUploadObject(updateOid, updateBytes.size());
    lfsServer.setDownloadObject(updateOid, updateBytes);

    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRoot->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    CHECK(authorProject->errorModel()->count() == 0);

    consumerProject->errorModel()->clear();
    REQUIRE(consumerProject->sync());
    consumerRoot->futureManagerModel()->waitForFinished();
    consumerProject->waitSaveToFinish();
    qDebug() << "[cwProject LFS hydration test] consumer sync error count:"
             << consumerProject->errorModel()->count();
    for (int i = 0; i < consumerProject->errorModel()->count(); ++i) {
        const cwError err = consumerProject->errorModel()->at(i);
        qDebug() << "[cwProject LFS hydration test] consumer sync error"
                 << i
                 << "type=" << err.type()
                 << "message=" << err.message();
    }
    CHECK(consumerProject->errorModel()->count() == 0);

    qDebug() << "[cwProject LFS hydration test] pulled cave count:"
             << consumerProject->cavingRegion()->caveCount();
    REQUIRE(consumerProject->cavingRegion()->caveCount() > 0);
    auto* pulledCave = consumerProject->cavingRegion()->cave(0);
    REQUIRE(pulledCave != nullptr);
    qDebug() << "[cwProject LFS hydration test] pulled cave name:"
             << pulledCave->name()
             << "trip count=" << pulledCave->tripCount();
    REQUIRE(pulledCave->tripCount() > 0);
    auto* pulledTrip = pulledCave->trip(0);
    REQUIRE(pulledTrip != nullptr);
    qDebug() << "[cwProject LFS hydration test] pulled trip name:"
             << pulledTrip->name()
             << "note count=" << pulledTrip->notes()->rowCount();
    for (cwNote* note : pulledTrip->notes()->notes()) {
        if (!note) {
            qDebug() << "[cwProject LFS hydration test] pulled note is null";
            continue;
        }
        qDebug() << "[cwProject LFS hydration test] pulled note path:"
                 << note->image().path();
    }

    cwNote* pulledUpdateNote = nullptr;
    for (cwNote* note : pulledTrip->notes()->notes()) {
        if (note != nullptr && note->image().path().endsWith(QStringLiteral("hydration-update.png"), Qt::CaseInsensitive)) {
            pulledUpdateNote = note;
            break;
        }
    }
    CHECK(pulledUpdateNote != nullptr); // Exposes current model-refresh gap after merge/sync.

    const QString pulledUpdatePath = QDir(clonedRepoPath).filePath(updateRelativePath);
    qDebug() << "[cwProject LFS hydration test] pulled update path from git working tree:"
             << pulledUpdatePath
             << "exists=" << QFileInfo::exists(pulledUpdatePath);
    REQUIRE(QFileInfo::exists(pulledUpdatePath));
    QFile pulledUpdateFile(pulledUpdatePath);
    REQUIRE(pulledUpdateFile.open(QIODevice::ReadOnly));
    const QByteArray pulledBytes = pulledUpdateFile.readAll();
    REQUIRE_FALSE(pulledBytes.isEmpty());

    QQuickGit::LfsPointer parsedPointer;
    CHECK_FALSE(QQuickGit::LfsPointer::parse(pulledBytes, &parsedPointer));
    CHECK(pulledBytes == updateBytes);

    CHECK(lfsServer.downloadBatchRequestCount() > 0);
    CHECK(lfsServer.downloadObjectRequestCount() > 0);
}

TEST_CASE("cwProject sync pulls remote-only changes into a clean local repo", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Pull Only Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-pull-only.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    cave->setName(QStringLiteral("Pull Only Cave Baseline"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);
    CHECK(project->isModified() == false);

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    auto headOid = [](git_repository* repoInstance) -> git_oid {
        git_reference* head = nullptr;
        REQUIRE(git_repository_head(&head, repoInstance) == GIT_OK);
        auto headGuard = qScopeGuard([&head]() {
            if (head) {
                git_reference_free(head);
            }
        });
        const git_oid* target = git_reference_target(head);
        REQUIRE(target != nullptr);
        return *target;
    };

    const git_oid beforePullHead = headOid(localRepo);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    const QString pulledFileName = QStringLiteral("remote-only.txt");
    const QString cloneFilePath = QDir(clonePath).filePath(pulledFileName);
    QFile cloneFile(cloneFilePath);
    REQUIRE(cloneFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(cloneFile.write("remote commit content\n") == 22);
    cloneFile.close();

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Remote only change"),
                                              QStringLiteral("pull-only")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&remoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
        if (remoteBranch) {
            git_reference_free(remoteBranch);
        }
    });
    const git_oid* remoteHeadOid = git_reference_target(remoteBranch);
    REQUIRE(remoteHeadOid != nullptr);

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    const auto syncReport = project->lastSyncReport();
    REQUIRE(syncReport.has_value());
    const bool isExpectedPullState =
        syncReport->pullState == cwSaveLoad::SyncReport::PullState::FastForward
        || syncReport->pullState == cwSaveLoad::SyncReport::PullState::MergeCommitCreated;
    CHECK(isExpectedPullState);
    CHECK(syncReport->beforeHead != syncReport->afterHead);
    CHECK(syncReport->hasCommitDiffPaths);
    CHECK(std::any_of(syncReport->commitDiffPaths.begin(),
                      syncReport->commitDiffPaths.end(),
                      [&pulledFileName](const QString& path) {
                          return path.endsWith(pulledFileName, Qt::CaseInsensitive);
                      }));
    CHECK(std::any_of(syncReport->changedPaths.begin(),
                      syncReport->changedPaths.end(),
                      [&pulledFileName](const QString& path) {
                          return path.endsWith(pulledFileName, Qt::CaseInsensitive);
                      }));

    const git_oid afterPullHead = headOid(localRepo);
    CHECK(git_oid_cmp(&beforePullHead, &afterPullHead) != 0);
    CHECK(git_oid_cmp(&afterPullHead, remoteHeadOid) == 0);

    git_commit* headCommit = nullptr;
    REQUIRE(git_commit_lookup(&headCommit, localRepo, &afterPullHead) == GIT_OK);
    auto headCommitGuard = qScopeGuard([&headCommit]() {
        if (headCommit) {
            git_commit_free(headCommit);
        }
    });
    CHECK(git_commit_parentcount(headCommit) == 1);
    CHECK(QString::fromUtf8(git_commit_message(headCommit))
              == QStringLiteral("Remote only change\n\npull-only"));

    const QString pulledFilePath = QDir(localRepoPath).filePath(pulledFileName);
    QFile pulledFile(pulledFilePath);
    REQUIRE(pulledFile.open(QIODevice::ReadOnly));
    CHECK(pulledFile.readAll() == QByteArray("remote commit content\n"));
}

TEST_CASE("cwProject sync reconciles pulled cave updates into memory", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Reconcile Baseline Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-reconcile.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    auto remoteRootData = std::make_unique<cwRootData>();
    remoteRootData->account()->setName(QStringLiteral("Remote Sync Tester"));
    remoteRootData->account()->setEmail(QStringLiteral("remote.sync.tester@example.com"));

    auto remoteProject = remoteRootData->project();
    const QString clonedProjectPath = QDir(clonePath).filePath(QFileInfo(projectPath).fileName());
    REQUIRE(QFileInfo::exists(clonedProjectPath));
    remoteProject->loadFile(clonedProjectPath);
    remoteProject->waitLoadToFinish();
    remoteProject->waitSaveToFinish();

    auto* remoteRepository = remoteProject->repository();
    REQUIRE(remoteRepository != nullptr);
    remoteRepository->setAccount(remoteRootData->account());

    auto* remoteCave = remoteProject->cavingRegion()->cave(0);
    REQUIRE(remoteCave != nullptr);
    const QString remoteCaveName = QStringLiteral("Reconcile Updated Cave");
    remoteCave->setName(remoteCaveName);
    remoteProject->waitSaveToFinish();
    REQUIRE(remoteProject->isModified());

    remoteProject->errorModel()->clear();
    REQUIRE(remoteProject->sync());
    remoteRootData->futureManagerModel()->waitForFinished();
    remoteProject->waitSaveToFinish();
    CHECK(remoteProject->errorModel()->count() == 0);

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    auto* localCave = project->cavingRegion()->cave(0);
    REQUIRE(localCave != nullptr);
    CHECK(localCave->name() == remoteCaveName);
}

TEST_CASE("cwProject sync incrementally reconciles pulled note updates without replacing cave and trip objects", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Incremental Note Reconcile Cave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Incremental Note Reconcile Trip"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-note-incremental.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QPointer<cwCave> localCavePtr = project->cavingRegion()->cave(0);
    REQUIRE(localCavePtr != nullptr);
    QPointer<cwTrip> localTripPtr = localCavePtr->trip(0);
    REQUIRE(localTripPtr != nullptr);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    auto remoteRootData = std::make_unique<cwRootData>();
    remoteRootData->account()->setName(QStringLiteral("Remote Sync Tester"));
    remoteRootData->account()->setEmail(QStringLiteral("remote.sync.tester@example.com"));

    auto remoteProject = remoteRootData->project();
    const QString clonedProjectPath = QDir(clonePath).filePath(QFileInfo(projectPath).fileName());
    REQUIRE(QFileInfo::exists(clonedProjectPath));
    remoteProject->loadFile(clonedProjectPath);
    remoteProject->waitLoadToFinish();
    remoteProject->waitSaveToFinish();

    auto* remoteRepository = remoteProject->repository();
    REQUIRE(remoteRepository != nullptr);
    remoteRepository->setAccount(remoteRootData->account());

    auto* remoteTrip = remoteProject->cavingRegion()->cave(0)->trip(0);
    REQUIRE(remoteTrip != nullptr);
    REQUIRE(remoteTrip->notes()->rowCount() == 1);
    auto* remoteNote = remoteTrip->notes()->notes().first();
    REQUIRE(remoteNote != nullptr);

    const QString remoteNoteName = QStringLiteral("Incrementally Updated Remote Note");
    constexpr double remoteNoteRotation = 17.5;
    remoteNote->setName(remoteNoteName);
    remoteNote->setRotate(remoteNoteRotation);
    remoteProject->waitSaveToFinish();
    REQUIRE(remoteProject->isModified());

    remoteProject->errorModel()->clear();
    REQUIRE(remoteProject->sync());
    remoteRootData->futureManagerModel()->waitForFinished();
    remoteProject->waitSaveToFinish();
    CHECK(remoteProject->errorModel()->count() == 0);

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    REQUIRE(localCavePtr != nullptr);
    REQUIRE(localTripPtr != nullptr);
    CHECK(project->cavingRegion()->cave(0) == localCavePtr.data());
    CHECK(localCavePtr->trip(0) == localTripPtr.data());

    REQUIRE(localTripPtr->notes()->rowCount() == 1);
    auto* localNote = localTripPtr->notes()->notes().first();
    REQUIRE(localNote != nullptr);
    CHECK(localNote->name() == remoteNoteName);
    CHECK(localNote->rotate() == Catch::Approx(remoteNoteRotation));

    const auto syncReport = project->lastSyncReport();
    REQUIRE(syncReport.has_value());
    CHECK(std::any_of(syncReport->changedPaths.cbegin(),
                      syncReport->changedPaths.cend(),
                      [](const QString& path) {
                          return path.endsWith(QStringLiteral(".cwnote"), Qt::CaseInsensitive);
                      }));
}

TEST_CASE("cwProject sync structurally reconciles note scraps by id without replacing note object", "[cwProject][sync]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Structural Note Reconcile Cave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Structural Note Reconcile Trip"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-note-structural.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    auto* localNote = trip->notes()->notes().first();
    REQUIRE(localNote != nullptr);

    auto addTriangleScrap = [](cwNote* note, const QPointF& origin) -> cwScrap* {
        auto* scrap = new cwScrap();
        scrap->insertPoint(0, origin + QPointF(0.10, 0.10));
        scrap->insertPoint(1, origin + QPointF(0.35, 0.10));
        scrap->insertPoint(2, origin + QPointF(0.22, 0.35));
        scrap->close();
        note->addScrap(scrap);
        return scrap;
    };

    cwScrap* const localFirstScrap = addTriangleScrap(localNote, QPointF(0.0, 0.0));
    cwScrap* const localSecondScrap = addTriangleScrap(localNote, QPointF(0.5, 0.0));
    REQUIRE(localFirstScrap != nullptr);
    REQUIRE(localSecondScrap != nullptr);

    QPointer<cwTrip> localTripPtr = trip;
    QPointer<cwNote> localNotePtr = localNote;

    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    QDirIterator noteFileIterator(clonePath,
                                  QStringList{QStringLiteral("*.cwnote")},
                                  QDir::Files,
                                  QDirIterator::Subdirectories);
    REQUIRE(noteFileIterator.hasNext());
    const QString clonedNotePath = noteFileIterator.next();

    auto noteProto = loadProtoFromJsonFile<CavewhereProto::Note>(clonedNotePath);
    REQUIRE(noteProto.scraps_size() == 2);
    REQUIRE(noteProto.scraps(0).has_id());
    REQUIRE(noteProto.scraps(1).has_id());

    const QUuid reorderedFirstScrapId = QUuid(QString::fromStdString(noteProto.scraps(1).id()));
    REQUIRE(!reorderedFirstScrapId.isNull());

    CavewhereProto::Scrap originalFirstScrap = noteProto.scraps(0);
    *noteProto.mutable_scraps(0) = noteProto.scraps(1);
    *noteProto.mutable_scraps(1) = originalFirstScrap;

    REQUIRE(noteProto.scraps(0).outlinepoints_size() > 1);
    auto* updatedOutlinePoint = noteProto.mutable_scraps(0)->mutable_outlinepoints(1);
    updatedOutlinePoint->set_x(0.39);
    updatedOutlinePoint->set_y(0.14);

    writeProtoToJsonFile(clonedNotePath, noteProto);

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Reorder scraps"),
                                              QStringLiteral("swap scrap order and mutate geometry")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    REQUIRE(localTripPtr != nullptr);
    REQUIRE(localNotePtr != nullptr);
    CHECK(localTripPtr->notes()->notes().first() == localNotePtr.data());

    REQUIRE(localNotePtr->scraps().size() == 2);
    CHECK(localNotePtr->scrap(0)->id() == reorderedFirstScrapId);
    CHECK(localNotePtr->scrap(1)->id() != reorderedFirstScrapId);

    auto findScrapById = [](cwNote* note, const QUuid& id) -> cwScrap* {
        for (cwScrap* scrap : note->scraps()) {
            if (scrap != nullptr && scrap->id() == id) {
                return scrap;
            }
        }
        return nullptr;
    };

    cwScrap* const reorderedScrapAfterSync = findScrapById(localNotePtr, reorderedFirstScrapId);
    REQUIRE(reorderedScrapAfterSync != nullptr);
    REQUIRE(reorderedScrapAfterSync->points().size() > 1);
    CHECK(reorderedScrapAfterSync->points().at(1) == QPointF(0.39, 0.14));

    const auto syncReport = project->lastSyncReport();
    REQUIRE(syncReport.has_value());
    CHECK(std::any_of(syncReport->diagnostics.cbegin(),
                      syncReport->diagnostics.cend(),
                      [](const QString& diagnostic) {
                          return diagnostic.contains(QStringLiteral("reconcile handler cwNoteSyncMergeHandler applied"));
                      }));
}

TEST_CASE("cwProject sync incrementally reconciles pulled scrap station updates", "[cwProject][sync]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Station Reconcile Cave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Station Reconcile Trip"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-station-reconcile.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    auto* localNote = trip->notes()->notes().first();
    REQUIRE(localNote != nullptr);

    auto* localScrap = new cwScrap();
    localScrap->insertPoint(0, QPointF(0.10, 0.10));
    localScrap->insertPoint(1, QPointF(0.40, 0.10));
    localScrap->insertPoint(2, QPointF(0.20, 0.40));
    localScrap->close();

    cwNoteStation initialStation;
    initialStation.setName(QStringLiteral("A1"));
    initialStation.setPositionOnNote(QPointF(0.20, 0.20));
    localScrap->addStation(initialStation);
    localNote->addScrap(localScrap);

    QPointer<cwNote> localNotePtr = localNote;
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    QDirIterator noteFileIterator(clonePath,
                                  QStringList{QStringLiteral("*.cwnote")},
                                  QDir::Files,
                                  QDirIterator::Subdirectories);
    REQUIRE(noteFileIterator.hasNext());
    const QString clonedNotePath = noteFileIterator.next();

    auto noteProto = loadProtoFromJsonFile<CavewhereProto::Note>(clonedNotePath);
    REQUIRE(noteProto.scraps_size() == 1);
    REQUIRE(noteProto.scraps(0).notestations_size() == 1);

    auto* protoStation = noteProto.mutable_scraps(0)->mutable_notestations(0);
    protoStation->set_name(QStringLiteral("A1-remote").toStdString());
    protoStation->mutable_positiononnote()->set_x(0.44);
    protoStation->mutable_positiononnote()->set_y(0.33);

    writeProtoToJsonFile(clonedNotePath, noteProto);

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Update scrap station"),
                                              QStringLiteral("mutate station name and position in note payload")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    REQUIRE(localNotePtr != nullptr);
    REQUIRE(localNotePtr->scraps().size() == 1);
    cwScrap* const syncedScrap = localNotePtr->scrap(0);
    REQUIRE(syncedScrap != nullptr);
    REQUIRE(syncedScrap->numberOfStations() == 1);
    CHECK(syncedScrap->stationData(cwScrap::StationName, 0).toString() == QStringLiteral("A1-remote"));
    CHECK(syncedScrap->stationData(cwScrap::StationPosition, 0).toPointF() == QPointF(0.44, 0.33));
}

TEST_CASE("cwProject sync falls back to full reconcile for ambiguous scrap structural mapping", "[cwProject][sync]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Ambiguous Scrap Merge Cave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Ambiguous Scrap Merge Trip"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-ambiguous-scrap.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    auto* localNote = trip->notes()->notes().first();
    REQUIRE(localNote != nullptr);

    auto addTriangleScrap = [](cwNote* note, const QPointF& origin) -> cwScrap* {
        auto* scrap = new cwScrap();
        scrap->insertPoint(0, origin + QPointF(0.10, 0.10));
        scrap->insertPoint(1, origin + QPointF(0.35, 0.10));
        scrap->insertPoint(2, origin + QPointF(0.22, 0.35));
        scrap->close();
        note->addScrap(scrap);
        return scrap;
    };

    cwScrap* const firstScrap = addTriangleScrap(localNote, QPointF(0.0, 0.0));
    cwScrap* const secondScrap = addTriangleScrap(localNote, QPointF(0.5, 0.0));
    REQUIRE(firstScrap != nullptr);
    REQUIRE(secondScrap != nullptr);
    REQUIRE(!firstScrap->id().isNull());
    secondScrap->setId(firstScrap->id());
    CHECK(secondScrap->id() == firstScrap->id());

    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    QDirIterator noteFileIterator(clonePath,
                                  QStringList{QStringLiteral("*.cwnote")},
                                  QDir::Files,
                                  QDirIterator::Subdirectories);
    REQUIRE(noteFileIterator.hasNext());
    const QString clonedNotePath = noteFileIterator.next();

    auto noteProto = loadProtoFromJsonFile<CavewhereProto::Note>(clonedNotePath);
    const QString remoteName = QStringLiteral("Remote rename with ambiguous local scrap ids");
    noteProto.set_name(remoteName.toStdString());
    writeProtoToJsonFile(clonedNotePath, noteProto);

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Rename note"),
                                              QStringLiteral("change note metadata only")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    QPointer<cwCave> localCavePtr = cave;
    QPointer<cwTrip> localTripPtr = trip;
    QPointer<cwNote> localNotePtr = localNote;

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    auto* syncedCave = project->cavingRegion()->cave(0);
    REQUIRE(syncedCave != nullptr);
    auto* syncedTrip = syncedCave->trip(0);
    REQUIRE(syncedTrip != nullptr);
    REQUIRE(syncedTrip->notes()->rowCount() == 1);
    auto* syncedNote = syncedTrip->notes()->notes().first();
    REQUIRE(syncedNote != nullptr);
    CHECK(syncedNote->name() == remoteName);

    // Ambiguous structural mapping should force broad reconcile fallback, replacing model objects.
    CHECK(syncedCave != localCavePtr.data());
    CHECK(syncedTrip != localTripPtr.data());
    CHECK(syncedNote != localNotePtr.data());

    const auto syncReport = project->lastSyncReport();
    REQUIRE(syncReport.has_value());
    CHECK(std::any_of(syncReport->diagnostics.cbegin(),
                      syncReport->diagnostics.cend(),
                      [](const QString& diagnostic) {
                          return diagnostic.contains(QStringLiteral("reconcile fallback to full reload"));
                      }));
}

TEST_CASE("cwProject sync handles local edit churn during reconcile apply window", "[cwProject][sync]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Remote Apply Guard Cave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    trip->setName(QStringLiteral("Remote Apply Guard Trip"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-remote-apply-guard.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    const QString pngSource = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    REQUIRE(QFileInfo::exists(pngSource));
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(trip->notes()->rowCount() == 1);

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    auto remoteRootData = std::make_unique<cwRootData>();
    remoteRootData->account()->setName(QStringLiteral("Remote Sync Tester"));
    remoteRootData->account()->setEmail(QStringLiteral("remote.sync.tester@example.com"));

    auto remoteProject = remoteRootData->project();
    const QString clonedProjectPath = QDir(clonePath).filePath(QFileInfo(projectPath).fileName());
    REQUIRE(QFileInfo::exists(clonedProjectPath));
    remoteProject->loadFile(clonedProjectPath);
    remoteProject->waitLoadToFinish();
    remoteProject->waitSaveToFinish();

    auto* remoteRepository = remoteProject->repository();
    REQUIRE(remoteRepository != nullptr);
    remoteRepository->setAccount(remoteRootData->account());

    auto* remoteCave = remoteProject->cavingRegion()->cave(0);
    REQUIRE(remoteCave != nullptr);
    auto* remoteTrip = remoteCave->trip(0);
    REQUIRE(remoteTrip != nullptr);
    const QString remoteCaveName = QStringLiteral("Remote Apply Guard Cave Updated");
    remoteCave->setName(remoteCaveName);
    remoteProject->waitSaveToFinish();
    REQUIRE(remoteProject->isModified());

    remoteProject->errorModel()->clear();
    REQUIRE(remoteProject->sync());
    remoteRootData->futureManagerModel()->waitForFinished();
    remoteProject->waitSaveToFinish();
    CHECK(remoteProject->errorModel()->count() == 0);

    project->errorModel()->clear();
    REQUIRE(project->sync());

    int localMutationCount = 0;
    QElapsedTimer churnTimer;
    churnTimer.start();
    while (rootData->futureManagerModel()->rowCount() > 0 && churnTimer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        auto* churnCave = project->cavingRegion()->cave(0);
        auto* churnTrip = churnCave ? churnCave->trip(0) : nullptr;
        if (churnTrip != nullptr) {
            churnTrip->setDate(QDateTime(QDate(2024, 1, 1).addDays(localMutationCount), QTime()));
            ++localMutationCount;
        }
    }

    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(localMutationCount > 0);
    if (project->errorModel()->count() > 0) {
        const auto syncError = project->errorModel()->last();
        CHECK(syncError.type() == cwError::Warning);
        const QString syncMessage = syncError.message();
        if (syncMessage.contains(QStringLiteral("Sync did not complete after 3 retries"))) {
            CHECK(syncMessage.contains(QStringLiteral("model changed before reconcile apply")));
        }
    } else {
        auto* localCave = project->cavingRegion()->cave(0);
        REQUIRE(localCave != nullptr);

        // If churn stops before the first sync finishes reconciliation, a follow-up
        // sync without additional churn should converge on the remote cave rename.
        if (localCave->name() != remoteCaveName) {
            project->errorModel()->clear();
            REQUIRE(project->sync());
            rootData->futureManagerModel()->waitForFinished();
            project->waitSaveToFinish();
            CHECK(project->errorModel()->count() == 0);

            localCave = project->cavingRegion()->cave(0);
            REQUIRE(localCave != nullptr);
        }

        CHECK(localCave->name() == remoteCaveName);
    }
}

TEST_CASE("cwProject sync reconciles pulled model changes before pushing local changes", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Reconcile Push Baseline Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-reconcile-push.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    auto remoteRootData = std::make_unique<cwRootData>();
    remoteRootData->account()->setName(QStringLiteral("Remote Sync Tester"));
    remoteRootData->account()->setEmail(QStringLiteral("remote.sync.tester@example.com"));

    auto remoteProject = remoteRootData->project();
    const QString clonedProjectPath = QDir(clonePath).filePath(QFileInfo(projectPath).fileName());
    REQUIRE(QFileInfo::exists(clonedProjectPath));
    remoteProject->loadFile(clonedProjectPath);
    remoteProject->waitLoadToFinish();
    remoteProject->waitSaveToFinish();

    auto* remoteRepository = remoteProject->repository();
    REQUIRE(remoteRepository != nullptr);
    remoteRepository->setAccount(remoteRootData->account());

    auto* remoteCave = remoteProject->cavingRegion()->cave(0);
    REQUIRE(remoteCave != nullptr);
    const QString remoteCaveName = QStringLiteral("Reconcile Push Updated Cave");
    remoteCave->setName(remoteCaveName);
    remoteProject->waitSaveToFinish();
    REQUIRE(remoteProject->isModified());

    remoteProject->errorModel()->clear();
    REQUIRE(remoteProject->sync());
    remoteRootData->futureManagerModel()->waitForFinished();
    remoteProject->waitSaveToFinish();
    CHECK(remoteProject->errorModel()->count() == 0);

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    const QString localOnlyFileName = QStringLiteral("local-after-reconcile.txt");
    const QString localOnlyFilePath = QDir(localRepoPath).filePath(localOnlyFileName);
    QFile localOnlyFile(localOnlyFilePath);
    REQUIRE(localOnlyFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(localOnlyFile.write("local change after remote rename\n") > 0);
    localOnlyFile.close();
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);
    CHECK(project->isModified() == false);

    auto* localCave = project->cavingRegion()->cave(0);
    REQUIRE(localCave != nullptr);
    CHECK(localCave->name() == remoteCaveName);

    QFile localFileCheck(localOnlyFilePath);
    REQUIRE(localFileCheck.open(QIODevice::ReadOnly));
    CHECK(localFileCheck.readAll() == QByteArray("local change after remote rename\n"));

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&remoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
        if (remoteBranch) {
            git_reference_free(remoteBranch);
        }
    });

    const git_oid* remoteHeadOid = git_reference_target(remoteBranch);
    REQUIRE(remoteHeadOid != nullptr);

    git_commit* remoteHeadCommit = nullptr;
    REQUIRE(git_commit_lookup(&remoteHeadCommit, remoteRepo, remoteHeadOid) == GIT_OK);
    auto remoteHeadCommitGuard = qScopeGuard([&remoteHeadCommit]() {
        if (remoteHeadCommit) {
            git_commit_free(remoteHeadCommit);
        }
    });

    git_tree* remoteTree = nullptr;
    REQUIRE(git_commit_tree(&remoteTree, remoteHeadCommit) == GIT_OK);
    auto remoteTreeGuard = qScopeGuard([&remoteTree]() {
        if (remoteTree) {
            git_tree_free(remoteTree);
        }
    });

    git_tree_entry* localOnlyEntry = nullptr;
    REQUIRE(git_tree_entry_bypath(&localOnlyEntry,
                                  remoteTree,
                                  localOnlyFileName.toLocal8Bit().constData()) == GIT_OK);
    auto localOnlyEntryGuard = qScopeGuard([&localOnlyEntry]() {
        if (localOnlyEntry) {
            git_tree_entry_free(localOnlyEntry);
        }
    });
}

TEST_CASE("cwProject sync persists pulled id repairs before push", "[cwProject][repair]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto* region = project->cavingRegion();
    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Repair Sync Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-repair-id.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    QDirIterator caveFileIterator(clonePath,
                                  QStringList{QStringLiteral("*.cwcave")},
                                  QDir::Files,
                                  QDirIterator::Subdirectories);
    REQUIRE(caveFileIterator.hasNext());
    const QString clonedCaveFile = caveFileIterator.next();

    auto clonedCaveProto = loadProtoFromJsonFile<CavewhereProto::Cave>(clonedCaveFile);
    REQUIRE(clonedCaveProto.has_id());
    clonedCaveProto.clear_id();
    writeProtoToJsonFile(clonedCaveFile, clonedCaveProto);

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Remove cave id"),
                                              QStringLiteral("simulate remote missing identity")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    auto remoteHeadOid = [repository](const QString& remoteRepoPath) -> git_oid {
        git_repository* remoteRepoLocal = nullptr;
        REQUIRE(git_repository_open(&remoteRepoLocal, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
        auto remoteRepoGuard = qScopeGuard([&remoteRepoLocal]() {
            if (remoteRepoLocal) {
                git_repository_free(remoteRepoLocal);
            }
        });

        const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
        git_reference* remoteBranch = nullptr;
        REQUIRE(git_reference_lookup(&remoteBranch, remoteRepoLocal, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
        auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
            if (remoteBranch) {
                git_reference_free(remoteBranch);
            }
        });

        const git_oid* remoteHead = git_reference_target(remoteBranch);
        REQUIRE(remoteHead != nullptr);
        return *remoteHead;
    };

    const git_oid remoteHeadBeforeRepair = remoteHeadOid(remoteRepoPath);

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    const git_oid remoteHeadAfterRepair = remoteHeadOid(remoteRepoPath);
    CHECK(git_oid_cmp(&remoteHeadBeforeRepair, &remoteHeadAfterRepair) != 0);

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    git_reference* localHeadRef = nullptr;
    REQUIRE(git_repository_head(&localHeadRef, localRepo) == GIT_OK);
    auto localHeadRefGuard = qScopeGuard([&localHeadRef]() {
        if (localHeadRef) {
            git_reference_free(localHeadRef);
        }
    });

    const git_oid* localHeadOid = git_reference_target(localHeadRef);
    REQUIRE(localHeadOid != nullptr);
    git_commit* localHeadCommit = nullptr;
    REQUIRE(git_commit_lookup(&localHeadCommit, localRepo, localHeadOid) == GIT_OK);
    auto localHeadCommitGuard = qScopeGuard([&localHeadCommit]() {
        if (localHeadCommit) {
            git_commit_free(localHeadCommit);
        }
    });
    const QString localHeadMessage = QString::fromUtf8(git_commit_message(localHeadCommit));
    CHECK(localHeadMessage.startsWith(QStringLiteral("Sync Reconcile from CaveWhere")));

    QTemporaryDir verifyCloneDir;
    REQUIRE(verifyCloneDir.isValid());
    const QString verifyClonePath = QDir(verifyCloneDir.path()).filePath(QStringLiteral("clone-verify"));

    QQuickGit::GitRepository verifyRepository;
    verifyRepository.setDirectory(QDir(verifyClonePath));
    verifyRepository.setAccount(rootData->account());

    auto verifyCloneFuture = verifyRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(verifyCloneFuture, 10000));
    INFO("Verify clone error:" << verifyCloneFuture.result().errorMessage().toStdString());
    REQUIRE(!verifyCloneFuture.result().hasError());

    QDirIterator verifyCaveFileIterator(verifyClonePath,
                                        QStringList{QStringLiteral("*.cwcave")},
                                        QDir::Files,
                                        QDirIterator::Subdirectories);
    REQUIRE(verifyCaveFileIterator.hasNext());
    const QString repairedCaveFile = verifyCaveFileIterator.next();

    const auto repairedCaveProto = loadProtoFromJsonFile<CavewhereProto::Cave>(repairedCaveFile);
    REQUIRE(repairedCaveProto.has_id());
    CHECK_FALSE(repairedCaveProto.id().empty());
}

TEST_CASE("cwProject sync creates and pushes merge commit for diverged non-conflicting changes", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Merge Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-merge.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    cave->setName(QStringLiteral("Merge Cave Baseline"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    auto headOid = [](git_repository* repoInstance) -> git_oid {
        git_reference* head = nullptr;
        REQUIRE(git_repository_head(&head, repoInstance) == GIT_OK);
        auto headGuard = qScopeGuard([&head]() {
            if (head) {
                git_reference_free(head);
            }
        });
        const git_oid* target = git_reference_target(head);
        REQUIRE(target != nullptr);
        return *target;
    };

    const git_oid baselineHead = headOid(localRepo);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    const QString remoteOnlyFileName = QStringLiteral("remote-side-only.txt");
    QFile remoteOnlyFile(QDir(clonePath).filePath(remoteOnlyFileName));
    REQUIRE(remoteOnlyFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(remoteOnlyFile.write("remote side change\n") == 19);
    remoteOnlyFile.close();

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Remote side"),
                                              QStringLiteral("non-conflicting remote change")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    const QString localOnlyFileName = QStringLiteral("local-side-only.txt");
    QFile localOnlyFile(QDir(localRepoPath).filePath(localOnlyFileName));
    REQUIRE(localOnlyFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(localOnlyFile.write("local side change\n") == 18);
    localOnlyFile.close();
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    const git_oid mergedHead = headOid(localRepo);
    CHECK(git_oid_cmp(&baselineHead, &mergedHead) != 0);

    git_commit* mergedCommit = nullptr;
    REQUIRE(git_commit_lookup(&mergedCommit, localRepo, &mergedHead) == GIT_OK);
    auto mergedCommitGuard = qScopeGuard([&mergedCommit]() {
        if (mergedCommit) {
            git_commit_free(mergedCommit);
        }
    });
    CHECK(git_commit_parentcount(mergedCommit) == 2);
    CHECK(QString::fromUtf8(git_commit_message(mergedCommit))
              == QStringLiteral("Merged branch origin/%1").arg(repository->headBranchName()));

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&remoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
        if (remoteBranch) {
            git_reference_free(remoteBranch);
        }
    });
    const git_oid* remoteHeadOid = git_reference_target(remoteBranch);
    REQUIRE(remoteHeadOid != nullptr);
    CHECK(git_oid_cmp(remoteHeadOid, &mergedHead) == 0);

    QFile localRemoteSideFile(QDir(localRepoPath).filePath(remoteOnlyFileName));
    REQUIRE(localRemoteSideFile.open(QIODevice::ReadOnly));
    CHECK(localRemoteSideFile.readAll() == QByteArray("remote side change\n"));

    QFile localLocalSideFile(QDir(localRepoPath).filePath(localOnlyFileName));
    REQUIRE(localLocalSideFile.open(QIODevice::ReadOnly));
    CHECK(localLocalSideFile.readAll() == QByteArray("local side change\n"));
}

TEST_CASE("cwProject sync surfaces remote access failure and preserves local commit", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Offline Sync Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-offline.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir missingRemoteRoot;
    REQUIRE(missingRemoteRoot.isValid());
    const QString missingRemotePath = QDir(missingRemoteRoot.path()).filePath(QStringLiteral("missing-remote.git"));
    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(missingRemotePath));
    REQUIRE(addRemoteError.isEmpty());

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    auto tryHeadOid = [](git_repository* repoInstance) -> std::optional<git_oid> {
        git_reference* head = nullptr;
        const int error = git_repository_head(&head, repoInstance);
        if (error == GIT_EUNBORNBRANCH || error == GIT_ENOTFOUND) {
            return std::nullopt;
        }
        REQUIRE(error == GIT_OK);
        auto headGuard = qScopeGuard([&head]() {
            if (head) {
                git_reference_free(head);
            }
        });
        const git_oid* target = git_reference_target(head);
        REQUIRE(target != nullptr);
        return *target;
    };

    cave->setName(QStringLiteral("Offline Sync Cave Local Change"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    const std::optional<git_oid> beforeSyncHead = tryHeadOid(localRepo);

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() > 0);
    const auto syncError = project->errorModel()->last();
    CHECK(syncError.type() == cwError::Warning);
    CHECK_FALSE(syncError.message().isEmpty());
    CHECK(syncError.message() != QStringLiteral("No git remote is configured for this project."));

    const std::optional<git_oid> afterSyncHead = tryHeadOid(localRepo);
    REQUIRE(afterSyncHead.has_value());
    if (beforeSyncHead.has_value()) {
        CHECK(git_oid_cmp(&(*beforeSyncHead), &(*afterSyncHead)) != 0);
    }

    git_commit* commit = nullptr;
    REQUIRE(git_commit_lookup(&commit, localRepo, &(*afterSyncHead)) == GIT_OK);
    auto commitGuard = qScopeGuard([&commit]() {
        if (commit) {
            git_commit_free(commit);
        }
    });
    const QString commitMessage = QString::fromUtf8(git_commit_message(commit));
    CHECK(commitMessage.startsWith(QStringLiteral("Sync from CaveWhere")));
    CHECK(project->isModified() == false);
}

TEST_CASE("cwProject sync is reentrant and converges to deterministic head", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Reentrant Sync Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-reentrant.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    cave->setName(QStringLiteral("Reentrant Sync Baseline"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);
    CHECK(project->isModified() == false);

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    git_repository* localRepo = nullptr;
    REQUIRE(git_repository_open(&localRepo, localRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto localRepoGuard = qScopeGuard([&localRepo]() {
        if (localRepo) {
            git_repository_free(localRepo);
        }
    });

    auto headOid = [](git_repository* repoInstance) -> git_oid {
        git_reference* head = nullptr;
        REQUIRE(git_repository_head(&head, repoInstance) == GIT_OK);
        auto headGuard = qScopeGuard([&head]() {
            if (head) {
                git_reference_free(head);
            }
        });
        const git_oid* target = git_reference_target(head);
        REQUIRE(target != nullptr);
        return *target;
    };

    const git_oid baselineHead = headOid(localRepo);

    const QString reentrantFilePath = QDir(localRepoPath).filePath(QStringLiteral("reentrant-sync.txt"));
    QFile localFile(reentrantFilePath);
    REQUIRE(localFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(localFile.write("sync called twice\n") == 18);
    localFile.close();
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    CHECK(project->errorModel()->count() == 0);
    CHECK(project->isModified() == false);

    const git_oid finalHead = headOid(localRepo);
    CHECK(git_oid_cmp(&baselineHead, &finalHead) != 0);

    git_commit* finalCommit = nullptr;
    REQUIRE(git_commit_lookup(&finalCommit, localRepo, &finalHead) == GIT_OK);
    auto finalCommitGuard = qScopeGuard([&finalCommit]() {
        if (finalCommit) {
            git_commit_free(finalCommit);
        }
    });

    CHECK(git_commit_parentcount(finalCommit) == 1);
    const git_oid* parentOid = git_commit_parent_id(finalCommit, 0);
    REQUIRE(parentOid != nullptr);
    CHECK(git_oid_cmp(parentOid, &baselineHead) == 0);
    CHECK(QString::fromUtf8(git_commit_message(finalCommit)).startsWith(QStringLiteral("Sync from CaveWhere")));

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&remoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
        if (remoteBranch) {
            git_reference_free(remoteBranch);
        }
    });
    const git_oid* remoteHeadOid = git_reference_target(remoteBranch);
    REQUIRE(remoteHeadOid != nullptr);
    CHECK(git_oid_cmp(remoteHeadOid, &finalHead) == 0);

    QFile finalFile(reentrantFilePath);
    REQUIRE(finalFile.open(QIODevice::ReadOnly));
    CHECK(finalFile.readAll() == QByteArray("sync called twice\n"));
}

TEST_CASE("cwProject sync stress under remote churn retries safely and converges", "[cwProject][sync][stress]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Stress Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.stress.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Retry Stress Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-retry-stress.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository churnRepository;
    churnRepository.setDirectory(QDir(clonePath));
    churnRepository.setAccount(rootData->account());

    auto cloneFuture = churnRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    const QString localChurnFilePath = QDir(localRepoPath).filePath(QStringLiteral("local-retry-stress.txt"));
    const QString remoteChurnFilePath = QDir(clonePath).filePath(QStringLiteral("remote-retry-stress.txt"));

    auto appendLine = [](const QString& path, const QByteArray& line) {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Append));
        REQUIRE(file.write(line) == line.size());
        file.close();
    };

    auto pushRemoteChurn = [&churnRepository, &remoteChurnFilePath, &appendLine](int i) {
        appendLine(remoteChurnFilePath, QStringLiteral("remote-%1\n").arg(i).toUtf8());

        REQUIRE_NOTHROW(churnRepository.commitAll(QStringLiteral("Remote churn %1").arg(i),
                                                  QStringLiteral("retry stress remote update")));

        auto pushFuture = churnRepository.push();
        REQUIRE(AsyncFuture::waitForFinished(pushFuture, 10000));
        if (!pushFuture.result().hasError()) {
            return;
        }

        auto pullFuture = churnRepository.pull();
        REQUIRE(AsyncFuture::waitForFinished(pullFuture, 10000));
        INFO("Churn pull error:" << pullFuture.result().errorMessage().toStdString());
        REQUIRE(!pullFuture.result().hasError());

        auto retryPushFuture = churnRepository.push();
        REQUIRE(AsyncFuture::waitForFinished(retryPushFuture, 10000));
        INFO("Churn push retry error:" << retryPushFuture.result().errorMessage().toStdString());
        REQUIRE(!retryPushFuture.result().hasError());
    };

    appendLine(localChurnFilePath, QByteArray("local-seed\n"));
    project->waitSaveToFinish();

    project->errorModel()->clear();
    REQUIRE(project->sync());

    QElapsedTimer churnTimer;
    churnTimer.start();
    constexpr int ChurnBudgetMs = 8000;
    int remotePushCount = 0;
    while (rootData->futureManagerModel()->rowCount() > 0 && churnTimer.elapsed() < ChurnBudgetMs) {
        ++remotePushCount;
        pushRemoteChurn(remotePushCount);

        appendLine(localChurnFilePath, QStringLiteral("local-%1\n").arg(remotePushCount).toUtf8());
        cave->setName(QStringLiteral("Retry Stress Cave %1").arg(remotePushCount));

        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(5);
    }

    const bool syncTimedOutDuringChurn = rootData->futureManagerModel()->rowCount() > 0;
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    WARN("sync timed out during churn window: " << (syncTimedOutDuringChurn ? "yes" : "no"));
    CHECK(remotePushCount > 0);

    bool sawRetryCapWarning = false;
    bool sawRawPushRejectWarning = false;
    QStringList warningMessages;
    for (int i = 0; i < project->errorModel()->count(); ++i) {
        const auto warning = project->errorModel()->at(i);
        const QString message = warning.message();
        warningMessages.append(message);
        if (message.contains(QStringLiteral("did not complete after"), Qt::CaseInsensitive)) {
            sawRetryCapWarning = true;
        }
        if (message.contains(QStringLiteral("contains commits that are not present locally"), Qt::CaseInsensitive)
            || message.contains(QStringLiteral("cannot push because a reference that you are trying to update on the remote"), Qt::CaseInsensitive)) {
            sawRawPushRejectWarning = true;
        }
    }
    WARN("retry cap warning observed during churn: " << (sawRetryCapWarning ? "yes" : "no"));
    WARN("warnings during churn sync: " << warningMessages.join(QStringLiteral(" | ")).toStdString());
    CHECK_FALSE(sawRawPushRejectWarning);

    project->errorModel()->clear();
    appendLine(localChurnFilePath, QByteArray("local-final\n"));
    project->waitSaveToFinish();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QDirIterator payloadIterator(localRepoPath,
                                 QStringList{
                                     QStringLiteral("*.cwproj"),
                                     QStringLiteral("*.cwcave"),
                                     QStringLiteral("*.cwtrip"),
                                     QStringLiteral("*.cwnote"),
                                     QStringLiteral("*.cwnote3d")
                                 },
                                 QDir::Files,
                                 QDirIterator::Subdirectories);
    while (payloadIterator.hasNext()) {
        const QString payloadPath = payloadIterator.next();
        QFile payload(payloadPath);
        REQUIRE(payload.open(QIODevice::ReadOnly));
        const QByteArray data = payload.readAll();
        CHECK(!data.contains("<<<<<<<"));
        CHECK(!data.contains("======="));
        CHECK(!data.contains(">>>>>>>"));
    }

    auto loadAllFuture = cwSaveLoad::loadAll(project->filename());
    REQUIRE(AsyncFuture::waitForFinished(loadAllFuture, 10000));
    CHECK_FALSE(loadAllFuture.result().hasError());
}

TEST_CASE("cwProject sync conflict keeps ours and push succeeds", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Conflict Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-conflict-project.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    const QString conflictFileName = QStringLiteral("sync-conflict.txt");
    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    const QString localConflictFilePath = QDir(localRepoPath).filePath(conflictFileName);

    QFile localFile(localConflictFilePath);
    REQUIRE(localFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(localFile.write("base\n") == 5);
    localFile.close();

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    QFile cloneFile(QDir(clonePath).filePath(conflictFileName));
    REQUIRE(cloneFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(cloneFile.write("theirs\n") == 7);
    cloneFile.close();

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Theirs"),
                                              QStringLiteral("remote side edit")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    QFile oursFile(localConflictFilePath);
    REQUIRE(oursFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(oursFile.write("ours\n") == 5);
    oursFile.close();

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() == 0);
    QFile localRead(localConflictFilePath);
    REQUIRE(localRead.open(QIODevice::ReadOnly));
    CHECK(localRead.readAll() == QByteArray("ours\n"));

    REQUIRE(git_repository_open(&remoteRepo, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
    auto remoteRepoGuard = qScopeGuard([&remoteRepo]() {
        if (remoteRepo) {
            git_repository_free(remoteRepo);
        }
    });

    const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
    git_reference* remoteBranch = nullptr;
    REQUIRE(git_reference_lookup(&remoteBranch, remoteRepo, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
    auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
        if (remoteBranch) {
            git_reference_free(remoteBranch);
        }
    });

    const git_oid* remoteHeadOid = git_reference_target(remoteBranch);
    REQUIRE(remoteHeadOid != nullptr);

    git_commit* remoteHeadCommit = nullptr;
    REQUIRE(git_commit_lookup(&remoteHeadCommit, remoteRepo, remoteHeadOid) == GIT_OK);
    auto remoteHeadCommitGuard = qScopeGuard([&remoteHeadCommit]() {
        if (remoteHeadCommit) {
            git_commit_free(remoteHeadCommit);
        }
    });

    git_tree* remoteTree = nullptr;
    REQUIRE(git_commit_tree(&remoteTree, remoteHeadCommit) == GIT_OK);
    auto remoteTreeGuard = qScopeGuard([&remoteTree]() {
        if (remoteTree) {
            git_tree_free(remoteTree);
        }
    });

    git_tree_entry* conflictEntry = nullptr;
    REQUIRE(git_tree_entry_bypath(&conflictEntry, remoteTree, conflictFileName.toLocal8Bit().constData()) == GIT_OK);
    auto conflictEntryGuard = qScopeGuard([&conflictEntry]() {
        if (conflictEntry) {
            git_tree_entry_free(conflictEntry);
        }
    });

    git_blob* conflictBlob = nullptr;
    REQUIRE(git_blob_lookup(&conflictBlob, remoteRepo, git_tree_entry_id(conflictEntry)) == GIT_OK);
    auto conflictBlobGuard = qScopeGuard([&conflictBlob]() {
        if (conflictBlob) {
            git_blob_free(conflictBlob);
        }
    });

    QByteArray remoteConflictContents(static_cast<const char*>(git_blob_rawcontent(conflictBlob)),
                                      static_cast<int>(git_blob_rawsize(conflictBlob)));
    CHECK(remoteConflictContents == QByteArray("ours\n"));
}

TEST_CASE("cwProject sync blocks pulled newer project version", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Incompatible Version Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-incompatible-version.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);

    QTemporaryDir remoteRoot;
    REQUIRE(remoteRoot.isValid());
    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(addRemoteError.isEmpty());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    QTemporaryDir cloneDir;
    REQUIRE(cloneDir.isValid());
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("clone-2"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error:" << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    const QString clonedProjectPath = QDir(clonePath).filePath(QFileInfo(projectPath).fileName());
    QFile clonedProjectFile(clonedProjectPath);
    REQUIRE(clonedProjectFile.open(QIODevice::ReadOnly));
    const QByteArray clonedProjectJson = clonedProjectFile.readAll();
    clonedProjectFile.close();

    CavewhereProto::Project clonedProjectProto;
    auto loadStatus = google::protobuf::util::JsonStringToMessage(clonedProjectJson.toStdString(), &clonedProjectProto);
    REQUIRE(loadStatus.ok());
    REQUIRE(clonedProjectProto.has_fileversion());
    clonedProjectProto.mutable_fileversion()->set_version(9999);

    std::string incompatibleProjectJson;
    auto saveStatus = google::protobuf::util::MessageToJsonString(clonedProjectProto, &incompatibleProjectJson);
    REQUIRE(saveStatus.ok());

    REQUIRE(clonedProjectFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(clonedProjectFile.write(QByteArray::fromStdString(incompatibleProjectJson)) > 0);
    clonedProjectFile.close();

    REQUIRE_NOTHROW(cloneRepository.commitAll(QStringLiteral("Bump project version"),
                                              QStringLiteral("set unsupported project version")));
    auto clonePushFuture = cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(clonePushFuture, 10000));
    INFO("Remote clone push error:" << clonePushFuture.result().errorMessage().toStdString());
    REQUIRE(!clonePushFuture.result().hasError());

    auto remoteHeadOid = [repository](const QString& remoteRepoPath) -> git_oid {
        git_repository* remoteRepoLocal = nullptr;
        REQUIRE(git_repository_open(&remoteRepoLocal, remoteRepoPath.toLocal8Bit().constData()) == GIT_OK);
        auto remoteRepoGuard = qScopeGuard([&remoteRepoLocal]() {
            if (remoteRepoLocal) {
                git_repository_free(remoteRepoLocal);
            }
        });

        const QString remoteRefName = QStringLiteral("refs/heads/") + repository->headBranchName();
        git_reference* remoteBranch = nullptr;
        REQUIRE(git_reference_lookup(&remoteBranch, remoteRepoLocal, remoteRefName.toLocal8Bit().constData()) == GIT_OK);
        auto remoteBranchGuard = qScopeGuard([&remoteBranch]() {
            if (remoteBranch) {
                git_reference_free(remoteBranch);
            }
        });

        const git_oid* remoteHead = git_reference_target(remoteBranch);
        REQUIRE(remoteHead != nullptr);
        return *remoteHead;
    };

    const git_oid expectedRemoteHead = remoteHeadOid(remoteRepoPath);

    const QString localRepoPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    QFile localFile(QDir(localRepoPath).filePath(QStringLiteral("local-before-incompatible-sync.txt")));
    REQUIRE(localFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(localFile.write("local change before incompatible sync\n") > 0);
    localFile.close();
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() > 0);
    const auto syncError = project->errorModel()->last();
    CHECK(syncError.type() == cwError::Warning);
    CHECK(syncError.message().contains(QStringLiteral("Project file version")));
    CHECK(syncError.message().contains(QStringLiteral("newer than supported version")));
    CHECK_FALSE(project->lastSyncReport().has_value());

    const git_oid finalRemoteHead = remoteHeadOid(remoteRepoPath);
    CHECK(git_oid_cmp(&expectedRemoteHead, &finalRemoteHead) == 0);
}

TEST_CASE("cwProject sync reports warning when no remote is configured", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    project->errorModel()->clear();
    rootData->account()->setName(QStringLiteral("Sync Tester"));
    rootData->account()->setEmail(QStringLiteral("sync.tester@example.com"));

    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(project->errorModel()->count() > 0);
    const auto error = project->errorModel()->last();
    CHECK(error.type() == cwError::Warning);
    CHECK(error.message() == QStringLiteral("No git remote is configured for this project."));
}

TEST_CASE("cwProject save skips commit and sync fails when account is invalid", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    rootData->account()->setName(QStringLiteral("Valid Tester"));
    rootData->account()->setEmail(QStringLiteral("valid.tester@example.com"));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("InvalidAccount Cave"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("invalid-account.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    cave->setName(QStringLiteral("Baseline Commit Cave"));
    project->waitSaveToFinish();
    REQUIRE(project->save());
    project->waitSaveToFinish();
    CHECK(project->isModified() == false);

    git_repository* repo = nullptr;
    REQUIRE(git_repository_open(&repo,
                                QFileInfo(project->filename()).absoluteDir().absolutePath().toLocal8Bit().constData()) == GIT_OK);
    auto repoGuard = qScopeGuard([&repo]() {
        if (repo) {
            git_repository_free(repo);
        }
    });

    auto headOid = [](git_repository* repository) {
        git_reference* head = nullptr;
        REQUIRE(git_repository_head(&head, repository) == GIT_OK);
        auto headGuard = qScopeGuard([&head]() {
            if (head) {
                git_reference_free(head);
            }
        });

        const git_oid* target = git_reference_target(head);
        REQUIRE(target != nullptr);
        return *target;
    };

    const git_oid beforeSave = headOid(repo);

    rootData->account()->setName(QString());
    rootData->account()->setEmail(QString());
    project->errorModel()->clear();

    cave->setName(QStringLiteral("InvalidAccount Cave Updated"));
    project->waitSaveToFinish();
    REQUIRE(project->isModified());

    REQUIRE(project->save());
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() > 0);
    auto saveError = project->errorModel()->last();
    CHECK(saveError.type() == cwError::Warning);
    CHECK(saveError.message() == QStringLiteral("Git account is not configured. Please set your name and email in CaveWhere."));

    const git_oid afterSave = headOid(repo);
    CHECK(git_oid_cmp(&beforeSave, &afterSave) == 0);
    CHECK(project->isModified());

    project->errorModel()->clear();
    REQUIRE(project->sync());
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(project->errorModel()->count() > 0);
    auto syncError = project->errorModel()->last();
    CHECK(syncError.type() == cwError::Warning);
    CHECK(syncError.message() == QStringLiteral("Git account is not configured. Please set your name and email in CaveWhere."));
}

TEST_CASE("Images should load correctly", "[cwProject]") {

    QSize size(1024, 1024);

    struct Image {
        QColor topLeftColor;
        QSize size;
        QUrl filename;
    };

    auto image = [size](const QColor& color)->Image {
        QImage image(size, QImage::Format_ARGB32);
        image.fill(color);
        QString imageFilename = QDir::tempPath() + "/" + QString("cavewhere-cwProject-image%1%2%3.png").arg(color.red()).arg(color.green()).arg(color.blue());
        REQUIRE(image.save(imageFilename, "png"));
        return {color, size, QUrl::fromLocalFile(imageFilename)};
    };

    QVector<QColor> imageColors = {"red", "green", "blue"};
    QList<Image> testImages;
    std::transform(imageColors.begin(), imageColors.end(), std::back_inserter(testImages), image);

    QString crashMapPath = "://datasets/test_cwProject/crashMap.png";
    QImage crashMap(crashMapPath);
    auto y = crashMap.size().height() - 1;
    testImages += Image {crashMap.pixelColor(0, y),
        crashMap.size(),
        QUrl::fromLocalFile(copyToTempFolder(crashMapPath))
    };

    QVector<QUrl> filenames;
    std::transform(testImages.begin(), testImages.end(), std::back_inserter(filenames),
                   [](const Image& image) {
                       return image.filename;
                   }
                   );

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    CHECK(ProjectFilenameTestHelper::projectDir(project).isAbsolute());

    int checked = 0;
    project->addImages(filenames, ProjectFilenameTestHelper::projectDir(project), [&checked, testImages, project, size](QList<cwImage> images){
        REQUIRE(images.size() == testImages.size());

        //Load the image and check that it's in the correct order
        for(int i = 0; i < images.size(); i++) {
            cwTextureUploadTask uploadTask;

            auto path = project->absolutePath(images.at(i).path());
            auto image = images.at(i);
            image.setPath(path);

            CHECK(QFile::exists(path));

            uploadTask.setImage(image);
            uploadTask.setDataRootDir(project->dataRootDir());
            uploadTask.setType(cwTextureUploadTask::OpenGL_RGBA);
            auto future = uploadTask.mipmaps();
            auto imageData = future.result();

            CHECK(imageData.image.size() == testImages.at(i).size);
            CHECK(imageData.image.pixelColor(0, 0) == testImages.at(i).topLeftColor);
        }

        checked++;
    });

    rootData->futureManagerModel()->waitForFinished();
    CHECK(checked == 1);
    CHECK(filenames.size() > 0);
}

TEST_CASE("New project should clear loaded scraps before reload", "[cwProject]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());

    const QString dataset = copyToTempFolder("://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
    project->loadOrConvert(dataset);
    project->waitLoadToFinish();

    cwScrap* first = firstScrap(project.get());

    project->newProject();
    project->loadOrConvert(dataset);
    project->waitLoadToFinish();

    cwScrap* second = firstScrap(project.get());
    CHECK(first != second);
}

TEST_CASE("Reloading the same project should replace scrap objects", "[cwProject]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());

    const QString dataset = copyToTempFolder("://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
    project->loadOrConvert(dataset);
    project->waitLoadToFinish();

    cwScrap* first = firstScrap(project.get());

    project->loadOrConvert(dataset);
    project->waitLoadToFinish();

    cwScrap* second = firstScrap(project.get());
    CHECK(first != second);
}


TEST_CASE("Files should be added to the project correctly", "[cwProject]") {
    // Arrange: create a temporary file with some content
    QTemporaryDir temporaryDirectory;
    REQUIRE(temporaryDirectory.isValid());

    const QString fileName = QStringLiteral("hello.txt");
    const QString sourceFilePath = temporaryDirectory.filePath(fileName);

    {
        QFile file(sourceFilePath);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
        QTextStream stream(&file);
        stream << QStringLiteral("hello world\n");
        file.close();
    }
    REQUIRE(QFileInfo::exists(sourceFilePath));

    // Arrange: project and root directory
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    const QDir projectRootDirectory = ProjectFilenameTestHelper::projectDir(project);
    CHECK(projectRootDirectory.isAbsolute());

    // Act: call addFiles
    int callbackCount = 0;
    QList<QString> returnedRelativePaths;

    const QList<QUrl> urls { QUrl::fromLocalFile(sourceFilePath) };
    project->addFiles(urls, projectRootDirectory,
                      [&callbackCount, &returnedRelativePaths](QList<QString> paths) {
                          ++callbackCount;
                          returnedRelativePaths = paths;
                      });

    // Wait for async jobs to finish
    rootData->futureManagerModel()->waitForFinished();

    // Assert: callback happened
    REQUIRE(callbackCount == 1);
    REQUIRE(returnedRelativePaths.size() == 1);

    const QString relativePath = returnedRelativePaths.front();
    CHECK(QDir::isRelativePath(relativePath));

    const QString destinationFilePath = projectRootDirectory.absoluteFilePath(relativePath);
    CHECK(QFileInfo::exists(destinationFilePath));

    // Assert: filename preserved
    CHECK(QFileInfo(destinationFilePath).fileName() == fileName);

    // Assert: file contents preserved
    auto readAllText = [](const QString& path) -> QString {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        QTextStream stream(&file);
        return stream.readAll();
    };

    const QString sourceText = readAllText(sourceFilePath);
    const QString destinationText = readAllText(destinationFilePath);

    CHECK_FALSE(sourceText.isEmpty());
    CHECK(sourceText == destinationText);
}

TEST_CASE("Temporary project saveAs should move the project directory", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    REQUIRE(project->isTemporaryProject());

    const QDir originalRoot = QFileInfo(project->filename()).absoluteDir();
    REQUIRE(originalRoot.exists());

    const QString markerFilePath = originalRoot.absoluteFilePath(QStringLiteral("marker.txt"));
    {
        QFile marker(markerFilePath);
        REQUIRE(marker.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
        marker.write("temporary marker\n");
        marker.close();
    }
    REQUIRE(QFileInfo::exists(markerFilePath));

    QTemporaryDir destinationParent;
    REQUIRE(destinationParent.isValid());

    const QString targetProjectFile = destinationParent.filePath(QStringLiteral("SavedProject.cwproj"));

    REQUIRE(project->saveAs(targetProjectFile));

    rootData->futureManagerModel()->waitForFinished();

    CHECK(project->canSaveDirectly());
    CHECK_FALSE(project->isTemporaryProject());

    const QString newProjectFile = project->filename();
    QFileInfo newFileInfo(newProjectFile);
    REQUIRE(newFileInfo.exists());

    const QDir newRootDir = newFileInfo.absoluteDir();
    REQUIRE(newRootDir.exists());

    CHECK(QFileInfo::exists(newRootDir.absoluteFilePath(QStringLiteral("marker.txt"))));
    CHECK_FALSE(QFileInfo::exists(markerFilePath));

    CHECK(newFileInfo.fileName() == QStringLiteral("SavedProject.cwproj"));
}

TEST_CASE("Temporary project saveAs reports error when destination exists", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    REQUIRE(project->isTemporaryProject());
    project->errorModel()->clear();

    QTemporaryDir destinationParent;
    REQUIRE(destinationParent.isValid());

    const QString baseName = QStringLiteral("ExistingProject");
    const QString existingRoot = destinationParent.filePath(baseName);
    REQUIRE(QDir().mkpath(existingRoot));

    const QString targetProjectFile = destinationParent.filePath(baseName + QStringLiteral(".cwproj"));

    CHECK_FALSE(project->saveAs(targetProjectFile));

    REQUIRE(project->errorModel()->count() > 0);
    const auto error = project->errorModel()->last();
    CHECK(error.type() == cwError::Fatal);
    CHECK(error.message() == QStringLiteral("Destination folder '%1' already exists.").arg(existingRoot));
    CHECK(project->isTemporaryProject());
    CHECK(project->isNewProject());
}

TEST_CASE("SaveAs updates dataRoot directory to match project name", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    REQUIRE(project->isTemporaryProject());

    auto cave = new cwCave();
    cave->setName(QStringLiteral("Test Cave"));
    project->cavingRegion()->addCave(cave);

    auto trip = new cwTrip();
    trip->setName(QStringLiteral("Trip 1"));
    cave->addTrip(trip);

    project->waitSaveToFinish();

    QTemporaryDir destinationParent;
    REQUIRE(destinationParent.isValid());

    const QString targetProjectFile = destinationParent.filePath(QStringLiteral("SlugProject.cwproj"));
    REQUIRE(project->saveAs(targetProjectFile));
    rootData->futureManagerModel()->waitForFinished();

    CHECK(project->canSaveDirectly());
    CHECK_FALSE(project->isTemporaryProject());

    const QString projectName = QFileInfo(project->filename()).completeBaseName();
    const QDir dataRootDir = ProjectFilenameTestHelper::projectDir(project);
    CHECK(dataRootDir.dirName().toStdString() == projectName.toStdString());

    CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(cave)));
    CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));
}

TEST_CASE("SaveAs persists dataRoot updates for reload", "[cwProject][saveAs]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    REQUIRE(project->isTemporaryProject());

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("ReloadCave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    trip->setName(QStringLiteral("ReloadTrip"));

    project->waitSaveToFinish();

    QTemporaryDir destinationParent;
    destinationParent.setAutoRemove(false);
    REQUIRE(destinationParent.isValid());
    REQUIRE(QFileInfo::exists(destinationParent.path()));

    const QString targetProjectFile = destinationParent.filePath(QStringLiteral("ReloadProject.cwproj"));
    REQUIRE(project->saveAs(targetProjectFile));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));
    const QString expectedProjectFile = QDir(destinationParent.path())
                                            .filePath(QStringLiteral("ReloadProject/ReloadProject.cwproj"));
    CHECK(savedProjectFile.toStdString() == expectedProjectFile.toStdString());

    const QString projectName = QFileInfo(savedProjectFile).completeBaseName();
    const QDir savedDataRootDir = ProjectFilenameTestHelper::projectDir(project);
    CHECK(savedDataRootDir.dirName().toStdString() == projectName.toStdString());
    CHECK(project->dataRoot().toStdString() == projectName.toStdString());
    CHECK(project->dataRootDir().dirName().toStdString() == projectName.toStdString());

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    auto reloadedCave = reloaded->cavingRegion()->cave(0);
    REQUIRE(reloadedCave != nullptr);
    CHECK(reloadedCave->name() == QStringLiteral("ReloadCave"));
    REQUIRE(reloadedCave->tripCount() == 1);
    CHECK(reloadedCave->trip(0)->name() == QStringLiteral("ReloadTrip"));

    const QDir dataRootDir = ProjectFilenameTestHelper::projectDir(reloaded.get());
    CHECK(dataRootDir.dirName().toStdString() == projectName.toStdString());
    CHECK(reloaded->dataRoot().toStdString() == projectName.toStdString());
    CHECK(reloaded->dataRootDir().dirName().toStdString() == projectName.toStdString());
}

TEST_CASE("Non-temporary project saveAs reports error when destination exists", "[cwProject]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    REQUIRE(project->isTemporaryProject());

    QTemporaryDir moveDestinationParent;
    REQUIRE(moveDestinationParent.isValid());

    const QString initialTarget = moveDestinationParent.filePath(QStringLiteral("PersistentProject.cwproj"));
    REQUIRE(project->saveAs(initialTarget));
    rootData->futureManagerModel()->waitForFinished();
    project->errorModel()->clear();

    REQUIRE(project->canSaveDirectly());
    REQUIRE_FALSE(project->isTemporaryProject());

    QTemporaryDir copyDestinationParent;
    REQUIRE(copyDestinationParent.isValid());

    const QString baseName = QStringLiteral("AlreadyThere");
    const QString existingRoot = copyDestinationParent.filePath(baseName);
    REQUIRE(QDir().mkpath(existingRoot));

    const QString targetProjectFile = copyDestinationParent.filePath(baseName + QStringLiteral(".cwproj"));

    CHECK_FALSE(project->saveAs(targetProjectFile));

    REQUIRE(project->errorModel()->count() > 0);
    const auto error = project->errorModel()->last();
    CHECK(error.type() == cwError::Fatal);
    CHECK(error.message() == QStringLiteral("Destination folder '%1' already exists.").arg(existingRoot));
}


// TEST_CASE("Images should be removed correctly", "[cwProject]") {

//     auto rootData = std::make_unique<cwRootData>();
//     auto project = rootData->project();

//     QList<QUrl> filenames {
//         QUrl::fromLocalFile(copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG")),
//         QUrl::fromLocalFile(copyToTempFolder("://datasets/test_cwProject/crashMap.png"))
//     };

//     QList<cwImage> loadedImages;
//     project->addImages(filenames, ProjectFilenameTestHelper::projectDir(project), [&loadedImages](QList<cwImage> images){
//         loadedImages += images;
//     });

//     rootData->futureManagerModel()->waitForFinished();

//     REQUIRE(loadedImages.size() == 2);
//     auto firstImage = loadedImages.at(0);
//     auto secondImage = loadedImages.at(1);

//     QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", QString("ImageRemoveTest"));
//     database.setDatabaseName(project->filename());
//     REQUIRE(database.open());

//     auto idExists = [database](int id) {
//         INFO("idExists:" << id);
//         QString SQL = QString("select count(*) from Images where id = %1").arg(id);

//         INFO("sql:" << SQL.toStdString());
//         INFO("database:" << database.databaseName());

//         REQUIRE(database.isOpen());
//         REQUIRE(database.isValid());

//         QSqlQuery query(database);
//         REQUIRE(query.prepare(SQL));

//         REQUIRE(query.exec());
//         REQUIRE(query.first());

//         auto record = query.record();
//         return record.value(0).toInt() == 1;
//     };

//     auto idNotExists = [idExists](int id) {
//         return !idExists(id);
//     };

//     auto imageIds = [](const cwImage& image) {
//         QList<int> ids {
//             image.original(),
//             image.icon(),
//         };

//         ids.append(image.mipmaps());
//         return ids;
//     };

//     auto imageTestFunc = [imageIds](const cwImage& image, auto func) {
//         auto ids = imageIds(image);

//         return std::accumulate(ids.begin(), ids.end(), true,
//                                [func](bool current, int id)
//                                {
//                                    return current && func(id);
//                                });
//     };

//     auto imageExists = [idExists, imageTestFunc](const cwImage& image) {
//         return imageTestFunc(image, idExists);
//     };

//     auto imageNotExists = [idNotExists, imageTestFunc](const cwImage& image) {
//         return imageTestFunc(image, idNotExists);
//     };

//     CHECK(imageExists(firstImage));
//     CHECK(imageExists(secondImage));

//     cwImageDatabase imageDatabase(project->filename());
//     imageDatabase.removeImage(secondImage);

//     CHECK(imageNotExists(secondImage));
//     CHECK(imageExists(firstImage));

//     imageDatabase.removeImages({firstImage.original()});

//     QList<int> ids = {
//         firstImage.icon()
// };
// ids += firstImage.mipmaps();

// for(auto id : ids){
//     INFO("id:" << id);
//     CHECK(idExists(id));
// }

// CHECK(idNotExists(firstImage.original()));

// database.close();
// }

TEST_CASE("cwProject should add PDF correctly", "[cwProject]") {
    if(cwProject::supportedImageFormats().contains("pdf")) {
        struct Row {
            QList<QSize> pixelSizes;
            int resolutionPPI;
        };

        QList<Row> rows = {
            {
                {
                    QSize(139, 139),
                    QSize(208, 139)
                },
                100
            },

            {
                {
                    QSize(417, 417),
                    QSize(625, 417)
                },
                300
            }
        };

        for(const auto& row : rows) {
            cwPDFSettings::initialize();
            auto rootData = std::make_unique<cwRootData>();
            auto project = rootData->project();

            auto region = project->cavingRegion();
            region->addCave();
            auto cave = region->cave(0);
            cave->addTrip();
            auto trip = cave->trip(0);

            const QString pdfPath = copyToTempFolder("://datasets/test_cwPDFConverter/2page-test.pdf");

            cwPDFSettings::instance()->setResolutionImport(row.resolutionPPI);

            trip->notes()->addFromFiles({QUrl::fromLocalFile(pdfPath)});
            rootData->futureManagerModel()->waitForFinished();

            const QList<cwNote*> notes = trip->notes()->notes();
            REQUIRE(notes.size() == 2);

            QList<QSize> expectedPointSizes;
            expectedPointSizes.reserve(row.pixelSizes.size());
            for (const QSize& pixelSize : row.pixelSizes) {
                expectedPointSizes.append(QSize(qRound(pixelSize.width() * 72.0 / row.resolutionPPI),
                                                qRound(pixelSize.height() * 72.0 / row.resolutionPPI)));
            }

            QList<int> pageIndexes;
            for (const cwNote* note : notes) {
                const cwImage image = note->image();
                CHECK(expectedPointSizes.contains(image.originalSize()));
                CHECK(image.unit() == cwImage::Unit::Points);
                pageIndexes.append(image.page());
            }

            CHECK(pageIndexes.contains(0));
            CHECK(pageIndexes.contains(1));
        }
    }
}

TEST_CASE("cwProject should detect the correct file type", "[cwProject]") {
    //Older sqlite project
    QString datasetFile = copyToTempFolder(":/datasets/test_cwProject/Phake Cave 3000.cw");
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    CHECK(project->projectType(datasetFile) == cwProject::SqliteFileType);

    //A file based file
    datasetFile = copyToTempFolder(":/datasets/test_cwProject/v8.cwproj");
    CHECK(project->projectType(datasetFile) == cwProject::GitFileType);

    //Empty file
    QTemporaryFile tempFile;
    tempFile.open();
    datasetFile = tempFile.fileName();
    CHECK(project->projectType(datasetFile) == cwProject::UnknownFileType);

    //File with random stuff in it
    tempFile.write("Test random data");
    tempFile.close();
    CHECK(project->projectType(datasetFile) == cwProject::UnknownFileType);
}

TEST_CASE("Updating scrap data from a loaded project should save", "[cwProject]") {
    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), QStringLiteral("://datasets/test_cwProject/jaws of the beast with scrap.zip"));
    root->project()->waitLoadToFinish();

    root->futureManagerModel()->waitForFinished();

    cwProject* const project = root->project();
    REQUIRE(project != nullptr);
    REQUIRE_FALSE(project->filename().isEmpty());
    REQUIRE(project->cavingRegion()->caveCount() == 1);

    cwCave* const cave = project->cavingRegion()->cave(0);
    REQUIRE(cave != nullptr);
    REQUIRE(cave->tripCount() == 1);

    cwTrip* const trip = cave->trip(0);
    REQUIRE(trip != nullptr);

    cwSurveyNoteModel* const notesModel = trip->notes();
    REQUIRE(notesModel != nullptr);
    REQUIRE(notesModel->rowCount() == 1);

    cwNote* const note = notesModel->notes().at(0);
    REQUIRE(note != nullptr);
    REQUIRE(note->scraps().size() == 1);

    cwScrap* const scrap = note->scrap(0);
    REQUIRE(scrap != nullptr);

    const QString filename = project->filename();
    const QVector<QPointF> originalPoints = scrap->points();
    REQUIRE_FALSE(originalPoints.isEmpty());

    const QPointF originalPoint = originalPoints.first();
    const double delta = 0.05;

    double newX = originalPoint.x() + delta;
    if(newX >= 1.0) {
        newX = originalPoint.x() - delta;
    }

    double newY = originalPoint.y() - delta;
    if(newY <= 0.0) {
        newY = originalPoint.y() + delta;
    }

    const QPointF updatedPoint(newX, newY);
    REQUIRE(updatedPoint != originalPoint);

    scrap->setPoint(0, updatedPoint);
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    auto reloadedProject = std::make_unique<cwProject>();
    addTokenManager(reloadedProject.get());
    reloadedProject->loadOrConvert(filename);
    reloadedProject->waitLoadToFinish();

    REQUIRE(reloadedProject->cavingRegion()->caveCount() == 1);
    cwCave* const reloadedCave = reloadedProject->cavingRegion()->cave(0);
    REQUIRE(reloadedCave != nullptr);
    REQUIRE(reloadedCave->tripCount() == 1);

    cwTrip* const reloadedTrip = reloadedCave->trip(0);
    REQUIRE(reloadedTrip != nullptr);

    cwSurveyNoteModel* const reloadedNotes = reloadedTrip->notes();
    REQUIRE(reloadedNotes != nullptr);
    REQUIRE(reloadedNotes->rowCount() == 1);

    cwNote* const reloadedNote = reloadedNotes->notes().at(0);
    REQUIRE(reloadedNote != nullptr);
    REQUIRE(reloadedNote->scraps().size() == 1);

    cwScrap* const reloadedScrap = reloadedNote->scrap(0);
    REQUIRE(reloadedScrap != nullptr);

    const QVector<QPointF> reloadedPoints = reloadedScrap->points();
    REQUIRE_FALSE(reloadedPoints.isEmpty());
    const QPointF reloadedFirstPoint = reloadedPoints.first();

    CHECK(reloadedFirstPoint.x() == Catch::Approx(updatedPoint.x()));
    CHECK(reloadedFirstPoint.y() == Catch::Approx(updatedPoint.y()));
}

TEST_CASE("Test save changes", "[cwProject]") {

    SECTION("Simple cave name change") {
        //Change the name of the cave
        auto project = std::make_unique<cwProject>();
        addTokenManager(project.get());
        project->waitSaveToFinish();

        CHECK(project->isTemporaryProject());
        CHECK(QFileInfo::exists(project->filename()));

        //Add the cave
        cwCave* cave = new cwCave();
        cave->setName("test");
        project->cavingRegion()->addCave(cave);
        project->waitSaveToFinish();

        //Check no-name cave
        CHECK(QFileInfo::exists(project->filename()));
        // CHECK(project->filename() == ProjectFilenameTestHelper::projectAbsolutePath(project.get()));

        auto testFilename = ProjectFilenameTestHelper::absolutePath(cave);
        QFileInfo info(testFilename);
        auto oldDir = info.absoluteDir();
        oldDir.cdUp();

        CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(cave)));

        //Test renaming the cave
        cave->setName("test2");
        project->waitSaveToFinish();

        CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(cave)));
        CHECK(ProjectFilenameTestHelper::absolutePath(cave).toStdString() == oldDir.filePath("test2/test2.cwcave").toStdString());
    }

    SECTION("Simple cave and trip") {
        //Change the name of the cave
        auto project = std::make_unique<cwProject>();
        addTokenManager(project.get());
        project->waitSaveToFinish();

        INFO("Project name:" << project->filename().toStdString());
        CHECK(project->isTemporaryProject());
        CHECK(QFileInfo::exists(project->filename()));

        //Add the cave
        cwCave* cave = new cwCave();
        cave->setName("test");

        //Add a trip
        cwTrip* trip = new cwTrip();
        trip->setName("test trip");
        cave->addTrip(trip);

        //Add them all
        project->cavingRegion()->addCave(cave);

        project->waitSaveToFinish();

        CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(cave)));
        CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

        SECTION("Rename the cave") {
            auto testFilename = ProjectFilenameTestHelper::absolutePath(cave);
            QFileInfo info(testFilename);
            auto oldDir = info.absoluteDir();
            oldDir.cdUp();

            //Test renaming the cave
            cave->setName("test2");

            project->waitSaveToFinish();

            CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(cave)));
            CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));
            CHECK(ProjectFilenameTestHelper::absolutePath(cave).toStdString() == oldDir.filePath("test2/test2.cwcave").toStdString());
            CHECK(ProjectFilenameTestHelper::absolutePath(trip).toStdString() == oldDir.filePath("test2/trips/test trip/test trip.cwtrip").toStdString());
        }

        SECTION("Make sure trip calibration changes are saved") {
            trip->calibrations()->setDeclination(12.0);
            project->waitSaveToFinish();

            CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

            auto loadingProject = std::make_unique<cwProject>();
            addTokenManager(loadingProject.get());
            loadingProject->loadOrConvert(project->filename());
            loadingProject->waitLoadToFinish();

            REQUIRE(loadingProject->cavingRegion()->caveCount() == 1);
            auto loadCave = loadingProject->cavingRegion()->cave(0);

            REQUIRE(loadCave->tripCount() == 1);
            auto loadTrip = loadCave->trip(0);

            CHECK(trip->calibrations()->declination() == 12.0);
            CHECK(loadTrip->calibrations()->declination() == 12.0);
        }
    }
}

TEST_CASE("Renaming a trip moves its files and note assets", "[cwProject][cwTrip]") {

    auto root = std::make_unique<cwRootData>();
    REQUIRE(root != nullptr);

    TestHelper helper;
    helper.loadProjectFromZip(root->project(), QStringLiteral("://datasets/test_cwProject/jaws of the beast with scrap.zip"));
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();

    cwProject* const project = root->project();
    REQUIRE(project != nullptr);
    REQUIRE_FALSE(project->filename().isEmpty());

    REQUIRE(project->cavingRegion()->caveCount() == 1);
    cwCave* const cave = project->cavingRegion()->cave(0);
    REQUIRE(cave != nullptr);
    REQUIRE(cave->tripCount() == 1);

    cwTrip* const trip = cave->trip(0);
    REQUIRE(trip != nullptr);

    cwSurveyNoteModel* const notesModel = trip->notes();
    REQUIRE(notesModel != nullptr);
    const int originalNoteCount = notesModel->rowCount();
    REQUIRE(originalNoteCount >= 1);

    cwSurveyNoteLiDARModel* const lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);

    const QString originalTripName = trip->name();
    REQUIRE(originalTripName.toStdString() == QStringLiteral("2019c154_-_party_fault").toStdString());

    const QString originalTripFile = ProjectFilenameTestHelper::absolutePath(trip);
    QFileInfo originalTripInfo(originalTripFile);
    const QString originalTripDirPath = originalTripInfo.absoluteDir().absolutePath();

    const QString originalNotesDirPath = QDir(originalTripDirPath).filePath(QStringLiteral("notes"));
    QDir originalNotesDir(originalNotesDirPath);

    const QStringList noteAssetFilters = {
        QStringLiteral("*.cwnote"),
        QStringLiteral("*.svg"),
        QStringLiteral("*.png"),
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.tif"),
        QStringLiteral("*.tiff")
    };

    const QStringList originalNoteFiles = originalNotesDir.entryList(noteAssetFilters, QDir::Files | QDir::NoDotAndDotDot);
    REQUIRE_FALSE(originalNoteFiles.isEmpty());

    QSet<QString> expectedNoteImages;
    for(cwNote* note : notesModel->notes()) {
        REQUIRE(note != nullptr);
        expectedNoteImages.insert(QFileInfo(note->image().path()).fileName());
    }

    // Add a LiDAR note with a copied GLB so rename covers LiDAR assets too
    const QString glbSource = copyToTempFolder(":/datasets/test_cwSurveyNotesConcatModel/bones.glb");
    REQUIRE(QFileInfo::exists(glbSource));

    lidarModel->addFromFiles({QUrl::fromLocalFile(glbSource)});
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    REQUIRE(lidarModel->rowCount() == 1);
    cwNoteLiDAR* lidarNote = dynamic_cast<cwNoteLiDAR*>(lidarModel->notes().at(0));
    REQUIRE(lidarNote != nullptr);
    const QString lidarFileName = QFileInfo(glbSource).fileName();

    const QString originalLidarPath = ProjectFilenameTestHelper::dir(lidarNote).absoluteFilePath(lidarFileName);
    REQUIRE(QFileInfo::exists(originalLidarPath));

    trip->setName(QStringLiteral("Trip 2"));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    const QString renamedTripFile = ProjectFilenameTestHelper::absolutePath(trip);
    QFileInfo renamedTripInfo(renamedTripFile);
    const QString renamedTripDirPath = renamedTripInfo.absoluteDir().absolutePath();
    const QString renamedNotesDirPath = QDir(renamedTripDirPath).filePath(QStringLiteral("notes"));

    CHECK_FALSE(QFileInfo::exists(originalTripFile));
    CHECK_FALSE(QDir(originalTripDirPath).exists());
    CHECK(QFileInfo::exists(renamedTripFile));
    CHECK(renamedTripInfo.completeBaseName() == QStringLiteral("Trip 2"));

    QDir tripsDirAfter = renamedTripInfo.dir();
    tripsDirAfter.cdUp();
    const QStringList tripDirs = tripsDirAfter.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    CHECK(tripDirs.contains(QStringLiteral("Trip 2")));
    CHECK_FALSE(tripDirs.contains(originalTripName));

    QDir renamedTripDir(renamedTripDirPath);
    const QStringList renamedCwFiles = renamedTripDir.entryList(QStringList() << "*.cwtrip" << "*.cwproj",
                                                                QDir::Files | QDir::NoDotAndDotDot);
    INFO("Files:" << renamedCwFiles.join(",").toStdString());

    REQUIRE(renamedCwFiles.size() == 1);
    CHECK(renamedCwFiles.first() == renamedTripInfo.fileName());

    QDir renamedNotesDir(renamedNotesDirPath);
    REQUIRE(renamedNotesDir.exists());
    const QStringList renamedNoteFiles = renamedNotesDir.entryList(noteAssetFilters, QDir::Files | QDir::NoDotAndDotDot);

    auto toSet = [](const QStringList& files) {
        QSet<QString> result;
        for(const auto& file : files) {
            result.insert(file);
        }
        return result;
    };

    CHECK(toSet(originalNoteFiles) == toSet(renamedNoteFiles));
    CHECK_FALSE(QDir(originalNotesDirPath).exists());

    const QString renamedLidarPath = ProjectFilenameTestHelper::dir(lidarNote).absoluteFilePath(lidarFileName);
    CHECK_FALSE(QFileInfo::exists(originalLidarPath));
    CHECK(QFileInfo::exists(renamedLidarPath));
    CHECK(lidarNote->filename().toStdString() == QFileInfo(lidarNote->filename()).fileName().toStdString());

    auto reloadedProject = std::make_unique<cwProject>();
    addTokenManager(reloadedProject.get());
    reloadedProject->loadOrConvert(project->filename());
    reloadedProject->waitLoadToFinish();

    REQUIRE(reloadedProject->cavingRegion()->caveCount() == 1);
    cwCave* const reloadedCave = reloadedProject->cavingRegion()->cave(0);
    REQUIRE(reloadedCave != nullptr);
    REQUIRE(reloadedCave->tripCount() == 1);

    cwTrip* const reloadedTrip = reloadedCave->trip(0);
    REQUIRE(reloadedTrip != nullptr);
    CHECK(reloadedTrip->name() == QStringLiteral("Trip 2"));

    cwSurveyNoteModel* const reloadedNotes = reloadedTrip->notes();
    REQUIRE(reloadedNotes != nullptr);
    REQUIRE(reloadedNotes->rowCount() == originalNoteCount);

    for(cwNote* note : reloadedNotes->notes()) {
        REQUIRE(note != nullptr);
        const QString imagePath = note->image().path();
        CHECK_FALSE(imagePath.isEmpty());
        const QString fileName = QFileInfo(imagePath).fileName();
        CHECK(expectedNoteImages.contains(fileName));
        const QDir projectDir = QFileInfo(reloadedProject->filename()).absoluteDir();
        const QString expectedPath = ProjectFilenameTestHelper::dir(note).absoluteFilePath(fileName);
        CHECK(ProjectFilenameTestHelper::absolutePathNoteImage(note).path().toStdString() == expectedPath.toStdString());
    }

    cwSurveyNoteLiDARModel* const reloadedLidarModel = reloadedTrip->notesLiDAR();
    REQUIRE(reloadedLidarModel != nullptr);
    REQUIRE(reloadedLidarModel->rowCount() == 1);
    cwNoteLiDAR* const reloadedLidarNote = dynamic_cast<cwNoteLiDAR*>(reloadedLidarModel->notes().at(0));
    REQUIRE(reloadedLidarNote != nullptr);
    const QString reloadedLidarAbsolute1 = ProjectFilenameTestHelper::absolutePath(reloadedLidarNote, reloadedLidarNote->filename());
    CHECK(!reloadedLidarAbsolute1.isEmpty());
    CHECK(QFileInfo::exists(reloadedLidarAbsolute1));
    const QString reloadedLidarAbsolute = ProjectFilenameTestHelper::dir(reloadedLidarNote).absoluteFilePath(QFileInfo(reloadedLidarAbsolute1).fileName());
    CHECK(QFileInfo::exists(reloadedLidarAbsolute));
}

TEST_CASE("Trip calibration persistence", "[cwProject]") {

    // Fresh project with one cave + one trip
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("test-cal");

    cwTrip* trip = new cwTrip();
    trip->setName("trip-cal");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

    SECTION("Booleans persist") {
        for(int i = 0; i < 100; i++) {
            auto calibration = trip->calibrations();
            calibration->setFrontSights(true);
            calibration->setBackSights(true);
            calibration->setCorrectedCompassFrontsight(true);
            calibration->setCorrectedClinoFrontsight(true);
            calibration->setCorrectedCompassBacksight(true);
            calibration->setCorrectedClinoBacksight(true);

            project->waitSaveToFinish();

            // Reload and verify
            auto loadingProject = std::make_unique<cwProject>();
            addTokenManager(loadingProject.get());
            loadingProject->loadOrConvert(project->filename());
            loadingProject->waitLoadToFinish();

            REQUIRE(loadingProject->cavingRegion()->caveCount() == 1);
            auto loadCave = loadingProject->cavingRegion()->cave(0);
            REQUIRE(loadCave->tripCount() == 1);
            auto loadTrip = loadCave->trip(0);
            auto loadCalibration = loadTrip->calibrations();

            CHECK(loadCalibration->hasFrontSights());
            CHECK(loadCalibration->hasBackSights());
            CHECK(loadCalibration->hasCorrectedCompassFrontsight());
            CHECK(loadCalibration->hasCorrectedClinoFrontsight());
            CHECK(loadCalibration->hasCorrectedCompassBacksight());
            CHECK(loadCalibration->hasCorrectedClinoBacksight());
        }
    }

    SECTION("Not Booleans persist") {
        auto calibration = trip->calibrations();
        calibration->setFrontSights(false);
        calibration->setBackSights(false);
        calibration->setCorrectedCompassFrontsight(false);
        calibration->setCorrectedClinoFrontsight(false);
        calibration->setCorrectedCompassBacksight(false);
        calibration->setCorrectedClinoBacksight(false);

        project->waitSaveToFinish();

        // Reload and verify
        auto loadingProject = std::make_unique<cwProject>();
        addTokenManager(loadingProject.get());
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        REQUIRE(loadingProject->cavingRegion()->caveCount() == 1);
        auto loadCave = loadingProject->cavingRegion()->cave(0);
        REQUIRE(loadCave->tripCount() == 1);
        auto loadTrip = loadCave->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(!loadCalibration->hasFrontSights());
        CHECK(!loadCalibration->hasBackSights());
        CHECK(!loadCalibration->hasCorrectedCompassFrontsight());
        CHECK(!loadCalibration->hasCorrectedClinoFrontsight());
        CHECK(!loadCalibration->hasCorrectedCompassBacksight());
        CHECK(!loadCalibration->hasCorrectedClinoBacksight());
    }

    SECTION("Numeric values persist") {
        const double tape = 1.0023;
        const double fComp = -0.75;
        const double fClino = 0.35;
        const double bComp = 1.10;
        const double bClino = -0.45;
        const double decl = 13.25;

        auto calibration = trip->calibrations();
        calibration->setTapeCalibration(tape);
        calibration->setFrontCompassCalibration(fComp);
        calibration->setFrontClinoCalibration(fClino);
        calibration->setBackCompassCalibration(bComp);
        calibration->setBackClinoCalibration(bClino);
        calibration->setDeclination(decl);

        project->waitSaveToFinish();

        auto loadingProject = std::make_unique<cwProject>();
        addTokenManager(loadingProject.get());
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        auto loadTrip = loadingProject->cavingRegion()->cave(0)->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(loadCalibration->tapeCalibration() == tape);
        CHECK(loadCalibration->frontCompassCalibration() == fComp);
        CHECK(loadCalibration->frontClinoCalibration() == fClino);
        CHECK(loadCalibration->backCompassCalibration() == bComp);
        CHECK(loadCalibration->backClinoCalibration() == bClino);
        CHECK(loadCalibration->declination() == decl);
    }

    SECTION("Distance unit persists") {
        // Pick a unit that differs from the default
        const cwUnits::LengthUnit unit = cwUnits::LengthUnit::Feet;

        auto calibration = trip->calibrations();
        calibration->setDistanceUnit(unit);

        project->waitSaveToFinish();

        auto loadingProject = std::make_unique<cwProject>();
        addTokenManager(loadingProject.get());
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        auto loadTrip = loadingProject->cavingRegion()->cave(0)->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(loadCalibration->distanceUnit() == unit);
    }

    SECTION("Last write wins for calibration fields") {
        auto calibration = trip->calibrations();

        calibration->setDeclination(5.0);
        project->waitSaveToFinish();

        calibration->setDeclination(9.5);
        calibration->setFrontCompassCalibration(0.2);
        project->waitSaveToFinish();

        // Final overwrite before reload
        calibration->setDeclination(12.75);
        calibration->setFrontCompassCalibration(-0.15);
        project->waitSaveToFinish();

        auto loadingProject = std::make_unique<cwProject>();
        addTokenManager(loadingProject.get());
        loadingProject->loadOrConvert(project->filename());
        loadingProject->waitLoadToFinish();

        auto loadTrip = loadingProject->cavingRegion()->cave(0)->trip(0);
        auto loadCalibration = loadTrip->calibrations();

        CHECK(loadCalibration->declination() == 12.75);
        CHECK(loadCalibration->frontCompassCalibration() == -0.15);
    }
}

TEST_CASE("Trip team member role changes persist and trigger save", "[cwProject][cwTeam]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("team-roles-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("team-roles-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

    // Use the concrete model so we can call add/remove helpers
    cwTeam* const team = static_cast<cwTeam*>(trip->team());
    REQUIRE(team != nullptr);

    SECTION("Insert member and set NameRole + JobsRole → save + reload") {
        team->addTeamMember(); // appends at end
        const int rowInserted = team->rowCount() - 1;
        REQUIRE(rowInserted >= 0);

        // Set roles
        const QString nameInserted = QStringLiteral("Alice Example");
        const QStringList jobsInserted = QStringList{QStringLiteral("Sketcher"), QStringLiteral("Reader")};

        QModelIndex nameIndex = team->index(rowInserted, 0);
        REQUIRE(nameIndex.isValid());

        bool okName = team->setData(nameIndex, nameInserted, cwTeam::NameRole);
        REQUIRE(okName);

        bool okJobs = team->setData(nameIndex, jobsInserted, cwTeam::JobsRole);
        REQUIRE(okJobs);

        project->waitSaveToFinish();

        // Reload and verify both roles
        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const reloadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(reloadedTrip != nullptr);

        cwTeam* const reloadedTeam = static_cast<cwTeam*>(reloadedTrip->team());
        REQUIRE(reloadedTeam != nullptr);

        REQUIRE(reloadedTeam->rowCount() >= 1);
        QModelIndex reloadedIndex = reloadedTeam->index(rowInserted, 0);
        REQUIRE(reloadedIndex.isValid());

        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::NameRole).toString() == nameInserted);
        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::JobsRole).toStringList() == jobsInserted);
    }

    SECTION("Edit NameRole + JobsRole on existing member → save + reload") {
        // Ensure one row exists
        if (team->rowCount() == 0) {
            team->addTeamMember();
            const int newRow = team->rowCount() - 1;
            QModelIndex idx = team->index(newRow, 0);
            REQUIRE(idx.isValid());
            REQUIRE(team->setData(idx, QStringLiteral("Temp Member"), cwTeam::NameRole));
            REQUIRE(team->setData(idx, QStringList{QStringLiteral("Assistant")}, cwTeam::JobsRole));
        }

        const int rowToEdit = 0;
        QModelIndex editIndex = team->index(rowToEdit, 0);
        REQUIRE(editIndex.isValid());

        const QString newName = QStringLiteral("Bob Renamed");
        const QStringList newJobs = QStringList{QStringLiteral("Sketcher"), QStringLiteral("Instrument Person")};

        REQUIRE(team->setData(editIndex, newName, cwTeam::NameRole));
        REQUIRE(team->setData(editIndex, newJobs, cwTeam::JobsRole));

        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const reloadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(reloadedTrip != nullptr);

        cwTeam* const reloadedTeam = static_cast<cwTeam*>(reloadedTrip->team());
        REQUIRE(reloadedTeam != nullptr);

        REQUIRE(reloadedTeam->rowCount() >= 1);
        QModelIndex reloadedIndex = reloadedTeam->index(rowToEdit, 0);
        REQUIRE(reloadedIndex.isValid());

        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::NameRole).toString() == newName);
        CHECK(reloadedTeam->data(reloadedIndex, cwTeam::JobsRole).toStringList() == newJobs);
    }

    SECTION("Remove member → save + reload") {
        // Ensure at least two members exist so removal changes the count
        while (team->rowCount() < 2) {
            team->addTeamMember();
            const int r = team->rowCount() - 1;
            QModelIndex idx = team->index(r, 0);
            REQUIRE(idx.isValid());
            REQUIRE(team->setData(idx, QStringLiteral("Member %1").arg(r + 1), cwTeam::NameRole));
            REQUIRE(team->setData(idx, QStringList{QStringLiteral("Helper")}, cwTeam::JobsRole));
        }

        const int previousCount = team->rowCount();
        const int removeRow = 0;
        team->removeTeamMember(removeRow);

        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const reloadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(reloadedTrip != nullptr);

        cwTeam* const reloadedTeam = static_cast<cwTeam*>(reloadedTrip->team());
        REQUIRE(reloadedTeam != nullptr);

        CHECK(reloadedTeam->rowCount() == previousCount - 1);
    }
}

TEST_CASE("Trip date persistence", "[cwProject][cwTrip]") {
    // Build project → cave → trip
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("date-persist-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("date-persist-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

    SECTION("Initial date persists after reload") {
        const QDateTime dateUtc(QDate(2024, 5, 1), QTime());
        trip->setDate(dateUtc);
        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const loadTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadTrip != nullptr);

        const QDateTime loadedDate = loadTrip->date();
        CHECK(loadedDate.toMSecsSinceEpoch() == dateUtc.toMSecsSinceEpoch());
    }

    SECTION("Last write wins for date field") {
        const QDateTime firstDate(QDate(2023, 11, 23), QTime());
        const QDateTime secondDate(QDate(2024, 1, 2), QTime());
        const QDateTime finalDate(QDate(2025, 8, 25), QTime());

        trip->setDate(firstDate);
        project->waitSaveToFinish();

        trip->setDate(secondDate);
        project->waitSaveToFinish();

        trip->setDate(finalDate);
        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const loadTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadTrip != nullptr);

        const QDateTime loadedDate = loadTrip->date();
        CHECK(loadedDate.toMSecsSinceEpoch() == finalDate.toMSecsSinceEpoch());
    }
}

TEST_CASE("Survey chunk persistence", "[cwProject][cwTrip][cwSurveyChunk]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("chunk-persist-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("chunk-persist-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

    SECTION("Add two chunks with data → save → reload → verify ALL fields") {
        // ---------- Chunk 0 ----------
        trip->addNewChunk();
        cwSurveyChunk* c0 = trip->chunk(0);
        REQUIRE(c0 != nullptr);

        // Fill stations[0,1], LRUDs
        c0->setData(cwSurveyChunk::StationNameRole, 0, "A");
        c0->setData(cwSurveyChunk::StationLeftRole, 0, "0.10");
        c0->setData(cwSurveyChunk::StationRightRole, 0, "0.20");
        c0->setData(cwSurveyChunk::StationUpRole, 0, "0.30");
        c0->setData(cwSurveyChunk::StationDownRole, 0, "0.40");

        c0->setData(cwSurveyChunk::StationNameRole, 1, "B");
        c0->setData(cwSurveyChunk::StationLeftRole, 1, "0.50");
        c0->setData(cwSurveyChunk::StationRightRole, 1, "0.60");
        c0->setData(cwSurveyChunk::StationUpRole, 1, "0.70");
        c0->setData(cwSurveyChunk::StationDownRole, 1, "0.80");

        // Shot 0 (A->B), include = true
        c0->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.50");
        c0->setData(cwSurveyChunk::ShotCompassRole, 0, "12.30");
        c0->setData(cwSurveyChunk::ShotBackCompassRole, 0, "12.40");
        c0->setData(cwSurveyChunk::ShotClinoRole, 0, "-2.50");
        c0->setData(cwSurveyChunk::ShotBackClinoRole, 0, "-2.60");
        c0->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

        // Shot 1 (B->C), include = false + LRUDs for C
        {
            const cwStation from("B");
            const cwStation to("C");
            const cwShot shot("4.75","33.0","213.0","0.50","0.60");
            c0->appendShot(from, to, shot);

            const int shotIdx = 1;
            const int stnIdx = 2;

            c0->setData(cwSurveyChunk::ShotBackCompassRole, shotIdx, "213.2");
            c0->setData(cwSurveyChunk::ShotBackClinoRole, shotIdx, "0.55");
            c0->setData(cwSurveyChunk::ShotDistanceIncludedRole, shotIdx, false);

            c0->setData(cwSurveyChunk::StationNameRole, stnIdx, "C");
            c0->setData(cwSurveyChunk::StationLeftRole, stnIdx, "0.90");
            c0->setData(cwSurveyChunk::StationRightRole, stnIdx, "1.00");
            c0->setData(cwSurveyChunk::StationUpRole, stnIdx, "1.10");
            c0->setData(cwSurveyChunk::StationDownRole, stnIdx, "1.20");
        }

        // ---------- Chunk 1 ----------
        trip->addNewChunk();
        cwSurveyChunk* c1 = trip->chunk(1);
        REQUIRE(c1 != nullptr);

        c1->setData(cwSurveyChunk::StationNameRole, 0, "C");
        c1->setData(cwSurveyChunk::StationLeftRole, 0, "0.11");
        c1->setData(cwSurveyChunk::StationRightRole, 0, "0.22");
        c1->setData(cwSurveyChunk::StationUpRole, 0, "0.33");
        c1->setData(cwSurveyChunk::StationDownRole, 0, "0.44");

        c1->setData(cwSurveyChunk::StationNameRole, 1, "D");
        c1->setData(cwSurveyChunk::StationLeftRole, 1, "0.55");
        c1->setData(cwSurveyChunk::StationRightRole, 1, "0.66");
        c1->setData(cwSurveyChunk::StationUpRole, 1, "0.77");
        c1->setData(cwSurveyChunk::StationDownRole, 1, "0.88");

        // Shot 0 (C->D), include = true
        c1->setData(cwSurveyChunk::ShotDistanceRole, 0, "7.25");
        c1->setData(cwSurveyChunk::ShotCompassRole, 0, "101.0");
        c1->setData(cwSurveyChunk::ShotBackCompassRole, 0, "281.0");
        c1->setData(cwSurveyChunk::ShotClinoRole, 0, "3.00");
        c1->setData(cwSurveyChunk::ShotBackClinoRole, 0, "3.10");
        c1->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

        // Shot 1 (D->E), include = false + LRUDs for E
        {
            const cwStation from("D");
            const cwStation to("E");
            const cwShot shot("2.10","280.0","100.0","-1.25","-1.20");
            c1->appendShot(from, to, shot);

            const int shotIdx = 1;
            const int stnIdx = 2;

            c1->setData(cwSurveyChunk::ShotBackCompassRole, shotIdx, "100.2");
            c1->setData(cwSurveyChunk::ShotBackClinoRole, shotIdx, "-1.22");
            c1->setData(cwSurveyChunk::ShotDistanceIncludedRole, shotIdx, false);

            c1->setData(cwSurveyChunk::StationNameRole, stnIdx, "E");
            c1->setData(cwSurveyChunk::StationLeftRole, stnIdx, "0.95");
            c1->setData(cwSurveyChunk::StationRightRole, stnIdx, "1.05");
            c1->setData(cwSurveyChunk::StationUpRole, stnIdx, "1.15");
            c1->setData(cwSurveyChunk::StationDownRole, stnIdx, "1.25");
        }

        project->waitSaveToFinish();

        // ---------- Reload & VERIFY EVERYTHING ----------
        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* lt = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(lt != nullptr);
        CHECK(lt->chunkCount() == 2);

        cwSurveyChunk* rc0 = lt->chunk(0);
        cwSurveyChunk* rc1 = lt->chunk(1);
        REQUIRE(rc0 != nullptr);
        REQUIRE(rc1 != nullptr);

        // Chunk 0: counts
        CHECK(rc0->stationCount() == 3);
        CHECK(rc0->shotCount() == 2);

        // Chunk 0: station 0 (A) + LRUD
        CHECK(rc0->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "A");
        CHECK(rc0->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.10");
        CHECK(rc0->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.20");
        CHECK(rc0->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.30");
        CHECK(rc0->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.40");

        // Chunk 0: station 1 (B) + LRUD
        CHECK(rc0->data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "B");
        CHECK(rc0->data(cwSurveyChunk::StationLeftRole, 1).toString().toStdString() == "0.50");
        CHECK(rc0->data(cwSurveyChunk::StationRightRole, 1).toString().toStdString() == "0.60");
        CHECK(rc0->data(cwSurveyChunk::StationUpRole, 1).toString().toStdString() == "0.70");
        CHECK(rc0->data(cwSurveyChunk::StationDownRole, 1).toString().toStdString() == "0.80");

        // Chunk 0: station 2 (C) + LRUD
        CHECK(rc0->data(cwSurveyChunk::StationNameRole, 2).toString().toStdString() == "C");
        CHECK(rc0->data(cwSurveyChunk::StationLeftRole, 2).toString().toStdString() == "0.90");
        CHECK(rc0->data(cwSurveyChunk::StationRightRole, 2).toString().toStdString() == "1.00");
        CHECK(rc0->data(cwSurveyChunk::StationUpRole, 2).toString().toStdString() == "1.10");
        CHECK(rc0->data(cwSurveyChunk::StationDownRole, 2).toString().toStdString() == "1.20");

        // Chunk 0: shot 0 fields (A->B)
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "10.50");
        CHECK(rc0->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "12.30");
        CHECK(rc0->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "12.40");
        CHECK(rc0->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "-2.50");
        CHECK(rc0->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "-2.60");
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == true);

        // Chunk 0: shot 1 fields (B->C)
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceRole, 1).toString().toStdString() == "4.75");
        CHECK(rc0->data(cwSurveyChunk::ShotCompassRole, 1).toString().toStdString() == "33.0");
        CHECK(rc0->data(cwSurveyChunk::ShotBackCompassRole, 1).toString().toStdString() == "213.2");
        CHECK(rc0->data(cwSurveyChunk::ShotClinoRole, 1).toString().toStdString() == "0.50");
        CHECK(rc0->data(cwSurveyChunk::ShotBackClinoRole, 1).toString().toStdString() == "0.55");
        CHECK(rc0->data(cwSurveyChunk::ShotDistanceIncludedRole, 1).toBool() == false);

        // Chunk 1: counts
        CHECK(rc1->stationCount() == 3);
        CHECK(rc1->shotCount() == 2);

        // Chunk 1: station 0 (C) + LRUD
        CHECK(rc1->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "C");
        CHECK(rc1->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.11");
        CHECK(rc1->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.22");
        CHECK(rc1->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.33");
        CHECK(rc1->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.44");

        // Chunk 1: station 1 (D) + LRUD
        CHECK(rc1->data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "D");
        CHECK(rc1->data(cwSurveyChunk::StationLeftRole, 1).toString().toStdString() == "0.55");
        CHECK(rc1->data(cwSurveyChunk::StationRightRole, 1).toString().toStdString() == "0.66");
        CHECK(rc1->data(cwSurveyChunk::StationUpRole, 1).toString().toStdString() == "0.77");
        CHECK(rc1->data(cwSurveyChunk::StationDownRole, 1).toString().toStdString() == "0.88");

        // Chunk 1: station 2 (E) + LRUD
        CHECK(rc1->data(cwSurveyChunk::StationNameRole, 2).toString().toStdString() == "E");
        CHECK(rc1->data(cwSurveyChunk::StationLeftRole, 2).toString().toStdString() == "0.95");
        CHECK(rc1->data(cwSurveyChunk::StationRightRole, 2).toString().toStdString() == "1.05");
        CHECK(rc1->data(cwSurveyChunk::StationUpRole, 2).toString().toStdString() == "1.15");
        CHECK(rc1->data(cwSurveyChunk::StationDownRole, 2).toString().toStdString() == "1.25");

        // Chunk 1: shot 0 fields (C->D)
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "7.25");
        CHECK(rc1->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "101.0");
        CHECK(rc1->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "281.0");
        CHECK(rc1->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "3.00");
        CHECK(rc1->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "3.10");
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == true);

        // Chunk 1: shot 1 fields (D->E)
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceRole, 1).toString().toStdString() == "2.10");
        CHECK(rc1->data(cwSurveyChunk::ShotCompassRole, 1).toString().toStdString() == "280.0");
        CHECK(rc1->data(cwSurveyChunk::ShotBackCompassRole, 1).toString().toStdString() == "100.2");
        CHECK(rc1->data(cwSurveyChunk::ShotClinoRole, 1).toString().toStdString() == "-1.25");
        CHECK(rc1->data(cwSurveyChunk::ShotBackClinoRole, 1).toString().toStdString() == "-1.22");
        CHECK(rc1->data(cwSurveyChunk::ShotDistanceIncludedRole, 1).toBool() == false);
    }

    SECTION("Edit one chunk: verify ALL changed fields survive reload") {
        trip->addNewChunk();
        cwSurveyChunk* chunk = trip->chunk(0);
        REQUIRE(chunk != nullptr);

        // Seed
        chunk->setData(cwSurveyChunk::StationNameRole, 0, "S1");
        chunk->setData(cwSurveyChunk::StationLeftRole, 0, "0.10");
        chunk->setData(cwSurveyChunk::StationRightRole, 0, "0.20");
        chunk->setData(cwSurveyChunk::StationUpRole, 0, "0.30");
        chunk->setData(cwSurveyChunk::StationDownRole, 0, "0.40");
        chunk->setData(cwSurveyChunk::StationNameRole, 1, "S2");

        chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "5.00");
        chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "45.0");
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "225.0");
        chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "1.00");
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "1.10");
        chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

        // Edits
        chunk->setData(cwSurveyChunk::StationNameRole, 0, "S1-renamed");
        chunk->setData(cwSurveyChunk::StationLeftRole, 0, "0.15");
        chunk->setData(cwSurveyChunk::StationRightRole, 0, "0.25");
        chunk->setData(cwSurveyChunk::StationUpRole, 0, "0.35");
        chunk->setData(cwSurveyChunk::StationDownRole, 0, "0.45");

        chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "5.25");
        chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "46.5");
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "226.2");
        chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "0.75");
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "1.05");
        chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, false);

        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* reloadTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        cwSurveyChunk* reloadChunk = reloadTrip->chunk(0);

        CHECK(reloadChunk->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "S1-renamed");
        CHECK(reloadChunk->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.15");
        CHECK(reloadChunk->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.25");
        CHECK(reloadChunk->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.35");
        CHECK(reloadChunk->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.45");

        CHECK(reloadChunk->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "5.25");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "46.5");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "226.2");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "0.75");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "1.05");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == false);
    }

    SECTION("Remove first chunk → save → reload → verify remaining chunk ALL fields") {
        // Chunk 0
        trip->addNewChunk();
        {
            cwSurveyChunk* chunk = trip->chunk(0);
            chunk->setData(cwSurveyChunk::StationNameRole, 0, "X1");
            chunk->setData(cwSurveyChunk::StationNameRole, 1, "X2");
            chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "3.00");
            chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "10.0");
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "190.0");
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "-1.00");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "-1.10");
            chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, true);

            // X2 -> X3
            const cwStation from("X2");
            const cwStation to("X3");
            const cwShot shot("1.50","15.0","195.0","-0.50","-0.40");
            chunk->appendShot(from, to, shot);
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 1, "195.2");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 1, "-0.45");
            chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 1, false);
        }

        // Chunk 1
        trip->addNewChunk();
        {
            cwSurveyChunk* chunk = trip->chunk(1);
            chunk->setData(cwSurveyChunk::StationNameRole, 0, "Y1");
            chunk->setData(cwSurveyChunk::StationLeftRole, 0, "0.11");
            chunk->setData(cwSurveyChunk::StationRightRole, 0, "0.22");
            chunk->setData(cwSurveyChunk::StationUpRole, 0, "0.33");
            chunk->setData(cwSurveyChunk::StationDownRole, 0, "0.44");

            chunk->setData(cwSurveyChunk::StationNameRole, 1, "Y2");
            chunk->setData(cwSurveyChunk::StationLeftRole, 1, "0.55");
            chunk->setData(cwSurveyChunk::StationRightRole, 1, "0.66");
            chunk->setData(cwSurveyChunk::StationUpRole, 1, "0.77");
            chunk->setData(cwSurveyChunk::StationDownRole, 1, "0.88");

            chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "8.00");
            chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "200.0");
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "20.0");
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "2.00");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "2.10");
            chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, false);

            const cwStation from("Y2");
            const cwStation to("Y3");
            const cwShot shot("2.10","205.0","25.0","2.10","2.05");
            chunk->appendShot(from, to, shot);
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 1, "25.2");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 1, "2.06");
            chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 1, true);

            chunk->setData(cwSurveyChunk::StationNameRole, 2, "Y3");
            chunk->setData(cwSurveyChunk::StationLeftRole, 2, "0.95");
            chunk->setData(cwSurveyChunk::StationRightRole, 2, "1.05");
            chunk->setData(cwSurveyChunk::StationUpRole, 2, "1.15");
            chunk->setData(cwSurveyChunk::StationDownRole, 2, "1.25");
        }

        REQUIRE(trip->chunkCount() == 2);
        trip->removeChunks(0, 0);
        project->waitSaveToFinish();

        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* lt = reloaded->cavingRegion()->cave(0)->trip(0);
        CHECK(lt->chunkCount() == 1);

        cwSurveyChunk* reloadChunk = lt->chunk(0);
        REQUIRE(reloadChunk != nullptr);

        // Remaining chunk is Y*; verify ALL fields
        CHECK(reloadChunk->stationCount() == 3);
        CHECK(reloadChunk->shotCount() == 2);

        CHECK(reloadChunk->data(cwSurveyChunk::StationNameRole, 0).toString().toStdString() == "Y1");
        CHECK(reloadChunk->data(cwSurveyChunk::StationLeftRole, 0).toString().toStdString() == "0.11");
        CHECK(reloadChunk->data(cwSurveyChunk::StationRightRole, 0).toString().toStdString() == "0.22");
        CHECK(reloadChunk->data(cwSurveyChunk::StationUpRole, 0).toString().toStdString() == "0.33");
        CHECK(reloadChunk->data(cwSurveyChunk::StationDownRole, 0).toString().toStdString() == "0.44");

        CHECK(reloadChunk->data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "Y2");
        CHECK(reloadChunk->data(cwSurveyChunk::StationLeftRole, 1).toString().toStdString() == "0.55");
        CHECK(reloadChunk->data(cwSurveyChunk::StationRightRole, 1).toString().toStdString() == "0.66");
        CHECK(reloadChunk->data(cwSurveyChunk::StationUpRole, 1).toString().toStdString() == "0.77");
        CHECK(reloadChunk->data(cwSurveyChunk::StationDownRole, 1).toString().toStdString() == "0.88");

        CHECK(reloadChunk->data(cwSurveyChunk::StationNameRole, 2).toString().toStdString() == "Y3");
        CHECK(reloadChunk->data(cwSurveyChunk::StationLeftRole, 2).toString().toStdString() == "0.95");
        CHECK(reloadChunk->data(cwSurveyChunk::StationRightRole, 2).toString().toStdString() == "1.05");
        CHECK(reloadChunk->data(cwSurveyChunk::StationUpRole, 2).toString().toStdString() == "1.15");
        CHECK(reloadChunk->data(cwSurveyChunk::StationDownRole, 2).toString().toStdString() == "1.25");

        CHECK(reloadChunk->data(cwSurveyChunk::ShotDistanceRole, 0).toString().toStdString() == "8.00");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotCompassRole, 0).toString().toStdString() == "200.0");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotBackCompassRole, 0).toString().toStdString() == "20.0");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotClinoRole, 0).toString().toStdString() == "2.00");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotBackClinoRole, 0).toString().toStdString() == "2.10");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotDistanceIncludedRole, 0).toBool() == false);

        CHECK(reloadChunk->data(cwSurveyChunk::ShotDistanceRole, 1).toString().toStdString() == "2.10");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotCompassRole, 1).toString().toStdString() == "205.0");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotBackCompassRole, 1).toString().toStdString() == "25.2");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotClinoRole, 1).toString().toStdString() == "2.10");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotBackClinoRole, 1).toString().toStdString() == "2.06");
        CHECK(reloadChunk->data(cwSurveyChunk::ShotDistanceIncludedRole, 1).toBool() == true);
    }


}


TEST_CASE("Note and Scrap persistence", "[cwProject][cwTrip][cwSurveyNoteModel][cwNote][cwScrap]") {
    // Project → cave → trip
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("note-persist-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("note-persist-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

    // Use the trip's note model
    cwSurveyNoteModel* const noteModel = trip->notes();
    REQUIRE(noteModel != nullptr);
    CHECK(noteModel->rowCount() == 0);

    SECTION("Create note + scrap, save, reload, and verify all fields") {
        // --- Create a note object and add via model ---
        auto* note = new cwNote();
        note->setName(QStringLiteral("Entrance Sketch"));
        note->setRotate(12.5);

        // Set an image path (no actual file required for persistence of the string path)
        cwImage image;
        image.setPath(QStringLiteral("entrance.png"));
        note->setImage(image);
        note->imageResolution()->setUnit(cwUnits::DotsPerCentimeter);
        note->imageResolution()->setValue(12.34);

        // Attach the note to the trip via the model
        noteModel->addNotes(QList<cwNote*>{note});
        project->waitSaveToFinish();
        CHECK(noteModel->rowCount() == 1);

        // --- Add a scrap and populate data ---
        auto* scrap = new cwScrap();
        note->addScrap(scrap);

        // qDebug() << "DPI" << note->imageResolution()->value();

        // Outline: insert 3 points and close; tweak one point
        scrap->insertPoint(0, QPointF(0.10, 0.20));
        scrap->insertPoint(1, QPointF(0.50, 0.20));
        scrap->insertPoint(2, QPointF(0.50, 0.60));
        CHECK(scrap->numberOfPoints() == 3);

        // Change middle point slightly and close polygon
        scrap->setPoint(1, QPointF(0.55, 0.20));
        scrap->close();
        CHECK(scrap->isClosed());

        // Stations: add two and then adjust #0 name/position via setStationData
        {
            cwNoteStation stationA;
            stationA.setName(QStringLiteral("A1"));
            stationA.setPositionOnNote(QPointF(0.20, 0.25));
            scrap->addStation(stationA);

            cwNoteStation stationB;
            stationB.setName(QStringLiteral("B1"));
            stationB.setPositionOnNote(QPointF(0.80, 0.75));
            scrap->addStation(stationB);

            REQUIRE(scrap->numberOfStations() == 2);
            // Edit station 0 to test stationNameChanged/stationPositionChanged
            scrap->setStationData(cwScrap::StationName, 0, QVariant(QStringLiteral("A1-renamed")));
            scrap->setStationData(cwScrap::StationPosition, 0, QVariant(QPointF(0.22, 0.28)));
        }

        // Leads: add one and then tweak it using setLeadData
        {
            cwLead lead;
            lead.setDescription(QStringLiteral("Tight crawl"));
            lead.setPositionOnNote(QPointF(0.60, 0.30));
            lead.setSize(QSizeF(0.50, 0.30));
            lead.setCompleted(false);
            scrap->addLead(lead);

            REQUIRE(scrap->numberOfLeads() == 1);
            // Change description and completion flag via role-based API
            scrap->setLeadData(cwScrap::LeadDesciption, 0, QVariant(QStringLiteral("Tight crawl amended")));
            scrap->setLeadData(cwScrap::LeadCompleted, 0, QVariant(true));
        }

        // Flags / type: switch type and calc-transform flag
        scrap->setType(cwScrap::RunningProfile);
        scrap->setCalculateNoteTransform(true);

        project->waitSaveToFinish();

        // --- Reload and verify everything persisted ---
        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
        cwTrip* const loadTrip = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadTrip != nullptr);

        cwSurveyNoteModel* const loadModel = loadTrip->notes();
        REQUIRE(loadModel != nullptr);
        CHECK(loadModel->rowCount() == 1);

        // Access note via the convenience list
        const QList<cwNote*>& loadedNotes = loadModel->notes();
        REQUIRE(loadedNotes.size() == 1);
        cwNote* const loadedNote = loadedNotes.at(0);
        REQUIRE(loadedNote != nullptr);

        // Note-level checks
        CHECK(loadedNote->name().toStdString() == std::string("Entrance Sketch"));
        CHECK(loadedNote->rotate() == 12.5);
        // If cwImage exposes path(), verify it:
        CHECK(loadedNote->image().path().toStdString() == std::string("entrance.png"));
        CHECK(loadedNote->imageResolution()->unit() == cwUnits::DotsPerCentimeter);
        CHECK(loadedNote->imageResolution()->value() == Catch::Approx(12.34).epsilon(1e-6));

        // Scrap-level checks
        REQUIRE(loadedNote->hasScraps());
        REQUIRE(loadedNote->scraps().size() == 1);
        cwScrap* const loadedScrap = loadedNote->scrap(0);
        REQUIRE(loadedScrap != nullptr);

        // Outline
        CHECK(loadedScrap->numberOfPoints() == 4);
        CHECK(loadedScrap->points().at(0) == QPointF(0.10, 0.20));
        CHECK(loadedScrap->points().at(1) == QPointF(0.55, 0.20));
        CHECK(loadedScrap->points().at(2) == QPointF(0.50, 0.60));
        CHECK(loadedScrap->points().at(3) == QPointF(0.10, 0.20));
        CHECK(loadedScrap->isClosed());

        // Stations
        REQUIRE(loadedScrap->numberOfStations() == 2);
        CHECK(loadedScrap->stationData(cwScrap::StationName, 0).toString().toStdString() == std::string("A1-renamed"));
        CHECK(loadedScrap->stationData(cwScrap::StationPosition, 0).toPointF() == QPointF(0.22, 0.28));
        CHECK(loadedScrap->stationData(cwScrap::StationName, 1).toString().toStdString() == std::string("B1"));
        CHECK(loadedScrap->stationData(cwScrap::StationPosition, 1).toPointF() == QPointF(0.80, 0.75));

        // Leads
        REQUIRE(loadedScrap->numberOfLeads() == 1);
        CHECK(loadedScrap->leadData(cwScrap::LeadDesciption, 0).toString().toStdString() == std::string("Tight crawl amended"));
        CHECK(loadedScrap->leadData(cwScrap::LeadPositionOnNote, 0).toPointF() == QPointF(0.60, 0.30));
        CHECK(loadedScrap->leadData(cwScrap::LeadSize, 0).toSizeF() == QSizeF(0.50, 0.30));
        CHECK(loadedScrap->leadData(cwScrap::LeadCompleted, 0).toBool() == true);

        // Flags / type
        CHECK(loadedScrap->type() == cwScrap::RunningProfile);
        CHECK(loadedScrap->calculateNoteTransform() == true);
    }

    SECTION("Edit and remove: last-write-wins + scrap removal persists") {
        // Build a minimal note + scrap
        auto* note = new cwNote();
        note->setName(QStringLiteral("First Name"));
        note->setRotate(1.0);
        cwImage img; img.setPath(QStringLiteral("img/a.png")); note->setImage(img);
        noteModel->addNotes(QList<cwNote*>{note});

        auto* scrap = new cwScrap();
        note->addScrap(scrap);
        scrap->insertPoint(0, QPointF(0.1, 0.1));
        scrap->insertPoint(1, QPointF(0.9, 0.1));
        scrap->insertPoint(2, QPointF(0.9, 0.9));
        scrap->close();

        // Initial station/lead
        {
            cwNoteStation noteStation;
            noteStation.setName(QStringLiteral("S0"));
            noteStation.setPositionOnNote(QPointF(0.2, 0.2));
            scrap->addStation(noteStation);

            cwLead lead;
            lead.setDescription(QStringLiteral("Lead 0"));
            lead.setPositionOnNote(QPointF(0.3, 0.3));
            lead.setSize(QSizeF(0.2, 0.1)); lead.setCompleted(false);
            scrap->addLead(lead);
        }
        project->waitSaveToFinish();

        // --- Edits (note + scrap) ---
        note->setName(QStringLiteral("Final Name"));
        note->setRotate(33.0);
        cwImage img2;
        img2.setPath(QStringLiteral("b.png"));
        note->setImage(img2);

        scrap->setPoint(1, QPointF(0.85, 0.15));       // pointChanged
        scrap->setStationData(cwScrap::StationName, 0, QVariant(QStringLiteral("S0-final")));
        scrap->setStationData(cwScrap::StationPosition, 0, QVariant(QPointF(0.25, 0.22)));
        scrap->setLeadData(cwScrap::LeadDesciption, 0, QVariant(QStringLiteral("Lead 0 final")));
        scrap->setLeadData(cwScrap::LeadCompleted, 0, QVariant(true));
        scrap->setType(cwScrap::ProjectedProfile);
        scrap->setCalculateNoteTransform(false);

        project->waitSaveToFinish();

        // --- Reload and verify edits (last write wins) ---
        auto reloaded = std::make_unique<cwProject>();
        addTokenManager(reloaded.get());
        reloaded->loadOrConvert(project->filename());
        reloaded->waitLoadToFinish();

        cwTrip* const loadedTrip1 = reloaded->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadedTrip1 != nullptr);
        cwSurveyNoteModel* const loadedNoteModel1 = loadedTrip1->notes();
        REQUIRE(loadedNoteModel1 != nullptr);
        REQUIRE(loadedNoteModel1->rowCount() == 1);

        cwNote* const loadedNote1 = loadedNoteModel1->notes().at(0);
        REQUIRE(loadedNote1 != nullptr);
        CHECK(loadedNote1->name().toStdString() == std::string("Final Name"));
        CHECK(loadedNote1->rotate() == 33.0);
        CHECK(loadedNote1->image().path().toStdString() == std::string("b.png"));

        REQUIRE(loadedNote1->hasScraps());
        cwScrap* const loadedScrap1 = loadedNote1->scrap(0);
        REQUIRE(loadedScrap1 != nullptr);

        CHECK(loadedScrap1->points().at(1) == QPointF(0.85, 0.15));
        CHECK(loadedScrap1->stationData(cwScrap::StationName, 0).toString().toStdString() == std::string("S0-final"));
        CHECK(loadedScrap1->stationData(cwScrap::StationPosition, 0).toPointF() == QPointF(0.25, 0.22));
        CHECK(loadedScrap1->leadData(cwScrap::LeadDesciption, 0).toString().toStdString() == std::string("Lead 0 final"));
        CHECK(loadedScrap1->leadData(cwScrap::LeadCompleted, 0).toBool() == true);
        CHECK(loadedScrap1->type() == cwScrap::ProjectedProfile);
        CHECK(loadedScrap1->calculateNoteTransform() == false);

        // --- Remove the scrap and verify persistence of removal ---
        note->removeScraps(0, 0);
        project->waitSaveToFinish();


        auto reloaded2 = std::make_unique<cwProject>();
        addTokenManager(reloaded2.get());
        reloaded2->loadOrConvert(project->filename());
        reloaded2->waitLoadToFinish();

        cwTrip* const loadedTrip2 = reloaded2->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadedTrip2 != nullptr);
        cwSurveyNoteModel* const lm2 = loadedTrip2->notes();
        REQUIRE(lm2 != nullptr);
        REQUIRE(lm2->rowCount() == 1);

        cwNote* const loadedNote2 = lm2->notes().at(0);
        REQUIRE(loadedNote2 != nullptr);
        CHECK(loadedNote2->hasScraps() == false);
        CHECK(loadedNote2->scraps().size() == 0);
    }
}

// Small helper
static QByteArray fileSha256(const QString& absolutePath) {
    QFile file(absolutePath);
    if(!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!file.atEnd()) {
        hash.addData(file.read(256 * 1024));
    }
    return hash.result();
}

TEST_CASE("LiDAR GLB persistence: file copy + stations", "[cwProject]") {
    // ---- Project → cave → trip ----
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->waitSaveToFinish();

    cwCave* cave = new cwCave();
    cave->setName("lidar-persist-cave");

    cwTrip* trip = new cwTrip();
    trip->setName("lidar-persist-trip");
    cave->addTrip(trip);

    project->cavingRegion()->addCave(cave);
    project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(trip)));

    // ---- LiDAR model ----
    cwSurveyNoteLiDARModel* const lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);
    CHECK(lidarModel->rowCount() == 0);

    // ---- Prepare a GLB file in temp (you can duplicate/rename if needed) ----
    const QString originalGlbPath = copyToTempFolder("://datasets/test_cwSurveyNotesConcatModel/bones.glb");
    REQUIRE(QFileInfo::exists(originalGlbPath));
    const QByteArray originalHash = fileSha256(originalGlbPath);
    REQUIRE(!originalHash.isEmpty());

    // ---- Add via addFromFiles (required) ----
    {
        const QList<QUrl> files{ QUrl::fromLocalFile(originalGlbPath) };
        lidarModel->addFromFiles(files);
    }
    project->waitSaveToFinish();
    REQUIRE(lidarModel->rowCount() == 1);

    // Access the LiDAR note (assuming convenience accessor; adapt if your API differs)
    cwNoteLiDAR* const lidarNote = dynamic_cast<cwNoteLiDAR*>(lidarModel->notes().at(0));
    REQUIRE(lidarNote != nullptr);

    // ---- Add three stations ----
    {
        cwNoteLiDARStation station0;
        station0.setName(QStringLiteral("S0"));
        station0.setPositionOnNote(QVector3D(0.10, 0.20, 0.30));
        lidarNote->addStation(station0);

        cwNoteLiDARStation station1;
        station1.setName(QStringLiteral("S1"));
        station1.setPositionOnNote(QVector3D(0.50, 0.40, 0.6));
        lidarNote->addStation(station1);

        cwNoteLiDARStation station2;
        station2.setName(QStringLiteral("S2"));
        station2.setPositionOnNote(QVector3D(0.85, 0.75, 0.95));
        lidarNote->addStation(station2);
    }
    project->waitSaveToFinish();

    // ---- Reload and verify ----
    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(project->filename());
    reloaded->waitLoadToFinish();

    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    cwTrip* const loadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
    REQUIRE(loadedTrip != nullptr);

    cwSurveyNoteLiDARModel* const loadedModel = loadedTrip->notesLiDAR();
    REQUIRE(loadedModel != nullptr);
    REQUIRE(loadedModel->rowCount() == 1);

    cwNoteLiDAR* const loadedNote = dynamic_cast<cwNoteLiDAR*>(loadedModel->notes().at(0));
    REQUIRE(loadedNote != nullptr);

    CHECK(lidarNote != loadedNote);

    // Stations persisted?
    {
        const QList<cwNoteLiDARStation> stations = loadedNote->stations();
        REQUIRE(stations.size() == 3);

        CHECK(stations.at(0).name().toStdString() == std::string("S0"));
        CHECK(stations.at(0).positionOnNote() == QVector3D(0.10, 0.20, 0.30));

        CHECK(stations.at(1).name().toStdString() == std::string("S1"));
        CHECK(stations.at(1).positionOnNote() == QVector3D(0.50, 0.40, 0.6));

        CHECK(stations.at(2).name().toStdString() == std::string("S2"));
        CHECK(stations.at(2).positionOnNote() == QVector3D(0.85, 0.75, 0.95));
    }

    // ---- File copy checks: exists in the project and matches bytes ----
    // Assume cwNoteLiDAR::filename() returns a path relative to the trip directory.
    {
        const QString glb = ProjectFilenameTestHelper::absolutePath(loadedNote, loadedNote->filename());          // e.g. "lidar/bones.glb"
        REQUIRE(!glb.isEmpty());

        REQUIRE(QFileInfo::exists(glb));

        const QByteArray copiedHash = fileSha256(glb);
        REQUIRE(!copiedHash.isEmpty());

        // Same bytes as the original source GLB?
        CHECK(copiedHash == originalHash);
    }

    SECTION("Make sure changing station positions and name are update in reload") {
        auto firstStation = loadedNote->index(0);
        CHECK(loadedNote->setData(firstStation, "T0", cwNoteLiDAR::NameRole));
        CHECK(loadedNote->setData(firstStation, QVector3D(0.15, 0.25, 0.35), cwNoteLiDAR::PositionOnNoteRole));

        const QList<cwNoteLiDARStation> stations = loadedNote->stations();
        REQUIRE(stations.size() == 3);

        CHECK(stations.at(0).name().toStdString() == std::string("T0"));
        CHECK(stations.at(0).positionOnNote() == QVector3D(0.15, 0.25, 0.35));

        CHECK(stations.at(1).name().toStdString() == std::string("S1"));
        CHECK(stations.at(1).positionOnNote() == QVector3D(0.50, 0.40, 0.6));

        CHECK(stations.at(2).name().toStdString() == std::string("S2"));
        CHECK(stations.at(2).positionOnNote() == QVector3D(0.85, 0.75, 0.95));

        reloaded->waitSaveToFinish();

        auto reloaded2 = std::make_unique<cwProject>();
        addTokenManager(reloaded2.get());
        reloaded2->loadOrConvert(project->filename());
        reloaded2->waitLoadToFinish();

        REQUIRE(reloaded2->cavingRegion()->caveCount() == 1);
        cwTrip* const loadedTrip2 = reloaded2->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadedTrip != nullptr);

        cwSurveyNoteLiDARModel* const loadedModel2 = loadedTrip2->notesLiDAR();
        REQUIRE(loadedModel2 != nullptr);
        REQUIRE(loadedModel2->rowCount() == 1);

        cwNoteLiDAR* const loadedNote2 = dynamic_cast<cwNoteLiDAR*>(loadedModel2->notes().at(0));
        REQUIRE(loadedNote2 != nullptr);

        const QList<cwNoteLiDARStation> stations2 = loadedNote2->stations();
        REQUIRE(stations2.size() == 3);

        CHECK(stations2.at(0).name().toStdString() == std::string("T0"));
        CHECK(stations2.at(0).positionOnNote() == QVector3D(0.15, 0.25, 0.35));

        CHECK(stations2.at(1).name().toStdString() == std::string("S1"));
        CHECK(stations2.at(1).positionOnNote() == QVector3D(0.50, 0.40, 0.6));

        CHECK(stations2.at(2).name().toStdString() == std::string("S2"));
        CHECK(stations2.at(2).positionOnNote() == QVector3D(0.85, 0.75, 0.95));
    }

    SECTION("Make sure adding station update in reload") {
        cwNoteLiDARStation stationNew3;
        stationNew3.setName(QStringLiteral("N0"));
        stationNew3.setPositionOnNote(QVector3D(0.11, 0.22, 0.33));
        loadedNote->addStation(stationNew3);

        reloaded->waitSaveToFinish();

        auto reloaded2 = std::make_unique<cwProject>();
        addTokenManager(reloaded2.get());
        reloaded2->loadOrConvert(project->filename());
        reloaded2->waitLoadToFinish();

        REQUIRE(reloaded2->cavingRegion()->caveCount() == 1);
        cwTrip* const loadedTrip2 = reloaded2->cavingRegion()->cave(0)->trip(0);
        REQUIRE(loadedTrip != nullptr);

        cwSurveyNoteLiDARModel* const loadedModel2 = loadedTrip2->notesLiDAR();
        REQUIRE(loadedModel2 != nullptr);
        REQUIRE(loadedModel2->rowCount() == 1);

        cwNoteLiDAR* const loadedNote2 = dynamic_cast<cwNoteLiDAR*>(loadedModel2->notes().at(0));
        REQUIRE(loadedNote2 != nullptr);

        const QList<cwNoteLiDARStation> stations2 = loadedNote2->stations();
        REQUIRE(stations2.size() == 4);

        CHECK(stations2.at(0).name().toStdString() == std::string("S0"));
        CHECK(stations2.at(0).positionOnNote() == QVector3D(0.10, 0.20, 0.30));

        CHECK(stations2.at(1).name().toStdString() == std::string("S1"));
        CHECK(stations2.at(1).positionOnNote() == QVector3D(0.50, 0.40, 0.6));

        CHECK(stations2.at(2).name().toStdString() == std::string("S2"));
        CHECK(stations2.at(2).positionOnNote() == QVector3D(0.85, 0.75, 0.95));

        CHECK(stations2.at(3).name().toStdString() == std::string("N0"));
        CHECK(stations2.at(3).positionOnNote() == QVector3D(0.11, 0.22, 0.33));
    }

    SECTION("LiDAR transform: persist northUp & scale (non-Custom mode)") {
        // Mutate transform on the original note, then reload and verify
        cwNoteLiDARTransformation* const xform = lidarNote->noteTransformation();
        REQUIRE(xform != nullptr);

        lidarNote->setAutoCalculateNorth(false);
        xform->setScale(2.5);
        xform->setNorthUp(33.0);

        project->waitSaveToFinish();

        auto reloadedT = std::make_unique<cwProject>();
        addTokenManager(reloadedT.get());
        reloadedT->loadOrConvert(project->filename());
        reloadedT->waitLoadToFinish();

        cwTrip* const tripT = reloadedT->cavingRegion()->cave(0)->trip(0);
        REQUIRE(tripT != nullptr);

        cwSurveyNoteLiDARModel* const modelT = tripT->notesLiDAR();
        REQUIRE(modelT != nullptr);
        REQUIRE(modelT->rowCount() == 1);

        cwNoteLiDAR* const noteT = dynamic_cast<cwNoteLiDAR*>(modelT->notes().at(0));
        REQUIRE(noteT != nullptr);

        cwNoteLiDARTransformation* const xformT = noteT->noteTransformation();
        REQUIRE(xformT != nullptr);

        CHECK(xformT->scale() == Approx(2.5));
        CHECK(xformT->northUp() == Approx(33.0));
        CHECK(xformT->upMode() == cwNoteLiDARTransformation::UpMode::YisUp);
        CHECK(xformT->upSign() == Approx(1.0f));
        CHECK(noteT->autoCalculateNorth() == false);
    }

    SECTION("LiDAR transform: persist Custom up quaternion") {
        cwNoteLiDARTransformation* const xform = lidarNote->noteTransformation();
        REQUIRE(xform != nullptr);

        lidarNote->setAutoCalculateNorth(false);

        // Choose a non-trivial quaternion and non-default sign
        const QQuaternion q = QQuaternion::fromAxisAndAngle(QVector3D(0.3f, 0.8f, 0.5f).normalized(), 47.5f).normalized();
        xform->setUpMode(cwNoteLiDARTransformation::UpMode::Custom);
        xform->setUpCustom(q);

        // Also change base props so we verify they round-trip together
        xform->setScale(0.75);
        xform->setNorthUp(-12.0);

        project->waitSaveToFinish();

        auto reloadedT = std::make_unique<cwProject>();
        addTokenManager(reloadedT.get());
        reloadedT->loadOrConvert(project->filename());
        reloadedT->waitLoadToFinish();

        cwTrip* const tripT = reloadedT->cavingRegion()->cave(0)->trip(0);
        REQUIRE(tripT != nullptr);

        cwSurveyNoteLiDARModel* const modelT = tripT->notesLiDAR();
        REQUIRE(modelT != nullptr);
        REQUIRE(modelT->rowCount() == 1);

        cwNoteLiDAR* const noteT = dynamic_cast<cwNoteLiDAR*>(modelT->notes().at(0));
        REQUIRE(noteT != nullptr);

        cwNoteLiDARTransformation* const xformT = noteT->noteTransformation();
        REQUIRE(xformT != nullptr);

        CHECK(xformT->upMode() == cwNoteLiDARTransformation::UpMode::Custom);
        CHECK(xformT->upSign() == Approx(1.0f));
        CHECK(xformT->scale() == Approx(0.75));
        CHECK(xformT->northUp() == Approx(-12.0));

        const QQuaternion qT = xformT->upCustom().normalized();
        CHECK(qT.x() == Approx(q.x()));
        CHECK(qT.y() == Approx(q.y()));
        CHECK(qT.z() == Approx(q.z()));
        CHECK(qT.scalar() == Approx(q.scalar()));
        CHECK(noteT->autoCalculateNorth() == false);
    }

    SECTION("LiDAR transform: persist upMode toggles across reloads") {
        // Flip through non-Custom modes and ensure the last one sticks.
        cwNoteLiDARTransformation* const xform = lidarNote->noteTransformation();
        REQUIRE(xform != nullptr);

        xform->setUpMode(cwNoteLiDARTransformation::UpMode::XisUp);
        xform->setUpSign(2.0f);
        project->waitSaveToFinish();

        // First reload
        {
            auto r1 = std::make_unique<cwProject>();
            addTokenManager(r1.get());
            r1->loadOrConvert(project->filename());
            r1->waitLoadToFinish();

            cwTrip* const t1 = r1->cavingRegion()->cave(0)->trip(0);
            REQUIRE(t1 != nullptr);
            cwNoteLiDARTransformation* const xf1 =
                dynamic_cast<cwNoteLiDAR*>(t1->notesLiDAR()->notes().at(0))->noteTransformation();
            REQUIRE(xf1 != nullptr);
            CHECK(xf1->upMode() == cwNoteLiDARTransformation::UpMode::XisUp);
            CHECK(xf1->upSign() == Approx(2.0f));

            // Change again and save for the second round-trip
            xf1->setUpMode(cwNoteLiDARTransformation::UpMode::ZisUp);
            xf1->setUpSign(-0.5f);
            CHECK(dynamic_cast<cwNoteLiDAR*>(t1->notesLiDAR()->notes().at(0))->autoCalculateNorth() == true);
            r1->waitSaveToFinish();
        }

        // Second reload
        {
            auto r2 = std::make_unique<cwProject>();
            addTokenManager(r2.get());
            r2->loadOrConvert(project->filename());
            r2->waitLoadToFinish();

            cwTrip* const t2 = r2->cavingRegion()->cave(0)->trip(0);
            REQUIRE(t2 != nullptr);
            cwNoteLiDARTransformation* const xf2 =
                dynamic_cast<cwNoteLiDAR*>(t2->notesLiDAR()->notes().at(0))->noteTransformation();
            REQUIRE(xf2 != nullptr);

            CHECK(xf2->upMode() == cwNoteLiDARTransformation::UpMode::ZisUp);
            CHECK(xf2->upSign() == Approx(-0.5f));
            CHECK(dynamic_cast<cwNoteLiDAR*>(t2->notesLiDAR()->notes().at(0))->autoCalculateNorth() == true);
        }
    }
}


TEST_CASE("cwProject should overwrite or touch loaded project", "[cwProject]") {
    struct ScanResult {
        QHash<QString, QDateTime> modificationTimes; // key = absolute path, value = last-modified (UTC)
        int filesVisited = 0;
        int directoriesVisited = 0;
    };

    auto scan = [](const QString& rootPath,
                   bool includeHidden = false,
                   bool followSymbolicLinks = false)
    {
        ScanResult result;

        QDir::Filters entryFilters = QDir::AllEntries | QDir::NoDotAndDotDot;
        if (includeHidden) {
            entryFilters |= QDir::Hidden;
        }

        QDirIterator::IteratorFlags iteratorFlags = QDirIterator::Subdirectories;
        if (followSymbolicLinks) {
            iteratorFlags |= QDirIterator::FollowSymlinks;
        }

        QDirIterator iterator(rootPath, entryFilters, iteratorFlags);
        while (iterator.hasNext()) {
            iterator.next();
            const QFileInfo fileInformation = iterator.fileInfo();
            const QString absolutePath = fileInformation.absoluteFilePath();

            // Normalize to UTC so values are comparable and stable across locales
            const QDateTime lastModifiedUtc = fileInformation.lastModified().toUTC();


            if (fileInformation.isDir()) {
                result.directoriesVisited += 1;
            } else if (fileInformation.isFile()) {
                if (fileInformation.fileName() == QStringLiteral(".gitattributes")) {
                    continue;
                }
                // Only store directory
                result.modificationTimes.insert(absolutePath, lastModifiedUtc);
                result.filesVisited += 1;
            }
        }

        return result;
    };

    QString convertedFilename = [](){
        auto root = std::make_unique<cwRootData>();
        auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

        root->project()->loadOrConvert(filename);
        root->project()->waitLoadToFinish();
        root->project()->waitSaveToFinish();

        REQUIRE(root->project()->projectType(root->project()->filename()) == cwProject::GitFileType);

        return root->project()->filename();
    }();

    auto initialLoad = scan(QFileInfo(convertedFilename).absolutePath());

    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->loadFile(convertedFilename);
    project->waitLoadToFinish();

    auto load = scan(QFileInfo(convertedFilename).absolutePath());

    CHECK(!initialLoad.modificationTimes.isEmpty());
    CHECK(!load.modificationTimes.isEmpty());

    CHECK(initialLoad.modificationTimes.size() == load.modificationTimes.size());

    auto checkLoadTimes = [](const ScanResult& accutal, const ScanResult& expected) {
        for(const auto& keyValue : accutal.modificationTimes.asKeyValueRange()) {
            INFO("Filename:" << keyValue.first);
            CHECK(expected.modificationTimes.contains(keyValue.first));
            CHECK(expected.modificationTimes.value(keyValue.first) == keyValue.second);
            CHECK(keyValue.second.isValid());
            CHECK(expected.modificationTimes.value(keyValue.first).isValid());
        }
    };

    checkLoadTimes(load, initialLoad);

    SECTION("Modify the date and make sure the file has been modified") {
        //Modify a value to make sure save still works
        REQUIRE(project->cavingRegion()->caveCount() > 0);
        auto cave = project->cavingRegion()->cave(0);

        REQUIRE(cave->tripCount() > 0);
        auto trip = cave->trip(0);

        const QDateTime originalDate = trip->date();
        QDate updatedDate = originalDate.isValid() ? originalDate.date().addDays(1) : QDate::currentDate().addDays(1);
        if (!updatedDate.isValid()) {
            updatedDate = originalDate.date().addDays(-1);
        }
        REQUIRE(updatedDate.isValid());
        trip->setDate(QDateTime(updatedDate, QTime()));
        REQUIRE(trip->date().date() == updatedDate);

        project->waitSaveToFinish();

        auto tripPath = ProjectFilenameTestHelper::absolutePath(trip);
        auto modifiedLoad = scan(QFileInfo(convertedFilename).absolutePath());

        //Trip's data change should show up as different
        CHECK(initialLoad.modificationTimes.contains(tripPath));
        CHECK(modifiedLoad.modificationTimes[tripPath] != initialLoad.modificationTimes[tripPath]);

        CHECK(modifiedLoad.modificationTimes.remove(tripPath));
        CHECK(initialLoad.modificationTimes.remove(tripPath));

        //Make sure non of the other files have changed
        checkLoadTimes(modifiedLoad, initialLoad);
    }
}

TEST_CASE("Caves should be removed correctly simple", "[cwProject]") {
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());

    project->loadOrConvert(filename);
    project->waitLoadToFinish();
    project->waitSaveToFinish();

    REQUIRE(project->cavingRegion()->caveCount() > 0);
    auto cave = project->cavingRegion()->cave(0);

    auto caveFileName = ProjectFilenameTestHelper::absolutePath(cave);
    auto caveDir = QFileInfo(caveFileName).absoluteDir();


    CHECK(QFileInfo::exists(caveFileName));
    CHECK(QFileInfo::exists(caveDir.absolutePath()));

    // qDebug() << "Starting the remove!";

    //Remove the cave
    project->cavingRegion()->removeCave(0);

    project->waitSaveToFinish();

    CHECK(!QFileInfo::exists(caveFileName));
    CHECK(!QFileInfo::exists(caveDir.absolutePath()));
}

TEST_CASE("Trips should be removed correctly simple", "[cwProject]") {
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());

    project->loadOrConvert(filename);
    project->waitLoadToFinish();
    project->waitSaveToFinish();

    REQUIRE(project->cavingRegion()->caveCount() > 0);
    auto cave = project->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() > 0);
    auto trip = cave->trip(0);

    auto tripFileName = ProjectFilenameTestHelper::absolutePath(trip);
    auto tripDir = QFileInfo(tripFileName).absoluteDir();


    CHECK(QFileInfo::exists(tripFileName));
    CHECK(QFileInfo::exists(tripDir.absolutePath()));

    //Remove the trip
    cave->removeTrip(0);

    project->waitSaveToFinish();

    CHECK(!QFileInfo::exists(tripFileName));
    CHECK(!QFileInfo::exists(tripDir.absolutePath()));
}

TEST_CASE("Note should be removed correctly simple", "[cwProject]") {
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());

    project->loadOrConvert(filename);
    project->waitLoadToFinish();
    project->waitSaveToFinish();

    REQUIRE(project->cavingRegion()->caveCount() > 0);
    auto cave = project->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() > 0);
    auto trip = cave->trip(0);

    REQUIRE(trip->notes()->rowCount() > 0);
    auto note = trip->notes()->notes().at(0);

    auto noteFileName = ProjectFilenameTestHelper::absolutePath(note);
    auto noteDir = QFileInfo(noteFileName).absoluteDir();

    CHECK(QFileInfo::exists(noteFileName));
    CHECK(QFileInfo::exists(noteDir.absolutePath()));

    //Remove the note
    trip->notes()->removeNote(0);

    project->waitSaveToFinish();

    CHECK(!QFileInfo::exists(noteFileName));

    //Notes directory should still exist because we store multiple notes in it
    CHECK(QFileInfo::exists(noteDir.absolutePath()));
}
