#ifndef CWUPDATABLETASK_H
#define CWUPDATABLETASK_H

//Qt includes
#include <QFuture>
#include <QObject>
#include <QtTaskTree/qtasktree.h>

//Our includes
#include "cwUpdatable.h"

/**
    Adapts one cwUpdatable pipeline into a task that QTaskTree can run.

    Deliberately not exported: this is cwUpdateCoordinator's private scheduling
    node, and keeping the TaskTree types out of cavewherelib's public surface is
    what keeps the choice of cascade engine an internal one.

    QDefaultTaskAdapter needs a default-constructible QObject with a public
    start() and a done() signal, which is all this class is. The running task
    tree owns the instance, so the pipeline it should drive is handed over by the
    recipe's setup handler rather than the constructor.

    The node reads the pipeline's state once to decide what to do, then waits on
    a future rather than on the pipeline's signals: a run that already covers the
    current data is waited on, a dirty pipeline is run, and either way the node
    finishes when that run's future does.
*/
class cwUpdatableTask : public QObject
{
    Q_OBJECT

public:
    explicit cwUpdatableTask(QObject* parent = nullptr);

    void setPipeline(cwUpdatable* pipeline);

    void start();

signals:
    void done(QtTaskTree::DoneResult result);

private:
    void drive();
    void waitFor(const QFuture<void>& run);

    cwUpdatable* m_pipeline = nullptr;
};

#endif // CWUPDATABLETASK_H
