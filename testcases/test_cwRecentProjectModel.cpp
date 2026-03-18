//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <memory>
#include <QSettings>
#include <QTemporaryDir>
#include <QModelIndex>
#include <QUrl>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>

//Our includes
#include "cwRecentProjectModel.h"
#include "cwProject.h"
#include "cwErrorListModel.h"
#include "cwFutureManagerModel.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "TestHelper.h"

namespace {
QString readGitExclude(const QDir& dir)
{
    QFile file(dir.filePath(QStringLiteral(".git/info/exclude")));
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}
} // namespace

TEST_CASE("cwRecentProjectModel initial state", "[cwRecentProjectModel]") {
    // Ensure settings are clean
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;
    REQUIRE(model.rowCount() == 0);

    auto roles = model.roleNames();
    REQUIRE(roles.contains(cwRecentProjectModel::PathRole));
    REQUIRE(roles.contains(cwRecentProjectModel::NameRole));
    REQUIRE(roles.value(cwRecentProjectModel::PathRole) == "pathRole");
    REQUIRE(roles.value(cwRecentProjectModel::NameRole) == "nameRole");
}

TEST_CASE("cwRecentProjectModel addRepository and persistence", "[cwRecentProjectModel]") {
    // Clear settings before test
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;
    REQUIRE(model.rowCount() == 0);

    // Create a temporary directory as a dummy repository
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());
    CHECK(repoDir.exists());

    auto result = model.addRepository(repoDir);
    INFO("Error:" << result.errorMessage().toStdString());
    CHECK(result.hasError() == false);
    REQUIRE(model.rowCount() == 1);

    // Verify data() returns correct path and name
    QModelIndex idx = model.index(0, 0);
    REQUIRE(model.data(idx, cwRecentProjectModel::PathRole).toString() == repoDir.absolutePath());
    REQUIRE(model.data(idx, cwRecentProjectModel::NameRole).toString() == repoDir.dirName());

    // Test adding a non-existent directory
    QDir nonExistentDir(tmpDir.path() + "/does_not_exist");
    REQUIRE_FALSE(nonExistentDir.exists());
    auto result2 = model.addRepository(nonExistentDir);
    INFO("Error on non-existent add: " << result2.errorMessage().toStdString());
    CHECK(result2.hasError() == false);
    // row count should remain unchanged
    REQUIRE(model.rowCount() == 2);

    QModelIndex idx1 = model.index(1, 0);
    CHECK(model.data(idx1, cwRecentProjectModel::PathRole).toString().toStdString() == nonExistentDir.absolutePath().toStdString());
    CHECK(model.data(idx1, cwRecentProjectModel::NameRole).toString().toStdString() == nonExistentDir.dirName().toStdString());

    // Create a fresh model to verify persistence from QSettings
    cwRecentProjectModel model2;
    REQUIRE(model2.rowCount() == 2);
    // QModelIndex idx2 = model2.index(0, 0);
    CHECK(model2.data(model2.index(0, 0), cwRecentProjectModel::PathRole).toString() == repoDir.absolutePath());
    CHECK(model2.data(model2.index(1, 0), cwRecentProjectModel::PathRole).toString() == nonExistentDir.absolutePath());
}

TEST_CASE("cwRecentProjectModel addRepository writes local sync excludes", "[cwRecentProjectModel]") {
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());

    auto result = model.addRepository(repoDir);
    INFO("Error:" << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    const QString contents = readGitExclude(repoDir);
    CHECK(contents.contains(".cw_cache/"));
    CHECK(contents.contains(".DS_Store"));
}

TEST_CASE("cwRecentProjectModel addRepository with url and persistence", "[cwRecentProjectModel]") {
    // Clear settings before test
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;
    REQUIRE(model.rowCount() == 0);

    // Create a temporary directory as a dummy repository
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir parentDir(tmpDir.path());
    CHECK(parentDir.exists());


    // Test adding a non-existent directory
    QDir nonExistentDir(tmpDir.path() + "/sauce");
    REQUIRE_FALSE(nonExistentDir.exists());
    auto result2 = model.addRepository(cwSaveLoad::repositoryDir(QUrl::fromLocalFile(parentDir.absolutePath()), "sauce"));
    INFO("Error on non-existent add: " << result2.errorMessage().toStdString());
    CHECK(result2.hasError() == false);
    // row count should remain unchanged
    REQUIRE(model.rowCount() == 1);

    QModelIndex idx1 = model.index(0, 0);
    CHECK(model.data(idx1, cwRecentProjectModel::PathRole).toString().toStdString() == nonExistentDir.absolutePath().toStdString());
    CHECK(model.data(idx1, cwRecentProjectModel::NameRole).toString().toStdString() == nonExistentDir.dirName().toStdString());

    // Create a fresh model to verify persistence from QSettings
    cwRecentProjectModel model2;
    REQUIRE(model2.rowCount() == 1);
    // QModelIndex idx2 = model2.index(0, 0);
    CHECK(model2.data(model2.index(0, 0), cwRecentProjectModel::PathRole).toString().toStdString() == nonExistentDir.absolutePath().toStdString());

}

TEST_CASE("cwRecentProjectModel make sure defaultDepositoryDir works", "[cwRecentProjectModel]") {

    // Clear settings before test
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;
    auto defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    CHECK(model.defaultRepositoryDir().toLocalFile().toStdString() == defaultPath.toStdString());

    //Set the default path
    const auto saucePath = defaultPath + "/test";
    model.setDefaultRepositoryDir(QUrl::fromLocalFile(saucePath));
    CHECK(model.defaultRepositoryDir().toLocalFile().toStdString() == saucePath.toStdString());

    //Make sure it's updated in a new cwRecentProjectModel, this is similar to restarting the app
    cwRecentProjectModel model2;
    CHECK(model2.defaultRepositoryDir().toLocalFile().toStdString() == saucePath.toStdString());

}

TEST_CASE("cwRecentProjectModel repositoryProjectFile locates project file", "[cwRecentProjectModel]") {
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());

    QFile projectFile(tmpDir.filePath("Sample.cwproj"));
    REQUIRE(projectFile.open(QIODevice::WriteOnly));
    REQUIRE(projectFile.write("{}") != -1);
    projectFile.close();

    auto addResult = model.addRepository(QDir(tmpDir.path()));
    INFO(addResult.errorMessage().toStdString());
    REQUIRE(addResult.hasError() == false);

    const auto fileResult = model.repositoryProjectFile(0);
    INFO(fileResult.errorMessage().toStdString());
    REQUIRE(fileResult.hasError() == false);
    CHECK(fileResult.value() == QFileInfo(projectFile).absoluteFilePath());
}

TEST_CASE("cwRecentProjectModel repositoryProjectFile reports missing or duplicate projects", "[cwRecentProjectModel]") {
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    const QDir repoDir(tmpDir.path());

    REQUIRE_FALSE(model.addRepository(repoDir).hasError());

    SECTION("missing project file") {
        const auto result = model.repositoryProjectFile(0);
        CHECK(result.hasError());
    }

    SECTION("duplicate project files") {
        QFile first(repoDir.filePath("one.cwproj"));
        QFile second(repoDir.filePath("two.cwproj"));
        REQUIRE(first.open(QIODevice::WriteOnly));
        REQUIRE(second.open(QIODevice::WriteOnly));
        first.close();
        second.close();

        const auto result = model.repositoryProjectFile(0);
        CHECK(result.hasError());
    }
}

TEST_CASE("cwRecentProjectModel addRepositoryFromProjectFile adds repositories once", "[cwRecentProjectModel]") {
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());

    QFile projectFile(tmpDir.filePath("Repo.cwproj"));
    REQUIRE(projectFile.open(QIODevice::WriteOnly));
    projectFile.close();
    cwSaveLoad::initializeGitRepository(QDir(tmpDir.path()));

    const QUrl projectUrl = QUrl::fromLocalFile(projectFile.fileName());

    auto addResult = model.addRepositoryFromProjectFile(projectUrl);
    INFO(addResult.errorMessage().toStdString());
    CHECK(addResult.hasError() == false);
    REQUIRE(model.rowCount() == 1);

    auto addAgainResult = model.addRepositoryFromProjectFile(projectUrl);
    INFO(addAgainResult.errorMessage().toStdString());
    CHECK(addAgainResult.hasError() == false);
    CHECK(model.rowCount() == 1);

    const auto remoteResult = model.addRepositoryFromProjectFile(QUrl(QStringLiteral("https://example.com/remote.cwproj")));
    CHECK(remoteResult.hasError());
}

TEST_CASE("cwRecentProjectModel addRepositoryFromProjectFile stores bundled project file path",
          "[cwRecentProjectModel][bundled]") {
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;

    const QString bundledSource = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    REQUIRE(QFileInfo::exists(bundledSource));

    const QUrl bundledUrl = QUrl::fromLocalFile(bundledSource);
    auto addResult = model.addRepositoryFromProjectFile(bundledUrl);
    INFO(addResult.errorMessage().toStdString());
    REQUIRE_FALSE(addResult.hasError());
    REQUIRE(model.rowCount() == 1);

    CHECK(model.data(model.index(0, 0), cwRecentProjectModel::PathRole).toString() == bundledSource);
    CHECK(model.repositoryProjectFile(0).value() == bundledSource);

    auto addAgainResult = model.addRepositoryFromProjectFile(bundledUrl);
    INFO(addAgainResult.errorMessage().toStdString());
    CHECK_FALSE(addAgainResult.hasError());
    CHECK(model.rowCount() == 1);
}

TEST_CASE("cwRecentProjectModel addRepositoryFromProjectFile stores saved project file path", "[cwRecentProjectModel]") {
    QSettings settings;
    settings.clear();

    cwRecentProjectModel model;
    auto project = std::make_unique<cwProject>();
    cwFutureManagerModel futureManagerModel;
    project->setFutureManagerToken(futureManagerModel.token());

    REQUIRE(model.rowCount() == 0);

    QTemporaryDir saveRoot;
    REQUIRE(saveRoot.isValid());

    const QString destinationBase = saveRoot.filePath("SavedProject/SavedProject");
    bool success = project->saveAs(destinationBase);
    CHECK(success);
    project->waitSaveToFinish();

    CHECK(project->errorModel()->count() == 0);

    const auto addResult = model.addRepositoryFromProjectFile(QUrl::fromLocalFile(project->filename()));
    INFO(addResult.errorMessage().toStdString());
    REQUIRE_FALSE(addResult.hasError());
    REQUIRE(model.rowCount() == 1);
    const QString expectedPath = project->filename();
    CHECK(model.data(model.index(0, 0), cwRecentProjectModel::PathRole).toString() == expectedPath);
}

TEST_CASE("cwRecentProjectModel saveAs bundled project uses bundled path instead of extraction directory",
          "[cwRecentProjectModel][bundled][saveAs]") {
    QSettings settings;
    settings.clear();

    auto root = std::make_unique<cwRootData>();
    root->account()->setName(QStringLiteral("Test User"));
    root->account()->setEmail(QStringLiteral("test@example.com"));
    auto* model = root->recentProjectModel();
    auto* project = root->project();

    const QString sqliteSource = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    REQUIRE(QFileInfo::exists(sqliteSource));

    project->loadOrConvert(sqliteSource);
    project->waitLoadToFinish();
    REQUIRE(project->errorModel()->count() == 0);
    model->clear();
    REQUIRE(model->rowCount() == 0);

    QTemporaryDir saveRoot;
    REQUIRE(saveRoot.isValid());
    const QString bundledPath = saveRoot.filePath(QStringLiteral("SavedBundle.cw"));

    REQUIRE(project->saveAs(bundledPath));
    project->waitSaveToFinish();
    REQUIRE(project->errorModel()->count() == 0);

    const auto addResult = model->addRepositoryFromProjectFile(QUrl::fromLocalFile(bundledPath));
    INFO(addResult.errorMessage().toStdString());
    REQUIRE_FALSE(addResult.hasError());
    REQUIRE(model->rowCount() == 1);
    const QString actualPath = model->data(model->index(0, 0), cwRecentProjectModel::PathRole).toString();
    INFO("Actual repository path: " << actualPath.toStdString());
    INFO("Expected bundled path: " << bundledPath.toStdString());
    CHECK(actualPath == bundledPath);
}

TEST_CASE("cwRecentProjectModel opening sqlite project adds converted git directory to recents",
          "[cwRecentProjectModel][bundled][open]") {
    // SQLite .cw files now convert to a temporary git directory (GitFileType).
    // Loading one adds the converted git dir as a second recents entry alongside the
    // original .cw entry.
    QSettings settings;
    settings.clear();

    auto root = std::make_unique<cwRootData>();
    auto* model = root->recentProjectModel();
    auto* project = root->project();

    const QString bundledSource = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    REQUIRE(QFileInfo::exists(bundledSource));

    const auto addResult = model->addRepositoryFromProjectFile(QUrl::fromLocalFile(bundledSource));
    INFO(addResult.errorMessage().toStdString());
    REQUIRE_FALSE(addResult.hasError());
    REQUIRE(model->rowCount() == 1);
    CHECK(model->data(model->index(0, 0), cwRecentProjectModel::PathRole).toString() == bundledSource);

    const auto fileResult = model->repositoryProjectFile(0);
    INFO(fileResult.errorMessage().toStdString());
    REQUIRE_FALSE(fileResult.hasError());
    project->loadFile(QUrl::fromLocalFile(fileResult.value()).toString());
    project->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    REQUIRE(project->errorModel()->count() == 0);

    // The converted git directory is added as a new entry; original .cw entry remains.
    REQUIRE(model->rowCount() == 2);
    CHECK(model->data(model->index(0, 0), cwRecentProjectModel::PathRole).toString() == bundledSource);
    CHECK(model->data(model->index(1, 0), cwRecentProjectModel::PathRole).toString() == project->filename());
}
