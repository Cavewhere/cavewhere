//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwFutureManagerModel.h"
#include "SpyChecker.h"
#include "cwAsyncFuture.h"

//Qt includes
#include <QtConcurrent>
#include <QSignalSpy>

//Async includes
#include <asyncfuture.h>


TEST_CASE("cwFutureManagerModel should add and watch futures correctly", "[cwFutureManagerModel]") {

    cwFutureManagerModel model;

    QSignalSpy rowsInsertedSpy(&model, &cwFutureManagerModel::rowsInserted);
    QSignalSpy rowsRemovedSpy(&model, &cwFutureManagerModel::rowsRemoved);
    QSignalSpy rowsMovedSpy(&model, &cwFutureManagerModel::rowsMoved);
    QSignalSpy columnsInsertedSpy(&model, &cwFutureManagerModel::columnsInserted);
    QSignalSpy columnsRemovedSpy(&model, &cwFutureManagerModel::columnsRemoved);
    QSignalSpy columnsMovedSpy(&model, &cwFutureManagerModel::columnsMoved);
    QSignalSpy dataChangedSpy(&model, &cwFutureManagerModel::dataChanged);
    QSignalSpy intervalChangedSpy(&model, &cwFutureManagerModel::intervalChanged);

    SpyChecker spyChecker({
                              {&rowsInsertedSpy, 0},
                              {&rowsRemovedSpy, 0},
                              {&rowsMovedSpy, 0},
                              {&columnsMovedSpy, 0},
                              {&columnsRemovedSpy, 0},
                              {&columnsInsertedSpy, 0},
                              {&dataChangedSpy, 0},
                              {&intervalChangedSpy, 0}
                          });

    rowsInsertedSpy.setObjectName("rowsInsertedSpy");
    rowsRemovedSpy.setObjectName("rowsRemovedSpy");
    rowsMovedSpy.setObjectName("rowsMovedSpy");
    columnsMovedSpy.setObjectName("columnsMovedSpy");
    columnsRemovedSpy.setObjectName("columnsRemovedSpy");
    columnsInsertedSpy.setObjectName("columnsInsertedSpy");
    dataChangedSpy.setObjectName("dataChangedSpy");
    intervalChangedSpy.setObjectName("intervalChangedSpy");

    class Row {
    public:
        Row() {}
        Row(QString name) : name(name) {}
        QString name;
        int numberOfSteps = 0;
        int progress = 0;
        QFuture<void> future;
    };

    auto checkRows = [&model](QVector<Row> rows) {
        REQUIRE(rows.size() == model.rowCount());
        for(int i = 0; i < rows.size(); i++) {
            QModelIndex index = model.index(i);
            const auto& row = rows.at(i);

            CHECK(row.name.toStdString() == index.data(cwFutureManagerModel::NameRole).toString().toStdString());
            CHECK(row.numberOfSteps == index.data(cwFutureManagerModel::NumberOfStepRole).toInt());
            CHECK(row.progress == index.data(cwFutureManagerModel::ProgressRole).toInt());
        }
    };

    SECTION("Model should handle multiple tasks") {

        auto longTask = [](int sleepTime) {
            QThread::msleep(sleepTime);
        };

        int numberOfTasks = 10;
        int sleepScale = 100;

        SECTION("Set the interval to 120") {
            model.setInterval(120);
            CHECK(model.interval() == 120);
            spyChecker[&intervalChangedSpy]++;
            spyChecker.checkSpies();
            spyChecker.clearSpyCounts();
        }

        SECTION("Set the interval to 250") {
            model.setInterval(250); //The default
            CHECK(model.interval() == 250);
            spyChecker.checkSpies();
            spyChecker.clearSpyCounts();
        }

        QString jobTemplate("Job%1");

        QVector<Row> rows;

        for(int i = 0; i < numberOfTasks; i++) {
            int timeToSleep = (i + 1) * sleepScale;
            auto future = QtConcurrent::run(std::bind(longTask, timeToSleep));
            auto jobName = jobTemplate.arg(i);
            model.addJob({future, jobName});

            Row row(jobName);
            row.future = future;
            rows.append(row);
        }

        CHECK(model.rowCount() == numberOfTasks);
        spyChecker[&rowsInsertedSpy] = numberOfTasks;
        spyChecker.checkSpies();

        checkRows(rows);

        auto combine = AsyncFuture::combine();
        for(auto row : rows) {
            combine << row.future;
        }

        REQUIRE(cwAsyncFuture::waitForFinished(combine.future(), numberOfTasks * sleepScale * 10));

        CHECK(model.rowCount() == 0);
        spyChecker[&rowsRemovedSpy] = numberOfTasks;

        CHECK(dataChangedSpy.size() > 0);

        //Progress changes for all tasks.
        int progressChanges = numberOfTasks;
        int timeOutChanges = numberOfTasks * sleepScale / model.interval();
        int buffer = 1;
        CHECK(dataChangedSpy.size() >= (progressChanges + timeOutChanges) - buffer);
    }

    SECTION("Model shouldn't add canceled futures") {
        model.addJob(cwFuture(QFuture<void>(), "Canceled Job"));
        CHECK(model.rowCount() == 0);
        spyChecker.checkSpies();
    }

    SECTION("Model shouldn't add finished futures") {
        model.addJob(cwFuture(AsyncFuture::completed(), "Finished Job"));
        CHECK(model.rowCount() == 0);
        spyChecker.checkSpies();
    }
}

TEST_CASE("Update should update number of steps correctly", "[cwFutureManagerModel]") {

    cwFutureManagerModel model;

    int size = 50;
    int sleepTime = 10;

    QVector<int> steps = {
        size * 2,
        size * 3
    };
    int currentIndex = 0;

    QObject::connect(&model, &cwFutureManagerModel::dataChanged,
            [&currentIndex, steps](const QModelIndex& topLeft,
                     const QModelIndex& bottomRight,
                     QVector<int> roles) {

        REQUIRE(topLeft == bottomRight);

        if(roles.contains(cwFutureManagerModel::NumberOfStepRole)) {
            bool okay;
            int numSteps = topLeft.data(cwFutureManagerModel::NumberOfStepRole).toInt(&okay);
            CHECK(okay);
            REQUIRE(currentIndex < steps.size());
            CHECK(steps.at(currentIndex) == numSteps);

            currentIndex++;
        }
    });

    QSignalSpy dataChangedSpy(&model, &cwFutureManagerModel::dataChanged);

    QVector<int> ints(size);
    std::iota(ints.begin(), ints.end(), ints.size());
    std::function<int (int)> func = [sleepTime](int x)->int {
        QThread::msleep(sleepTime);
        return x * x;
    };
    QFuture<int> mappedFuture = QtConcurrent::mapped(ints, func);

    auto nextFuture = AsyncFuture::observe(mappedFuture).subscribe([ints, func](){
        QFuture<int> mappedFuture2 = QtConcurrent::mapped(ints, func);
        return mappedFuture2;
    }).future();

    bool nextExecuted2 = false;
    auto nextFuture2 = AsyncFuture::observe(nextFuture).subscribe([&nextExecuted2, ints, func](){
        QFuture<int> mappedFuture2 = QtConcurrent::mapped(ints, func);
        nextExecuted2 = true;
        return mappedFuture2;
    }).future();


    model.addJob({nextFuture2, "ChainedFutures"});

    REQUIRE(cwAsyncFuture::waitForFinished(nextFuture2, size * sleepTime * 3));
}

TEST_CASE("cwFutureManagerModel waitForFinished should work correctly", "[cwFutureManagerModel]") {

    cwFutureManagerModel model;

    int sleepTime = 10;
    QAtomicInt count;
    std::function<int (int)> func = [sleepTime, &count](int x)->int {
        QThread::msleep(sleepTime);
        count++;
        return x * x;
    };

    int runs = 5;
    for(int i = 0; i < runs; i++) {
        auto future = QtConcurrent::run(std::bind(func, i));
        model.addJob({future, QString("Future %1").arg(i)});
    }

    QTimer timer;
    timer.setInterval(sleepTime * 0.5);
    timer.singleShot(sleepTime * 0.5, [func, &model]() {
        auto future = QtConcurrent::run(std::bind(func, 10));
        model.addJob({future, QString("delayed future")});
    });

    model.waitForFinished();

    CHECK(count == runs + 1);
}

TEST_CASE("cwFutureManagerModel waitForFinished should work correctly with cwAsyncFuture::Restarter", "[cwFutureManagerModel]") {

    cwAsyncFuture::Restarter<int> restarter(QCoreApplication::instance());
    cwFutureManagerModel model;

    QAtomicInt count(0);

    restarter.onFutureChanged([&model, &restarter]() {
        model.addJob({restarter.future(), "Job}"});
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

    model.waitForFinished();

    CHECK(restarter.future().isRunning() == false);
    REQUIRE(restarter.future().isCanceled() == false);
    CHECK(restarter.future().isFinished() == true);
    CHECK(restarter.future().result() == 19); //We restarted from 0 to 19

    int finalCount = count;
    CHECK(finalCount <= 2);
    CHECK(finalCount >= 1);
}
