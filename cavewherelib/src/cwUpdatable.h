#ifndef CWUPDATABLE_H
#define CWUPDATABLE_H

/**
    Contract for a derived-data pipeline (line plot, scraps, ...) that can fall
    behind its source data and be brought current on demand.

    Implementers are pure mechanism: a source edit marks the pipeline dirty and
    emits needsUpdateChanged(); they never decide whether to recompute. That
    policy — run immediately vs. accumulate until asked — belongs to
    cwUpdateCoordinator, which owns the automatic-update flag and drives
    update() when appropriate.

    update() recomputes unconditionally, with no automatic-update gate, so a
    "Solve" / "Compute Scraps" style action always forces the work.

    The concrete QObject subclass also declares a  void needsUpdateChanged()
    signal; cwUpdateCoordinator::add() connects it via the concrete type.
*/
class cwUpdatable
{
public:
    virtual ~cwUpdatable() = default;

    //True when the pipeline has pending derived work.
    virtual bool needsUpdate() const = 0;

    //Recompute now, unconditionally.
    virtual void update() = 0;

    //True while an update() started here is still running asynchronously. The
    //coordinator holds a forced update (updateNow) open until every pipeline is
    //both clean and not updating, so a pipeline whose needsUpdate() clears at
    //the START of update() (the line plot) is not mistaken for finished while
    //its solve is still in flight. Pipelines that stay needsUpdate()==true until
    //their async work completes (scraps, LiDAR) leave this at the default false.
    virtual bool isUpdating() const { return false; }

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
    doesn't repeat it. Implementers still provide needsUpdate()/update() and,
    being QObjects, declare their own void needsUpdateChanged() signal.
*/
class cwUpdatableBase : public cwUpdatable
{
public:
    void setCoordinated(bool coordinated) override { m_coordinated = coordinated; }
    bool isCoordinated() const { return m_coordinated; }

protected:
    //Recompute immediately when standalone; once a coordinator drives this
    //pipeline it owns the run decision. Call after marking dirty + emitting
    //needsUpdateChanged().
    void runIfStandalone() { if(!m_coordinated) { update(); } }

private:
    bool m_coordinated = false;
};

#endif // CWUPDATABLE_H
