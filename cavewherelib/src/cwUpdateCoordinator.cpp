//Our includes
#include "cwUpdateCoordinator.h"
#include "cwJobSettings.h"
#include "cwUpdatableTask.h"

//Std includes
#include <algorithm>

//Qt includes
#include <QLoggingCategory>
#include <QMap>
#include <QtTaskTree/qtasktree.h>
#include <QtTaskTree/qtasktreerunner.h>

// Traces the cascade's layer ordering and per-pipeline timings. Off by default.
// Enable with QT_LOGGING_RULES="cw.updateCascade.debug=true"
Q_LOGGING_CATEGORY(lcUpdateCascade, "cw.updateCascade", QtInfoMsg)

namespace {
    QString pipelineName(cwUpdatable* pipeline)
    {
        const QObject* object = dynamic_cast<const QObject*>(pipeline);
        return object != nullptr ? QString::fromLatin1(object->metaObject()->className())
                                 : QStringLiteral("cwUpdatable");
    }

    //Depth of a pipeline in the dependency graph; roots are 0. A pipeline sits one
    //below its *deepest* dependency, so it lands in a layer after every pipeline it
    //consumes — taking the first parent instead would let it run alongside a
    //dependency that is itself waiting on something. remainingHops bounds the walk
    //so a mis-registered cycle terminates instead of spinning. Re-walking shared
    //ancestors is fine at this scale (a handful of pipelines, once per cascade).
    int dependencyDepth(cwUpdatable* pipeline,
                        const QHash<cwUpdatable*, QList<cwUpdatable*>>& dependencies,
                        int remainingHops)
    {
        const auto it = dependencies.constFind(pipeline);
        if(it == dependencies.constEnd()) {
            return 0;
        }

        if(remainingHops <= 0) {
            //Only reachable if the registered edges contain a cycle: the bound is
            //the pipeline count, so an acyclic graph always terminates above this.
            //Say so — the walk still returns and the cascade still runs, but the
            //layering is meaningless for the pipelines in the cycle.
            qCWarning(lcUpdateCascade)
                << "Dependency cycle registered for" << pipelineName(pipeline)
                << "- cascade ordering is undefined for the pipelines involved.";
            return 0;
        }

        int deepest = 0;
        for(cwUpdatable* parent : it.value()) {
            deepest = std::max(deepest,
                               dependencyDepth(parent, dependencies, remainingHops - 1) + 1);
        }
        return deepest;
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
    //Drop the running tree, and with it the tasks holding pointers back into
    //this object, before the rest of the coordinator tears down.
    m_taskTreeRunner.reset();
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
    using namespace QtTaskTree;

    if(m_taskTreeRunner == nullptr) {
        m_taskTreeRunner = std::make_unique<QSingleTaskTreeRunner>();
    }

    //start() resets any running tree, so pressing Run twice supersedes the first
    //cascade rather than racing a second one alongside it.
    m_taskTreeRunner->start(buildCascadeRecipe(),
                            [] {},
                            [this](DoneWith) {
        refreshNeedsUpdate();
        //Queued so the next cascade starts from a clean stack rather than from
        //inside the finishing tree's own handler.
        QMetaObject::invokeMethod(this, &cwUpdateCoordinator::flushAfterCascade,
                                  Qt::QueuedConnection);
    });
}

void cwUpdateCoordinator::onChildStateChanged(cwUpdatable* updatable)
{
    //A running tree owns the run decision for every node in the cascade: each
    //node waits on its pipeline's run future and re-runs it if the run ends with
    //the pipeline dirty again, so driving it here would only duplicate that.
    //flushAfterCascade() picks up anything dirtied after its node had finished.
    if(taskTreeRunning()) {
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

QtTaskTree::Group cwUpdateCoordinator::buildCascadeRecipe()
{
    using namespace QtTaskTree;

    //Layer the pipelines by dependency depth. Everything at one depth is
    //independent and runs in parallel; the next depth starts only once the whole
    //previous layer finished, which is what orders line plot -> scraps/LiDAR.
    QMap<int, QList<cwUpdatable*>> byDepth;
    const int maxHops = static_cast<int>(m_updatables.size());
    for(cwUpdatable* u : std::as_const(m_updatables)) {
        byDepth[dependencyDepth(u, m_dependencies, maxHops)].append(u);
    }

    //finishAllAndSuccess: one pipeline failing must not cancel its siblings or
    //abandon the layers below it. Staleness, not the tree's result, is the
    //coordinator's report of what still needs work.
    GroupItems layers { sequential, finishAllAndSuccess };

    for(const QList<cwUpdatable*>& pipelines : std::as_const(byDepth)) {
        GroupItems layer { parallel, finishAllAndSuccess };

        for(cwUpdatable* pipeline : pipelines) {
            //Evaluated when the node starts, not when the recipe is built: a
            //pipeline the layer above has just dirtied is Dirty by then, and one
            //nothing dirtied is skipped.
            const auto onSetup = [this, pipeline](cwUpdatableTask& task) {
                if(pipeline->updateState() == cwUpdatable::State::Clean) {
                    return SetupResult::StopWithSuccess;
                }
                task.setPipeline(pipeline);
                return SetupResult::Continue;
            };

            //No timeout: a large cave's solve is legitimately slow, and a node
            //whose pipeline never finishes its run is a bug in that pipeline, not
            //something to time out. A cancelled run does end the node — see
            //cwUpdatableTask::waitFor().
            const QCustomTask<cwUpdatableTask> task(onSetup);

            //withLog() prints on every node start and finish with no category of
            //its own, so it is only attached when the trace is asked for.
            if(lcUpdateCascade().isDebugEnabled()) {
                layer << task.withLog(pipelineName(pipeline));
            } else {
                layer << task;
            }
        }

        layers << Group(layer);
    }

    return Group(layers);
}

bool cwUpdateCoordinator::taskTreeRunning() const
{
    return m_taskTreeRunner != nullptr && m_taskTreeRunner->isRunning();
}

void cwUpdateCoordinator::dropRunningCascade()
{
    //Reset, not cancel: the runner's destructor drops a running tree without
    //calling its done handler, so no flush is posted while a pipeline is
    //mid-destruction. The nodes cannot be told a pipeline is going away —
    //cwUpdatable isn't a QObject — so abandoning the cascade is what keeps a
    //later layer's setup handler from reading a freed pipeline. Whatever is still
    //dirty is reported by needsUpdate() and picked up by the normal policy.
    m_taskTreeRunner.reset();
}

void cwUpdateCoordinator::flushAfterCascade()
{
    //Only the automatic path chases leftovers. A forced Run resolves the cascade
    //it started — including everything that run dirties downstream, which the
    //dependency edges already order — but not edits the user made after it.
    if(taskTreeRunning() || !automaticUpdate() || !needsUpdate()) {
        return;
    }
    updateNow();
}
