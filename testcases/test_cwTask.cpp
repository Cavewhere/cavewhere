//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTask.h"
#include "TaskWithBlockingAsync.h"
#include "cwJobSettings.h"

//Qt includes
#include <QThread>

TEST_CASE("cwTask should complete with asyncfuture", "[cwTask]") {

    // The thread count sizes CaveWhere's own task pool and leaves the global pool as Qt set it.
    cwJobSettings::initialize();

    SECTION("Thread Count idleCount") {
        cwJobSettings::instance()->setThreadCount(QThread::idealThreadCount());
        CHECK(cwTask::maxThreadCount() == QThread::idealThreadCount());
    }

    SECTION("Thread Count of 1") {
        cwJobSettings::instance()->setThreadCount(1);
        CHECK(cwTask::maxThreadCount() == 1);
    }

    TaskWithBlockingAsync task;
    CHECK(task.result() == 0);

    task.start();
    task.waitToFinish();

    CHECK(task.result() == 2318350);

    cwJobSettings::instance()->setThreadCount(QThread::idealThreadCount());
    CHECK(cwTask::maxThreadCount() == QThread::idealThreadCount());
}
