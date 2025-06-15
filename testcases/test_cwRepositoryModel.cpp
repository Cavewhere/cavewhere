//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSettings>
#include <QTemporaryDir>
#include <QModelIndex>
#include <QUrl>

//Our includes
#include "cwRepositoryModel.h"

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
    auto result2 = model.addRepository(QUrl::fromLocalFile(parentDir.absolutePath()), "sauce");
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
