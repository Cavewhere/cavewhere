//Catch includes
#include "catch.hpp"

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

        CHECK(cwAsyncFuture::waitForFinished(combine.future(), numberOfTasks * sleepScale * 10));

        CHECK(model.rowCount() == 0);
        spyChecker[&rowsRemovedSpy] = numberOfTasks;

        CHECK(dataChangedSpy.size() > 0);

        //Progress changes for all tasks.
        int progressChanges = numberOfTasks;
        int timeOutChanges = numberOfTasks * sleepScale / model.interval();
        CHECK(dataChangedSpy.size() >= (progressChanges + timeOutChanges));
    }
}

