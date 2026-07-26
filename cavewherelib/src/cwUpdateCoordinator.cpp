//Our includes
#include "cwUpdateCoordinator.h"
#include "cwJobSettings.h"

//Async includes
#include "asyncfuture.h"

//Std includes
#include <algorithm>

//Qt includes
#include <QLoggingCategory>

// Traces the cascade's ordering and per-pipeline drives. Off by default; enable
// with QT_LOGGING_RULES="cw.updateCascade.debug=true". Debug builds only —
// release configs define QT_NO_DEBUG_OUTPUT, which compiles the traces out.
Q_LOGGING_CATEGORY(lcUpdateCascade, "cw.updateCascade", QtInfoMsg)

namespace {
    QString pipelineName(const cwUpdatable* pipeline)
    {
        const QObject* object = dynamic_cast<const QObject*>(pipeline);
        return object != nullptr ? QString::fromLatin1(object->metaObject()->className())
                                 : QStringLiteral("cwUpdatable");
    }

    //Deferred::complete(QFuture) always installs a watcher, so a result that is
    //already finished — the common case, since most nodes end up with nothing
    //left to do — would otherwise cost an event-loop turn per link in the chain.
    void completeWith(AsyncFuture::Deferred<void>& node, const QFuture<void>& next)
    {
        if(next.isFinished()) {
            node.complete();
        } else {
            node.complete(next);
        }
    }

    //Waits for a set of nodes, short-circuiting when they are all already done: a
    //combinator delivers even an all-finished set through a watcher, so a cascade
    //with nothing to do would otherwise cost an event-loop turn per join.
    QFuture<void> joinNodes(const QList<QFuture<void>>& nodes)
    {
        const bool settled = std::all_of(nodes.begin(), nodes.end(),
                                         [](const QFuture<void>& node) { return node.isFinished(); });
        if(settled) {
            return QtFuture::makeReadyVoidFuture();
        }

        auto join = AsyncFuture::combine(AsyncFuture::AllSettled);
        join << nodes;
        return join.future();
    }
}

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
    //Every continuation that reads a pipeline is bound to this as its context
    //object, so they are torn down with it. Bumping the generation as well keeps
    //anything already queued from touching a half-destroyed coordinator.
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
    //Pressing Run twice supersedes the first cascade rather than racing a second
    //one alongside it: the older generation's continuations still fire, but every
    //one of them returns without touching a pipeline.
    m_cascadeGeneration++;
    m_cascadeRunning = true;

    const quint64 generation = m_cascadeGeneration;
    const QFuture<void> cascade = startCascade(generation);

    //A cascade that found nothing to do has to end here rather than an
    //event-loop turn later. Otherwise it is still "running" when an edit made
    //immediately afterwards arrives, and onChildStateChanged defers to it — so
    //turning automatic update on and then editing would leave the edit unrun
    //until something else started a cascade.
    if(cascade.isFinished()) {
        finishCascade(generation);
        return;
    }

    //Both handlers, like every other join here: combine(AllSettled) cancels its
    //output when any node settled canceled, and a cancel that reached only the
    //completion handler would leave m_cascadeRunning latched true — which stops
    //onChildStateChanged from ever driving a pipeline again.
    const auto onCascadeSettled = [this, generation]() { finishCascade(generation); };
    AsyncFuture::observe(cascade).context(this, onCascadeSettled, onCascadeSettled);
}

void cwUpdateCoordinator::finishCascade(quint64 generation)
{
    if(generation != m_cascadeGeneration) {
        //Superseded by a newer cascade, which owns the running state now.
        return;
    }

    m_cascadeRunning = false;
    refreshNeedsUpdate();
    //Queued so the next cascade starts from a clean stack rather than from
    //inside the finishing one's own continuation.
    QMetaObject::invokeMethod(this, &cwUpdateCoordinator::flushAfterCascade,
                              Qt::QueuedConnection);
}

void cwUpdateCoordinator::onChildStateChanged(cwUpdatable* updatable)
{
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

QFuture<void> cwUpdateCoordinator::startCascade(quint64 generation)
{
    if(m_updatables.isEmpty()) {
        //A combinator with nothing added never finishes, so the empty cascade is
        //an already-finished future rather than one.
        return QtFuture::makeReadyVoidFuture();
    }

    QHash<cwUpdatable*, QFuture<void>> built;
    QSet<cwUpdatable*> building;

    //Waiting on every node, not just the leaves: a pipeline nothing depends on is
    //still part of the cascade, and a node that finishes early is already ready.
    //
    //Over a snapshot, not the member: building a node reaches run(), and a
    //pipeline destroyed from inside one would remove itself from m_updatables
    //while this loop is walking it.
    const QList<cwUpdatable*> pipelines = m_updatables;
    QList<QFuture<void>> nodes;
    for(cwUpdatable* pipeline : pipelines) {
        nodes.append(cascadeNode(pipeline, generation, built, building));
    }
    return joinNodes(nodes);
}

QFuture<void> cwUpdateCoordinator::cascadeNode(cwUpdatable* pipeline,
                                               quint64 generation,
                                               QHash<cwUpdatable*, QFuture<void>>& built,
                                               QSet<cwUpdatable*>& building)
{
    const auto existing = built.constFind(pipeline);
    if(existing != built.constEnd()) {
        return existing.value();
    }

    if(building.contains(pipeline)) {
        //Only reachable if the registered edges contain a cycle. Treat the edge
        //that closes it as already satisfied: the cascade still runs every
        //pipeline, but the ordering is meaningless for the ones in the cycle.
        qCWarning(lcUpdateCascade)
            << "Dependency cycle registered for" << pipelineName(pipeline)
            << "- cascade ordering is undefined for the pipelines involved.";
        return QtFuture::makeReadyVoidFuture();
    }

    building.insert(pipeline);

    const QList<cwUpdatable*> parents = m_dependencies.value(pipeline);

    QList<QFuture<void>> dependencyNodes;
    for(cwUpdatable* parent : parents) {
        dependencyNodes.append(cascadeNode(parent, generation, built, building));
    }
    const QFuture<void> dependencies = joinNodes(dependencyNodes);

    //Nothing left to wait for — a root, or a dependency that had nothing to do —
    //so the state is read now. Otherwise it is read once the dependencies are
    //done, which is what lets a pipeline they just dirtied be seen as Dirty.
    QFuture<void> node;
    if(dependencies.isFinished()) {
        node = driveToClean(pipeline, generation);
    } else {
        auto deferred = AsyncFuture::deferred<void>();
        const auto onDependenciesSettled = [this, pipeline, generation, deferred]() mutable {
            completeWith(deferred, driveToClean(pipeline, generation));
        };
        //Both paths run this pipeline: one dependency failing must neither cancel
        //its siblings nor abandon what is below it, so a canceled dependency is
        //settled, not fatal.
        AsyncFuture::observe(dependencies)
            .context(this, onDependenciesSettled, onDependenciesSettled);
        node = deferred.future();
    }

    building.remove(pipeline);
    built.insert(pipeline, node);
    return node;
}

QFuture<void> cwUpdateCoordinator::driveToClean(cwUpdatable* pipeline, quint64 generation)
{
    if(generation != m_cascadeGeneration) {
        //A newer cascade superseded this one, or a pipeline was destroyed and
        //the whole cascade abandoned. Either way this node is no longer anyone's.
        return QtFuture::makeReadyVoidFuture();
    }

    switch(pipeline->updateState()) {
    case cwUpdatable::State::Clean:
        qCDebug(lcUpdateCascade) << "skip" << pipelineName(pipeline);
        return QtFuture::makeReadyVoidFuture();
    case cwUpdatable::State::Working:
        //A run in flight already covers the current data. Waiting on it, rather
        //than forcing another, is what keeps an unrelated cascade from canceling
        //work that is already doing this node's job.
        qCDebug(lcUpdateCascade) << "wait" << pipelineName(pipeline);
        return waitForRun(pipeline, pipeline->currentRun(), generation);
    case cwUpdatable::State::Dirty:
        qCDebug(lcUpdateCascade) << "run" << pipelineName(pipeline);
        return waitForRun(pipeline, pipeline->run(), generation);
    }

    Q_UNREACHABLE_RETURN(QtFuture::makeReadyVoidFuture());
}

QFuture<void> cwUpdateCoordinator::waitForRun(cwUpdatable* pipeline,
                                              const QFuture<void>& run,
                                              quint64 generation)
{
    //An explicit deferred rather than a plain continuation, so that a canceled
    //run finishes this node instead of canceling it: one pipeline failing must
    //neither cancel its siblings nor abandon what is below it.
    auto node = AsyncFuture::deferred<void>();

    //shield() is what makes a run safe to share. A pipeline's run future is
    //handed to every node waiting on it, and observe().context() pushes a cancel
    //back up the chain it observed, so without the shield one node giving up
    //would cancel the run itself — and with it the work every other waiter, and
    //the pipeline, is relying on.
    AsyncFuture::observe(AsyncFuture::shield(run)).context(this,
        [this, pipeline, generation, node]() mutable {
            //A run can finish with the pipeline dirty again, because the source
            //was edited while it ran. Reading the state again runs it once more so
            //the nodes below see current data. The future's completion arrives
            //through the event loop, so this iterates rather than recursing, and
            //it ends as soon as the edits do — provided a pipeline that reports
            //Dirty actually starts work when run(). One that returned an
            //already-finished future while still calling itself Dirty would spin.
            completeWith(node, driveToClean(pipeline, generation));
        },
        [node]() mutable {
            //Settle rather than cancel, so a run that never reports doesn't take
            //the nodes below it down; what is still dirty stays in the staleness
            //aggregate. Note the shield in front absorbs a canceled run and
            //delivers it as a completion, so nothing reaches this today — it is
            //here for the case where the observed future is canceled directly.
            node.complete();
        });

    return node.future();
}

void cwUpdateCoordinator::dropRunningCascade()
{
    //Abandon by generation, not by canceling: a canceled continuation still runs,
    //and it is exactly the one that would read the pipeline this cascade is being
    //dropped because of. The generation check in driveToClean() is what makes it
    //a no-op instead. Whatever is still dirty is reported by needsUpdate() and
    //picked up by the normal policy.
    m_cascadeGeneration++;
    m_cascadeRunning = false;
}

void cwUpdateCoordinator::flushAfterCascade()
{
    //Only the automatic path chases leftovers. A forced Run resolves the cascade
    //it started — including everything that run dirties downstream, which the
    //dependency edges already order — but not edits the user made after it.
    if(cascadeRunning() || !automaticUpdate() || !needsUpdate()) {
        return;
    }
    updateNow();
}
