//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwUpdateCoordinator.h"
#include "cwJobSettings.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QCoreApplication>

namespace {

    //Event-loop turns spun to let the queued hops settle: the fake pipeline's
    //queued finish, the coordinator's queued flush, and the cascade a flush
    //starts. The deepest case here measures 6, and each extra link in a
    //dependency chain costs several more, so this carries headroom for one — a
    //case that needs more should wait on its own post-condition instead.
    constexpr int kSettleTurns = 16;

    void settle()
    {
        for(int i = 0; i < kSettleTurns; i++) {
            QCoreApplication::processEvents();
        }
    }

    /**
     * Stand-in for a real derived-data pipeline. Mirrors the managers: markDirty()
     * sets the pending bit, run() trades it for a run whose future finish()
     * resolves.
     *
     * autoFinish makes run() complete on the next event-loop turn (a short async
     * job); leaving it off lets a test hold a pipeline in Working for as long as
     * it needs, which is how a cascade is kept open on purpose.
     */
    class FakePipeline : public QObject, public cwUpdatableBase
    {
        Q_OBJECT

    public:
        explicit FakePipeline(bool autoFinish = true) :
            m_autoFinish(autoFinish)
        {
        }

        //Mirrors the real managers, whose destructors announce teardown before
        //doing anything else. A subclass that pumps in its own destructor has to
        //repeat this, since a base destructor runs too late to help — see
        //PumpingPipeline.
        ~FakePipeline() override { beginTeardown(); }

        void markDirty()
        {
            m_pending = true;
            emit updateStateChanged();
            //Mirrors the real managers: eager until a coordinator takes over.
            runIfStandalone();
        }

        void finish()
        {
            if(!isRunning()) { return; }
            endRun();
            emit updateStateChanged();
        }

        //Runs started, versus every run() call including ones that started nothing.
        int updateCount() const { return m_updateCount; }
        int driveCount() const { return m_driveCount; }

        //Protected on the real thing, since it is a pipeline's statement about
        //itself. Exposed here so a case can check the latch head-on rather than
        //only through a destructor.
        using cwUpdatable::beginTeardown;

    protected:
        cwUpdatable::State doUpdateState() const override
        {
            if(m_pending) { return cwUpdatable::State::Dirty; }
            if(isRunning()) { return cwUpdatable::State::Working; }
            return cwUpdatable::State::Clean;
        }

        QFuture<void> doRun() override
        {
            //Counted before the early return: run() is a force path on the real
            //pipelines too (cwLinePlotManager restarts its solve
            //unconditionally), so a redundant drive would restart real work.
            m_driveCount++;
            if(!m_pending) { return currentRun(); }
            m_pending = false;
            const QFuture<void> future = beginRun();
            m_updateCount++;
            emit updateStateChanged();

            if(m_autoFinish) {
                QMetaObject::invokeMethod(this, &FakePipeline::finish, Qt::QueuedConnection);
            }
            return future;
        }

    private:
        bool m_autoFinish;
        bool m_pending = false;
        int m_updateCount = 0;
        int m_driveCount = 0;

    signals:
        void updateStateChanged();
    };
}

TEST_CASE("A pipeline dirtied after its node finished is picked up when the cascade ends",
          "[cwUpdateCoordinator]")
{
    //The DAG is fixed once the cascade starts, so this can't be handled by the
    //run in flight — the coordinator has to re-apply policy when it ends.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    FakePipeline root;
    FakePipeline fast;
    FakePipeline slow(false); //Held in Working by the test to keep the cascade open

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

TEST_CASE("A pipeline waits for every dependency it declares, not just one",
          "[cwUpdateCoordinator]")
{
    //middle depends on root, and leaf depends on both. leaf's node joins both of
    //its own parents, so holding middle in Working holds leaf too — releasing it
    //on root alone would let it run against output middle hasn't produced yet.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakePipeline root;
    FakePipeline middle(false); //Held in Working so leaf can't legally run yet
    FakePipeline leaf;

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&middle, {&root});
    coordinator.add(&leaf, {&root, &middle});

    root.markDirty();
    middle.markDirty();
    leaf.markDirty();

    coordinator.updateNow();
    settle();

    REQUIRE(root.updateState() == cwUpdatable::State::Clean);
    REQUIRE(middle.updateState() == cwUpdatable::State::Working);
    CHECK(leaf.updateCount() == 0);

    middle.finish();
    settle();

    CHECK(leaf.updateCount() == 1);
    CHECK(leaf.updateState() == cwUpdatable::State::Clean);
    CHECK_FALSE(coordinator.needsUpdate());
    //root is a dependency of both middle and leaf, so it is reached twice while
    //the DAG is built and must still be driven once.
    CHECK(root.driveCount() == 1);
}

TEST_CASE("A forced Run doesn't chase edits made after it", "[cwUpdateCoordinator]")
{
    //With automatic update off, staleness is the report the user asked for: Run
    //resolves the cascade it started, and an edit made afterwards leaves the
    //footer asking for another one rather than silently starting a second run.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakePipeline root;
    FakePipeline fast;
    FakePipeline slow(false);

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

TEST_CASE("A pipeline re-edited while its own node runs is driven again",
          "[cwUpdateCoordinator]")
{
    //A run that is already in flight can't cover an edit made after it started,
    //so the node has to run the pipeline a second time. Without this the cascade
    //would finish reporting Clean against data the user has already changed.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakePipeline slow(false);

    cwUpdateCoordinator coordinator;
    coordinator.add(&slow);

    slow.markDirty();
    coordinator.updateNow();
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
    CHECK_FALSE(coordinator.needsUpdate());
}

TEST_CASE("A pipeline already working when the cascade starts isn't restarted",
          "[cwUpdateCoordinator]")
{
    //Working already covers the current data. Driving update() again would make a
    //real pipeline's restarter cancel the run in flight and start it over, so a
    //long solve would be thrown away by an unrelated Run.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakePipeline busy(false);

    cwUpdateCoordinator coordinator;
    coordinator.add(&busy);

    //Put it in Working outside the coordinator, the way rerunSurvex() and
    //updateAllScraps() do when they bypass the coordinator entirely.
    busy.markDirty();
    busy.run();
    REQUIRE(busy.updateState() == cwUpdatable::State::Working);
    REQUIRE(busy.driveCount() == 1);

    coordinator.updateNow();
    settle();

    //The node attaches and waits; it must not call update() a second time.
    CHECK(busy.driveCount() == 1);
    CHECK(busy.updateState() == cwUpdatable::State::Working);

    busy.finish();
    settle();

    CHECK(busy.updateState() == cwUpdatable::State::Clean);
}

TEST_CASE("The staleness aggregate signals only when it changes", "[cwUpdateCoordinator]")
{
    //The footer binds needsUpdate, so a missed emission leaves it wrong and an
    //emission per child transition makes it thrash.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakePipeline first(false);
    FakePipeline second(false);

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
    FakePipeline standalone;
    standalone.markDirty();
    CHECK(standalone.updateCount() == 1);

    FakePipeline coordinated;

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

    FakePipeline root(false);
    FakePipeline dependent;

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

TEST_CASE("Destroying the coordinator doesn't cancel work in flight",
          "[cwUpdateCoordinator]")
{
    //A cascade observes each pipeline's run future, and those futures are shared:
    //the pipeline itself and anyone who asked for currentRun() are holding the
    //same handle. A coordinator that canceled them on its way out would reach
    //into a solve it doesn't own and stop it for everybody.
    //
    //Note this does not exercise waitForRun()'s shield(): teardown deletes the
    //watcher that would have pushed a cancel upstream before it can deliver, so
    //the run survives here either way. The shield covers a node canceled while
    //the coordinator is still alive, which nothing reaches today.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakePipeline slow(false);
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

    FakePipeline pipeline;

    cwUpdateCoordinator coordinator;
    coordinator.add(&pipeline);

    coordinator.updateNow();

    //No settle() — the point is that the edit lands in the same turn.
    pipeline.markDirty();
    CHECK(pipeline.updateCount() == 1);
}

TEST_CASE("A mis-registered dependency cycle still terminates", "[cwUpdateCoordinator]")
{
    //Nothing should register a cycle, but the node walk must notice one rather
    //than recursing until the stack runs out.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    FakePipeline a;
    FakePipeline b;

    cwUpdateCoordinator coordinator;
    coordinator.add(&a, {&b});
    coordinator.add(&b, {&a});

    a.markDirty();
    b.markDirty();

    coordinator.updateNow();
    settle();

    CHECK(a.updateState() == cwUpdatable::State::Clean);
    CHECK(b.updateState() == cwUpdatable::State::Clean);
    CHECK_FALSE(coordinator.needsUpdate());
}

#include "test_cwUpdateCoordinator.moc"

namespace {

    //Models the real pipelines' destructors, which pump a nested event loop
    //(waitToFinish() -> AsyncFuture::waitForFinished) in the destructor *body* —
    //before ~QObject emits destroyed(), which is when the coordinator abandons
    //its cascade. Records whether the coordinator drove it while it was dying.
    class PumpingPipeline : public FakePipeline
    {
    public:
        explicit PumpingPipeline(bool* ranWhileDestructing) :
            FakePipeline(false),
            m_ranWhileDestructing(ranWhileDestructing)
        {
        }

        ~PumpingPipeline() override
        {
            //First statement, exactly as the real managers do it: ~FakePipeline
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
            return FakePipeline::doRun();
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
    //off destroyed(), which comes later, so the generation still matches — so
    //what has to stop the drive is the pipeline reporting itself torn down.
    delete pipeline;

    CHECK_FALSE(ranWhileDestructing);
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

    FakePipeline pipeline;
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

    FakePipeline pipeline;
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

    FakePipeline root(false); //Held in Working so the cascade stays open
    FakePipeline dependent;

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

    FakePipeline pipeline;
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

    FakePipeline pipeline;
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
