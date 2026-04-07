//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTaskManagerModel.h"
#include "cwTask.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QCoreApplication>

namespace {

class TestTask : public cwTask
{
    Q_OBJECT
public:
    using cwTask::cwTask;

    void runTask() override
    {
        done();
    }
};

} // namespace

#include "test_cwTaskManagerModel.moc"

TEST_CASE("cwTaskManagerModel isIdle and becameIdle signal", "[cwTaskManagerModel]") {

    cwTaskManagerModel model;
    cwSignalSpy becameIdleSpy(&model, &cwTaskManagerModel::becameIdle);

    SECTION("isIdle returns true when no tasks are added") {
        CHECK(model.isIdle() == true);
    }

    SECTION("isIdle returns true when task is added but not running") {
        TestTask task;
        task.setUsingThreadPool(true);
        model.addTask(&task);

        CHECK(model.isIdle() == true);
        model.removeTask(&task);
    }

    SECTION("isIdle returns false when task is running and becameIdle emits on finish") {
        TestTask task;
        task.setUsingThreadPool(true);
        model.addTask(&task);

        task.start();
        task.waitToFinish();

        QCoreApplication::processEvents();

        CHECK(model.isIdle() == true);
        CHECK(becameIdleSpy.size() == 1);

        model.removeTask(&task);
    }

    SECTION("becameIdle emits once when multiple tasks finish") {
        TestTask task1;
        TestTask task2;
        task1.setUsingThreadPool(true);
        task2.setUsingThreadPool(true);
        model.addTask(&task1);
        model.addTask(&task2);

        task1.start();
        task2.start();

        task1.waitToFinish();
        task2.waitToFinish();

        QCoreApplication::processEvents();

        CHECK(model.isIdle() == true);
        CHECK(becameIdleSpy.size() >= 1);

        model.removeTask(&task1);
        model.removeTask(&task2);
    }
}
