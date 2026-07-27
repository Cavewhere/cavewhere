//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "FakeUpdatable.h"
#include "cwUpdateCascade.h"

//Qt includes
#include <QCoreApplication>

using FakeUpdatableTest::settle;

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

    //Put it in Working outside the cascade, the way rerunSurvex() and
    //updateAllScraps() do when they bypass the coordinator entirely.
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
