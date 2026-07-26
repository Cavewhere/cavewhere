//Our includes
#include "cwUpdatableTask.h"

//Async includes
#include "asyncfuture.h"

cwUpdatableTask::cwUpdatableTask(QObject* parent) :
    QObject(parent)
{
}

void cwUpdatableTask::setPipeline(cwUpdatable* pipeline)
{
    m_pipeline = pipeline;
}

void cwUpdatableTask::start()
{
    if(m_pipeline == nullptr) {
        emit done(QtTaskTree::DoneResult::Error);
        return;
    }

    drive();
}

void cwUpdatableTask::drive()
{
    switch(m_pipeline->updateState()) {
    case cwUpdatable::State::Clean:
        emit done(QtTaskTree::DoneResult::Success);
        return;
    case cwUpdatable::State::Working:
        //A run in flight already covers the current data. Waiting on it, rather
        //than forcing another, is what keeps an unrelated cascade from cancelling
        //work that is already doing this node's job.
        waitFor(m_pipeline->currentRun());
        return;
    case cwUpdatable::State::Dirty:
        waitFor(m_pipeline->run());
        return;
    }
}

void cwUpdatableTask::waitFor(const QFuture<void>& run)
{
    AsyncFuture::observe(run).context(this,
        [this]() {
            //A run can finish with the pipeline dirty again, because the source
            //was edited while it ran. drive() then runs it once more so the
            //layers below this node see current data. The future's completion
            //arrives through the event loop, so this iterates rather than
            //recursing, and it ends as soon as the edits do — provided a
            //pipeline that reports Dirty actually starts work when run(). One
            //that returned an already-finished future while still calling itself
            //Dirty would spin here.
            drive();
        },
        [this]() {
            //Cancelled: the run this node waited on will never report, so finish
            //rather than leave the cascade holding a node open forever. What is
            //still dirty stays in the coordinator's staleness aggregate.
            emit done(QtTaskTree::DoneResult::Error);
        });
}
