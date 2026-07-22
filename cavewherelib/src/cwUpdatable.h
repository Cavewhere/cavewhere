#ifndef CWUPDATABLE_H
#define CWUPDATABLE_H

/**
    Contract for a derived-data pipeline (line plot, scraps, ...) that can fall
    behind its source data and be brought current on demand.

    Implementers are pure mechanism: a source edit marks the pipeline dirty and
    emits updateStateChanged(); they never decide whether to recompute. That
    policy — run immediately vs. accumulate until asked — belongs to
    cwUpdateCoordinator, which owns the automatic-update flag and drives
    update() when appropriate.

    update() recomputes unconditionally, with no automatic-update gate, so a
    "Solve" / "Compute Scraps" style action always forces the work.

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
        Dirty (not Working) so the coordinator drives update() again and the
        pipeline's restarter coalesces the fresh edit into the in-flight run.
        Only a pipeline that clears its dirty marker synchronously at the top of
        update() (the line plot) is ever observably Working; a pipeline that
        holds its dirty set until the async task completes (scraps, LiDAR)
        reports Dirty for the whole run and Clean when it finishes. The
        coordinator's forced-cascade settle logic handles both — a Working keeps
        the cascade open for the line plot, a persistent Dirty keeps it open for
        the others.

        There is no default: every implementer must state its value explicitly,
        so none can silently clear its dirty marker yet forget it is still busy
        (the failure the old isUpdating()==false default invited).
    */
    enum class State { Clean, Dirty, Working };

    virtual ~cwUpdatable() = default;

    //The pipeline's current state (see State) — the one thing the coordinator reads.
    virtual State updateState() const = 0;

    //Recompute now, unconditionally.
    virtual void update() = 0;

    //Called by cwUpdateCoordinator when it takes over driving this pipeline.
    //Until coordinated, an implementer may recompute eagerly on a source edit
    //(the sensible standalone default); once coordinated, it only marks dirty
    //and leaves the run decision to the coordinator. This is a driver handoff,
    //not the automatic-update policy — that lives entirely in the coordinator.
    virtual void setCoordinated(bool coordinated) = 0;
};

/**
    Common mechanism for the eager-until-coordinated handoff.

    Holds the coordinated flag and the shared "recompute now unless a coordinator
    is driving" idiom, so each cwUpdatable implementer (line plot, scraps, LiDAR)
    doesn't repeat it. Implementers still provide updateState()/update() and,
    being QObjects, declare their own void updateStateChanged() signal.
*/
class cwUpdatableBase : public cwUpdatable
{
public:
    void setCoordinated(bool coordinated) override { m_coordinated = coordinated; }
    bool isCoordinated() const { return m_coordinated; }

protected:
    //Recompute immediately when standalone; once a coordinator drives this
    //pipeline it owns the run decision. Call after marking dirty + emitting
    //updateStateChanged().
    void runIfStandalone() { if(!m_coordinated) { update(); } }

private:
    bool m_coordinated = false;
};

#endif // CWUPDATABLE_H
