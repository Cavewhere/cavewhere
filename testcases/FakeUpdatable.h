#ifndef FAKEUPDATABLE_H
#define FAKEUPDATABLE_H

//Our includes
#include "cwUpdatable.h"

//Qt includes
#include <QCoreApplication>
#include <QObject>

/**
 * Stand-in for a real derived-data pipeline, shared by the cascade and
 * coordinator suites. Mirrors the managers: markDirty() sets the pending bit,
 * run() trades it for a run whose future finish() resolves.
 *
 * autoFinish makes run() complete on the next event-loop turn (a short async
 * job); leaving it off lets a test hold a pipeline in Working for as long as it
 * needs, which is how a cascade is kept open on purpose.
 */
class FakeUpdatable : public QObject, public cwUpdatableBase
{
    Q_OBJECT

public:
    explicit FakeUpdatable(bool autoFinish = true) :
        m_autoFinish(autoFinish)
    {
    }

    //Mirrors the real managers, whose destructors announce teardown before doing
    //anything else. A subclass that pumps in its own destructor has to repeat
    //this, since a base destructor runs too late to help.
    ~FakeUpdatable() override { beginTeardown(); }

    void markDirty()
    {
        m_pending = true;
        emit updateStateChanged();
        //Mirrors the real managers: eager until a coordinator takes over.
        runIfStandalone();
    }

    void finish()
    {
        if(!isRunning()) { return; }
        endRun();
        emit updateStateChanged();
    }

    //Runs started, versus every run() call including ones that started nothing.
    int updateCount() const { return m_updateCount; }
    int driveCount() const { return m_driveCount; }

    //Off by default, which is cwScrapManager: a run with no dirty scrap has
    //nothing to triangulate. On models cwLinePlotManager, which solves whether or
    //not the survey changed — the property that makes it forceable by a button.
    void setRunsWhenClean(bool runsWhenClean) { m_runsWhenClean = runsWhenClean; }

    //Protected on the real thing, since it is a pipeline's statement about
    //itself. Exposed here so a case can check the latch head-on rather than only
    //through a destructor.
    using cwUpdatable::beginTeardown;

protected:
    cwUpdatable::State doUpdateState() const override
    {
        if(m_pending) { return cwUpdatable::State::Dirty; }
        if(isRunning()) { return cwUpdatable::State::Working; }
        return cwUpdatable::State::Clean;
    }

    QFuture<void> doRun() override
    {
        //Counted before the early return: run() is a force path on the real
        //pipelines too (cwLinePlotManager restarts its solve unconditionally), so
        //a redundant drive would restart real work.
        m_driveCount++;
        if(!m_pending && !m_runsWhenClean) { return currentRun(); }
        m_pending = false;
        const QFuture<void> future = beginRun();
        m_updateCount++;
        emit updateStateChanged();

        if(m_autoFinish) {
            QMetaObject::invokeMethod(this, &FakeUpdatable::finish, Qt::QueuedConnection);
        }
        return future;
    }

private:
    bool m_autoFinish;
    bool m_runsWhenClean = false;
    bool m_pending = false;
    int m_updateCount = 0;
    int m_driveCount = 0;

signals:
    void updateStateChanged();
};

namespace FakeUpdatableTest {

    //Event-loop turns spun to let the queued hops settle: the fake pipeline's
    //queued finish, a coordinator's queued flush, and the cascade a flush starts.
    //Deliberately generous, since each extra link in a dependency chain costs
    //several more. Deterministic rather than timing-based — processEvents() drains
    //what is already queued and nothing here is threaded — so too small a budget
    //fails outright instead of flaking on a slower machine. A case needing a deeper
    //chain than this should wait on its own post-condition rather than raise it.
    constexpr int kSettleTurns = 16;

    inline void settle()
    {
        for(int i = 0; i < kSettleTurns; i++) {
            QCoreApplication::processEvents();
        }
    }

    //Models a dependency edge the way the real managers have one — "the solve
    //dirtied the scraps". The fake has no data of its own, so the edge is
    //exercised by dirtying the consumer once the producer starts running.
    inline void dirtyWhenWorking(FakeUpdatable* producer, FakeUpdatable* consumer)
    {
        QObject::connect(producer, &FakeUpdatable::updateStateChanged, consumer,
                         [producer, consumer]() {
            if(producer->updateState() == cwUpdatable::State::Working) {
                consumer->markDirty();
            }
        });
    }
}

#endif // FAKEUPDATABLE_H
