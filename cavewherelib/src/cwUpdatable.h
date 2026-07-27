#ifndef CWUPDATABLE_H
#define CWUPDATABLE_H

//Qt includes
#include <QFuture>
#include <QPromise>

//Std includes
#include <optional>

/**
    Contract for a derived-data pipeline (line plot, scraps, ...) that can fall
    behind its source data and be brought current on demand.

    Implementers are pure mechanism: a source edit marks the pipeline dirty and
    emits updateStateChanged(); they never decide whether to recompute. That
    policy — run immediately vs. accumulate until asked — belongs to
    cwUpdateCoordinator, which owns the automatic-update flag and drives run()
    when appropriate.

    A pipeline answers two separate questions, and neither substitutes for the
    other. updateState() says what the pipeline needs *next*; the future from
    run() / currentRun() says when the work in flight is over. A run can finish
    with the pipeline Dirty again, because the source was edited while it ran.

    updateState() and run() are non-virtual. They answer for a pipeline that is
    being torn down (see beginTeardown()) and otherwise forward to
    doUpdateState() / doRun(), which is what implementers provide.

    The concrete QObject subclass also declares a  void updateStateChanged()
    signal; cwUpdateCoordinator::add() connects it via the concrete type.
*/
class cwUpdatable
{
public:
    /**
        The single observable a pipeline exposes. The coordinator reads this one
        value per pipeline — rather than separate staleness and busy queries — so
        the two can never drift out of sync.

          Clean   - up to date; nothing pending and nothing running.
          Dirty   - source data is not yet reflected in a finished computation
                    and the pipeline is waiting to be run (or was re-edited while
                    a run was already in flight).
          Working - a computation covering every currently-known dirty input is
                    in flight, so there is nothing left to (re-)drive right now.

        Dirty takes priority over Working: a pipeline re-edited mid-run reports
        Dirty (not Working) so its driver runs it again once the run in flight
        is over. Working lasts exactly as long as the run's future, so a
        pipeline cannot report itself busy after its work has ended.

        There is no default: every implementer must state its value explicitly,
        so none can silently clear its dirty marker yet forget it is still busy
        (the failure the old isUpdating()==false default invited).
    */
    enum class State { Clean, Dirty, Working };

    virtual ~cwUpdatable() = default;

    //The pipeline's current state (see State) — the one thing the coordinator
    //reads. Clean once the pipeline is tearing down: whatever it still owed,
    //nothing can act on it any more.
    State updateState() const { return m_tearingDown ? State::Clean : doUpdateState(); }

    /**
        Recomputes now, unconditionally, and returns the future for that run.

        The primitive underneath runIfNeeded()'s Dirty branch, not a drive in its
        own right: it reads no state, so a call on a pipeline that is already
        Working restarts it, which for the real managers cancels a run that was
        doing the job. A caller that wants to *wait* for the current run wants
        currentRun() instead.

        Nothing drives a pipeline through here. An action that must recompute
        regardless — "Solve", "Compute Scraps" — marks the pipeline dirty and
        then drives it: marking is the force, and runIfNeeded() stays the only
        reading of the state.

        The future finishes when the run is over, which is the same instant the
        pipeline stops reporting Working. A call with nothing to do returns an
        already-finished future, so callers never need a separate "did it start
        anything" question. A pipeline that is tearing down starts nothing and
        takes that same path.
    */
    QFuture<void> run()
    {
        if(m_tearingDown) {
            return QtFuture::makeReadyVoidFuture();
        }
        return doRun();
    }

    /**
        Brings the pipeline up to date the way a driver should, and returns the
        future for whatever that leaves in flight.

        Not a force, and that is the whole point: Working attaches to the run
        already covering the current data instead of restarting it, which for the
        real managers would cancel a solve that was doing the job. A caller whose
        purpose is to recompute regardless marks the pipeline dirty first and then
        calls this — there is no second, forcing kind of drive.

        Says nothing about whether the pipeline is clean afterwards: a run can end
        with the pipeline dirty again, so a driver that must reach Clean calls this
        again when the future settles.
    */
    QFuture<void> runIfNeeded()
    {
        switch(updateState()) {
        case State::Clean:
            return QtFuture::makeReadyVoidFuture();
        case State::Working:
            return currentRun();
        case State::Dirty:
            return run();
        }
        Q_UNREACHABLE_RETURN(QtFuture::makeReadyVoidFuture());
    }

    /**
        The future for the run in flight, or an already-finished future when
        nothing is running. Waiting on it is how a driver lets a run that already
        covers the current data finish, rather than canceling it by forcing
        another.
    */
    virtual QFuture<void> currentRun() const = 0;

    //Called by cwUpdateCoordinator when it takes over driving this pipeline.
    //Until coordinated, an implementer may recompute eagerly on a source edit
    //(the sensible standalone default); once coordinated, it only marks dirty
    //and leaves the run decision to the coordinator. This is a driver handoff,
    //not the automatic-update policy — that lives entirely in the coordinator.
    virtual void setCoordinated(bool coordinated) = 0;

protected:
    /**
        Announces that this pipeline is being torn down: from here on
        updateState() reports Clean and run() starts nothing.

        Call it as the first statement of the destructor, ahead of anything that
        pumps an event loop. Every pipeline cancels its run and waits for the
        worker to stop on the way out, and that wait runs the event queue, so a
        driver's queued continuation can land inside the destructor body — while
        a state transition emitted from that same body reaches a driver
        synchronously, with no loop involved at all. QObject::destroyed is
        emitted by ~QObject, after the derived destructor body has finished, so
        it is too late to be the latch.

        The guard lives on this side of the call rather than in the driver
        because a pipeline also drives itself — runIfStandalone(), and each
        manager's own force path — so there is no single caller to guard. It is
        non-virtual, with the flag on this class rather than a subclass, so
        reading it while a derived destructor unwinds does not dispatch through a
        vtable that is already coming apart.

        Protected because it is a pipeline's statement about itself, and a
        one-way one: there is no untearing down, and nothing outside would have
        anything to say here that the pipeline doesn't already know.
    */
    void beginTeardown() { m_tearingDown = true; }
    bool isTearingDown() const { return m_tearingDown; }

    //The implementer's half of updateState(), called only while the pipeline is
    //not tearing down. Overriding it privately is fine, and preferred: dispatch
    //happens through this class, so the derived access level never applies.
    virtual State doUpdateState() const = 0;

    //The implementer's half of run(), same access note as doUpdateState().
    //
    //One invariant is owed here: a pipeline reporting Dirty must start work. A
    //driver waits on the future and runs the pipeline again if it is still Dirty
    //afterwards, so one that reported Dirty and handed back an already-finished
    //future would be asked again immediately, forever.
    virtual QFuture<void> doRun() = 0;

private:
    bool m_tearingDown = false;
};

/**
    Common mechanism for the eager-until-coordinated handoff and for the run in
    flight.

    Holds the coordinated flag, the shared "recompute now unless a coordinator is
    driving" idiom, and the run's promise, so each cwUpdatable implementer (line
    plot, scraps, LiDAR) doesn't repeat them. Implementers still provide
    doUpdateState()/doRun() and, being QObjects, declare their own void
    updateStateChanged() signal.

    The Working bit lives in that promise rather than in a bool each pipeline
    maintains: isRunning() is true exactly between beginRun() and endRun(), and a
    pipeline destroyed mid-run finishes its run on the way out rather than
    leaving whoever waits on it hanging. A completion path that never reaches
    endRun() still wedges — but it wedges a future, which a driver can cancel,
    where a stuck bool left nothing to act on.
*/
class cwUpdatableBase : public cwUpdatable
{
public:
    ~cwUpdatableBase() override
    {
        //Every pipeline has to announce teardown before its destructor pumps an
        //event loop; a miss is a use-after-free waiting for the right timing, so
        //fail here where the cause is obvious rather than there.
        Q_ASSERT_X(isTearingDown(), "~cwUpdatableBase",
                   "the destructor must call beginTeardown() first");
        endRun();
    }

    void setCoordinated(bool coordinated) override { m_coordinated = coordinated; }
    bool isCoordinated() const { return m_coordinated; }

    QFuture<void> currentRun() const override
    {
        return m_run.has_value() ? m_run->future() : QtFuture::makeReadyVoidFuture();
    }

protected:
    //Recompute immediately when standalone; once a coordinator drives this
    //pipeline it owns the run decision. Call after marking dirty + emitting
    //updateStateChanged().
    void runIfStandalone() { if(!m_coordinated) { run(); } }

    //True between beginRun() and endRun(): the Working bit.
    bool isRunning() const { return m_run.has_value(); }

    //Enters a run and returns the future to hand back from doRun(). While a run
    //is in flight it returns that run's future, so a restart coalescing into work
    //already going keeps the same handle.
    QFuture<void> beginRun()
    {
        if(!m_run.has_value()) {
            m_run.emplace();
            m_run->start();
        }
        return m_run->future();
    }

    //Leaves the run, finishing the future beginRun() handed out. A no-op when
    //nothing is running, so every completion path can call it unconditionally.
    //
    //Call it from the pipeline's own thread: the promise's interior is
    //thread-safe, but the optional holding it is not. Note the destructor path
    //reports the run *finished*, not canceled, so a driver waiting on it takes
    //its success branch — safe because that branch is delivered through the event
    //loop, by which time the pipeline has announced its teardown and the driver
    //reads it as Clean, and by which time ~QObject has emitted destroyed() and the
    //driver has dropped the pipeline outright. The second guarantee is Qt's and
    //holds even for an implementer that forgets beginTeardown().
    void endRun()
    {
        if(!m_run.has_value()) {
            return;
        }
        //Moved out first so anything woken by finish() sees the pipeline out of
        //its run rather than still in it.
        QPromise<void> run = std::move(*m_run);
        m_run.reset();
        run.finish();
    }

private:
    bool m_coordinated = false;
    std::optional<QPromise<void>> m_run;
};

#endif // CWUPDATABLE_H
