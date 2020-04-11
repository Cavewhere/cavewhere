//Catch includes
#include "catch.hpp"

//Our includes
#include "cwJobSettings.h"
#include "SpyChecker.h"

//Qt includes
#include <QThread>
#include <QThreadPool>
#include <QSettings>
#include <QSignalSpy>

TEST_CASE("cwJobSettings should initilize correctly and update settings correcly", "[cwJobSettings]") {
    cwJobSettings::initialize();
    auto settings = cwJobSettings::instance();
    cwJobSettings::initialize();
    auto settings2 = cwJobSettings::instance();
    CHECK(settings == settings2);
    REQUIRE(settings);

    QSignalSpy threadCountSpy(settings, &cwJobSettings::threadCountChanged);
    QSignalSpy autoUpdateSpy(settings, &cwJobSettings::automaticUpdateChanged);

    threadCountSpy.setObjectName("threadCountSpy");
    autoUpdateSpy.setObjectName("autoUpdateSpy");

    SpyChecker checker = {
        {&threadCountSpy, 0},
        {&autoUpdateSpy, 0}
    };

    QSettings diskSettings;

    SECTION("Check thread count") {
        CHECK(settings->idleThreadCount() == QThread::idealThreadCount());
        CHECK(settings->threadCount() == QThreadPool::globalInstance()->maxThreadCount());

        settings->setThreadCount(1);
        CHECK(settings->threadCount() == QThreadPool::globalInstance()->maxThreadCount());

        checker[&threadCountSpy]++;
        checker.checkSpies();

        CHECK(diskSettings.value("threadCount").toInt() == 1);

        settings->setThreadCount(QThread::idealThreadCount());
    }

    SECTION("Check auto update") {
        CHECK(settings->automaticUpdate() == true);

        settings->setAutomaticUpdate(false);
        checker[&autoUpdateSpy]++;
        checker.checkSpies();

        CHECK(settings->automaticUpdate() == false);
        CHECK(diskSettings.value("automaticUpdate").toBool() == false);

        settings->setAutomaticUpdate(true);
        checker[&autoUpdateSpy]++;
        checker.checkSpies();

        CHECK(settings->automaticUpdate() == true);
        CHECK(diskSettings.value("automaticUpdate").toBool() == true);
    }

    diskSettings.clear();

    CHECK(settings->threadCount() == QThread::idealThreadCount());
    CHECK(QThreadPool::globalInstance()->maxThreadCount() == QThread::idealThreadCount());
}
