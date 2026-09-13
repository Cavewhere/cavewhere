//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QWaitCondition>

//Std includes
#include <limits>
#include <memory>

//Our includes
#include "cwRenderMemoryLedger.h"
#include "cwStreamedTexture.h"
#include "cwMipMath.h"
#include "cwTextureStreamer.h"

namespace {

    constexpr int kTextureDimension = 1024;
    constexpr QRhiTexture::Format kTargetFormat = QRhiTexture::BC7;
    constexpr int kWaitTimeoutMs = 30000;
    constexpr int kPollIntervalMs = 1;
    constexpr int kQuietPeriodMs = 30;
    constexpr int kLevelBytes = 16;
    constexpr qint64 kTinyCpuCap = 1;
    const QString kRelativeRootName = QStringLiteral("streamed-root");

    // Shared state for the fake loader. The loader itself is copied into a
    // std::function and runs on cwConcurrent threads, so everything it touches
    // lives here behind a mutex.
    struct LoaderState
    {
        QMutex mutex;
        QWaitCondition gateChanged;
        // How many loads, counted in the order the loader was entered, may run
        int releasedCalls = 0;
        bool gated = false;
        QStringList calledSourceIds;
        QString errorMessage;
        int concurrent = 0;
        int maxConcurrent = 0;
        int finished = 0;
        int sleepMs = 0;

        int callCount()
        {
            QMutexLocker locker(&mutex);
            return static_cast<int>(calledSourceIds.size());
        }

        // Let the next count loads finish, oldest call first. Ordered rather
        // than a semaphore: with two loads waiting, a semaphore hands its
        // permit to whichever thread the scheduler picks, so a test that says
        // "let the stale one finish" sometimes finished the superseding one.
        void releaseCalls(int count = 1)
        {
            {
                QMutexLocker locker(&mutex);
                releasedCalls += count;
            }
            gateChanged.wakeAll();
        }

        int finishedCount()
        {
            QMutexLocker locker(&mutex);
            return finished;
        }

        QStringList sourceIds()
        {
            QMutexLocker locker(&mutex);
            return calledSourceIds;
        }
    };

    cwTextureStreamer::Loader makeLoader(const std::shared_ptr<LoaderState>& state)
    {
        return [state](const cwStreamedTexture& source,
                       QRhiTexture::Format target,
                       int firstLevel) -> Monad::Result<cwCompressedTexture> {
            int ticket = 0;
            {
                QMutexLocker locker(&state->mutex);
                ticket = static_cast<int>(state->calledSourceIds.size());
                state->calledSourceIds.append(source.key.id);
                ++state->concurrent;
                state->maxConcurrent = std::max(state->maxConcurrent, state->concurrent);
            }

            if (state->gated) {
                QMutexLocker locker(&state->mutex);
                while (state->releasedCalls <= ticket) {
                    state->gateChanged.wait(&state->mutex);
                }
            }

            if (state->sleepMs > 0) {
                QThread::msleep(static_cast<unsigned long>(state->sleepMs));
            }

            QString errorMessage;
            {
                QMutexLocker locker(&state->mutex);
                --state->concurrent;
                ++state->finished;
                errorMessage = state->errorMessage;
            }

            if (!errorMessage.isEmpty()) {
                return Monad::Result<cwCompressedTexture>(errorMessage);
            }

            cwCompressedTexture texture;
            texture.format = target;
            texture.size = cw::mip::mipLevelSize(source.size, firstLevel);

            const int levelCount = cw::mip::mipLevelCount(source.size) - firstLevel;
            for (int i = 0; i < levelCount; i++) {
                texture.mipLevels.append(QByteArray(kLevelBytes, '\0'));
            }

            return texture;
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
            state->releaseCalls(kTeardownPermits);
        }
    };

    cwStreamedTexture makeSource(const QString& id)
    {
        cwStreamedTexture source;
        source.setDataRootPath(QStringLiteral("/not/read/by/the/fake/loader"));
        source.key.id = id;
        source.key.path = QStringLiteral("textures");
        source.key.checksum = QStringLiteral("checksum-") + id;
        source.size = QSize(kTextureDimension, kTextureDimension);
        return source;
    }

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
    QVector<cwTextureStreamer::Result> drain(cwTextureStreamer& streamer, int resultCount)
    {
        QVector<cwTextureStreamer::Result> results;
        waitFor([&]() {
            results += streamer.takeReady();
            return results.size() >= resultCount;
        });
        return results;
    }

    qint64 ledgerCpuBytes()
    {
        return cwRenderMemoryLedger::instance()->bytes(
            cwRenderMemoryLedger::Category::TexturedItemTexture,
            cwRenderMemoryLedger::Residency::Cpu);
    }
}

TEST_CASE("cwTextureStreamer loads a request and hands it back", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    cwTextureStreamer streamer(makeLoader(state));

    constexpr quint32 kItemId = 7;
    constexpr int kTopLevel = 2;
    constexpr quint64 kPriority = 10;

    const cwStreamedTexture source = makeSource(QStringLiteral("item-7"));
    streamer.request(kItemId, source, kTargetFormat, kTopLevel, kPriority);

    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);
    CHECK(results.at(0).itemId == kItemId);
    CHECK(results.at(0).generation == 1);
    CHECK(results.at(0).topLevel == kTopLevel);
    CHECK(results.at(0).error.isEmpty());
    CHECK_FALSE(results.at(0).texture.isNull());
    CHECK(results.at(0).texture.size == cw::mip::mipLevelSize(source.size, kTopLevel));

    CHECK_FALSE(streamer.hasWork());
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer ignores a repeat of what it is already doing", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    cwTextureStreamer streamer(makeLoader(state));
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 3;
    constexpr int kTopLevel = 1;
    constexpr quint64 kPriority = 5;

    const cwStreamedTexture source = makeSource(QStringLiteral("item-3"));
    streamer.request(kItemId, source, kTargetFormat, kTopLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kItemId, source, kTargetFormat, kTopLevel, kPriority);
    CHECK(state->callCount() == 1);

    state->releaseCalls();
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));
    QThread::msleep(kQuietPeriodMs);

    //A repeat of a result still waiting to be drained is a no-op too
    streamer.request(kItemId, source, kTargetFormat, kTopLevel, kPriority);
    CHECK(state->callCount() == 1);

    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);
    CHECK(results.at(0).generation == 1);
    CHECK_FALSE(streamer.hasWork());
}

TEST_CASE("cwStreamedTexture treats every spelling of one data root as one root",
          "[TextureStreamer]") {
    // A root under the working directory, so the relative spelling names it too
    const QString absoluteRoot = QDir::current().absoluteFilePath(kRelativeRootName);

    cwStreamedTexture canonical = makeSource(QStringLiteral("item-root"));
    canonical.setDataRootPath(absoluteRoot);

    SECTION("a trailing slash is the same root") {
        cwStreamedTexture other = canonical;
        other.setDataRootPath(absoluteRoot + QStringLiteral("/"));
        CHECK(other.dataRootPath() == absoluteRoot);
        CHECK(other == canonical);
    }

    SECTION("dot segments are the same root") {
        cwStreamedTexture other = canonical;
        other.setDataRootPath(absoluteRoot + QStringLiteral("/./sub/.."));
        CHECK(other.dataRootPath() == absoluteRoot);
        CHECK(other == canonical);
    }

    SECTION("the relative spelling is the same root") {
        cwStreamedTexture other = canonical;
        other.setDataRootPath(kRelativeRootName);
        CHECK(other.dataRootPath() == absoluteRoot);
        CHECK(other == canonical);
    }

    SECTION("the constructor normalizes as the setter does") {
        const cwStreamedTexture constructed(absoluteRoot + QStringLiteral("/"),
                                            canonical.key,
                                            canonical.size);
        CHECK(constructed.dataRootPath() == absoluteRoot);
        CHECK(constructed == canonical);
    }

    SECTION("an empty root stays empty instead of becoming the working directory") {
        cwStreamedTexture other = canonical;
        other.setDataRootPath(QString());
        CHECK(other.dataRootPath().isEmpty());
        CHECK(other.isNull());
    }
}

TEST_CASE("cwTextureStreamer dedups a repeat whose data root is spelled differently",
          "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    cwTextureStreamer streamer(makeLoader(state));
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 21;
    constexpr int kTopLevel = 1;
    constexpr quint64 kPriority = 5;

    const QString absoluteRoot = QDir::current().absoluteFilePath(kRelativeRootName);

    cwStreamedTexture source = makeSource(QStringLiteral("item-21"));
    source.setDataRootPath(absoluteRoot);
    streamer.request(kItemId, source, kTargetFormat, kTopLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    cwStreamedTexture respelled = source;
    respelled.setDataRootPath(kRelativeRootName);
    streamer.request(kItemId, respelled, kTargetFormat, kTopLevel, kPriority);
    CHECK(state->callCount() == 1);

    state->releaseCalls();
    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);
    CHECK(results.at(0).generation == 1);
    CHECK_FALSE(streamer.hasWork());
}

TEST_CASE("cwTextureStreamer supersedes a request for a different level", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    cwTextureStreamer streamer(makeLoader(state));
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 11;
    constexpr int kStaleLevel = 4;
    constexpr int kFinalLevel = 2;
    constexpr quint64 kPriority = 1;

    const cwStreamedTexture source = makeSource(QStringLiteral("item-11"));
    streamer.request(kItemId, source, kTargetFormat, kStaleLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kItemId, source, kTargetFormat, kFinalLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));

    state->releaseCalls(2);

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).topLevel == kFinalLevel);
    CHECK(results.at(0).generation == 2);
    CHECK(streamer.takeReady().isEmpty());
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer keeps the superseding load while the stale one finishes",
          "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    cwTextureStreamer streamer(makeLoader(state));
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 13;
    constexpr int kStaleLevel = 5;
    constexpr int kFinalLevel = 1;
    constexpr quint64 kPriority = 1;

    const cwStreamedTexture source = makeSource(QStringLiteral("item-13"));
    streamer.request(kItemId, source, kTargetFormat, kStaleLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.request(kItemId, source, kTargetFormat, kFinalLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));

    //Let only the stale load finish — the superseding one is still in flight
    state->releaseCalls();
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));
    QThread::msleep(kQuietPeriodMs);

    CHECK(streamer.hasWork());
    CHECK(streamer.takeReady().isEmpty());

    state->releaseCalls();

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).topLevel == kFinalLevel);
    CHECK(results.at(0).generation == 2);
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer hands back only the newest result for an item",
          "[TextureStreamer]") {
    //One slot per item id: asking for a new level forgets the result the old
    //load already published, so a drain never sees a stale chain for an item.
    //That is what lets cwStreamedItemState match a landing chain on its level
    //alone, with no generation guard of its own.
    auto state = std::make_shared<LoaderState>();
    cwTextureStreamer streamer(makeLoader(state));

    constexpr quint32 kItemId = 19;
    constexpr int kStaleLevel = 4;
    constexpr int kFinalLevel = 1;
    constexpr quint64 kPriority = 1;

    const cwStreamedTexture source = makeSource(QStringLiteral("item-19"));
    streamer.request(kItemId, source, kTargetFormat, kStaleLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->finishedCount() == 1; }));

    //The first chain is sitting in the ready queue, undrained, when the camera
    //asks for a finer one
    streamer.request(kItemId, source, kTargetFormat, kFinalLevel, kPriority);

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).itemId == kItemId);
    CHECK(results.at(0).topLevel == kFinalLevel);
    CHECK(streamer.takeReady().isEmpty());
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer reloads a canceled item asked for again", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    cwTextureStreamer streamer(makeLoader(state));
    GateOpener gateOpener {state};

    constexpr quint32 kItemId = 17;
    constexpr int kTopLevel = 3;
    constexpr quint64 kPriority = 1;

    const cwStreamedTexture source = makeSource(QStringLiteral("item-17"));
    streamer.request(kItemId, source, kTargetFormat, kTopLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));

    streamer.cancel(kItemId);

    //The canceled load is still in flight, but its result is doomed, so asking
    //for the same levels again has to start a fresh load
    streamer.request(kItemId, source, kTargetFormat, kTopLevel, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));

    state->releaseCalls(2);

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).itemId == kItemId);
    CHECK(results.at(0).topLevel == kTopLevel);
    CHECK(results.at(0).generation == 3);
    CHECK(streamer.takeReady().isEmpty());
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer runs queued requests highest priority first", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    cwTextureStreamer streamer(makeLoader(state));
    GateOpener gateOpener {state};

    //A one byte cap holds every request but the one already in flight
    streamer.setMaxPendingCpuBytes(kTinyCpuCap);

    constexpr int kQueuedCount = 6;
    constexpr quint32 kFirstItemId = 100;
    constexpr quint32 kPinnedItemId = 200;
    constexpr int kTopLevel = 3;
    constexpr quint64 kLowestPriority = 0;

    const cwStreamedTexture firstSource = makeSource(QStringLiteral("first"));
    streamer.request(kFirstItemId, firstSource, kTargetFormat, kTopLevel, kLowestPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 1; }));
    CHECK(ledgerCpuBytes() > 0);

    for (int i = 0; i < kQueuedCount; i++) {
        const QString id = QStringLiteral("queued-%1").arg(i);
        streamer.request(static_cast<quint32>(i), makeSource(id), kTargetFormat, kTopLevel,
                         static_cast<quint64>(i));
    }

    streamer.request(kPinnedItemId, makeSource(QStringLiteral("pinned")), kTargetFormat, kTopLevel,
                     std::numeric_limits<quint64>::max());

    //The cap keeps everything queued while the first load is in flight
    QThread::msleep(kQuietPeriodMs);
    CHECK(state->callCount() == 1);

    //One release and one drain per load: the first, the pinned one, and the rest
    for (int i = 0; i < kQueuedCount + 2; i++) {
        state->releaseCalls();
        REQUIRE(drain(streamer, 1).size() == 1);
    }

    CHECK_FALSE(streamer.hasWork());

    const QStringList sourceIds = state->sourceIds();
    REQUIRE(sourceIds.size() == kQueuedCount + 2);
    CHECK(sourceIds.at(0) == QStringLiteral("first"));
    CHECK(sourceIds.at(1) == QStringLiteral("pinned"));
    for (int i = 0; i < kQueuedCount; i++) {
        CHECK(sourceIds.at(i + 2) == QStringLiteral("queued-%1").arg(kQueuedCount - 1 - i));
    }

    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer runs at most kMaxConcurrentLoads at once", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    constexpr int kSleepMs = 2;
    constexpr int kRequestCount = 24;
    state->sleepMs = kSleepMs;

    cwTextureStreamer streamer(makeLoader(state));

    for (int i = 0; i < kRequestCount; i++) {
        streamer.request(static_cast<quint32>(i),
                         makeSource(QStringLiteral("item-%1").arg(i)),
                         kTargetFormat, 0, static_cast<quint64>(i));
    }

    REQUIRE(drain(streamer, kRequestCount).size() == kRequestCount);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    QMutexLocker locker(&state->mutex);
    CHECK(state->maxConcurrent <= cwTextureStreamer::kMaxConcurrentLoads);
}

TEST_CASE("cwTextureStreamer forgets canceled items", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    state->gated = true;
    cwTextureStreamer streamer(makeLoader(state));
    GateOpener gateOpener {state};

    constexpr quint32 kCanceledItemId = 1;
    constexpr quint32 kKeptItemId = 2;
    constexpr quint64 kPriority = 1;

    streamer.request(kCanceledItemId, makeSource(QStringLiteral("canceled")), kTargetFormat, 0,
                     kPriority);
    streamer.request(kKeptItemId, makeSource(QStringLiteral("kept")), kTargetFormat, 0, kPriority);
    REQUIRE(waitFor([&]() { return state->callCount() == 2; }));

    streamer.cancel(kCanceledItemId);
    state->releaseCalls(2);

    const auto results = drain(streamer, 1);
    REQUIRE(waitFor([&]() { return !streamer.hasWork(); }));

    REQUIRE(results.size() == 1);
    CHECK(results.at(0).itemId == kKeptItemId);
    CHECK(streamer.takeReady().isEmpty());
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer cancelAll waits for the loads in flight", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    constexpr int kSleepMs = 5;
    constexpr int kRequestCount = 8;
    state->sleepMs = kSleepMs;

    cwTextureStreamer streamer(makeLoader(state));

    for (int i = 0; i < kRequestCount; i++) {
        streamer.request(static_cast<quint32>(i),
                         makeSource(QStringLiteral("item-%1").arg(i)),
                         kTargetFormat, 0, static_cast<quint64>(i));
    }

    streamer.cancelAll();

    CHECK_FALSE(streamer.hasWork());
    CHECK(streamer.takeReady().isEmpty());
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer destroyed with loads in flight returns cleanly", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    constexpr int kSleepMs = 5;
    constexpr int kRequestCount = 8;
    state->sleepMs = kSleepMs;

    {
        cwTextureStreamer streamer(makeLoader(state));
        for (int i = 0; i < kRequestCount; i++) {
            streamer.request(static_cast<quint32>(i),
                             makeSource(QStringLiteral("item-%1").arg(i)),
                             kTargetFormat, 0, static_cast<quint64>(i));
        }
    }

    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer reports a failed load", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    const QString errorMessage = QStringLiteral("missing-entry.ktx2");
    state->errorMessage = errorMessage;

    cwTextureStreamer streamer(makeLoader(state));

    constexpr quint32 kItemId = 4;
    constexpr quint64 kPriority = 1;
    streamer.request(kItemId, makeSource(QStringLiteral("broken")), kTargetFormat, 0, kPriority);

    const auto results = drain(streamer, 1);
    REQUIRE(results.size() == 1);
    CHECK(results.at(0).error == errorMessage);
    CHECK(results.at(0).texture.isNull());
    CHECK_FALSE(streamer.hasWork());
    CHECK(ledgerCpuBytes() == 0);
}

TEST_CASE("cwTextureStreamer survives a storm of supersedes", "[TextureStreamer]") {
    auto state = std::make_shared<LoaderState>();
    cwTextureStreamer streamer(makeLoader(state));

    constexpr int kRequestCount = 200;
    constexpr quint32 kItemCount = 50;
    constexpr int kMaxTopLevel = 4;
    constexpr quint64 kMaxPriority = 100;

    QRandomGenerator random(QCoreApplication::applicationPid());

    for (int i = 0; i < kRequestCount; i++) {
        const quint32 itemId = random.bounded(kItemCount);
        const int topLevel = static_cast<int>(random.bounded(kMaxTopLevel));
        const quint64 priority = random.bounded(kMaxPriority);

        streamer.request(itemId, makeSource(QStringLiteral("item-%1").arg(itemId)), kTargetFormat,
                         topLevel, priority);
        streamer.takeReady();
    }

    REQUIRE(waitFor([&]() {
        streamer.takeReady();
        return !streamer.hasWork();
    }));

    CHECK(streamer.takeReady().isEmpty());
    CHECK_FALSE(streamer.hasWork());
    CHECK(ledgerCpuBytes() == 0);
}
