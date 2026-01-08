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
#include "cwRepositoryModel.h"
#include "cwProject.h"
#include "cwErrorListModel.h"

namespace {
QString readGitIgnore(const QDir& dir)
{
    QFile file(dir.filePath(QStringLiteral(".gitignore")));
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}
} // namespace

TEST_CASE("cwRepositoryModel initial state", "[cwRepositoryModel]") {
    // Ensure settings are clean
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;
    REQUIRE(model.rowCount() == 0);

    auto roles = model.roleNames();
    REQUIRE(roles.contains(cwRepositoryModel::PathRole));
    REQUIRE(roles.contains(cwRepositoryModel::NameRole));
    REQUIRE(roles.value(cwRepositoryModel::PathRole) == "pathRole");
    REQUIRE(roles.value(cwRepositoryModel::NameRole) == "nameRole");
}

TEST_CASE("cwRepositoryModel addRepository and persistence", "[cwRepositoryModel]") {
    // Clear settings before test
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;
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
    REQUIRE(model.data(idx, cwRepositoryModel::PathRole).toString() == repoDir.absolutePath());
    REQUIRE(model.data(idx, cwRepositoryModel::NameRole).toString() == repoDir.dirName());

    // Test adding a non-existent directory
    QDir nonExistentDir(tmpDir.path() + "/does_not_exist");
    REQUIRE_FALSE(nonExistentDir.exists());
    auto result2 = model.addRepository(nonExistentDir);
    INFO("Error on non-existent add: " << result2.errorMessage().toStdString());
    CHECK(result2.hasError() == false);
    // row count should remain unchanged
    REQUIRE(model.rowCount() == 2);

    QModelIndex idx1 = model.index(1, 0);
    CHECK(model.data(idx1, cwRepositoryModel::PathRole).toString().toStdString() == nonExistentDir.absolutePath().toStdString());
    CHECK(model.data(idx1, cwRepositoryModel::NameRole).toString().toStdString() == nonExistentDir.dirName().toStdString());

    // Create a fresh model to verify persistence from QSettings
    cwRepositoryModel model2;
    REQUIRE(model2.rowCount() == 2);
    // QModelIndex idx2 = model2.index(0, 0);
    CHECK(model2.data(model2.index(0, 0), cwRepositoryModel::PathRole).toString() == repoDir.absolutePath());
    CHECK(model2.data(model2.index(1, 0), cwRepositoryModel::PathRole).toString() == nonExistentDir.absolutePath());
}

TEST_CASE("cwRepositoryModel addRepository writes .gitignore cache entry", "[cwRepositoryModel]") {
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());

    auto result = model.addRepository(repoDir);
    INFO("Error:" << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    const QString contents = readGitIgnore(repoDir);
    CHECK(contents.contains(".cw_cache/"));
}

TEST_CASE("cwRepositoryModel addRepository with url and persistence", "[cwRepositoryModel]") {
    // Clear settings before test
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;
    REQUIRE(model.rowCount() == 0);

    // Create a temporary directory as a dummy repository
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir parentDir(tmpDir.path());
    CHECK(parentDir.exists());


    // Test adding a non-existent directory
    QDir nonExistentDir(tmpDir.path() + "/sauce");
    REQUIRE_FALSE(nonExistentDir.exists());
    auto result2 = model.addRepository(model.repositoryDir(QUrl::fromLocalFile(parentDir.absolutePath()), "sauce"));
    INFO("Error on non-existent add: " << result2.errorMessage().toStdString());
    CHECK(result2.hasError() == false);
    // row count should remain unchanged
    REQUIRE(model.rowCount() == 1);

    QModelIndex idx1 = model.index(0, 0);
    CHECK(model.data(idx1, cwRepositoryModel::PathRole).toString().toStdString() == nonExistentDir.absolutePath().toStdString());
    CHECK(model.data(idx1, cwRepositoryModel::NameRole).toString().toStdString() == nonExistentDir.dirName().toStdString());

    // Create a fresh model to verify persistence from QSettings
    cwRepositoryModel model2;
    REQUIRE(model2.rowCount() == 1);
    // QModelIndex idx2 = model2.index(0, 0);
    CHECK(model2.data(model2.index(0, 0), cwRepositoryModel::PathRole).toString().toStdString() == nonExistentDir.absolutePath().toStdString());

}

TEST_CASE("cwRepositoryModel make sure defaultDepositoryDir works", "[cwRepositoryModel]") {

    // Clear settings before test
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;
    auto defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    CHECK(model.defaultRepositoryDir().toLocalFile().toStdString() == defaultPath.toStdString());

    //Set the default path
    const auto saucePath = defaultPath + "/test";
    model.setDefaultRepositoryDir(QUrl::fromLocalFile(saucePath));
    CHECK(model.defaultRepositoryDir().toLocalFile().toStdString() == saucePath.toStdString());

    //Make sure it's updated in a new cwRepositoryModel, this is similar to restarting the app
    cwRepositoryModel model2;
    CHECK(model2.defaultRepositoryDir().toLocalFile().toStdString() == saucePath.toStdString());

}

TEST_CASE("cwRepositoryModel repositoryProjectFile locates project file", "[cwRepositoryModel]") {
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());

    QFile projectFile(tmpDir.filePath("Sample.cw"));
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

TEST_CASE("cwRepositoryModel repositoryProjectFile reports missing or duplicate projects", "[cwRepositoryModel]") {
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    const QDir repoDir(tmpDir.path());

    REQUIRE_FALSE(model.addRepository(repoDir).hasError());

    SECTION("missing project file") {
        const auto result = model.repositoryProjectFile(0);
        CHECK(result.hasError());
    }

    SECTION("duplicate project files") {
        QFile first(repoDir.filePath("one.cw"));
        QFile second(repoDir.filePath("two.cw"));
        REQUIRE(first.open(QIODevice::WriteOnly));
        REQUIRE(second.open(QIODevice::WriteOnly));
        first.close();
        second.close();

        const auto result = model.repositoryProjectFile(0);
        CHECK(result.hasError());
    }
}

TEST_CASE("cwRepositoryModel addRepositoryFromProjectFile adds repositories once", "[cwRepositoryModel]") {
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;

    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());

    QFile projectFile(tmpDir.filePath("Repo.cw"));
    REQUIRE(projectFile.open(QIODevice::WriteOnly));
    projectFile.close();

    const QUrl projectUrl = QUrl::fromLocalFile(projectFile.fileName());

    auto addResult = model.addRepositoryFromProjectFile(projectUrl);
    INFO(addResult.errorMessage().toStdString());
    CHECK(addResult.hasError() == false);
    REQUIRE(model.rowCount() == 1);

    auto addAgainResult = model.addRepositoryFromProjectFile(projectUrl);
    INFO(addAgainResult.errorMessage().toStdString());
    CHECK(addAgainResult.hasError() == false);
    CHECK(model.rowCount() == 1);

    const auto remoteResult = model.addRepositoryFromProjectFile(QUrl(QStringLiteral("https://example.com/remote.cw")));
    CHECK(remoteResult.hasError());
}

TEST_CASE("cwRepositoryModel adds saved current project", "[cwRepositoryModel]") {
    QSettings settings;
    settings.clear();

    cwRepositoryModel model;
    auto project = std::make_unique<cwProject>();
    model.setProject(project.get());

    REQUIRE(model.rowCount() == 0);

    QTemporaryDir saveRoot;
    REQUIRE(saveRoot.isValid());

    const QString destinationBase = saveRoot.filePath("SavedProject/SavedProject");
    bool success = project->saveAs(destinationBase);
    CHECK(success);
    project->waitSaveToFinish();

    CHECK(project->errorModel()->count() == 0);

    REQUIRE(model.rowCount() == 1);
    const QString expectedPath = QFileInfo(project->filename()).absoluteDir().absolutePath();
    CHECK(model.data(model.index(0, 0), cwRepositoryModel::PathRole).toString() == expectedPath);
}
