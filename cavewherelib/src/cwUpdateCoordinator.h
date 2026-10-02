#ifndef CWUPDATECOORDINATOR_H
#define CWUPDATECOORDINATOR_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QList>
#include <QHash>

//Our includes
#include "cwGlobals.h"
#include "cwUpdatable.h"

class cwUpdateCascade;

/**
    Single owner of the derived-data update policy and the aggregates the UI
    binds to.

    Registered pipelines (cwUpdatable) report when they go dirty; the
    coordinator decides what happens: with automaticUpdate on, a dirty pipeline
    recomputes immediately; with it off, staleness accumulates and needsUpdate
    becomes true, which the UI surfaces as "update needed". updateNow()
    recomputes every dirty pipeline regardless of the flag (the footer / Solve /
    Compute-Scraps "Run" path).

    Two aggregates are published, and neither implies the other: needsUpdate is
    "something is stale", running is "something is being recomputed". A footer
    showing one of idle / update-needed / running reads running first, since a
    pipeline re-edited mid-run is both.

    Policy is all this class is. Deciding *when* to recompute lives here; working
    out what to recompute in what order is a cwUpdateCascade, which updateNow()
    builds from the registered pipelines and their edges and then follows.

    The automaticUpdate flag is persisted by cwJobSettings (same QSettings key,
    so existing preferences carry over); the coordinator is the API surface QML
    binds to and mirrors that setting, flushing pending work when it turns on.
*/
class CAVEWHERE_LIB_EXPORT cwUpdateCoordinator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UpdateCoordinator)
    QML_UNCREATABLE("UpdateCoordinator is owned by cwRootData and can't be created directly")

    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)
    Q_PROPERTY(bool needsUpdate READ needsUpdate NOTIFY needsUpdateChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)

public:
    explicit cwUpdateCoordinator(QObject* parent = nullptr);
    ~cwUpdateCoordinator();

    //Registers a pipeline and connects its updateStateChanged signal. Flushes
    //immediately if it is already dirty and automatic update is on. T must be a
    //QObject deriving cwUpdatable that declares a void updateStateChanged() signal.
    //
    //dependsOn lists the pipelines whose output this one consumes (scraps consume
    //the line plot's station positions). Those edges order each cwUpdateCascade a
    //Run starts: a dependent only runs once every pipeline it depends on is
    //finished, so the "solve dirties scraps" handoff is a declared edge rather
    //than something the run-until-settled latch rediscovers at runtime.
    template<class T>
    void add(T* updatable, QList<cwUpdatable*> dependsOn = {})
    {
        cwUpdatable* u = updatable;
        u->setCoordinated(true);
        m_updatables.append(u);
        if(!dependsOn.isEmpty()) {
            m_dependencies.insert(u, dependsOn);
        }
        connect(updatable, &T::updateStateChanged, this, [this, u]() { onChildStateChanged(u); });
        //Drop the raw pointer if the pipeline is destroyed first (the handler only
        //compares the pointer value, never dereferences it).
        connect(updatable, &QObject::destroyed, this, [this, u]() {
            //A running cascade may hold a node driving this pipeline, so abandon
            //it before the pointer goes stale.
            dropRunningCascade();
            m_updatables.removeAll(u);
            m_dependencies.remove(u);
            //Also drop it from every other pipeline's dependency list, so a
            //survivor isn't left ordered behind a pipeline that no longer exists.
            for(auto it = m_dependencies.begin(); it != m_dependencies.end(); ++it) {
                it.value().removeAll(u);
            }
            refreshAggregates();
            //Dropping the pass gave up on every *other* pipeline's ordered work
            //too, and unlike a supersede nothing is being built to replace it. So
            //re-apply policy to the survivors rather than leaving them for whatever
            //edit happens to arrive next. Queued, both so the fresh pass starts
            //from a clean stack rather than from inside a destructor, and so it is
            //dropped if the coordinator is going down too.
            QMetaObject::invokeMethod(this, &cwUpdateCoordinator::flushAfterCascade,
                                      Qt::QueuedConnection);
        });
        onChildStateChanged(u);
    }

    bool automaticUpdate() const;
    bool needsUpdate() const;

    //True while derived data is being recomputed, whichever path started it.
    bool running() const;

    /**
        Announces that the application is going down: no further derived-data
        work is started from here on, and the cascade in flight is abandoned.

        Armed by both of cwRootData's exit paths: shutdown(), which drains tasks
        and futures asynchronously behind the shutdown screen, and
        shutdownBlocking(), which waits on the task manager, the future manager
        and the save flush from ~cwRootData. A task or a save completing in either
        window can dirty a pipeline, and starting a fresh solve while the app is
        being dismantled is work whose results nothing will read. Idempotent, so
        both paths can call it.

        Mostly policy: a pipeline that is being destroyed is already inert
        (cwUpdatable::beginTeardown()), and this stops the coordinator from
        beginning work in the window before that. It does more than that during
        the drain, though, so don't read it as decoration — a pipeline that is
        still alive keeps answering updateState(), and answering means walking a
        dirty set whose scraps and notes belong to a region already being
        dismantled. Latching here stops those walks for the rest of the exit.
    */
    void beginShutdown();

public slots:
    void setAutomaticUpdate(bool automaticUpdate);

    //Recompute every dirty pipeline now, ignoring the automatic-update flag.
    void updateNow();

    /**
        Bring one pipeline up to date now, along with everything that consumes it,
        and leave the rest of the registry for the ordinary policy.

        This is the second half of a "Solve" / "Compute Scraps" button; the first
        half is the caller marking the pipeline dirty (cwLinePlotManager::
        markNeedsUpdate(), cwScrapManager::markAllScrapsDirty()), which is what
        makes the button mean "recompute regardless". The two are separate on
        purpose: the manager owns what dirty means, the coordinator owns when and
        in what order, and a manager that had to reach for a coordinator to run its
        own button would have that backwards.

        Marking without this leaves the dependents holding output the run has just
        made stale — with automatic update off, solving used to dirty the scraps
        and nothing recomputed them.

        pipeline must be a registered cwUpdatable; anything else is ignored with a
        warning, since the coordinator can only order what it has edges for. Takes
        a QObject* rather than a cwUpdatable*, which is not a QObject and so cannot
        cross into QML.
    */
    void updateNow(QObject* pipeline);

signals:
    void automaticUpdateChanged();
    void needsUpdateChanged();
    void runningChanged();

private:
    void onChildStateChanged(cwUpdatable* updatable);
    void onAutomaticUpdateChanged();

    //Call wherever a pipeline's state can have changed — neither aggregate is a
    //stored value.
    void refreshAggregates();

    bool anyPipelineIs(cwUpdatable::State state) const;

    //Takes over a freshly built pass: supersedes whatever was running, adopts it,
    //and arranges for finishCascade() once it settles. Takes ownership.
    void startCascade(cwUpdateCascade* cascade);
    void finishCascade();

    bool cascadeRunning() const { return m_cascadeRunning; }
    void dropRunningCascade();

    //Re-applies the update policy once a cascade has finished. The DAG is fixed
    //for its run, so a pipeline dirtied after its own node already finished was
    //skipped while the cascade owned the run decision; this picks it up.
    void flushAfterCascade();

    QList<cwUpdatable*> m_updatables;
    //Pipeline -> the pipelines whose output it consumes. Absent or empty is a root.
    QHash<cwUpdatable*, QList<cwUpdatable*>> m_dependencies;
    bool m_lastNeedsUpdate = false;
    bool m_lastRunning = false;

    //The pass in flight, parented here so it goes away with the coordinator. A
    //cascade is superseded by dropping it, and only ever one at a time: the
    //replacement is built after the incumbent has been given up on, so nothing
    //below is driven by two passes at once.
    QPointer<cwUpdateCascade> m_cascade;
    //Separate from m_cascade being set, because a finished pass is kept until a
    //Run replaces it — the coordinator has nothing to gain from deleting it
    //promptly, and doing so would mean destroying it from inside its own
    //completion handler.
    //
    //An input to running(), but writing it deliberately does not announce: a pass
    //really in flight always has a pipeline that reported its own Dirty -> Working
    //transition, and one that turns out to have nothing to do ends in
    //finishCascade(), which does refresh. Announcing at the set would flash
    //running true and back on a Run that found nothing. The one write left silent
    //on purpose is dropRunningCascade() during beginShutdown(): re-reading the
    //aggregates means asking every pipeline for its state, which is the walk
    //beginShutdown() exists to stop.
    bool m_cascadeRunning = false;
    bool m_shuttingDown = false;
};

#endif // CWUPDATECOORDINATOR_H
