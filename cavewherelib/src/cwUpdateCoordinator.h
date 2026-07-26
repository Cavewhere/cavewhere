#ifndef CWUPDATECOORDINATOR_H
#define CWUPDATECOORDINATOR_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QList>
#include <QHash>
#include <QSet>

//Our includes
#include "cwGlobals.h"
#include "cwUpdatable.h"

/**
    Single owner of the derived-data update policy and staleness aggregate.

    Registered pipelines (cwUpdatable) report when they go dirty; the
    coordinator decides what happens: with automaticUpdate on, a dirty pipeline
    recomputes immediately; with it off, staleness accumulates and needsUpdate
    becomes true, which the UI surfaces as "update needed". updateNow()
    recomputes every dirty pipeline regardless of the flag (the footer / Solve /
    Compute-Scraps "Run" path).

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

public:
    explicit cwUpdateCoordinator(QObject* parent = nullptr);
    ~cwUpdateCoordinator();

    //Registers a pipeline and connects its updateStateChanged signal. Flushes
    //immediately if it is already dirty and automatic update is on. T must be a
    //QObject deriving cwUpdatable that declares a void updateStateChanged() signal.
    //
    //dependsOn lists the pipelines whose output this one consumes (scraps consume
    //the line plot's station positions). Those edges order the forced cascade: a
    //dependent only runs once every pipeline it depends on is finished, so the
    //"solve dirties scraps" handoff is a declared edge rather than something the
    //run-until-settled latch rediscovers at runtime.
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
            refreshNeedsUpdate();
        });
        onChildStateChanged(u);
    }

    bool automaticUpdate() const;
    bool needsUpdate() const;

public slots:
    void setAutomaticUpdate(bool automaticUpdate);

    //Recompute every dirty pipeline now, ignoring the automatic-update flag.
    void updateNow();

signals:
    void automaticUpdateChanged();
    void needsUpdateChanged();

private:
    void onChildStateChanged(cwUpdatable* updatable);
    void onAutomaticUpdateChanged();
    void refreshNeedsUpdate();

    //The cascade is a future DAG: one node per pipeline, waiting on the nodes of
    //the pipelines it consumes. The dependency edges *are* the ordering, so
    //nothing re-derives it — a pipeline waits for its own dependencies rather
    //than for every pipeline that happens to sit at the same depth.
    QFuture<void> startCascade(quint64 generation);
    void finishCascade(quint64 generation);

    //The node for one pipeline, memoized in built so a shared dependency is
    //waited on rather than driven twice. building carries the recursion's own
    //path, which is how a mis-registered cycle is caught.
    QFuture<void> cascadeNode(cwUpdatable* pipeline,
                              quint64 generation,
                              QHash<cwUpdatable*, QFuture<void>>& built,
                              QSet<cwUpdatable*>& building);

    //Reads the pipeline's state and returns a future that finishes once it has
    //nothing left to do. The state is read as late as the edges allow: at build
    //time only when the dependencies are already finished — which is to say they
    //had nothing to do and so dirtied nothing — and otherwise from inside their
    //continuation, so a pipeline they just dirtied is still seen as Dirty here.
    QFuture<void> driveToClean(cwUpdatable* pipeline, quint64 generation);
    QFuture<void> waitForRun(cwUpdatable* pipeline,
                             const QFuture<void>& run,
                             quint64 generation);

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

    //A cascade is superseded by bumping the generation, which makes every
    //continuation left over from the previous one a no-op. Canceling the cascade
    //would not do it: the queued continuation still runs, and it is the one that
    //would read a pipeline the cascade was dropped because of.
    quint64 m_cascadeGeneration = 0;
    bool m_cascadeRunning = false;
};

#endif // CWUPDATECOORDINATOR_H
