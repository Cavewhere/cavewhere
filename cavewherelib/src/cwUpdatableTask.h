#ifndef CWUPDATABLETASK_H
#define CWUPDATABLETASK_H

//Qt includes
#include <QObject>
#include <QtTaskTree/qtasktree.h>

//Our includes
#include "cwUpdatable.h"

class cwUpdateCoordinator;

/**
    Adapts one cwUpdatable pipeline into a task that QTaskTree can run.

    Deliberately not exported: this is cwUpdateCoordinator's private scheduling
    node, and keeping the TaskTree types out of cavewherelib's public surface is
    what keeps the choice of cascade engine an internal one.

    QDefaultTaskAdapter needs a default-constructible QObject with a public
    start() and a done() signal, which is all this class is: start() drives the
    pipeline's update() and the task finishes when the pipeline reports Clean.
    The running task tree owns the instance, so the pipeline it should drive is
    handed over by the recipe's setup handler rather than the constructor.

    Because cwUpdatable is not a QObject, state changes arrive via the
    coordinator, which already connects each pipeline's concrete
    updateStateChanged signal and re-emits it as pipelineStateChanged.
*/
class cwUpdatableTask : public QObject
{
    Q_OBJECT

public:
    explicit cwUpdatableTask(QObject* parent = nullptr);

    void setPipeline(cwUpdateCoordinator* coordinator, cwUpdatable* pipeline);

    void start();

signals:
    void done(QtTaskTree::DoneResult result);

private:
    void onPipelineStateChanged(cwUpdatable* pipeline);
    void finish(QtTaskTree::DoneResult result);

    cwUpdateCoordinator* m_coordinator = nullptr;
    cwUpdatable* m_pipeline = nullptr;
    bool m_finished = false;
};

#endif // CWUPDATABLETASK_H
