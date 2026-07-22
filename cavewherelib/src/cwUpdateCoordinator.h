#ifndef CWUPDATECOORDINATOR_H
#define CWUPDATECOORDINATOR_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QList>

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

    //Registers a pipeline and connects its updateStateChanged signal. Flushes
    //immediately if it is already dirty and automatic update is on. T must be a
    //QObject deriving cwUpdatable that declares a void updateStateChanged() signal.
    template<class T>
    void add(T* updatable)
    {
        cwUpdatable* u = updatable;
        u->setCoordinated(true);
        m_updatables.append(u);
        connect(updatable, &T::updateStateChanged, this, [this, u]() { onChildStateChanged(u); });
        //Drop the raw pointer if the pipeline is destroyed first (the handler only
        //compares the pointer value, never dereferences it).
        connect(updatable, &QObject::destroyed, this, [this, u]() {
            m_updatables.removeAll(u);
            refreshNeedsUpdate();
            //A pipeline destroyed mid-cascade may have been the last thing
            //keeping the forced update open; re-settle so m_forcing can't stick.
            clearForcingIfSettled();
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

    //True while an updateNow() cascade is in flight. Keeps force-running dirty
    //pipelines (regardless of the automatic-update flag) until every pipeline is
    //clean and idle, so an async cascade — the line plot solve dirtying scraps
    //after updateNow() has already iterated — is driven to completion by the
    //one Run press instead of leaving the user to press Run again.
    bool anyWorking() const;
    void clearForcingIfSettled();

    QList<cwUpdatable*> m_updatables;
    bool m_lastNeedsUpdate = false;
    bool m_forcing = false;
};

#endif // CWUPDATECOORDINATOR_H
