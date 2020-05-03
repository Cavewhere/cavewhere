//Catch includes
#include "catch.hpp"

//Our includes
#include "cwTask.h"
#include "TaskWithBlockingAsync.h"
#include "cwJobSettings.h"

//Qt includes
#include <QThreadPool>

TEST_CASE("cwTask should complete with asyncfuture", "[cwTask]") {

    cwJobSettings::initialize();

    SECTION("Thread Count idleCount") {
        cwJobSettings::instance()->setThreadCount(QThread::idealThreadCount());
        CHECK(cwTask::maxThreadCount() == QThreadPool::globalInstance()->maxThreadCount());
        CHECK(cwTask::maxThreadCount() == QThread::idealThreadCount());
    }

    SECTION("Thread Count of 1") {
        cwJobSettings::instance()->setThreadCount(1);
        CHECK(cwTask::maxThreadCount() == QThreadPool::globalInstance()->maxThreadCount());
        CHECK(cwTask::maxThreadCount() == 1);
    }

    TaskWithBlockingAsync task;
    CHECK(task.result() == 0);

    task.start();
    task.waitToFinish();

    CHECK(task.result() == 2318350);

    cwJobSettings::instance()->setThreadCount(QThread::idealThreadCount());
    CHECK(cwTask::maxThreadCount() == QThreadPool::globalInstance()->maxThreadCount());
    CHECK(cwTask::maxThreadCount() == QThread::idealThreadCount());
}
