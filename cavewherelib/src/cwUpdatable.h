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

    //The pipeline's current state (see State) — the one thing the coordinator reads.
    virtual State updateState() const = 0;

    /**
        Recomputes now, unconditionally, and returns the future for that run.

        There is no automatic-update gate, so a "Solve" / "Compute Scraps" style
        action always forces the work — including on a pipeline that is already
        Working, which restarts it. A caller that wants to *wait* for the current
        run rather than force a new one wants currentRun() instead.

        The future finishes when the run is over, which is the same instant the
        pipeline stops reporting Working. A call with nothing to do returns an
        already-finished future, so callers never need a separate "did it start
        anything" question.

        Implementers owe one invariant: a pipeline reporting Dirty must start
        work here. A driver waits on the future and runs the pipeline again if
        it is still Dirty afterwards, so one that reported Dirty and handed back
        an already-finished future would be asked again immediately, forever.
    */
    virtual QFuture<void> run() = 0;

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
};

/**
    Common mechanism for the eager-until-coordinated handoff and for the run in
    flight.

    Holds the coordinated flag, the shared "recompute now unless a coordinator is
    driving" idiom, and the run's promise, so each cwUpdatable implementer (line
    plot, scraps, LiDAR) doesn't repeat them. Implementers still provide
    updateState()/run() and, being QObjects, declare their own void
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
    ~cwUpdatableBase() override { endRun(); }

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

    //Enters a run and returns the future to hand back from run(). While a run is
    //in flight it returns that run's future, so a restart coalescing into work
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
    //its success branch — safe only because that branch is delivered through the
    //event loop, by which time ~QObject has announced destroyed() and the driver
    //has dropped the pipeline.
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
