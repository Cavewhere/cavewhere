//Our includes
#include "cwUpdatableTask.h"
#include "cwUpdateCoordinator.h"

cwUpdatableTask::cwUpdatableTask(QObject* parent) :
    QObject(parent)
{
}

void cwUpdatableTask::setPipeline(cwUpdateCoordinator* coordinator, cwUpdatable* pipeline)
{
    m_coordinator = coordinator;
    m_pipeline = pipeline;
}

void cwUpdatableTask::start()
{
    if(m_coordinator == nullptr || m_pipeline == nullptr) {
        finish(QtTaskTree::DoneResult::Error);
        return;
    }

    connect(m_coordinator, &cwUpdateCoordinator::pipelineStateChanged,
            this, &cwUpdatableTask::onPipelineStateChanged);

    //Only Dirty needs driving. A pipeline already Working covers the current
    //data, and calling update() on it would make its restarter cancel the run in
    //flight and start over from zero; the node just waits for that run instead.
    if(m_pipeline->updateState() == cwUpdatable::State::Dirty) {
        m_pipeline->update();
    }

    //update() may resolve the pipeline synchronously, in which case no further
    //state change is coming and the task finishes without reaching the event loop.
    if(m_pipeline->updateState() == cwUpdatable::State::Clean) {
        finish(QtTaskTree::DoneResult::Success);
    }
}

void cwUpdatableTask::onPipelineStateChanged(cwUpdatable* pipeline)
{
    if(pipeline != m_pipeline || m_finished) {
        return;
    }

    switch(m_pipeline->updateState()) {
    case cwUpdatable::State::Clean:
        finish(QtTaskTree::DoneResult::Success);
        break;
    case cwUpdatable::State::Dirty:
        //Re-edited while this node was running: drive it again so the pipeline's
        //restarter coalesces the fresh edit into the run this node is waiting on.
        m_pipeline->update();
        break;
    case cwUpdatable::State::Working:
        break;
    }
}

void cwUpdatableTask::finish(QtTaskTree::DoneResult result)
{
    if(m_finished) {
        return;
    }
    m_finished = true;
    emit done(result);
}
