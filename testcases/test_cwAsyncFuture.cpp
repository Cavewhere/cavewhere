//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwAsyncFuture.h"
#include "cwConcurrent.h"

//Qt includes
#include <QThread>
#include <QtConcurrent>
#include <QAtomicInt>

TEST_CASE("cwAsyncFuture::Restarter should restart correctly", "[cwAsyncFuture]") {

    cwAsyncFuture::Restarter<int> restarter(QCoreApplication::instance());

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

            auto future = cwConcurrent::run(concurrentRun);
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

TEST_CASE("cwAsyncFuture::Restarter should not restart if parent has been deleted", "[cwAsyncFuture]") {

    class ParentObject : public QObject {
    public:
        ParentObject() {
            values.resize(1000);
            values.last() = 40;
        }

        int last() const {
            return values.last();
        }

        void setLast(int value) {
            values.last() = value;
        }

    private:
        QVector<int> values;
    };

    auto contextObj = new ParentObject();
    QAtomicInt count(0);

    cwAsyncFuture::Restarter<int> restarter(contextObj);
    QFuture<int> latestFuture;

    restarter.onFutureChanged([&latestFuture, &restarter]() {
        latestFuture = restarter.future();
    });

    for(int i = 0; i < 20; i++) {
        auto run = [&count, i, contextObj]() {
            contextObj->setLast(contextObj->last() + i);
            int value = contextObj->last();
            auto concurrentRun = [&count, value]() {
                QThread::msleep(10);
                count++;
                return value;
            };

            auto future = cwConcurrent::run(concurrentRun);
            return future;
        };

        restarter.restart(run);
    }

    delete contextObj;

    while(latestFuture.isRunning()) {
        cwAsyncFuture::waitForFinished(latestFuture);
    }

    CHECK(latestFuture.isCanceled() == true);
    CHECK(count == 1);

}
