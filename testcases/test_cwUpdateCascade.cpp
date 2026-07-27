//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "FakeUpdatable.h"
#include "cwUpdateCascade.h"

//Qt includes
#include <QCoreApplication>

using FakeUpdatableTest::settle;
using FakeUpdatableTest::dirtyWhenWorking;

namespace {

    //A cascade drives whatever it is handed, so these cases build the graph
    //directly rather than through a coordinator: no update policy, no
    //cwJobSettings, nothing to configure. Coordinated because that is the state a
    //registered pipeline is in — standalone, markDirty() would recompute on the
    //spot and there would be nothing left for the pass to order.
    QList<cwUpdatable*> coordinate(std::initializer_list<FakeUpdatable*> pipelines)
    {
        QList<cwUpdatable*> list;
        list.reserve(static_cast<qsizetype>(pipelines.size()));
        for(FakeUpdatable* pipeline : pipelines) {
            pipeline->setCoordinated(true);
            list.append(pipeline);
        }
        return list;
    }
}

TEST_CASE("A pipeline waits for every dependency it declares, not just one",
          "[cwUpdateCascade]")
{
    //middle depends on root, and leaf depends on both. leaf's node joins both of
    //its own parents, so holding middle in Working holds leaf too — releasing it
    //on root alone would let it run against output middle hasn't produced yet.
    FakeUpdatable root;
    FakeUpdatable middle(false); //Held in Working so leaf can't legally run yet
    FakeUpdatable leaf;

    cwUpdateCascade cascade(coordinate({&root, &middle, &leaf}),
                            {{&middle, {&root}}, {&leaf, {&root, &middle}}});

    root.markDirty();
    middle.markDirty();
    leaf.markDirty();

    cascade.start();
    settle();

    REQUIRE(root.updateState() == cwUpdatable::State::Clean);
    REQUIRE(middle.updateState() == cwUpdatable::State::Working);
    CHECK(leaf.updateCount() == 0);

    middle.finish();
    settle();

    CHECK(leaf.updateCount() == 1);
    CHECK(leaf.updateState() == cwUpdatable::State::Clean);
    //root is a dependency of both middle and leaf, so it is reached twice while
    //the DAG is built and must still be driven once.
    CHECK(root.driveCount() == 1);
}

TEST_CASE("A pipeline re-edited while its own node runs is driven again",
          "[cwUpdateCascade]")
{
    //A run that is already in flight can't cover an edit made after it started,
    //so the node has to run the pipeline a second time. Without this the cascade
    //would finish reporting Clean against data the user has already changed.
    FakeUpdatable slow(false);

    cwUpdateCascade cascade(coordinate({&slow}), {});

    slow.markDirty();
    cascade.start();
    settle();

    REQUIRE(slow.updateCount() == 1);
    REQUIRE(slow.updateState() == cwUpdatable::State::Working);

    slow.markDirty();
    settle();

    //The node waits on the run's future rather than reacting to the edit, so the
    //work in flight is left to finish instead of being canceled and restarted.
    CHECK(slow.updateCount() == 1);
    CHECK(slow.updateState() == cwUpdatable::State::Dirty);

    slow.finish();
    settle();

    //That future finishing is what re-drives the pipeline: the node re-reads the
    //state, finds it dirty again, and runs it rather than reporting done.
    CHECK(slow.updateCount() == 2);
    CHECK(slow.updateState() == cwUpdatable::State::Working);

    slow.finish();
    settle();

    CHECK(slow.updateState() == cwUpdatable::State::Clean);
}

TEST_CASE("A pipeline already working when the cascade starts isn't restarted",
          "[cwUpdateCascade]")
{
    //Working already covers the current data. Driving run() again would make a
    //real pipeline's restarter cancel the run in flight and start it over, so a
    //long solve would be thrown away by an unrelated Run.
    FakeUpdatable busy(false);

    cwUpdateCascade cascade(coordinate({&busy}), {});

    //Put it in Working outside the cascade, the way updateAllScraps() does when it
    //bypasses the coordinator entirely.
    busy.markDirty();
    busy.run();
    REQUIRE(busy.updateState() == cwUpdatable::State::Working);
    REQUIRE(busy.driveCount() == 1);

    cascade.start();
    settle();

    //The node attaches and waits; it must not call run() a second time.
    CHECK(busy.driveCount() == 1);
    CHECK(busy.updateState() == cwUpdatable::State::Working);

    busy.finish();
    settle();

    CHECK(busy.updateState() == cwUpdatable::State::Clean);
}

TEST_CASE("runIfNeeded reads the state the way a driver has to", "[cwUpdatable]")
{
    //The cascade's per-node drive is this call, and so are the managers' force
    //paths. Pinning it here rather than only through a cascade is what stops the
    //next caller from re-deriving the branch — and getting the Working case wrong,
    //which for a real manager cancels the work already doing the job.
    FakeUpdatable pipeline(false);
    pipeline.setCoordinated(true);

    REQUIRE(pipeline.updateState() == cwUpdatable::State::Clean);
    CHECK(pipeline.runIfNeeded().isFinished());
    CHECK(pipeline.driveCount() == 0);

    pipeline.markDirty();
    const QFuture<void> run = pipeline.runIfNeeded();
    CHECK(pipeline.updateCount() == 1);
    CHECK_FALSE(run.isFinished());

    //Working: what comes back is the run in flight, and nothing is started over.
    REQUIRE(pipeline.updateState() == cwUpdatable::State::Working);
    const QFuture<void> attached = pipeline.runIfNeeded();
    CHECK(pipeline.driveCount() == 1);
    CHECK(pipeline.updateCount() == 1);
    CHECK_FALSE(attached.isFinished());

    pipeline.finish();
    CHECK(run.isFinished());
    CHECK(attached.isFinished());
}

TEST_CASE("A cascade with nothing to do finishes before it returns",
          "[cwUpdateCascade]")
{
    //Synchronously, not an event-loop turn later. A driver defers edits to a pass
    //in flight, so a pass that stayed "running" for a turn would swallow an edit
    //made in that turn — the join and completion short-circuits exist for this.
    FakeUpdatable root;
    FakeUpdatable dependent;

    cwUpdateCascade cascade(coordinate({&root, &dependent}), {{&dependent, {&root}}});

    CHECK(cascade.start().isFinished());
}

TEST_CASE("A forced pass runs a clean pipeline and carries the result onward",
          "[cwUpdateCascade]")
{
    //The "Solve" button. Nothing is dirty, so an ordinary pass would do nothing at
    //all; the point of forcing is that solving dirties what consumes the solve,
    //and the declared edges are what recompute it. Before this the button forced
    //the manager directly and left its dependents holding stale output.
    FakeUpdatable root;
    FakeUpdatable dependent;
    root.setRunsWhenClean(true);
    const QList<cwUpdatable*> pipelines = coordinate({&root, &dependent});

    dirtyWhenWorking(&root, &dependent);

    REQUIRE(root.updateState() == cwUpdatable::State::Clean);

    cwUpdateCascade cascade(&root, pipelines, {{&dependent, {&root}}});
    cascade.start();
    settle();

    CHECK(root.updateCount() == 1);
    CHECK(dependent.updateCount() == 1);
    CHECK(dependent.updateState() == cwUpdatable::State::Clean);
}

TEST_CASE("A forced pass leaves the pipelines it doesn't reach alone",
          "[cwUpdateCascade]")
{
    //Scoping is the difference between this and a plain Run: forcing the scraps
    //must not drag in an unrelated pipeline just because it happens to be dirty.
    FakeUpdatable forced;
    FakeUpdatable unrelated;
    forced.setRunsWhenClean(true);

    cwUpdateCascade cascade(&forced, coordinate({&forced, &unrelated}), {});

    unrelated.markDirty();
    cascade.start();
    settle();

    CHECK(forced.updateCount() == 1);
    CHECK(unrelated.driveCount() == 0);
    CHECK(unrelated.updateState() == cwUpdatable::State::Dirty);
}

TEST_CASE("A forced pipeline's own dependencies are still driven ahead of it",
          "[cwUpdateCascade]")
{
    //Narrowed on the consuming side only. Forcing the scraps while the line plot
    //has fallen behind has to solve first, or the triangulation is fitted to
    //station positions the survey has already moved.
    FakeUpdatable upstream(false); //Held in Working so the ordering is observable
    FakeUpdatable forced;
    forced.setRunsWhenClean(true);

    cwUpdateCascade cascade(&forced, coordinate({&upstream, &forced}),
                            {{&forced, {&upstream}}});

    upstream.markDirty();
    cascade.start();
    settle();

    REQUIRE(upstream.updateState() == cwUpdatable::State::Working);
    CHECK(forced.driveCount() == 0);

    upstream.finish();
    settle();

    CHECK(forced.updateCount() == 1);
}

TEST_CASE("A forced pipeline is forced once, not every time its node is read",
          "[cwUpdateCascade]")
{
    //The node re-reads its pipeline when the run ends, to catch an edit made
    //while it was going. That second read has to be the ordinary one: a force
    //that renewed itself would run a clean pipeline again for as long as its runs
    //kept succeeding, which for a real solve never terminates.
    FakeUpdatable forced(false);
    forced.setRunsWhenClean(true);

    cwUpdateCascade cascade(&forced, coordinate({&forced}), {});

    cascade.start();
    settle();

    REQUIRE(forced.updateCount() == 1);
    REQUIRE(forced.updateState() == cwUpdatable::State::Working);

    forced.finish();
    settle();

    CHECK(forced.updateCount() == 1);
    CHECK(forced.driveCount() == 1);
    CHECK(forced.updateState() == cwUpdatable::State::Clean);
}

TEST_CASE("A mis-registered dependency cycle still terminates", "[cwUpdateCascade]")
{
    //Nothing should register a cycle, but the node walk must notice one rather
    //than recursing until the stack runs out.
    FakeUpdatable a;
    FakeUpdatable b;

    cwUpdateCascade cascade(coordinate({&a, &b}), {{&a, {&b}}, {&b, {&a}}});

    a.markDirty();
    b.markDirty();

    cascade.start();
    settle();

    CHECK(a.updateState() == cwUpdatable::State::Clean);
    CHECK(b.updateState() == cwUpdatable::State::Clean);
}

TEST_CASE("An abandoned cascade drives nothing further", "[cwUpdateCascade]")
{
    //Giving up on a pass is what supersedes it, and the pass has to be inert from
    //that moment: its continuations outlive the decision, and the pipeline one of
    //them would drive may be the reason the pass was dropped. This is the half of
    //the mechanism destruction can't cover, since a pass can be dropped from
    //inside its own walk and has to survive that frame.
    FakeUpdatable root(false); //Held in Working so the pass stays open
    FakeUpdatable dependent;

    cwUpdateCascade cascade(coordinate({&root, &dependent}), {{&dependent, {&root}}});

    root.markDirty();
    dependent.markDirty();
    cascade.start();
    settle();

    REQUIRE(root.updateState() == cwUpdatable::State::Working);
    REQUIRE(dependent.updateCount() == 0);

    cascade.abandon();

    //Releasing the root would otherwise let the pass's own continuation run the
    //dependent.
    root.finish();
    settle();

    CHECK(dependent.updateCount() == 0);
}

TEST_CASE("Destroying a cascade doesn't cancel the run it was waiting on",
          "[cwUpdateCascade]")
{
    //A pass observes each pipeline's run future, and those futures are shared: the
    //pipeline itself and anyone who asked for currentRun() hold the same handle. A
    //pass that canceled them on its way out would reach into a solve it doesn't own
    //and stop it for everybody.
    FakeUpdatable slow(false);
    QFuture<void> run;

    {
        cwUpdateCascade cascade(coordinate({&slow}), {});

        slow.markDirty();
        cascade.start();
        settle();

        REQUIRE(slow.updateState() == cwUpdatable::State::Working);
        run = slow.currentRun();
        REQUIRE_FALSE(run.isFinished());
    }

    settle();

    CHECK_FALSE(run.isCanceled());

    //Still the pipeline's own run to finish, and finishing it still resolves the
    //future every other waiter is holding.
    slow.finish();

    CHECK(run.isFinished());
    CHECK(slow.updateState() == cwUpdatable::State::Clean);
}
