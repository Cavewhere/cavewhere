#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QDir>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>

//Std includes
#include <algorithm>
#include <functional>
#include <memory>

//AsyncFuture
#include <asyncfuture.h>

//Our includes
#include "cwJobSettings.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwMemoryBudget.h"
#include "cwTask.h"

#include "LazFixtureHelper.h"
#include "TestHelper.h"
#include "cwBuildBudgetOverride.h"
#include "cwThreadCountRestorer.h"

namespace {
constexpr int kOneBuildAtATime = 1;
constexpr int kOneLayer = 1;
constexpr int kTwoLayers = 2;
constexpr qint64 kHalf = 2;
constexpr int kWaitTimeoutMs = 10000;
constexpr int kOneThreadDeadlineMs = 60000;
constexpr int kStillQueuedMs = 200;

// Spins the event loop for durationMs; returns whether condition held at every check.
bool holdsFor(const std::function<bool()>& condition, int durationMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < durationMs) {
        if (!condition()) {
            return false;
        }
        spinEventLoopSlice();
    }
    return condition();
}

// A model scanned from a folder holding layerCount fixture files
void scanLayers(cwLazLayerModel& model, const QTemporaryDir& tempDir, int layerCount)
{
    const QDir gisLayers = makeGisLayersDir(tempDir);
    for (int i = 0; i < layerCount; ++i) {
        const QString name = QStringLiteral("layer%1.laz").arg(i);
        REQUIRE(!writeMinimalLaz(gisLayers.filePath(name)).isEmpty());
    }
    model.setGisLayersDir(gisLayers);
    model.rescan();
    REQUIRE(model.count() == layerCount);
}

void scanTwoLayers(cwLazLayerModel& model, const QTemporaryDir& tempDir)
{
    scanLayers(model, tempDir, kTwoLayers);
}

int loadedCount(const cwLazLayerModel& model)
{
    const QList<cwLazLayer*>& layers = model.layers();
    return int(std::count_if(layers.begin(), layers.end(), [](const cwLazLayer* layer) {
        return layer->loadStatus() == cwLazLayer::LoadStatus::Loaded;
    }));
}

// Occupies every thread of the task pool until it is destroyed, so a build
// admitted meanwhile holds its slot without running.
class PoolBlocker
{
public:
    PoolBlocker() :
        m_threadCount(cwTask::threadPool()->maxThreadCount())
    {
        for (int i = 0; i < m_threadCount; ++i) {
            cwTask::threadPool()->start([started = m_started, release = m_release]() {
                started->release();
                release->acquire();
            });
        }
    }

    ~PoolBlocker()
    {
        m_release->release(m_threadCount);
    }

    bool isBlocking() const
    {
        return m_started->tryAcquire(m_threadCount, kWaitTimeoutMs);
    }

    PoolBlocker(const PoolBlocker&) = delete;
    PoolBlocker& operator=(const PoolBlocker&) = delete;

private:
    int m_threadCount;
    std::shared_ptr<QSemaphore> m_started = std::make_shared<QSemaphore>();
    std::shared_ptr<QSemaphore> m_release = std::make_shared<QSemaphore>();
};

// Sets the ledger's cap for as long as it lives, then puts back the one it found.
class CapOverride
{
public:
    CapOverride(cwMemoryBudget* budget, qint64 capBytes) :
        m_budget(budget),
        m_previousBytes(budget->capOverrideBytes())
    {
        m_budget->setCapOverrideBytes(capBytes);
    }

    ~CapOverride()
    {
        m_budget->setCapOverrideBytes(m_previousBytes);
    }

    CapOverride(const CapOverride&) = delete;
    CapOverride& operator=(const CapOverride&) = delete;

private:
    cwMemoryBudget* m_budget;
    qint64 m_previousBytes;
};

// Scans one layer into the model, holds its admitted build at running and
// returns what the build added to the ledger; the build then finishes and the
// ledger returns to where it was.
qint64 runningBuildCharge(cwLazLayerModel& model, const QTemporaryDir& tempDir)
{
    cwMemoryBudget* budget = cwMemoryBudget::instance();
    AsyncFuture::Limiter* limiter = model.buildLimiter();
    const qint64 before = budget->inFlightBytes();

    auto ahead = AsyncFuture::deferred<void>();
    limiter->admit([ahead]() { return ahead.future(); });

    scanLayers(model, tempDir, kOneLayer);
    REQUIRE(waitUntil([limiter]() { return limiter->queuedCount() == kOneLayer; }));

    qint64 charge = 0;
    {
        // The admitted build's pool task waits behind the blockers, so the
        // build is running as far as the window is concerned.
        const PoolBlocker blocker;
        REQUIRE(blocker.isBlocking());

        ahead.complete();
        REQUIRE(waitUntil([limiter]() {
            return limiter->runningCount() == kOneBuildAtATime && limiter->queuedCount() == 0;
        }));
        charge = budget->inFlightBytes() - before;
    }

    REQUIRE(waitUntil([&]() { return loadedCount(model) == kOneLayer; }, kWaitTimeoutMs));
    CHECK(waitUntil([&]() { return budget->inFlightBytes() == before; }));
    return charge;
}
} // namespace

TEST_CASE("cwLazLayerBuildQueue: a build waits for the item ahead of it",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("queued")));
    REQUIRE(!path.isEmpty());

    AsyncFuture::Limiter limiter(AsyncFuture::Limit {kOneBuildAtATime});
    auto ahead = AsyncFuture::deferred<void>();
    limiter.admit([ahead]() { return ahead.future(); });
    REQUIRE(limiter.runningCount() == kOneBuildAtATime);

    cwLazLayer layer;
    layer.setBuildLimiter(&limiter);
    layer.setCacheRootPath(tempDir.path());
    layer.setSourcePath(path);

    REQUIRE(waitUntil([&limiter]() { return limiter.queuedCount() == kOneLayer; }));
    CHECK(layer.loadStatus() == cwLazLayer::LoadStatus::Loading);
    CHECK(holdsFor([&]() {
        return limiter.queuedCount() == kOneLayer
            && layer.loadStatus() == cwLazLayer::LoadStatus::Loading;
    }, kStillQueuedMs));

    ahead.complete();

    REQUIRE(waitForLazLayerLoaded(&layer, kWaitTimeoutMs));
    CHECK(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);
    CHECK(limiter.queuedCount() == 0);
    CHECK(waitUntil([&limiter]() { return limiter.runningCount() == 0; }));
}

TEST_CASE("cwLazLayerBuildQueue: a model's layers build one at a time",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    AsyncFuture::Limiter* limiter = model.buildLimiter();
    REQUIRE(limiter != nullptr);

    // Held ahead of both layers, so both are seen waiting in the model's window
    auto ahead = AsyncFuture::deferred<void>();
    limiter->admit([ahead]() { return ahead.future(); });

    scanTwoLayers(model, tempDir);
    REQUIRE(waitUntil([limiter]() { return limiter->queuedCount() == kTwoLayers; }));

    ahead.complete();

    int peakRunning = 0;
    REQUIRE(waitUntil([&]() {
        peakRunning = std::max(peakRunning, limiter->runningCount());
        return loadedCount(model) == kTwoLayers;
    }, kWaitTimeoutMs));
    CHECK(peakRunning <= kOneBuildAtATime);
    CHECK(waitUntil([limiter]() { return limiter->runningCount() == 0; }));
}

TEST_CASE("cwLazLayerBuildQueue: a layer deleted while queued launches nothing",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("deleted")));
    REQUIRE(!path.isEmpty());

    AsyncFuture::Limiter limiter(AsyncFuture::Limit {kOneBuildAtATime});
    auto ahead = AsyncFuture::deferred<void>();
    limiter.admit([ahead]() { return ahead.future(); });

    auto* layer = new cwLazLayer();
    layer->setBuildLimiter(&limiter);
    layer->setCacheRootPath(tempDir.path());
    layer->setSourcePath(path);
    REQUIRE(waitUntil([&limiter]() { return limiter.queuedCount() == kOneLayer; }));

    delete layer;
    CHECK(limiter.queuedCount() == 0);

    ahead.complete();
    CHECK(waitUntil([&limiter]() { return limiter.runningCount() == 0; }));
    CHECK(holdsFor([&limiter]() {
        return limiter.runningCount() == 0 && limiter.queuedCount() == 0;
    }, kStillQueuedMs));
}

TEST_CASE("cwLazLayerBuildQueue: a model's running build holds its budget on the ledger",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();
    REQUIRE(cwMemoryBudget::instance() != nullptr);

    const cwBuildBudgetOverride override;

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    CHECK(model.buildLimiter()->limit().ledger == cwMemoryBudget::instance());
    CHECK(runningBuildCharge(model, tempDir) == override.budgetBytes());
}

TEST_CASE("cwLazLayerBuildQueue: a build budget above the ledger cap charges the cap",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();
    cwMemoryBudget* budget = cwMemoryBudget::instance();
    REQUIRE(budget != nullptr);

    const cwBuildBudgetOverride override;
    const qint64 capBytes = override.budgetBytes() / kHalf;
    const CapOverride cap(budget, capBytes);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    CHECK(runningBuildCharge(model, tempDir) == capBytes);
}

TEST_CASE("cwLazLayerBuildQueue: cancelQueuedBuilds leaves only the running build",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    AsyncFuture::Limiter* limiter = model.buildLimiter();
    REQUIRE(limiter != nullptr);

    auto ahead = AsyncFuture::deferred<void>();
    limiter->admit([ahead]() { return ahead.future(); });

    scanTwoLayers(model, tempDir);
    REQUIRE(waitUntil([limiter]() { return limiter->queuedCount() == kTwoLayers; }));

    {
        // Holds the first build at running while the second waits behind it
        const PoolBlocker blocker;
        REQUIRE(blocker.isBlocking());

        ahead.complete();
        REQUIRE(waitUntil([limiter]() {
            return limiter->runningCount() == kOneBuildAtATime
                && limiter->queuedCount() == kOneLayer;
        }));

        model.cancelQueuedBuilds();
        CHECK(limiter->queuedCount() == 0);
        CHECK(limiter->runningCount() == kOneBuildAtATime);
    }

    REQUIRE(waitUntil([&]() { return loadedCount(model) == kOneLayer; }, kWaitTimeoutMs));
    CHECK(waitUntil([limiter]() { return limiter->runningCount() == 0; }));
    CHECK(holdsFor([&]() {
        return limiter->runningCount() == 0 && loadedCount(model) == kOneLayer;
    }, kStillQueuedMs));
}

TEST_CASE("cwLazLayerBuildQueue: a build that arrives after cancelQueuedBuilds never queues",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    AsyncFuture::Limiter* limiter = model.buildLimiter();

    auto ahead = AsyncFuture::deferred<void>();
    limiter->admit([ahead]() { return ahead.future(); });

    // Each layer's cache probe is still on the pool here, so its build has
    // yet to reach the window.
    scanTwoLayers(model, tempDir);
    model.cancelQueuedBuilds();

    CHECK(holdsFor([limiter]() {
        return limiter->queuedCount() == 0 && limiter->runningCount() == kOneBuildAtATime;
    }, kStillQueuedMs));

    ahead.complete();
    CHECK(waitUntil([limiter]() { return limiter->runningCount() == 0; }));
    CHECK(holdsFor([&]() {
        return limiter->runningCount() == 0 && loadedCount(model) == 0;
    }, kStillQueuedMs));
}

TEST_CASE("cwLazLayerBuildQueue: two layers load through the window on one thread",
          "[cwLazLayerBuildQueue]") {
    cwJobSettings::initialize();
    const cwThreadCountRestorer oneThread(kSingleThread);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    scanTwoLayers(model, tempDir);

    REQUIRE(waitUntil([&]() { return loadedCount(model) == kTwoLayers; }, kOneThreadDeadlineMs));
}
