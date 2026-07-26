#ifndef CWUPDATECOORDINATOR_H
#define CWUPDATECOORDINATOR_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QList>
#include <QHash>

//Std includes
#include <memory>

//Our includes
#include "cwGlobals.h"
#include "cwUpdatable.h"

namespace QtTaskTree {
    class Group;
    class QSingleTaskTreeRunner;
}

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
            //A running cascade may hold a node driving this pipeline, so drop the
            //tree before the pointer goes stale.
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

    //Re-emits a registered pipeline's updateStateChanged with the pipeline that
    //changed. cwUpdatable isn't a QObject, so this is how a cwUpdatableTask
    //observes the one pipeline it drives.
    void pipelineStateChanged(cwUpdatable* pipeline);

private:
    void onChildStateChanged(cwUpdatable* updatable);
    void onAutomaticUpdateChanged();
    void refreshNeedsUpdate();

    //Layers the registered pipelines by dependency depth: everything at one depth
    //runs in parallel, and the next depth starts only once the previous finished.
    QtTaskTree::Group buildCascadeRecipe();
    bool taskTreeRunning() const;

    //Abandons a running cascade. Out of line because m_taskTreeRunner's type is
    //only forward declared here.
    void dropRunningCascade();

    //Re-applies the update policy once a cascade has finished. A recipe is fixed
    //for its run, so a pipeline dirtied after its own node already reported done
    //was skipped while the tree owned the run decision; this picks it up.
    void flushAfterCascade();

    std::unique_ptr<QtTaskTree::QSingleTaskTreeRunner> m_taskTreeRunner;

    QList<cwUpdatable*> m_updatables;
    //Pipeline -> the pipelines whose output it consumes. Absent or empty is a root.
    QHash<cwUpdatable*, QList<cwUpdatable*>> m_dependencies;
    bool m_lastNeedsUpdate = false;
};

#endif // CWUPDATECOORDINATOR_H
