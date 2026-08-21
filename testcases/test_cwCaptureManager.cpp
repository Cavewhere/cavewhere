//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include "cwSignalSpy.h"

//Our includes
#include "cwCaptureManager.h"
#include "cwCaptureViewport.h"
#include "SpyChecker.h"

TEST_CASE("cwCaptureManager should update filename with the correct extention", "[cwCaptureManager]") {

    cwCaptureManager manager;
    CHECK(manager.filename().toString().toStdString() == "");
    CHECK(manager.fileType() == cwCaptureManager::PNG);

    cwSignalSpy filenameSpy(&manager, &cwCaptureManager::filenameChanged);
    cwSignalSpy fileTypeSpy(&manager, &cwCaptureManager::fileTypeChanged);

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

TEST_CASE("cwCaptureViewport capture with an empty viewport aborts as canceled",
          "[cwCaptureViewport][cwCaptureManager]") {

    //A 0x0 rect passes QSize::isValid() (which only requires width/height
    //>= 0), so the capture() guard must use QRect::isEmpty(). Without it an
    //empty viewport marched past the guard into an empty tile-job list and
    //crashed — leaving a driving manager's m_capturing latched true and every
    //later capture() silently dropped.
    cwCaptureViewport viewport;
    cwSignalSpy canceledSpy(&viewport, &cwCaptureViewport::captureCanceled);
    cwSignalSpy finishedSpy(&viewport, &cwCaptureViewport::finishedCapture);

    SECTION("default-constructed (null) viewport") {
        //Nothing to set — the default QRect is 0x0
    }

    SECTION("explicit zero-area viewport") {
        viewport.setViewport(QRect(10, 10, 0, 0));
    }

    viewport.capture();
    CHECK(canceledSpy.size() == 1);
    CHECK(finishedSpy.size() == 0);

    //The abort left the viewport startable, not wedged: a second capture()
    //reaches the guard again instead of the CapturingImages early-return.
    viewport.capture();
    CHECK(canceledSpy.size() == 2);
    CHECK(finishedSpy.size() == 0);
}

TEST_CASE("cwCaptureManager cancel while idle should do nothing", "[cwCaptureManager]") {

    cwCaptureManager manager;
    cwSignalSpy canceledSpy(&manager, &cwCaptureManager::canceledCapture);

    //No capture run is in flight, so cancel() has nothing to abort and must
    //not announce a canceled capture to QML listeners.
    manager.cancel();
    CHECK(canceledSpy.count() == 0);

    //Repeated idle cancels stay silent too.
    manager.cancel();
    CHECK(canceledSpy.count() == 0);
}
