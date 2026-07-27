//Our includes
#include "cwUpdateCascade.h"

//Async includes
#include "asyncfuture.h"

//Std includes
#include <algorithm>
#include <utility>

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

    //Which branch runIfNeeded() takes, for the trace.
    const char* driveVerb(cwUpdatable::State state)
    {
        switch(state) {
        case cwUpdatable::State::Clean:
            return "skip";
        case cwUpdatable::State::Working:
            return "wait";
        case cwUpdatable::State::Dirty:
            return "run";
        }
        Q_UNREACHABLE_RETURN("");
    }

    //root, plus every pipeline that transitively consumes it, in registration
    //order. Sweeps to a fixpoint rather than walking edges outward: dependencies is
    //indexed consumer -> consumed, and with a handful of pipelines that is cheaper
    //than building the reverse index.
    QList<cwUpdatable*> reachableFrom(cwUpdatable* root,
                                      const QList<cwUpdatable*>& pipelines,
                                      const QHash<cwUpdatable*, QList<cwUpdatable*>>& dependencies)
    {
        QSet<cwUpdatable*> reached { root };
        bool grew = true;
        while(grew) {
            grew = false;
            for(cwUpdatable* pipeline : pipelines) {
                if(reached.contains(pipeline)) {
                    continue;
                }
                const QList<cwUpdatable*> parents = dependencies.value(pipeline);
                const bool consumesReached =
                    std::any_of(parents.begin(), parents.end(),
                                [&reached](cwUpdatable* parent) { return reached.contains(parent); });
                if(consumesReached) {
                    reached.insert(pipeline);
                    grew = true;
                }
            }
        }

        QList<cwUpdatable*> scoped;
        scoped.reserve(reached.size());
        for(cwUpdatable* pipeline : pipelines) {
            if(reached.contains(pipeline)) {
                scoped.append(pipeline);
            }
        }
        return scoped;
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
    //with nothing to do would otherwise cost an event-loop turn per join. The
    //empty set takes the same path out of necessity rather than economy — a
    //combinator with nothing added never finishes at all, and both a pass over no
    //pipelines and a root's empty dependency list land here.
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

cwUpdateCascade::cwUpdateCascade(QList<cwUpdatable*> pipelines,
                                 QHash<cwUpdatable*, QList<cwUpdatable*>> dependencies,
                                 QObject* parent) :
    QObject(parent),
    m_pipelines(std::move(pipelines)),
    m_dependencies(std::move(dependencies))
{
}

cwUpdateCascade::cwUpdateCascade(cwUpdatable* forcedRoot,
                                 const QList<cwUpdatable*>& pipelines,
                                 const QHash<cwUpdatable*, QList<cwUpdatable*>>& dependencies,
                                 QObject* parent) :
    QObject(parent),
    m_pipelines(reachableFrom(forcedRoot, pipelines, dependencies)),
    //The whole edge set, not just the scoped pipelines': a node still builds its
    //dependencies, and narrowing here would leave a forced pipeline ordered ahead
    //of nothing.
    m_dependencies(dependencies),
    m_forcedRoot(forcedRoot)
{
}

QFuture<void> cwUpdateCascade::start()
{
    QHash<cwUpdatable*, QFuture<void>> built;
    QSet<cwUpdatable*> building;

    //Waiting on every node, not just the leaves: a pipeline nothing depends on is
    //still part of the pass, and a node that finishes early is already ready.
    QList<QFuture<void>> nodes;
    nodes.reserve(m_pipelines.size());
    for(cwUpdatable* pipeline : m_pipelines) {
        nodes.append(buildNode(pipeline, built, building));
    }
    return joinNodes(nodes);
}

QFuture<void> cwUpdateCascade::buildNode(cwUpdatable* pipeline,
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
    dependencyNodes.reserve(parents.size());
    for(cwUpdatable* parent : parents) {
        dependencyNodes.append(buildNode(parent, built, building));
    }
    const QFuture<void> dependencies = joinNodes(dependencyNodes);

    //Nothing left to wait for — a root, or a dependency that had nothing to do —
    //so the state is read now. Otherwise it is read once the dependencies are
    //done, which is what lets a pipeline they just dirtied be seen as Dirty.
    //buildNode() runs once per pipeline, so passing the force down from here is
    //what makes it a single act rather than a property of the node: driveToClean()
    //is re-entered once a run ends, and a force that renewed itself there would
    //restart the pipeline for as long as its runs kept succeeding.
    const bool force = (pipeline == m_forcedRoot);

    QFuture<void> node;
    if(dependencies.isFinished()) {
        node = driveToClean(pipeline, force);
    } else {
        auto deferred = AsyncFuture::deferred<void>();
        const auto onDependenciesSettled = [this, pipeline, force, deferred]() mutable {
            completeWith(deferred, driveToClean(pipeline, force));
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

QFuture<void> cwUpdateCascade::driveToClean(cwUpdatable* pipeline, bool force)
{
    if(m_abandoned) {
        //Given up on: a newer pass owns these pipelines now, or one of them is on
        //its way out. Placed before any dereference, which is what makes this
        //memory safety rather than tidiness.
        return QtFuture::makeReadyVoidFuture();
    }

    qCDebug(lcUpdateCascade) << (force ? "force" : driveVerb(pipeline->updateState()))
                             << pipelineName(pipeline);

    //runIfNeeded() is the Clean-skip / Working-attach / Dirty-run reading, and the
    //pipeline owns it — a cascade is not the only thing that has to drive one
    //correctly. The forced root is the one node that ignores it: its button means
    //"recompute" whether or not anything has changed since the last time.
    const QFuture<void> run = force ? pipeline->run() : pipeline->runIfNeeded();

    if(run.isFinished()) {
        //Nothing in flight — Clean, or a pipeline tearing down. Answering
        //synchronously matters as much here as in joinNodes(): a pass with nothing
        //to do has to end before it returns.
        return QtFuture::makeReadyVoidFuture();
    }

    return waitForRun(pipeline, run);
}

QFuture<void> cwUpdateCascade::waitForRun(cwUpdatable* pipeline, const QFuture<void>& run)
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
        [this, pipeline, node]() mutable {
            //A run can finish with the pipeline dirty again, because the source
            //was edited while it ran. Reading the state again runs it once more so
            //the nodes below see current data. The future's completion arrives
            //through the event loop, so this iterates rather than recursing, and
            //it ends as soon as the edits do — provided a pipeline that reports
            //Dirty actually starts work when run(). One that returned an
            //already-finished future while still calling itself Dirty would spin.
            completeWith(node, driveToClean(pipeline, false));
        },
        [pipeline, node]() mutable {
            //Settle rather than cancel, so a run that never reports doesn't take
            //the nodes below it down; what is still dirty stays in the staleness
            //aggregate. Note the shield in front absorbs a canceled run and
            //delivers it as a completion, so nothing reaches this today — it is
            //here for the case where the observed future is canceled directly.
            //
            //Debug-only tripwire, because "nothing reaches this today" is a
            //measured claim about shield() rather than a guarantee: if it stops
            //holding, a development build says so instead of the branch quietly
            //becoming live. Release still settles the node, so the graceful path
            //is what ships either way.
            Q_ASSERT_X(false, "cwUpdateCascade::waitForRun",
                       qPrintable(QStringLiteral("run future canceled directly for %1 — "
                                                 "the shield no longer absorbs it")
                                      .arg(pipelineName(pipeline))));
            node.complete();
        });

    return node.future();
}
