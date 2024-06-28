//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwCaptureManager.h"
#include "SpyChecker.h"

TEST_CASE("cwCaptureManager should update filename with the correct extention", "[cwCaptureManager]") {

    cwCaptureManager manager;
    CHECK(manager.filename().toString().toStdString() == "");
    CHECK(manager.fileType() == cwCaptureManager::PNG);

    QSignalSpy filenameSpy(&manager, &cwCaptureManager::filenameChanged);
    QSignalSpy fileTypeSpy(&manager, &cwCaptureManager::fileTypeChanged);

    SpyChecker checker = {
        {&filenameSpy, 0},
        {&fileTypeSpy, 0}
    };

    manager.setFilename(QUrl::fromLocalFile("test"));
    CHECK(manager.filename().toLocalFile().toStdString() == "test.png");
    checker[&filenameSpy]++;
    checker.checkSpies();

    manager.setFileType(cwCaptureManager::PDF);
    CHECK(manager.filename().toLocalFile().toStdString() == "test.pdf");
    checker[&fileTypeSpy]++;
    checker[&filenameSpy]++;
    checker.checkSpies();

    manager.setFilename(QUrl::fromLocalFile("test.sauce"));
    CHECK(manager.filename().toLocalFile().toStdString() == "test.sauce.pdf");
    checker[&filenameSpy]++;
    checker.checkSpies();

    manager.setFilename(QUrl::fromLocalFile("test.jpg"));
    CHECK(manager.filename().toLocalFile().toStdString() == "test.pdf");
    checker[&filenameSpy]++;
    checker.checkSpies();

    manager.setFileType(cwCaptureManager::JPG);
    CHECK(manager.filename().toLocalFile().toStdString() == "test.jpg");
    checker[&fileTypeSpy]++;
    checker[&filenameSpy]++;
    checker.checkSpies();

    QString fullPath = QDir::tempPath() + "/sauceExport";
    manager.setFilename(QUrl::fromLocalFile(fullPath));
    CHECK(manager.filename().toLocalFile().toStdString() == QString(fullPath + ".jpg").toStdString());
    checker[&filenameSpy]++;
    checker.checkSpies();

    fullPath = QDir::tempPath() + "/sauceExport.jpg";
    manager.setFilename(QUrl::fromLocalFile(fullPath));
    CHECK(manager.filename().toLocalFile().toStdString() == QString(fullPath).toStdString());
    checker.checkSpies();

    fullPath = QDir::tempPath() + "/sauceExport.";
    manager.setFilename(QUrl::fromLocalFile(fullPath));
    CHECK(manager.filename().toLocalFile().toStdString() == QString(fullPath + "jpg").toStdString());
    checker.checkSpies();
}
