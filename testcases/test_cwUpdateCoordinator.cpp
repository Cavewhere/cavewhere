//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "FakeUpdatable.h"
#include "cwUpdateCoordinator.h"
#include "cwJobSettings.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QCoreApplication>

using FakeUpdatableTest::settle;
using FakeUpdatableTest::dirtyWhenWorking;

TEST_CASE("A pipeline dirtied after its node finished is picked up when the cascade ends",
          "[cwUpdateCoordinator]")
{
    //The DAG is fixed once the cascade starts, so this can't be handled by the
    //run in flight — the coordinator has to re-apply policy when it ends.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    FakeUpdatable root;
    FakeUpdatable fast;
    FakeUpdatable slow(false); //Held in Working by the test to keep the cascade open

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&fast, {&root});
    coordinator.add(&slow, {&root});

    //Automatic update drives slow the moment it is dirtied, so the cascade
    //updateNow() opens is one whose dependent is already busy and can't finish
    //until the test says so.
    slow.markDirty();
    coordinator.updateNow();
    settle();

    CHECK(slow.updateState() == cwUpdatable::State::Working);
    CHECK(fast.updateState() == cwUpdatable::State::Clean);

    //fast's node has already been skipped as clean, but the cascade is still alive
    //waiting on its sibling. The coordinator defers to the running cascade here, so
    //nothing drives this edit at the moment it arrives.
    fast.markDirty();
    settle();

    REQUIRE(fast.updateCount() == 0);
    REQUIRE(fast.updateState() == cwUpdatable::State::Dirty);
    REQUIRE(coordinator.needsUpdate());

    //Ending the cascade must hand the stranded edit back to the normal policy.
    slow.finish();
    settle();

    CHECK(fast.updateCount() == 1);
    CHECK(fast.updateState() == cwUpdatable::State::Clean);
    CHECK_FALSE(coordinator.needsUpdate());
}

TEST_CASE("A forced Run doesn't chase edits made after it", "[cwUpdateCoordinator]")
{
    //With automatic update off, staleness is the report the user asked for: Run
    //resolves the cascade it started, and an edit made afterwards leaves the
    //footer asking for another one rather than silently starting a second run.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable root;
    FakeUpdatable fast;
    FakeUpdatable slow(false);

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&fast, {&root});
    coordinator.add(&slow, {&root});

    slow.markDirty();
    coordinator.updateNow();
    settle();

    //Nothing below this distinguishes "Run ran and declined to chase the edit"
    //from "Run did nothing at all", so pin that the forced cascade really started.
    REQUIRE(slow.updateCount() == 1);
    REQUIRE(slow.updateState() == cwUpdatable::State::Working);

    fast.markDirty();
    settle();

    slow.finish();
    settle();

    CHECK(slow.updateState() == cwUpdatable::State::Clean);
    CHECK(fast.updateCount() == 0);
    CHECK(fast.updateState() == cwUpdatable::State::Dirty);
    CHECK(coordinator.needsUpdate());
}

TEST_CASE("The staleness aggregate signals only when it changes", "[cwUpdateCoordinator]")
{
    //The footer binds needsUpdate, so a missed emission leaves it wrong and an
    //emission per child transition makes it thrash.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable first(false);
    FakeUpdatable second(false);

    cwUpdateCoordinator coordinator;
    coordinator.add(&first);
    coordinator.add(&second);

    cwSignalSpy spy(&coordinator, &cwUpdateCoordinator::needsUpdateChanged);

    first.markDirty();
    CHECK(coordinator.needsUpdate());
    CHECK(spy.count() == 1);

    //Already stale, so the aggregate hasn't changed and must stay quiet.
    second.markDirty();
    CHECK(spy.count() == 1);

    //Both move to Working, so nothing is Dirty and the aggregate flips back once.
    coordinator.updateNow();
    settle();

    CHECK_FALSE(coordinator.needsUpdate());
    CHECK(spy.count() == 2);
}

TEST_CASE("A pipeline stops self-driving once the coordinator takes it over",
          "[cwUpdateCoordinator]")
{
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    //Standalone, a source edit recomputes eagerly — the sensible default for a
    //pipeline nobody is driving.
    FakeUpdatable standalone;
    standalone.markDirty();
    CHECK(standalone.updateCount() == 1);

    FakeUpdatable coordinated;

    cwUpdateCoordinator coordinator;
    coordinator.add(&coordinated);

    //Coordinated, the same edit only marks dirty: the run decision is the
    //coordinator's, and with automatic update off it waits for a Run.
    coordinated.markDirty();

    CHECK(coordinated.updateCount() == 0);
    CHECK(coordinated.updateState() == cwUpdatable::State::Dirty);
}

TEST_CASE("Pressing Run again supersedes the cascade in flight", "[cwUpdateCoordinator]")
{
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable root(false);
    FakeUpdatable dependent;

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&dependent, {&root});

    root.markDirty();
    dependent.markDirty();
    coordinator.updateNow();
    settle();

    REQUIRE(root.updateState() == cwUpdatable::State::Working);
    REQUIRE(dependent.updateCount() == 0);

    //A second Run replaces the cascade in flight rather than racing a second one
    //alongside it, so the busy pipeline is not driven again.
    coordinator.updateNow();
    settle();

    CHECK(root.driveCount() == 1);
    CHECK(dependent.updateCount() == 0);

    //The superseding cascade still carries the dependents to completion.
    root.finish();
    settle();

    CHECK(dependent.updateCount() == 1);
    CHECK(dependent.updateState() == cwUpdatable::State::Clean);
    CHECK_FALSE(coordinator.needsUpdate());
}

TEST_CASE("A superseded cascade doesn't report the one that replaced it finished",
          "[cwUpdateCoordinator]")
{
    //Dropping a cascade schedules its deletion but cannot perform it — a pass can
    //be dropped from inside its own walk — so a pass that has been given up on can
    //still outlive the decision and settle. Reporting that would clear the running
    //flag out from under its replacement, handing the run decision back to
    //onChildStateChanged while a cascade is still driving.
    //
    //There is no event loop here, so the deferred delete never lands and the
    //abandoned pass stays alive for the whole case. In the app it lands promptly,
    //except across the nested event loop a pipeline destructor pumps — which is the
    //same shape, and the reason this isn't only a test artifact.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    FakeUpdatable slow(false);
    FakeUpdatable other(false);
    FakeUpdatable late;

    cwUpdateCoordinator coordinator;
    coordinator.add(&slow);
    coordinator.add(&other);
    coordinator.add(&late);

    //The first cascade covers slow alone: the other two are clean when it starts.
    slow.markDirty();
    coordinator.updateNow();
    settle();
    REQUIRE(slow.updateState() == cwUpdatable::State::Working);

    //Deferred to the cascade in flight, so the second Run is what picks it up —
    //giving the replacement a node the first pass doesn't have.
    other.markDirty();
    coordinator.updateNow();
    settle();

    REQUIRE(other.updateState() == cwUpdatable::State::Working);

    //Enough to settle the superseded pass, since slow was all it was waiting on,
    //while the pass that replaced it is still waiting on other.
    slow.finish();
    settle();

    //Pins that the superseded pass really did settle in that window, so the guard
    //this case exists for is reached rather than skipped. Without this the check
    //below passes just as well when the pass never settled at all — green, and
    //covering nothing.
    REQUIRE(slow.updateState() == cwUpdatable::State::Clean);

    //Still deferred: the replacement owns the run decision until it says otherwise.
    late.markDirty();
    settle();
    CHECK(late.updateCount() == 0);

    other.finish();
    settle();

    CHECK(late.updateCount() == 1);
    CHECK_FALSE(coordinator.needsUpdate());
}

TEST_CASE("Destroying a pipeline mid-pass doesn't strand the survivors",
          "[cwUpdateCoordinator]")
{
    //Giving up on a pass gives up on every other pipeline's ordered work with it,
    //and unlike a supersede there is no replacement coming. So the give-up has to
    //re-apply policy itself; otherwise the survivor waits for whatever edit
    //happens to arrive next, having already been told its edit was in hand.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    auto* slow = new FakeUpdatable(false);
    FakeUpdatable survivor(false);

    cwUpdateCoordinator coordinator;
    coordinator.add(slow);
    coordinator.add(&survivor);

    slow->markDirty();
    coordinator.updateNow();
    settle();
    REQUIRE(slow->updateState() == cwUpdatable::State::Working);

    //Deferred to the pass in flight rather than driven on the spot.
    survivor.markDirty();
    settle();
    REQUIRE(survivor.updateCount() == 0);

    delete slow;
    settle();

    CHECK(survivor.updateCount() == 1);
}

TEST_CASE("A destroyed pipeline is pruned from every other dependency list",
          "[cwUpdateCoordinator]")
{
    //The cascade suite builds its dependency hash literally, so registering a
    //two-parent dependent through add() — and pruning one of those parents back
    //out — is only ever exercised here. A parent left behind in leaf's list would
    //order it behind a pipeline that no longer exists.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable root;
    auto* middle = new FakeUpdatable;
    FakeUpdatable leaf;

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(middle);
    coordinator.add(&leaf, {&root, middle});

    delete middle;

    leaf.markDirty();
    coordinator.updateNow();
    settle();

    CHECK(leaf.updateCount() == 1);
    CHECK_FALSE(coordinator.needsUpdate());
}

TEST_CASE("Destroying the coordinator doesn't cancel work in flight",
          "[cwUpdateCoordinator]")
{
    //The cascade suite checks that a pass dropped on its own doesn't cancel the run
    //it was waiting on; this is the coordinator's exit reaching the same result,
    //where the pass is dropped and then destroyed as a child on the way out. Those
    //run futures are shared — the pipeline itself and anyone who asked for
    //currentRun() hold the same handle — so canceling one would reach into a solve
    //the coordinator doesn't own and stop it for everybody.
    //
    //Note this does not exercise cwUpdateCascade's shield(): teardown deletes the
    //watcher that would have pushed a cancel upstream before it can deliver, so
    //the run survives here either way. The shield covers a node canceled while
    //the pass is still alive, which nothing reaches today.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable slow(false);
    QFuture<void> run;

    {
        cwUpdateCoordinator coordinator;
        coordinator.add(&slow);

        slow.markDirty();
        coordinator.updateNow();
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

TEST_CASE("A cascade with nothing to do ends before the next edit arrives",
          "[cwUpdateCoordinator]")
{
    //updateNow() on a clean graph has to finish synchronously. If it stayed
    //"running" for an event-loop turn, an edit made in that turn would be
    //deferred to a cascade that had already ended and never driven.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    FakeUpdatable pipeline;

    cwUpdateCoordinator coordinator;
    coordinator.add(&pipeline);

    coordinator.updateNow();

    //No settle() — the point is that the edit lands in the same turn.
    pipeline.markDirty();
    CHECK(pipeline.updateCount() == 1);
}

namespace {

    //Models the real pipelines' destructors, which pump a nested event loop
    //(waitToFinish() -> AsyncFuture::waitForFinished) in the destructor *body* —
    //before ~QObject emits destroyed(), which is when the coordinator abandons
    //its cascade. Records whether the coordinator drove it while it was dying.
    class PumpingPipeline : public FakeUpdatable
    {
    public:
        explicit PumpingPipeline(bool* ranWhileDestructing) :
            FakeUpdatable(false),
            m_ranWhileDestructing(ranWhileDestructing)
        {
        }

        ~PumpingPipeline() override
        {
            //First statement, exactly as the real managers do it: ~FakeUpdatable
            //also announces teardown, but base destructors run after this body,
            //which is where the pumping happens. Dropping this line is enough to
            //fail both cases below.
            beginTeardown();

            m_destructing = true;

            //Both doors a dying pipeline can be driven through. markDirty() emits,
            //which is the synchronous one — the real managers do exactly this when
            //a canceled run's completion handler reaches the coordinator from the
            //destructor body. settle() delivers the queued cascade continuation,
            //which is the one that needs the nested loop the pumping provides.
            markDirty();
            settle();
        }

    protected:
        QFuture<void> doRun() override
        {
            if(m_destructing) { *m_ranWhileDestructing = true; }
            return FakeUpdatable::doRun();
        }

    private:
        bool* m_ranWhileDestructing;
        bool m_destructing = false;
    };
}

TEST_CASE("A pipeline destroyed with a continuation queued isn't driven while it dies",
          "[cwUpdateCoordinator]")
{
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    bool ranWhileDestructing = false;
    auto* pipeline = new PumpingPipeline(&ranWhileDestructing);

    cwUpdateCoordinator coordinator;
    coordinator.add(pipeline);

    pipeline->markDirty();
    coordinator.updateNow();
    settle();
    REQUIRE(pipeline->updateState() == cwUpdatable::State::Working);

    //The run ends, so the node's continuation is queued but not yet delivered.
    pipeline->finish();
    //And the source is edited again, so that continuation will read Dirty.
    pipeline->markDirty();
    REQUIRE(pipeline->updateState() == cwUpdatable::State::Dirty);

    //The destructor pumps, which delivers that continuation. The coordinator's
    //own defenses are both still down at this point — dropRunningCascade() runs
    //off destroyed(), which comes later, so the cascade is neither abandoned nor
    //gone — so what has to stop the drive is the pipeline reporting itself torn
    //down.
    delete pipeline;

    CHECK_FALSE(ranWhileDestructing);
}

namespace {

    //Destroys another registered pipeline from inside its own run. That is how a
    //pass gets given up on while its own walk is still on the stack: the drive
    //reaches the destroyed handler, which drops the pass the walk is running.
    class KillingPipeline : public FakeUpdatable
    {
    public:
        explicit KillingPipeline(FakeUpdatable** victim) :
            FakeUpdatable(true),
            m_victim(victim)
        {
        }

    protected:
        QFuture<void> doRun() override
        {
            const QFuture<void> future = FakeUpdatable::doRun();
            delete *m_victim;
            *m_victim = nullptr;
            return future;
        }

    private:
        FakeUpdatable** m_victim;
    };
}

TEST_CASE("A pass given up on from inside its own walk stops reading its snapshot",
          "[cwUpdateCoordinator]")
{
    //The scenario abandon() exists for, and the one destruction cannot cover: the
    //pass is dropped from inside start(), so it has to survive the frame it was
    //dropped in. What is left of the walk still holds the destroyed pipeline in
    //its snapshot — the coordinator scrubbed its own registry, not the pass's copy
    //— so the abandoned check is the only thing standing between the rest of the
    //walk and freed memory. Without it the walk reads updateState() off a deleted
    //pipeline, which is a use-after-free the Debug build's sanitizer catches.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    auto* victim = new FakeUpdatable(false);
    FakeUpdatable* victimSlot = victim;
    KillingPipeline killer(&victimSlot);

    cwUpdateCoordinator coordinator;
    //Registration order is the walk order, so the victim's node is built after
    //the drive that destroys it.
    coordinator.add(&killer);
    coordinator.add(victim);

    killer.markDirty();
    victim->markDirty();

    coordinator.updateNow();
    settle();

    CHECK(victimSlot == nullptr);
    CHECK(killer.updateCount() == 1);
}

TEST_CASE("A pipeline announcing from inside its destructor isn't driven by it",
          "[cwUpdateCoordinator]")
{
    //The other door, and the one that needs no event loop at all: a state change
    //emitted from a destructor body reaches onChildStateChanged() synchronously,
    //and with automatic update on a Dirty reading there is answered by running the
    //pipeline. Only the teardown latch making that reading Clean stops it.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    bool ranWhileDestructing = false;
    auto* pipeline = new PumpingPipeline(&ranWhileDestructing);

    cwUpdateCoordinator coordinator;
    coordinator.add(pipeline);

    delete pipeline;

    CHECK_FALSE(ranWhileDestructing);
}

TEST_CASE("A pipeline that has announced teardown reports Clean and starts nothing",
          "[cwUpdateCoordinator]")
{
    //The guard the case above depends on, checked directly rather than through a
    //destructor: it is the pipeline's own answer, so it holds for every caller
    //including the pipeline's internal force paths.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable pipeline;
    cwUpdateCoordinator coordinator;
    coordinator.add(&pipeline);

    pipeline.markDirty();
    REQUIRE(pipeline.updateState() == cwUpdatable::State::Dirty);
    REQUIRE(coordinator.needsUpdate());

    pipeline.beginTeardown();

    CHECK(pipeline.updateState() == cwUpdatable::State::Clean);
    //A pipeline on its way out drops out of the staleness aggregate too: there is
    //no point telling the user an update is needed for something that is leaving.
    CHECK_FALSE(coordinator.needsUpdate());

    pipeline.run();
    CHECK(pipeline.updateCount() == 0);
}

TEST_CASE("Nothing is driven once shutdown has been announced",
          "[cwUpdateCoordinator]")
{
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    FakeUpdatable pipeline;
    cwUpdateCoordinator coordinator;
    coordinator.add(&pipeline);

    coordinator.beginShutdown();

    //Automatic update is on, so without the shutdown latch this edit would run
    //the pipeline the moment it announced itself dirty.
    pipeline.markDirty();
    settle();
    CHECK(pipeline.updateCount() == 0);

    //The forced Run path is closed as well, not just the automatic one.
    coordinator.updateNow();
    settle();
    CHECK(pipeline.updateCount() == 0);
}

TEST_CASE("An in-flight cascade stops driving once shutdown is announced",
          "[cwUpdateCoordinator]")
{
    //Those early returns only close the doors a *new* cascade comes through. A
    //cascade already running owns the run decision for every node in it and drives
    //them from its own continuations, which pass through neither
    //onChildStateChanged() nor updateNow() — so dropping the cascade is what
    //actually stops work during the shutdown drain.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable root(false); //Held in Working so the cascade stays open
    FakeUpdatable dependent;

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&dependent, {&root});

    root.markDirty();
    dependent.markDirty();
    coordinator.updateNow();
    settle();

    REQUIRE(root.updateState() == cwUpdatable::State::Working);
    REQUIRE(dependent.updateCount() == 0);

    coordinator.beginShutdown();

    //Releasing the root would otherwise let the cascade's own continuation run the
    //dependent, on the way out.
    root.finish();
    settle();

    CHECK(dependent.updateCount() == 0);
}

TEST_CASE("A pipeline outliving its coordinator drives itself again",
          "[cwUpdateCoordinator]")
{
    //add() hands the run decision to the coordinator; giving it back is what
    //keeps a pipeline whose coordinator is gone from marking itself dirty
    //forever with nothing left to run it.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable pipeline;
    {
        cwUpdateCoordinator coordinator;
        coordinator.add(&pipeline);
        REQUIRE(pipeline.isCoordinated());
    }

    CHECK_FALSE(pipeline.isCoordinated());

    //Standalone again: the edit recomputes on the spot, with automatic update
    //off and no coordinator involved.
    pipeline.markDirty();
    CHECK(pipeline.updateCount() == 1);
}

TEST_CASE("A coordinator torn down during shutdown leaves its pipelines inert",
          "[cwUpdateCoordinator]")
{
    //The exception to the hand-back above. Eager means "recompute on the next
    //source edit", and during shutdown those edits come from the survey data
    //being dismantled — so a pipeline handed back here would solve against a
    //region already coming apart.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable pipeline;
    {
        cwUpdateCoordinator coordinator;
        coordinator.add(&pipeline);
        coordinator.beginShutdown();
    }

    CHECK(pipeline.isCoordinated());

    pipeline.markDirty();
    settle();
    CHECK(pipeline.updateCount() == 0);
}

TEST_CASE("Forcing one pipeline recomputes it and everything that consumes it",
          "[cwUpdateCoordinator]")
{
    //The "Solve" button, with automatic update off. Forcing the manager directly
    //is what it used to do, and that left the scraps holding output the solve had
    //just made stale with nothing to recompute them.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable root;
    FakeUpdatable dependent;
    FakeUpdatable unrelated;
    root.setRunsWhenClean(true);

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&dependent, {&root});
    coordinator.add(&unrelated);

    dirtyWhenWorking(&root, &dependent);

    unrelated.markDirty();
    REQUIRE(root.updateState() == cwUpdatable::State::Clean);

    coordinator.updateNow(&root);
    settle();

    CHECK(root.updateCount() == 1);
    CHECK(dependent.updateCount() == 1);
    //Scoped: a pipeline the forced one doesn't reach is left for the normal
    //policy, which with automatic update off means left dirty.
    CHECK(unrelated.updateCount() == 0);
    CHECK(coordinator.needsUpdate());
}

TEST_CASE("Forcing a pipeline the coordinator doesn't know starts nothing",
          "[cwUpdateCoordinator]")
{
    //There are no edges for it, so a cascade built around it could only run it
    //alone — which is the thing this overload exists to stop callers doing.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable registered;
    FakeUpdatable stranger;
    stranger.setRunsWhenClean(true);

    cwUpdateCoordinator coordinator;
    coordinator.add(&registered);

    coordinator.updateNow(&stranger);
    coordinator.updateNow(nullptr);
    settle();

    CHECK(stranger.driveCount() == 0);
    CHECK(registered.driveCount() == 0);
}

TEST_CASE("A forced pass is superseded by the next one, like any other",
          "[cwUpdateCoordinator]")
{
    //Scoped or not, only one pass owns the pipelines at a time: the incumbent is
    //given up on before the replacement is built, and the one given up on must
    //not report itself finished from under it.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakeUpdatable root(false); //Held in Working so the first pass stays open
    FakeUpdatable dependent;
    root.setRunsWhenClean(true);

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&dependent, {&root});

    coordinator.updateNow(&root);
    settle();

    REQUIRE(root.updateState() == cwUpdatable::State::Working);
    REQUIRE(dependent.updateCount() == 0);

    coordinator.updateNow();
    dependent.markDirty();
    settle();

    //The replacement owns the run decision now, so the edit waits for it rather
    //than being driven by the pass that was dropped.
    CHECK(dependent.updateCount() == 0);

    root.finish();
    settle();

    CHECK(dependent.updateCount() == 1);
}
