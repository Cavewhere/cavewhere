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
    //starts. Comfortably above the ~4 hops the deepest case needs, so a spurious
    //failure means a hop was added rather than that the margin was too thin.
    constexpr int kSettleTurns = 8;

    void settle()
    {
        for(int i = 0; i < kSettleTurns; i++) {
            QCoreApplication::processEvents();
        }
    }

    /**
     * Stand-in for a real derived-data pipeline. Mirrors the two-bit model the
     * scrap manager and line plot use: markDirty() sets the pending bit, update()
     * trades it for the in-flight bit, and finish() clears that.
     *
     * autoFinish makes update() complete on the next event-loop turn (a short
     * async job); leaving it off lets a test hold a pipeline in Working for as
     * long as it needs, which is how a cascade is kept open on purpose.
     */
    class FakePipeline : public QObject, public cwUpdatableBase
    {
        Q_OBJECT

    public:
        explicit FakePipeline(bool autoFinish = true) :
            m_autoFinish(autoFinish)
        {
        }

        cwUpdatable::State updateState() const override
        {
            if(m_pending) { return cwUpdatable::State::Dirty; }
            if(m_running) { return cwUpdatable::State::Working; }
            return cwUpdatable::State::Clean;
        }

        void update() override
        {
            //Counted before the early return: the real pipelines have no such
            //guard (cwLinePlotManager::update() restarts its solve unconditionally),
            //so a redundant drive that this fake absorbs would restart real work.
            m_driveCount++;
            if(!m_pending) { return; }
            m_pending = false;
            m_running = true;
            m_updateCount++;
            emit updateStateChanged();

            if(m_autoFinish) {
                QMetaObject::invokeMethod(this, &FakePipeline::finish, Qt::QueuedConnection);
            }
        }

        void markDirty()
        {
            m_pending = true;
            emit updateStateChanged();
            //Mirrors the real managers: eager until a coordinator takes over.
            runIfStandalone();
        }

        void finish()
        {
            if(!m_running) { return; }
            m_running = false;
            emit updateStateChanged();
        }

        //Runs started, versus every update() call including ones that started nothing.
        int updateCount() const { return m_updateCount; }
        int driveCount() const { return m_driveCount; }

    signals:
        void updateStateChanged();

    private:
        bool m_autoFinish;
        bool m_pending = false;
        bool m_running = false;
        int m_updateCount = 0;
        int m_driveCount = 0;
    };
}

TEST_CASE("A pipeline dirtied after its node finished is picked up when the cascade ends",
          "[cwUpdateCoordinator]")
{
    //The recipe is fixed once the tree starts, so this can't be handled by the
    //run in flight — the coordinator has to re-apply policy when it ends.
    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(true);

    FakePipeline root;
    FakePipeline fast;
    FakePipeline slow(false); //Held in Working by the test to keep the tree open

    cwUpdateCoordinator coordinator;
    coordinator.add(&root);
    coordinator.add(&fast, {&root});
    coordinator.add(&slow, {&root});

    //Automatic update drives slow the moment it is dirtied, so the cascade
    //updateNow() opens is one whose second layer is already busy and can't finish
    //until the test says so.
    slow.markDirty();
    coordinator.updateNow();
    settle();

    CHECK(slow.updateState() == cwUpdatable::State::Working);
    CHECK(fast.updateState() == cwUpdatable::State::Clean);

    //fast's node has already been skipped as clean, but the tree is still alive
    //waiting on its sibling. The coordinator defers to the running tree here, so
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

TEST_CASE("A pipeline runs after its deepest dependency, not its first",
          "[cwUpdateCoordinator]")
{
    //middle depends on root, and leaf depends on both. Ordering leaf by its first
    //dependency would put it alongside middle, letting it run against output
    //middle hasn't produced yet; ordering by the deepest puts it after both.
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
    //Dirty outranks Working, so a mid-run edit re-drives the pipeline rather than
    //letting the node settle on a run that predates it. Without this the cascade
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
    busy.update();
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

    //A second Run replaces the tree in flight rather than racing a second one
    //alongside it, so the busy pipeline is not driven again.
    coordinator.updateNow();
    settle();

    CHECK(root.driveCount() == 1);
    CHECK(dependent.updateCount() == 0);

    //The superseding cascade still carries the dependent layer to completion.
    root.finish();
    settle();

    CHECK(dependent.updateCount() == 1);
    CHECK(dependent.updateState() == cwUpdatable::State::Clean);
    CHECK_FALSE(coordinator.needsUpdate());
}

TEST_CASE("A mis-registered dependency cycle still terminates", "[cwUpdateCoordinator]")
{
    //Nothing should register a cycle, but the depth walk must be bounded rather
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
