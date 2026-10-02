//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwFutureManagerModel.h"
#include "cwProgressNode.h"
#include "SpyChecker.h"

//Qt includes
#include <QtConcurrent>
#include <QPromise>
#include <QElapsedTimer>
#include "cwSignalSpy.h"

//Std includes
#include <functional>

//Async includes
#include <asyncfuture.h>

namespace {
    constexpr int kSettleTimeoutMs = 2000;
    constexpr int kSettlePollMs = 5;

    //Waits for a posted event to land. The model reads progress off its
    //QFutureWatchers, and a watcher only learns of a change by processing an
    //event the future interface can hold back briefly — it throttles progress
    //reports — so one turn of the loop is not enough to rely on.
    bool settle(const std::function<bool ()>& done)
    {
        QElapsedTimer timer;
        timer.start();

        while(timer.elapsed() < kSettleTimeoutMs) {
            QCoreApplication::processEvents();
            if(done()) {
                return true;
            }
            QThread::msleep(kSettlePollMs);
        }

        return done();
    }

    //A job the test drives directly. QtConcurrent decides its own progress; a
    //QPromise lets the test say how many steps there are and how far along it is.
    class TestJob
    {
    public:
        //Reported before the model sees the future: attaching a watcher syncs
        //whatever the future already says, and that sync is not throttled.
        TestJob(cwFutureManagerModel* model, const QString& name, int completed = 0, int steps = 0)
        {
            m_promise.start();

            if(steps > 0) {
                m_promise.setProgressRange(0, steps);
                m_promise.setProgressValue(completed);
            }

            model->addJob({m_promise.future(), name});

            //Attaching the watcher posts the sync of the range and the value,
            //each of which announces progress in its own right. Drained here so
            //a later assertion isn't racing callouts posted at attach time.
            QCoreApplication::processEvents();
        }

        void setSteps(int steps) { m_promise.setProgressRange(0, steps); }
        void setCompleted(int completed) { m_promise.setProgressValue(completed); }
        void finish() { m_promise.finish(); }

    private:
        QPromise<void> m_promise;
    };
}


TEST_CASE("cwFutureManagerModel should add and watch futures correctly", "[cwFutureManagerModel]") {

    cwFutureManagerModel model;

    cwSignalSpy rowsInsertedSpy(&model, &cwFutureManagerModel::rowsInserted);
    cwSignalSpy rowsRemovedSpy(&model, &cwFutureManagerModel::rowsRemoved);
    cwSignalSpy rowsMovedSpy(&model, &cwFutureManagerModel::rowsMoved);
    cwSignalSpy columnsInsertedSpy(&model, &cwFutureManagerModel::columnsInserted);
    cwSignalSpy columnsRemovedSpy(&model, &cwFutureManagerModel::columnsRemoved);
    cwSignalSpy columnsMovedSpy(&model, &cwFutureManagerModel::columnsMoved);
    cwSignalSpy dataChangedSpy(&model, &cwFutureManagerModel::dataChanged);
    cwSignalSpy intervalChangedSpy(&model, &cwFutureManagerModel::intervalChanged);

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

        REQUIRE(AsyncFuture::waitForFinished(combine.future(), numberOfTasks * sleepScale * 10));

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

    cwSignalSpy dataChangedSpy(&model, &cwFutureManagerModel::dataChanged);

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


    model.addJob({QFuture<void>(nextFuture2), "ChainedFutures"});

    REQUIRE(AsyncFuture::waitForFinished(nextFuture2, size * sleepTime * 3));
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
        model.addJob({QFuture<void>(future), QString("Future %1").arg(i)});
    }

    QTimer timer;
    timer.setInterval(sleepTime * 0.5);
    timer.singleShot(sleepTime * 0.5, [func, &model]() {
        auto future = QtConcurrent::run(std::bind(func, 10));
        model.addJob({QFuture<void>(future), QString("delayed future")});
    });

    model.waitForFinished();

    CHECK(count == runs + 1);
}

TEST_CASE("cwFutureManagerModel waitForFinished should work correctly with AsyncFuture::Restarter", "[cwFutureManagerModel]") {

    AsyncFuture::Restarter<int> restarter(QCoreApplication::instance());
    cwFutureManagerModel model;

    auto count = std::make_shared<QAtomicInt>(0);

    restarter.onFutureChanged([&model, &restarter]() {
        model.addJob({QFuture<void>(restarter.future()), "Job}"});
    });

    for(int i = 0; i < 20; i++) {
        auto run = [count, i]() {
            auto concurrentRun = [count, i]() {
                QThread::msleep(10);
                *(count.get()) += 1;
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

    int finalCount = *(count.get());
    CHECK(finalCount <= 2);
    CHECK(finalCount >= 1);
}

TEST_CASE("cwFutureManagerModel isEmpty and allFinished signal", "[cwFutureManagerModel]") {

    cwFutureManagerModel model;
    cwSignalSpy allFinishedSpy(&model, &cwFutureManagerModel::allFinished);

    SECTION("isEmpty returns true when no jobs are added") {
        CHECK(model.isEmpty() == true);
    }

    SECTION("isEmpty returns false when a job is running") {
        auto future = QtConcurrent::run([]() { QThread::msleep(100); });
        model.addJob({QFuture<void>(future), "TestJob"});

        CHECK(model.isEmpty() == false);
        CHECK(allFinishedSpy.size() == 0);

        REQUIRE(AsyncFuture::waitForFinished(future, 5000));

        CHECK(model.isEmpty() == true);
        CHECK(allFinishedSpy.size() == 1);
    }

    SECTION("allFinished emits once when multiple jobs complete") {
        auto future1 = QtConcurrent::run([]() { QThread::msleep(50); });
        auto future2 = QtConcurrent::run([]() { QThread::msleep(100); });

        model.addJob({QFuture<void>(future1), "Job1"});
        model.addJob({QFuture<void>(future2), "Job2"});

        CHECK(model.isEmpty() == false);
        CHECK(model.rowCount() == 2);

        auto combined = AsyncFuture::combine();
        combined << future1 << future2;
        REQUIRE(AsyncFuture::waitForFinished(combined.future(), 5000));

        CHECK(model.isEmpty() == true);
        CHECK(allFinishedSpy.size() == 1);
    }

    SECTION("allFinished does not emit for canceled or finished futures that are never added") {
        model.addJob(cwFuture(QFuture<void>(), "Canceled"));
        model.addJob(cwFuture(AsyncFuture::completed(), "Finished"));

        CHECK(model.isEmpty() == true);
        CHECK(allFinishedSpy.size() == 0);
    }
}

TEST_CASE("cwFutureManagerModel count reflects the jobs it is watching", "[cwFutureManagerModel]") {

    cwFutureManagerModel model;
    cwSignalSpy countChangedSpy(&model, &cwFutureManagerModel::countChanged);

    SECTION("count is zero with nothing running") {
        CHECK(model.count() == 0);
        CHECK(countChangedSpy.size() == 0);
    }

    SECTION("count rises when a job is added and falls when it finishes") {
        auto future = QtConcurrent::run([]() { QThread::msleep(50); });
        model.addJob({QFuture<void>(future), "TaskA"});

        CHECK(model.count() == 1);
        CHECK(countChangedSpy.size() == 1);

        REQUIRE(AsyncFuture::waitForFinished(future, 5000));

        CHECK(model.count() == 0);
        CHECK(countChangedSpy.size() == 2);
    }

    SECTION("count tracks several jobs at once") {
        auto futureA = QtConcurrent::run([]() { QThread::msleep(100); });
        auto futureB = QtConcurrent::run([]() { QThread::msleep(100); });

        model.addJob({QFuture<void>(futureA), "TaskA"});
        model.addJob({QFuture<void>(futureB), "TaskB"});

        CHECK(model.count() == 2);

        auto combined = AsyncFuture::combine();
        combined << futureA << futureB;
        REQUIRE(AsyncFuture::waitForFinished(combined.future(), 5000));

        CHECK(model.count() == 0);
    }

    SECTION("a job that is already over never counts") {
        model.addJob(cwFuture(QFuture<void>(), "Canceled"));
        model.addJob(cwFuture(AsyncFuture::completed(), "Finished"));

        CHECK(model.count() == 0);
        CHECK(countChangedSpy.size() == 0);
    }
}

TEST_CASE("cwFutureManagerModel aggregates progress across its jobs", "[cwFutureManagerModel]") {

    constexpr int kRetimeIntervalMs = 20;

    cwFutureManagerModel model;
    cwSignalSpy progressChangedSpy(&model, &cwFutureManagerModel::progressChanged);

    SECTION("progress is indeterminate with nothing running") {
        CHECK(model.progress() < 0.0);
    }

    SECTION("progress is indeterminate while no job reports steps") {
        TestJob silentA(&model, "SilentA");
        TestJob silentB(&model, "SilentB");

        CHECK(model.progress() < 0.0);
    }

    SECTION("progress averages the jobs that report steps") {
        TestJob half(&model, "Half", 5, 10);
        TestJob quarter(&model, "Quarter", 1, 4);

        CHECK(model.progress() == Catch::Approx(0.375));
    }

    SECTION("a job that cannot report steps is left out of the average") {
        TestJob half(&model, "Half", 5, 10);
        TestJob silent(&model, "Silent");

        CHECK(model.progress() == Catch::Approx(0.5));
    }

    SECTION("a job standing on its last step reads as full") {
        TestJob done(&model, "Done", 10, 10);

        CHECK(model.progress() == Catch::Approx(1.0));
    }

    SECTION("progress is republished when a job's progress moves") {
        TestJob job(&model, "Job", 0, 10);
        progressChangedSpy.clear();

        job.setCompleted(5);

        REQUIRE(settle([&]() { return progressChangedSpy.size() > 0; }));
        CHECK(model.progress() == Catch::Approx(0.5));
    }

    SECTION("progress is republished when a job learns how many steps it has") {
        TestJob job(&model, "Job");
        REQUIRE(model.progress() < 0.0);
        progressChangedSpy.clear();

        job.setSteps(10);

        REQUIRE(settle([&]() { return progressChangedSpy.size() > 0; }));
        CHECK(model.progress() == Catch::Approx(0.0));
    }

    SECTION("progress is republished when the last job finishes") {
        TestJob job(&model, "Job", 5, 10);
        progressChangedSpy.clear();

        job.finish();

        //Not waitForFinished: the promise finished on this thread, so the future
        //is already settled and nothing would pump the watcher's own event.
        REQUIRE(settle([&]() { return model.count() == 0; }));

        CHECK(progressChangedSpy.size() > 0);
        CHECK(model.progress() < 0.0);
    }

    SECTION("the elapsed clock ticking does not republish progress") {
        TestJob job(&model, "Job", 5, 10);

        cwSignalSpy dataChangedSpy(&model, &cwFutureManagerModel::dataChanged);
        progressChangedSpy.clear();

        //The retime is what this guards: it republishes every row four times a
        //second with no progress behind it, and progress must not ride along.
        model.setInterval(kRetimeIntervalMs);
        REQUIRE(settle([&]() { return dataChangedSpy.size() > 0; }));

        CHECK(progressChangedSpy.size() == 0);
    }
}

namespace {
    //Margin on top of the age threshold, so a test never races the clock
    constexpr int kAgeMarginMs = 60;
    //Long enough for several detail polls to run without anything changing
    constexpr int kQuietMs = 300;

    //Keeps the event loop turning for a while, so timers and posted callouts
    //all get their chance
    void spin(int milliseconds)
    {
        QElapsedTimer timer;
        timer.start();

        settle([milliseconds, &timer]() { return timer.elapsed() >= milliseconds; });
    }

    //Lets a node grow old enough to be worth naming on the detail line
    void spinPastDetailAge()
    {
        spin(cwFutureManagerModel::kDetailMinAgeMs + kAgeMarginMs);
    }

    QList<int> rolesOf(const QList<QVariant>& arguments)
    {
        return arguments.at(2).value<QList<int>>();
    }

    bool isDetailChange(const QList<QVariant>& arguments)
    {
        return rolesOf(arguments).contains(cwFutureManagerModel::DetailNameRole);
    }

    int detailChangeCount(const cwSignalSpy& spy)
    {
        int count = 0;
        for(const auto& arguments : spy) {
            if(isDetailChange(arguments)) {
                count++;
            }
        }
        return count;
    }
}

TEST_CASE("cwFutureManagerModel names the detail line of a progress tree", "[cwFutureManagerModel]") {

    SECTION("a job with no tree keeps its detail roles empty and stays quiet") {
        cwFutureManagerModel model;
        TestJob job(&model, "Plain", 1, 10);

        cwSignalSpy dataChangedSpy(&model, &cwFutureManagerModel::dataChanged);

        const QModelIndex index = model.index(0);
        CHECK(index.data(cwFutureManagerModel::DetailNameRole).toString().isEmpty());
        CHECK(index.data(cwFutureManagerModel::DetailProgressRole).toLongLong() == 0);
        CHECK(index.data(cwFutureManagerModel::DetailTotalRole).toLongLong() == 0);
        CHECK(index.data(cwFutureManagerModel::TreeBackedRole).toBool() == false);

        spin(kQuietMs);

        CHECK(detailChangeCount(dataChangedSpy) == 0);
    }

    SECTION("detailFor picks the leaf that has been running longest") {
        auto root = cwProgressNode::createRoot("Run");

        QString name;
        qint64 done = 0;
        qint64 total = 0;

        SECTION("a tree with nothing in it has no detail") {
            CHECK(cwFutureManagerModel::detailFor(root, name, done, total) == false);
            CHECK(name.isEmpty());
        }

        SECTION("a leaf too young to name yields no detail") {
            auto leaf = root->addChild("Morphing");
            leaf->setTotal(100);
            leaf->report(10);

            CHECK(cwFutureManagerModel::detailFor(root, name, done, total) == false);
        }

        SECTION("a leaf that has lived long enough names itself and its counts") {
            auto leaf = root->addChild("Morphing");
            leaf->setTotal(100);
            leaf->report(10);

            spinPastDetailAge();

            REQUIRE(cwFutureManagerModel::detailFor(root, name, done, total));
            CHECK(name.toStdString() == std::string("Morphing"));
            CHECK(done == 10);
            CHECK(total == 100);
        }

        SECTION("an opaque leaf reports no counts") {
            auto leaf = root->addChild("Compressing texture");

            spinPastDetailAge();

            REQUIRE(cwFutureManagerModel::detailFor(root, name, done, total));
            CHECK(name.toStdString() == std::string("Compressing texture"));
            CHECK(done == 0);
            CHECK(total == 0);
        }

        SECTION("the older of two leaves holds the line until it finishes") {
            auto older = root->addChild("Older");
            older->setTotal(10);

            spin(kAgeMarginMs);

            auto younger = root->addChild("Younger");
            younger->setTotal(10);

            spinPastDetailAge();

            REQUIRE(cwFutureManagerModel::detailFor(root, name, done, total));
            CHECK(name.toStdString() == std::string("Older"));

            //The younger leaf working is no reason to take the line away
            younger->report(5);
            REQUIRE(cwFutureManagerModel::detailFor(root, name, done, total));
            CHECK(name.toStdString() == std::string("Older"));

            older->finish();
            REQUIRE(cwFutureManagerModel::detailFor(root, name, done, total));
            CHECK(name.toStdString() == std::string("Younger"));
            CHECK(done == 5);
            CHECK(total == 10);
        }

        SECTION("a leaf deeper in the tree can hold the line") {
            auto parent = root->addChild("Note");
            auto leaf = parent->addChild("Parsing");

            spinPastDetailAge();

            REQUIRE(cwFutureManagerModel::detailFor(root, name, done, total));
            CHECK(name.toStdString() == std::string("Parsing"));
        }

        root->cancel();
    }

    SECTION("polling publishes the detail roles, and only when they change") {
        cwFutureManagerModel model;

        auto root = cwProgressNode::createRoot("Run");
        model.addJob(cwFuture(root->future(), "Run", root));
        REQUIRE(model.rowCount() == 1);

        cwSignalSpy dataChangedSpy(&model, &cwFutureManagerModel::dataChanged);

        //The row says it has a tree from the moment it appears, so its caption
        //can read as a percent before any leaf is old enough to be named
        CHECK(model.index(0).data(cwFutureManagerModel::TreeBackedRole).toBool());

        auto leaf = root->addChild("Morphing");
        leaf->setTotal(100);
        leaf->report(10);

        REQUIRE(settle([&]() { return detailChangeCount(dataChangedSpy) > 0; }));

        const QModelIndex index = model.index(0);
        CHECK(index.data(cwFutureManagerModel::DetailNameRole).toString().toStdString()
              == std::string("Morphing"));
        CHECK(index.data(cwFutureManagerModel::DetailProgressRole).toLongLong() == 10);
        CHECK(index.data(cwFutureManagerModel::DetailTotalRole).toLongLong() == 100);

        //Exactly the three detail roles: the row's own bar rides the promise
        for(const auto& arguments : dataChangedSpy) {
            if(isDetailChange(arguments)) {
                const QList<int> roles = rolesOf(arguments);
                CHECK(roles == QList<int>({cwFutureManagerModel::DetailNameRole,
                                           cwFutureManagerModel::DetailProgressRole,
                                           cwFutureManagerModel::DetailTotalRole}));
            }
        }

        const int settledChanges = detailChangeCount(dataChangedSpy);
        spin(kQuietMs);
        CHECK(detailChangeCount(dataChangedSpy) == settledChanges);

        leaf->report(20);
        REQUIRE(settle([&]() { return detailChangeCount(dataChangedSpy) > settledChanges; }));
        CHECK(index.data(cwFutureManagerModel::DetailProgressRole).toLongLong() == 20);

        root->cancel();
    }
}
