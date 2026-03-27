//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTaskFutureCombineModel.h"
#include "cwFutureManagerModel.h"
#include "cwSignalSpy.h"
#include "asyncfuture.h"

//Qt includes
#include <QtConcurrent>

TEST_CASE("cwTaskFutureCombineModel count reflects combined source model rows", "[cwTaskFutureCombineModel]") {

    cwFutureManagerModel modelA;
    cwFutureManagerModel modelB;

    cwTaskFutureCombineModel combine;
    combine.setModels({&modelA, &modelB});

    cwSignalSpy countChangedSpy(&combine, &cwTaskFutureCombineModel::countChanged);

    SECTION("count is zero when both source models are empty") {
        CHECK(combine.count() == 0);
        CHECK(countChangedSpy.size() == 0);
    }

    SECTION("count increases when a future is added to modelA") {
        auto future = QtConcurrent::run([]() { QThread::msleep(50); });
        modelA.addJob({QFuture<void>(future), "TaskA"});

        CHECK(combine.count() == 1);
        CHECK(countChangedSpy.size() == 1);

        REQUIRE(AsyncFuture::waitForFinished(future, 5000));
        CHECK(combine.count() == 0);
        CHECK(countChangedSpy.size() == 2);
    }

    SECTION("count reflects futures from both source models") {
        auto futureA = QtConcurrent::run([]() { QThread::msleep(100); });
        auto futureB = QtConcurrent::run([]() { QThread::msleep(100); });

        modelA.addJob({QFuture<void>(futureA), "TaskA"});
        modelB.addJob({QFuture<void>(futureB), "TaskB"});

        CHECK(combine.count() == 2);

        auto combined = AsyncFuture::combine();
        combined << futureA << futureB;
        REQUIRE(AsyncFuture::waitForFinished(combined.future(), 5000));

        CHECK(combine.count() == 0);
    }

    SECTION("countChanged is not emitted when models are set but empty") {
        cwTaskFutureCombineModel fresh;
        cwSignalSpy spy(&fresh, &cwTaskFutureCombineModel::countChanged);

        fresh.setModels({&modelA, &modelB});

        CHECK(fresh.count() == 0);
        CHECK(spy.size() == 0);
    }
}
