#ifndef CWUPDATECASCADE_H
#define CWUPDATECASCADE_H

//Qt includes
#include <QFuture>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>

//Our includes
#include "cwGlobals.h"
#include "cwUpdatable.h"

/**
    One pass over a set of derived-data pipelines, ordered by their dependency
    edges.

    The pass is a future DAG: one node per pipeline, waiting on the nodes of the
    pipelines it consumes. The edges *are* the ordering, so nothing re-derives it —
    a pipeline waits for its own dependencies rather than for every pipeline that
    happens to sit at the same depth. start() returns a future that finishes when
    every node has, which is when the pass is over.

    The object is the pass, and its lifetime is the epoch. The graph is a snapshot
    taken at construction, so nothing here depends on the caller's registry still
    looking the same afterwards, and every continuation the pass queues is bound to
    this object as its context — so destroying it drops them unrun. That is the
    whole supersede-and-abandon mechanism: pressing Run twice builds a second
    cascade and lets go of the first, rather than tagging continuations with an
    epoch and teaching each one to recognize its own.

    abandon() covers the window destruction cannot. start() drives pipelines
    synchronously, and a pipeline destroyed from inside a drive takes its
    coordinator through the same drop — so a pass can be given up on while its own
    walk is still on the stack, which is exactly when the object must not be
    deleted. An abandoned cascade drives nothing further, whether that would have
    come from the rest of the walk or from a continuation delivered by a nested
    event loop.

    There is no failure channel. A pipeline whose run is canceled, or that never
    reports, settles its node rather than failing it: one pipeline failing must
    neither cancel its siblings nor abandon what is below it. Whatever is still
    dirty when the pass ends is the report, and the caller's staleness aggregate is
    what carries it.
*/
class CAVEWHERE_LIB_EXPORT cwUpdateCascade : public QObject
{
public:
    cwUpdateCascade(QList<cwUpdatable*> pipelines,
                    QHash<cwUpdatable*, QList<cwUpdatable*>> dependencies,
                    QObject* parent = nullptr);

    /**
        A pass whose reason for existing is one pipeline: forcedRoot is run whether
        or not it is dirty, and the pass covers only what that reaches. This is the
        "Solve" / "Compute Scraps" button — forcing is what those already did; what
        they could not do is carry the result onward.

        Narrowed on the consuming side only. A pipeline forcedRoot itself depends
        on is still built as its dependency and driven the ordinary way, so
        forcing scraps against a line plot that has fallen behind solves the line
        plot first rather than triangulating onto stale stations.

        forcedRoot must be one of pipelines; anything else yields an empty pass.
    */
    cwUpdateCascade(cwUpdatable* forcedRoot,
                    const QList<cwUpdatable*>& pipelines,
                    const QHash<cwUpdatable*, QList<cwUpdatable*>>& dependencies,
                    QObject* parent = nullptr);

    /**
        Drives every pipeline in the snapshot, ordered by the edges, and returns
        the future for the pass. Call once.

        A pass with nothing to do hands back a future that is *already* finished,
        and callers depend on that being synchronous rather than an event-loop turn
        later: a driver that defers edits to a pass in flight would otherwise
        swallow an edit made in that turn.
    */
    QFuture<void> start();

    //Gives up the pass: nothing further is driven, and the future from start()
    //settles once whatever is already in flight does. Destroying the cascade does
    //this and drops the queued continuations with it; call this when the caller
    //cannot destroy it yet, and see the class docs for when that is.
    void abandon() { m_abandoned = true; }
    bool isAbandoned() const { return m_abandoned; }

private:
    //The node for one pipeline, memoized in built so a shared dependency is waited
    //on rather than driven twice. building carries the recursion's own path, which
    //is how a mis-registered cycle is caught.
    QFuture<void> buildNode(cwUpdatable* pipeline,
                            QHash<cwUpdatable*, QFuture<void>>& built,
                            QSet<cwUpdatable*>& building);

    //Reads the pipeline's state and returns a future that finishes once it has
    //nothing left to do. The state is read as late as the edges allow: at build
    //time only when the dependencies are already finished — which is to say they
    //had nothing to do and so dirtied nothing — and otherwise from inside their
    //continuation, so a pipeline they just dirtied is still seen as Dirty here.
    //force skips that reading and runs regardless; only buildNode() passes it.
    QFuture<void> driveToClean(cwUpdatable* pipeline, bool force);
    QFuture<void> waitForRun(cwUpdatable* pipeline, const QFuture<void>& run);

    const QList<cwUpdatable*> m_pipelines;
    //Pipeline -> the pipelines whose output it consumes. Absent or empty is a root.
    const QHash<cwUpdatable*, QList<cwUpdatable*>> m_dependencies;
    //The one pipeline this pass runs whether or not it is dirty, or null.
    cwUpdatable* const m_forcedRoot = nullptr;
    bool m_abandoned = false;
};

#endif // CWUPDATECASCADE_H
