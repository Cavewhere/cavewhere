//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QSemaphore>
#include <QString>
#include <QThread>

//Std includes
#include <algorithm>
#include <limits>
#include <memory>
#include <thread>

//Our includes
#include "cwRenderMemoryLedger.h"
#include "cwTileStreamer.h"

namespace {

    constexpr int kWaitTimeoutMs = 30000;
    constexpr int kPollIntervalMs = 1;
    constexpr int kQuietPeriodMs = 30;
    constexpr int kPayloadBytes = 64;
    constexpr qint64 kTinyCpuCap = 1;
    constexpr int kGateDelayMs = 50;
    constexpr auto kCategory = cwRenderMemoryLedger::Category::PointCloudGeometry;

    struct FakeSource
    {
        int id = 0;

        bool operator==(const FakeSource& other) const = default;
    };

    using Streamer = cwTileStreamer<FakeSource, QByteArray>;

    // Shared state for the fake loader. The loader itself is copied into a
    // std::function and runs on cwConcurrent threads, so everything it touches
    // lives here behind a mutex.
    struct LoaderState
    {
        QMutex mutex;
        QSemaphore gate;
        bool gated = false;
        QList<int> calledSourceIds;
        QString errorMessage;
        int concurrent = 0;
        int maxConcurrent = 0;
        int finished = 0;

        int callCount()
        {
            QMutexLocker locker(&mutex);
            return static_cast<int>(calledSourceIds.size());
        }

        int finishedCount()
        {
            QMutexLocker locker(&mutex);
            return finished;
        }

        QList<int> sourceIds()
        {
            QMutexLocker locker(&mutex);
            return calledSourceIds;
        }
    };

    Streamer::Loader makeLoader(const std::shared_ptr<LoaderState>& state)
    {
        return [state](const FakeSource& source, int level) -> Monad::Result<QByteArray> {
            {
                QMutexLocker locker(&state->mutex);
                state->calledSourceIds.append(source.id);
                ++state->concurrent;
                state->maxConcurrent = std::max(state->maxConcurrent, state->concurrent);
            }

            if (state->gated) {
                state->gate.acquire();
            }

            QString errorMessage;
            {
                QMutexLocker locker(&state->mutex);
                --state->concurrent;
                ++state->finished;
                errorMessage = state->errorMessage;
            }

            if (!errorMessage.isEmpty()) {
                return Monad::Result<QByteArray>(errorMessage);
            }

            return QByteArray(kPayloadBytes, static_cast<char>(level));
        };
    }

    //Depends on both arguments, so an estimator handed the wrong source or the
    //wrong level shows up in the ledger and cap assertions below
    qint64 expectedBytes(const FakeSource& source, int level)
    {
        return static_cast<qint64>(kPayloadBytes) * (level + 1) + source.id;
    }

    Streamer::ByteEstimator makeEstimator()
    {
        return [](const FakeSource& source, int level) -> qint64 {
            return expectedBytes(source, level);
        };
    }

    // Declared after the streamer in gated tests so a failed assertion opens the
    // gate before the streamer's destructor waits on the loads in flight.
    struct GateOpener
    {
        static constexpr int kTeardownPermits = 64;

        std::shared_ptr<LoaderState> state;

        ~GateOpener()
        {
            state->gate.release(kTeardownPermits);
        }
    };

    template <typename Predicate>
    bool waitFor(Predicate predicate)
    {
        QElapsedTimer timer;
        timer.start();

        while (timer.elapsed() < kWaitTimeoutMs) {
            if (predicate()) {
                return true;
            }
            QThread::msleep(kPollIntervalMs);
        }

        return predicate();
    }

    // Drains until resultCount results have arrived, or the timeout expires.
    QVector<Streamer::Result> drain(Streamer& streamer, int resultCount)
    {
        QVector<Streamer::Result> results;
        waitFor([&]() {
            results += streamer.takeReady();
            return results.size() >= resultCount;
        });
        return results;
    }

    qint64 ledgerCpuBytes(cwRenderMemoryLedger::Category category = kCategory)
    {
        return cwRenderMemoryLedger::instance()->bytes(category,
                                                       cwRenderMemoryLedger::Residency::Cpu);
    }
}

TEST_CASE("cwTileStreamer loads a request and hands it back", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);

    constexpr quint32 kItemId = 7;
    constexpr int kLevel = 2;
    constexpr quint64 kPriority = 10;
    const qint64 baselineBytes = ledgerCpuBytes();

    streamer.request(kItemId, FakeSource {1}, kLevel, kPriority);

    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);
    CHECK(results.at(0).itemId == kItemId);
    CHECK(results.at(0).generation == 1);
    CHECK(results.at(0).level == kLevel);
    CHECK(results.at(0).error.isEmpty());
    CHECK(results.at(0).payload.size() == kPayloadBytes);

    CHECK_FALSE(streamer.hasWork());
    CHECK(ledgerCpuBytes() == baselineBytes);
}

TEST_CASE("cwTileStreamer ignores a repeat of what it is already doing", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 3;
    constexpr int kLevel = 1;
    constexpr quint64 kPriority = 5;
    const FakeSource source {3};

    streamer.request(kItemId, source, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kItemId, source, kLevel, kPriority);
    CHECK(state->callCount() == 1);

    state->gate.release();
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));
    QThread::msleep(kQuietPeriodMs);

    //A repeat of a result still waiting to be drained is a no-op too
    streamer.request(kItemId, source, kLevel, kPriority);
    CHECK(state->callCount() == 1);

    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);
    CHECK(results.at(0).generation == 1);
    CHECK_FALSE(streamer.hasWork());
}

TEST_CASE("cwTileStreamer supersedes a request for a different level", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 11;
    constexpr int kStaleLevel = 4;
    constexpr int kFinalLevel = 2;
    constexpr quint64 kPriority = 1;
    const FakeSource source {11};
    const qint64 baselineBytes = ledgerCpuBytes();

    streamer.request(kItemId, source, kStaleLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kItemId, source, kFinalLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));

    //Both loads are in flight, each holding the estimate for its own level
    CHECK(streamer.pending().cpuBytes
          == expectedBytes(source, kStaleLevel) + expectedBytes(source, kFinalLevel));

    state->gate.release(2);

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).level == kFinalLevel);
    CHECK(results.at(0).generation == 2);
    CHECK(streamer.takeReady().isEmpty());
    CHECK(ledgerCpuBytes() == baselineBytes);
}

TEST_CASE("cwTileStreamer keeps the superseding load while the stale one finishes",
          "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 13;
    constexpr int kStaleLevel = 5;
    constexpr int kFinalLevel = 1;
    constexpr quint64 kPriority = 1;
    const FakeSource source {13};

    streamer.request(kItemId, source, kStaleLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kItemId, source, kFinalLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));

    //Let only the stale load finish — the superseding one is still in flight
    state->gate.release();
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));
    QThread::msleep(kQuietPeriodMs);

    CHECK(streamer.hasWork());
    CHECK(streamer.takeReady().isEmpty());

    state->gate.release();

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).level == kFinalLevel);
    CHECK(results.at(0).generation == 2);
}

TEST_CASE("cwTileStreamer reloads a canceled item asked for again", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 17;
    constexpr int kLevel = 3;
    constexpr quint64 kPriority = 1;
    const FakeSource source {17};

    streamer.request(kItemId, source, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.cancel(kItemId);

    //The canceled load is still in flight, but its result is doomed, so asking
    //for the same level again has to start a fresh load
    streamer.request(kItemId, source, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));

    state->gate.release(2);

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).itemId == kItemId);
    CHECK(results.at(0).level == kLevel);
    CHECK(results.at(0).generation == 3);
    CHECK(streamer.takeReady().isEmpty());
}

TEST_CASE("cwTileStreamer runs queued requests highest priority first", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    //A one byte cap holds every request but the one already in flight
    streamer.setMaxPendingCpuBytes(kTinyCpuCap);

    constexpr int kQueuedCount = 6;
    constexpr quint32 kFirstItemId = 100;
    constexpr quint32 kPinnedItemId = 200;
    constexpr int kFirstSourceId = 1000;
    constexpr int kPinnedSourceId = 2000;
    constexpr int kLevel = 3;
    constexpr quint64 kLowestPriority = 0;
    const qint64 baselineBytes = ledgerCpuBytes();

    streamer.request(kFirstItemId, FakeSource {kFirstSourceId}, kLevel, kLowestPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    //The cap is smaller than a single payload, so the first load still went out
    CHECK(ledgerCpuBytes() > baselineBytes);

    for (int i = 0; i < kQueuedCount; i++) {
        streamer.request(static_cast<quint32>(i), FakeSource {i}, kLevel,
                         static_cast<quint64>(i));
    }

    streamer.request(kPinnedItemId, FakeSource {kPinnedSourceId}, kLevel,
                     std::numeric_limits<quint64>::max());

    //The cap keeps everything queued while the first load is in flight
    QThread::msleep(kQuietPeriodMs);
    CHECK(state->callCount() == 1);

    //One release and one drain per load: the first, the pinned one, and the rest
    for (int i = 0; i < kQueuedCount + 2; i++) {
        state->gate.release();
        REQUIRE(drain(streamer, 1).size() == 1);
    }

    CHECK_FALSE(streamer.hasWork());

    const QList<int> sourceIds = state->sourceIds();
    REQUIRE(sourceIds.size() == kQueuedCount + 2);
    CHECK(sourceIds.at(0) == kFirstSourceId);
    CHECK(sourceIds.at(1) == kPinnedSourceId);
    for (int i = 0; i < kQueuedCount; i++) {
        CHECK(sourceIds.at(i + 2) == kQueuedCount - 1 - i);
    }

    CHECK(ledgerCpuBytes() == baselineBytes);
}

TEST_CASE("cwTileStreamer lets one load through a cap smaller than a payload", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    streamer.setMaxPendingCpuBytes(kTinyCpuCap);

    constexpr quint32 kFirstItemId = 31;
    constexpr quint32 kSecondItemId = 37;
    constexpr int kLevel = 0;
    constexpr quint64 kPriority = 1;

    streamer.request(kFirstItemId, FakeSource {31}, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kSecondItemId, FakeSource {37}, kLevel, kPriority);
    QThread::msleep(kQuietPeriodMs);
    CHECK(state->callCount() == 1);

    state->gate.release();
    REQUIRE(drain(streamer, 1).size() == 1);

    //Draining frees the cap for the next load
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));
    state->gate.release();
    REQUIRE(drain(streamer, 1).size() == 1);
    CHECK_FALSE(streamer.hasWork());
}

TEST_CASE("cwTileStreamer runs exactly kMaxConcurrentLoads at once", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr int kExtraRequests = 2;
    constexpr int kRequestCount = Streamer::kMaxConcurrentLoads + kExtraRequests;

    for (int i = 0; i < kRequestCount; i++) {
        streamer.request(static_cast<quint32>(i), FakeSource {i}, 0, static_cast<quint64>(i));
    }

    //The streamer fills up to the limit and stops there
    REQUIRE(waitFor([&]() { return state->callCount() == Streamer::kMaxConcurrentLoads; }));
    QThread::msleep(kQuietPeriodMs);
    CHECK(state->callCount() == Streamer::kMaxConcurrentLoads);

    state->gate.release(kRequestCount);
    REQUIRE(drain(streamer, kRequestCount).size() == kRequestCount);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    QMutexLocker locker(&state->mutex);
    CHECK(state->maxConcurrent == Streamer::kMaxConcurrentLoads);
}

TEST_CASE("cwTileStreamer reports a failed load", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    const QString errorMessage = QStringLiteral("the tile did not load");
    state->errorMessage = errorMessage;

    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);

    constexpr quint32 kItemId = 23;
    constexpr int kLevel = 0;
    constexpr quint64 kPriority = 1;
    const qint64 baselineBytes = ledgerCpuBytes();

    streamer.request(kItemId, FakeSource {23}, kLevel, kPriority);

    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);
    CHECK(results.at(0).itemId == kItemId);
    CHECK(results.at(0).error == errorMessage);
    CHECK(results.at(0).payload.isEmpty());

    //A failed load holds no payload, so it never counted against the ledger
    CHECK(ledgerCpuBytes() == baselineBytes);
}

TEST_CASE("cwTileStreamer reports pending payload bytes under its own category",
          "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 29;
    constexpr int kLevel = 1;
    constexpr quint64 kPriority = 1;
    const FakeSource source {29};
    const qint64 baselineBytes = ledgerCpuBytes();
    const qint64 otherBaselineBytes =
        ledgerCpuBytes(cwRenderMemoryLedger::Category::TexturedItemTexture);

    streamer.request(kItemId, source, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    CHECK(ledgerCpuBytes() == baselineBytes + expectedBytes(source, kLevel));
    CHECK(ledgerCpuBytes(cwRenderMemoryLedger::Category::TexturedItemTexture)
          == otherBaselineBytes);

    //The bytes stay reported while the finished payload waits to be drained
    state->gate.release();
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));
    REQUIRE(waitFor([&]() {
        return ledgerCpuBytes() == baselineBytes + expectedBytes(source, kLevel);
    }));

    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);

    CHECK_FALSE(streamer.hasWork());
    CHECK(ledgerCpuBytes() == baselineBytes);

    const Streamer::Pending pending = streamer.pending();
    CHECK(pending.loads == 0);
    CHECK(pending.cpuBytes == 0);
}

TEST_CASE("cwTileStreamer ignores a repeat of a request still queued", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    //A one byte cap holds the second request in the queue
    streamer.setMaxPendingCpuBytes(kTinyCpuCap);

    constexpr quint32 kRunningItemId = 51;
    constexpr quint32 kQueuedItemId = 53;
    constexpr int kLevel = 2;
    constexpr quint64 kPriority = 1;
    const FakeSource queuedSource {53};

    streamer.request(kRunningItemId, FakeSource {51}, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kQueuedItemId, queuedSource, kLevel, kPriority);
    streamer.request(kQueuedItemId, queuedSource, kLevel, kPriority);

    QThread::msleep(kQuietPeriodMs);
    CHECK(state->callCount() == 1);
    CHECK(streamer.pending().loads == 2);

    state->gate.release();
    REQUIRE(drain(streamer, 1).size() == 1);

    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));
    state->gate.release();
    REQUIRE(drain(streamer, 1).size() == 1);

    CHECK(state->callCount() == 2);
    CHECK_FALSE(streamer.hasWork());
}

TEST_CASE("cwTileStreamer cancel returns while the load is still running", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 59;
    constexpr int kLevel = 1;
    constexpr quint64 kPriority = 1;

    streamer.request(kItemId, FakeSource {59}, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.cancel(kItemId);
    CHECK(state->finishedCount() == 0);

    state->gate.release();
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));
    QThread::msleep(kQuietPeriodMs);

    CHECK(streamer.takeReady().isEmpty());
    CHECK_FALSE(streamer.hasWork());
}

TEST_CASE("cwTileStreamer cancel drops a result waiting to be drained", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 61;
    constexpr int kLevel = 3;
    constexpr quint64 kPriority = 1;
    const FakeSource source {61};
    const qint64 baselineBytes = ledgerCpuBytes();

    streamer.request(kItemId, source, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    state->gate.release();
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));
    REQUIRE(waitFor([&]() {
        return ledgerCpuBytes() == baselineBytes + expectedBytes(source, kLevel);
    }));

    streamer.cancel(kItemId);

    CHECK(streamer.takeReady().isEmpty());
    CHECK_FALSE(streamer.hasWork());
    CHECK(streamer.pending().cpuBytes == 0);
    CHECK(ledgerCpuBytes() == baselineBytes);
}

TEST_CASE("cwTileStreamer cancelAll waits for the load in flight", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    Streamer streamer(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 67;
    constexpr int kLevel = 1;
    constexpr quint64 kPriority = 1;

    streamer.request(kItemId, FakeSource {67}, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    QElapsedTimer timer;
    timer.start();

    std::thread opener([state]() {
        QThread::msleep(kGateDelayMs);
        state->gate.release();
    });

    streamer.cancelAll();
    const qint64 elapsed = timer.elapsed();
    opener.join();

    CHECK(elapsed >= kGateDelayMs);
    CHECK(state->finishedCount() == 1);
    CHECK_FALSE(streamer.hasWork());
}

TEST_CASE("cwTileStreamer destructor waits for the load in flight", "[TileStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;

    auto streamer = std::make_unique<Streamer>(makeLoader(state), makeEstimator(), kCategory);
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 71;
    constexpr int kLevel = 1;
    constexpr quint64 kPriority = 1;
    const qint64 baselineBytes = ledgerCpuBytes();

    streamer->request(kItemId, FakeSource {71}, kLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    QElapsedTimer timer;
    timer.start();

    std::thread opener([state]() {
        QThread::msleep(kGateDelayMs);
        state->gate.release();
    });

    streamer.reset();
    const qint64 elapsed = timer.elapsed();
    opener.join();

    CHECK(elapsed >= kGateDelayMs);
    CHECK(state->finishedCount() == 1);
    CHECK(ledgerCpuBytes() == baselineBytes);
}
