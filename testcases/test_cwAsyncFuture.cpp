//Catch includes
#include "catch.hpp"

//Our includes
#include "cwAsyncFuture.h"

//Qt includes
#include <QThread>
#include <QtConcurrent>
#include <QAtomicInt>

TEST_CASE("cwAsyncFuture should restart correctly", "[cwAsyncFuture]") {

    QAtomicInt count(0);
    auto concurrentRun = [&count]() {
        QThread::msleep(10);
        count++;
    };

    auto run = [concurrentRun]() {
        auto future = QtConcurrent::run(concurrentRun);
        return future;
    };

    QFuture<void> runFuture;

    for(int i = 0; i < 20; i++) {
        cwAsyncFuture::restart(&runFuture, run);
    }

    cwAsyncFuture::waitForFinished(runFuture);

    int finalCount = count;
    CHECK(finalCount <= 2);
    CHECK(finalCount >= 1);
}
