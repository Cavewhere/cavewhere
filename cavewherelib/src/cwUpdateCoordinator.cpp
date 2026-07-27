//Our includes
#include "cwUpdateCoordinator.h"
#include "cwJobSettings.h"
#include "cwUpdateCascade.h"

//Async includes
#include "asyncfuture.h"

//Std includes
#include <algorithm>
#include <utility>

cwUpdateCoordinator::cwUpdateCoordinator(QObject* parent) :
    QObject(parent)
{
    cwJobSettings::initialize();
    m_lastNeedsUpdate = needsUpdate();

    connect(cwJobSettings::instance(), &cwJobSettings::automaticUpdateChanged,
            this, &cwUpdateCoordinator::onAutomaticUpdateChanged);
}

cwUpdateCoordinator::~cwUpdateCoordinator()
{
    //Every continuation that reads a pipeline is owned by the cascade that queued
    //it, and the cascade is a child of this, so ~QObject drops them. Giving up on
    //it here as well closes the window before that: this destructor is not the
    //last thing to run, and a continuation delivered in the meantime would reach a
    //half-destroyed coordinator.
    dropRunningCascade();

    //Hand each surviving pipeline back to its standalone eager default, so one
    //that outlives its coordinator isn't left marking itself dirty forever with
    //no driver to run it.
    //
    //Nothing survives today: cwRootData creates the managers as children of
    //Project and the coordinator after it, and QObject destroys children in
    //creation order, so this list is already empty here. That is an ownership
    //accident rather than a guarantee, and it is the whole reason a pipeline may
    //outlive a coordinator at all — so this stays as the answer for when it does.
    //
    //Not during shutdown, and this is the one case the pipeline's own teardown
    //guard cannot cover: eager means "recompute on the next source edit", and
    //the next edits during shutdown come from the survey data itself being
    //dismantled. Those objects aren't pipelines and have nothing to announce, so
    //a pipeline made eager here would solve against a region already coming
    //apart.
    if(!m_shuttingDown) {
        for(cwUpdatable* pipeline : std::as_const(m_updatables)) {
            pipeline->setCoordinated(false);
        }
    }
}

void cwUpdateCoordinator::beginShutdown()
{
    if(m_shuttingDown) {
        return;
    }
    m_shuttingDown = true;
    dropRunningCascade();
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
    if(m_shuttingDown) {
        return;
    }

    //Pressing Run twice supersedes the first cascade rather than racing a second
    //one alongside it. The incumbent is given up on before the replacement exists,
    //so no pipeline is ever driven by two passes at once.
    dropRunningCascade();

    auto* cascade = new cwUpdateCascade(m_updatables, m_dependencies, this);
    m_cascade = cascade;
    m_cascadeRunning = true;

    const QFuture<void> pass = cascade->start();

    //A cascade that found nothing to do has to end here rather than an
    //event-loop turn later. Otherwise it is still "running" when an edit made
    //immediately afterwards arrives, and onChildStateChanged defers to it — so
    //turning automatic update on and then editing would leave the edit unrun
    //until something else started a cascade.
    if(pass.isFinished()) {
        //Guarded exactly like the asynchronous branch below, and for the same
        //reason: a pass given up on during its own walk finishes immediately —
        //every node left short-circuits — and reporting that would clear the
        //running flag out from under the replacement that superseded it. Whoever
        //gave up on it re-applies policy itself, so nothing is stranded by
        //returning here.
        if(!cascade->isAbandoned()) {
            finishCascade();
        }
        return;
    }

    //Bound to the cascade rather than to this, so a pass that has been given up on
    //cannot report itself finished from under its replacement. The abandoned check
    //covers the window between the two: dropping a pass schedules its deletion but
    //cannot perform it, since a pass can be dropped from inside its own walk.
    //
    //Both handlers, like every other join here: combine(AllSettled) cancels its
    //output when any node settled canceled, and a cancel that reached only the
    //completion handler would leave m_cascadeRunning latched true — which stops
    //onChildStateChanged from ever driving a pipeline again.
    const auto onCascadeSettled = [this, cascade]() {
        if(!cascade->isAbandoned()) {
            finishCascade();
        }
    };
    AsyncFuture::observe(pass).context(cascade, onCascadeSettled, onCascadeSettled);
}

void cwUpdateCoordinator::finishCascade()
{
    m_cascadeRunning = false;
    refreshNeedsUpdate();
    //Queued so the next cascade starts from a clean stack rather than from
    //inside the finishing one's own continuation.
    QMetaObject::invokeMethod(this, &cwUpdateCoordinator::flushAfterCascade,
                              Qt::QueuedConnection);
}

void cwUpdateCoordinator::onChildStateChanged(cwUpdatable* updatable)
{
    if(m_shuttingDown) {
        return;
    }

    //A running cascade owns the run decision for every node in it: each node
    //waits on its pipeline's run future and re-runs it if the run ends with the
    //pipeline dirty again, so driving it here would only duplicate that.
    //flushAfterCascade() picks up anything dirtied after its node had finished.
    if(cascadeRunning()) {
        refreshNeedsUpdate();
        return;
    }

    //Guard on Dirty so run()'s own Working transition (dirty cleared) doesn't
    //re-enter and re-run.
    if(automaticUpdate() && updatable->updateState() == cwUpdatable::State::Dirty) {
        updatable->run();
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

void cwUpdateCoordinator::dropRunningCascade()
{
    if(!m_cascade.isNull()) {
        //Abandoned first, deleted after. A pass can be dropped from inside its own
        //synchronous walk — driving a pipeline can destroy one, which lands here —
        //so the object has to outlive this frame, and abandon() is what keeps it
        //inert for as long as it does. Deleting it is what drops the continuations
        //it has already queued, which is the half that matters: a canceled
        //continuation still runs its body, and it is exactly the one that would
        //read the pipeline this pass is being dropped because of.
        m_cascade->abandon();
        m_cascade->deleteLater();
        m_cascade = nullptr;
    }

    //Whatever is still dirty is reported by needsUpdate() and picked up by the
    //normal policy.
    m_cascadeRunning = false;
}

void cwUpdateCoordinator::flushAfterCascade()
{
    //Only the automatic path chases leftovers. A forced Run resolves the cascade
    //it started — including everything that run dirties downstream, which the
    //dependency edges already order — but not edits the user made after it.
    if(m_shuttingDown || cascadeRunning() || !automaticUpdate() || !needsUpdate()) {
        return;
    }
    updateNow();
}
