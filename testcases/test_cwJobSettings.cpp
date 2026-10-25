//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwJobSettings.h"
#include "cwTask.h"
#include "cwThreadCountRestorer.h"
#include "SpyChecker.h"

//Qt includes
#include <QThread>
#include <QThreadPool>
#include <QSettings>
#include "cwSignalSpy.h"

TEST_CASE("cwJobSettings should initilize correctly and update settings correcly", "[cwJobSettings]") {
    cwJobSettings::initialize();
    auto settings = cwJobSettings::instance();
    cwJobSettings::initialize();
    auto settings2 = cwJobSettings::instance();
    CHECK(settings == settings2);
    REQUIRE(settings);

    cwSignalSpy threadCountSpy(settings, &cwJobSettings::threadCountChanged);
    cwSignalSpy autoUpdateSpy(settings, &cwJobSettings::automaticUpdateChanged);

    threadCountSpy.setObjectName("threadCountSpy");
    autoUpdateSpy.setObjectName("autoUpdateSpy");

    SpyChecker checker = {
        {&threadCountSpy, 0},
        {&autoUpdateSpy, 0}
    };

    QSettings diskSettings;

    SECTION("Check thread count") {
        CHECK(settings->idleThreadCount() == QThread::idealThreadCount());
        CHECK(settings->threadCount() == cwTask::threadPool()->maxThreadCount());

        settings->setThreadCount(1);
        CHECK(settings->threadCount() == cwTask::threadPool()->maxThreadCount());

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

namespace {
    const QString kThreadCountKey = QStringLiteral("threadCount");

    //! Puts the thread count setting and its stored value back the way the test found them
    class ThreadCountSettingRestorer {
    public:
        explicit ThreadCountSettingRestorer(cwJobSettings* settings) :
            m_settings(settings),
            m_previousCount(settings->threadCount()),
            m_previousStored(QSettings().value(kThreadCountKey))
        {
        }

        ~ThreadCountSettingRestorer()
        {
            m_settings->setThreadCount(m_previousCount);
            QSettings diskSettings;
            if(m_previousStored.isValid()) {
                diskSettings.setValue(kThreadCountKey, m_previousStored);
            } else {
                diskSettings.remove(kThreadCountKey);
            }
        }

        ThreadCountSettingRestorer(const ThreadCountSettingRestorer&) = delete;
        ThreadCountSettingRestorer& operator=(const ThreadCountSettingRestorer&) = delete;

    private:
        cwJobSettings* m_settings;
        int m_previousCount;
        QVariant m_previousStored;
    };
}

TEST_CASE("cwJobSettings sizes only the task pool", "[cwJobSettings]") {
    cwJobSettings::initialize();
    auto settings = cwJobSettings::instance();
    REQUIRE(settings);

    if(settings->idleThreadCount() <= kSingleThread) {
        SKIP("Needs more than one ideal thread to pick a count below idleThreadCount");
    }

    ThreadCountSettingRestorer restorer(settings);
    const int globalCountBefore = QThreadPool::globalInstance()->maxThreadCount();

    settings->setThreadCount(kSingleThread);

    CHECK(cwTask::threadPool()->maxThreadCount() == kSingleThread);
    CHECK(settings->threadCount() == kSingleThread);
    CHECK(QThreadPool::globalInstance()->maxThreadCount() == globalCountBefore);
}

TEST_CASE("cwJobSettings reads the thread count from the task pool", "[cwJobSettings]") {
    cwJobSettings::initialize();
    auto settings = cwJobSettings::instance();
    REQUIRE(settings);

    if(settings->idleThreadCount() <= kSingleThread) {
        SKIP("Needs more than one ideal thread to give the task pool a distinct size");
    }

    cwThreadCountRestorer threadCount(kSingleThread);

    CHECK(settings->threadCount() == kSingleThread);
}
