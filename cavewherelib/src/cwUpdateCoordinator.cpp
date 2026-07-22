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
                       [](const cwUpdatable* u) { return u->updateState() == cwUpdatable::State::Dirty; });
}

void cwUpdateCoordinator::updateNow()
{
    //Open a forced cascade: every dirty pipeline runs now, and any pipeline a
    //running solve dirties afterwards (line plot -> scraps) is driven too, so a
    //single Run resolves the whole chain without a second press.
    m_forcing = true;
    for(cwUpdatable* u : std::as_const(m_updatables)) {
        if(u->updateState() == cwUpdatable::State::Dirty) {
            u->update();
        }
    }
    refreshNeedsUpdate();
    clearForcingIfSettled();
}

void cwUpdateCoordinator::onChildStateChanged(cwUpdatable* updatable)
{
    //Guard on Dirty so update()'s own Working transition (dirty cleared) doesn't
    //re-enter and re-run. While a forced cascade is open we run the child
    //regardless of the automatic-update flag — that is what carries the
    //line-plot -> scraps handoff to completion under one Run.
    if((automaticUpdate() || m_forcing) && updatable->updateState() == cwUpdatable::State::Dirty) {
        updatable->update();
    }
    refreshNeedsUpdate();
    clearForcingIfSettled();
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

bool cwUpdateCoordinator::anyWorking() const
{
    return std::any_of(m_updatables.begin(), m_updatables.end(),
                       [](const cwUpdatable* u) { return u->updateState() == cwUpdatable::State::Working; });
}

void cwUpdateCoordinator::clearForcingIfSettled()
{
    //The cascade is done when nothing is dirty and no async solve is still in
    //flight. anyWorking() covers the line plot's window between its synchronous
    //dirty clear (at update() start) and its async completion — without it the
    //cascade would look settled mid-solve and drop the forced handoff to scraps.
    if(m_forcing && !needsUpdate() && !anyWorking()) {
        m_forcing = false;
    }
}
