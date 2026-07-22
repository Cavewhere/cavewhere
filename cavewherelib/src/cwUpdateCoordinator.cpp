//Our includes
#include "cwUpdateCoordinator.h"
#include "cwJobSettings.h"

//Std includes
#include <algorithm>

cwUpdateCoordinator::cwUpdateCoordinator(QObject* parent) :
    QObject(parent)
{
    cwJobSettings::initialize();
    m_lastNeedsUpdate = needsUpdate();

    connect(cwJobSettings::instance(), &cwJobSettings::automaticUpdateChanged,
            this, &cwUpdateCoordinator::onAutomaticUpdateChanged);
}

bool cwUpdateCoordinator::automaticUpdate() const
{
    return cwJobSettings::instance()->automaticUpdate();
}

void cwUpdateCoordinator::setAutomaticUpdate(bool automaticUpdate)
{
    //Persistence lives in cwJobSettings; its automaticUpdateChanged signal
    //drives onAutomaticUpdateChanged (re-emit + flush).
    cwJobSettings::instance()->setAutomaticUpdate(automaticUpdate);
}

bool cwUpdateCoordinator::needsUpdate() const
{
    return std::any_of(m_updatables.begin(), m_updatables.end(),
                       [](const cwUpdatable* u) { return u->needsUpdate(); });
}

void cwUpdateCoordinator::updateNow()
{
    for(cwUpdatable* u : std::as_const(m_updatables)) {
        if(u->needsUpdate()) {
            u->update();
        }
    }
    refreshNeedsUpdate();
}

void cwUpdateCoordinator::onChildDirty(cwUpdatable* updatable)
{
    //Guard on needsUpdate() so update()'s own needsUpdateChanged (dirty
    //cleared) doesn't re-enter and re-run.
    if(automaticUpdate() && updatable->needsUpdate()) {
        updatable->update();
    }
    refreshNeedsUpdate();
}

void cwUpdateCoordinator::onAutomaticUpdateChanged()
{
    emit automaticUpdateChanged();
    if(automaticUpdate()) {
        updateNow();
    }
}

void cwUpdateCoordinator::refreshNeedsUpdate()
{
    const bool current = needsUpdate();
    if(current != m_lastNeedsUpdate) {
        m_lastNeedsUpdate = current;
        emit needsUpdateChanged();
    }
}
