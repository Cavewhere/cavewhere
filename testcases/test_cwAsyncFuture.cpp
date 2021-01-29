//Catch includes
#include "catch.hpp"

//Our includes
#include "cwAsyncFuture.h"

//Qt includes
#include <QThread>
#include <QtConcurrent>
#include <QAtomicInt>

TEST_CASE("cwAsyncFuture::Restarter should restart correctly", "[cwAsyncFuture]") {

    cwAsyncFuture::Restarter<int> restarter;

    QList<QFuture<int>> allFutures;
    QAtomicInt count(0);

    restarter.onFutureChanged([&allFutures, &restarter]() {
        Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
        allFutures.append(restarter.future());
    });

    for(int i = 0; i < 20; i++) {
        auto run = [&count, i]() {
            auto concurrentRun = [&count, i]() {
                QThread::msleep(10);
                count++;
                return i;
            };

            auto future = QtConcurrent::run(concurrentRun);
            return future;
        };

        restarter.restart(run);
    }

    for(int i = 0; i < allFutures.size(); i++) {
        cwAsyncFuture::waitForFinished(allFutures.at(i));
    }

    CHECK(restarter.future().isRunning() == false);
    REQUIRE(restarter.future().isCanceled() == false);
    CHECK(restarter.future().isFinished() == true);
    CHECK(restarter.future().result() == 19); //We restarted from 0 to 19

    CHECK(!allFutures.isEmpty());
    CHECK(allFutures.last().isRunning() == false);
    REQUIRE(allFutures.last().isCanceled() == false);
    CHECK(allFutures.last().isFinished() == true);
    CHECK(allFutures.last().result() == 19);

    int finalCount = count;
    CHECK(finalCount <= 2);
    CHECK(finalCount >= 1);
}
