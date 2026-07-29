//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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


namespace {
    //Stands in for the task and future managers, which can only be given a job
    //by starting real work. Unlike QStandardItemModel it keeps the four roles
    //distinct: cwFutureManagerModel's enum starts at 0, so NumberOfStepRole is
    //Qt::EditRole, which QStandardItem folds into the name.
    class FakeJobModel : public QAbstractListModel
    {
    public:
        int rowCount(const QModelIndex& parent = QModelIndex()) const override
        {
            return parent.isValid() ? 0 : static_cast<int>(m_jobs.size());
        }

        QVariant data(const QModelIndex& index, int role) const override
        {
            if(!index.isValid()) {
                return QVariant();
            }

            const auto& job = m_jobs.at(index.row());

            switch(role) {
            case cwFutureManagerModel::NameRole:
                return job.name;
            case cwFutureManagerModel::ProgressRole:
                return job.completed;
            case cwFutureManagerModel::NumberOfStepRole:
                return job.steps;
            case cwFutureManagerModel::RunTimeRole:
                return job.runTime;
            default:
                break;
            }

            return QVariant();
        }

        void addJob(int completed, int steps)
        {
            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            m_jobs.append({QStringLiteral("Job"), completed, steps, 0});
            endInsertRows();
        }

        void finishJob(int row)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_jobs.removeAt(row);
            endRemoveRows();
        }

        void setCompleted(int row, int completed)
        {
            m_jobs[row].completed = completed;
            announce(row, cwFutureManagerModel::ProgressRole);
        }

        void setSteps(int row, int steps)
        {
            m_jobs[row].steps = steps;
            announce(row, cwFutureManagerModel::NumberOfStepRole);
        }

        void setRunTime(int row, int runTime)
        {
            m_jobs[row].runTime = runTime;
            announce(row, cwFutureManagerModel::RunTimeRole);
        }

    private:
        struct Job {
            QString name;
            int completed;
            int steps;
            int runTime;
        };

        void announce(int row, int role)
        {
            const QModelIndex changed = index(row);
            emit dataChanged(changed, changed, {role});
        }

        QList<Job> m_jobs;
    };
}

TEST_CASE("cwTaskFutureCombineModel aggregates progress across source models", "[cwTaskFutureCombineModel]") {

    FakeJobModel modelA;
    FakeJobModel modelB;

    cwTaskFutureCombineModel combine;
    combine.setModels({&modelA, &modelB});

    cwSignalSpy progressChangedSpy(&combine, &cwTaskFutureCombineModel::progressChanged);

    SECTION("progress is indeterminate with nothing running") {
        CHECK(combine.progress() < 0.0);
    }

    SECTION("progress is indeterminate while no job reports steps") {
        modelA.addJob(0, 0);
        modelB.addJob(0, 0);

        CHECK(combine.progress() < 0.0);
    }

    SECTION("progress averages the jobs that report steps") {
        modelA.addJob(5, 10);
        modelB.addJob(1, 4);

        CHECK(combine.progress() == Catch::Approx(0.375));
    }

    SECTION("a job that cannot report steps is left out of the average") {
        modelA.addJob(5, 10);
        modelB.addJob(0, 0);

        CHECK(combine.progress() == Catch::Approx(0.5));
    }

    SECTION("a job reporting past its own step count cannot push the total over full") {
        modelA.addJob(30, 10);

        CHECK(combine.progress() == Catch::Approx(1.0));
    }

    SECTION("progressChanged is emitted when a job's progress moves") {
        modelA.addJob(0, 10);
        progressChangedSpy.clear();

        modelA.setCompleted(0, 5);

        CHECK(progressChangedSpy.size() == 1);
        CHECK(combine.progress() == Catch::Approx(0.5));
    }

    SECTION("progressChanged is emitted when a job learns how many steps it has") {
        modelA.addJob(5, 0);
        REQUIRE(combine.progress() < 0.0);
        progressChangedSpy.clear();

        modelA.setSteps(0, 10);

        CHECK(progressChangedSpy.size() == 1);
        CHECK(combine.progress() == Catch::Approx(0.5));
    }

    SECTION("progressChanged is emitted when a job appears or finishes") {
        modelA.addJob(5, 10);
        CHECK(progressChangedSpy.size() == 1);

        progressChangedSpy.clear();
        modelA.finishJob(0);

        CHECK(progressChangedSpy.size() == 1);
        CHECK(combine.progress() < 0.0);
    }

    SECTION("the elapsed clock ticking does not republish progress") {
        modelA.addJob(5, 10);
        progressChangedSpy.clear();

        modelA.setRunTime(0, 1234);

        CHECK(progressChangedSpy.size() == 0);
    }

    //The proxy publishes a new source model's rows before its columns, and with
    //no columns there is nothing to read — so progress read from inside
    //rowsInserted is still indeterminate. Whatever is announced last has to be
    //the real number.
    SECTION("a source model that arrives with work already running announces it") {
        FakeJobModel alreadyRunning;
        alreadyRunning.addJob(4, 10);

        cwTaskFutureCombineModel fresh;
        double lastAnnounced = -99.0;
        QObject::connect(&fresh, &cwTaskFutureCombineModel::progressChanged, &fresh,
                         [&fresh, &lastAnnounced]() { lastAnnounced = fresh.progress(); });

        fresh.setModels({&alreadyRunning});

        CHECK(fresh.progress() == Catch::Approx(0.4));
        CHECK(lastAnnounced == Catch::Approx(0.4));
    }
}
