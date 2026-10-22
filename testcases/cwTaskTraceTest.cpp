//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTaskTrace.h"
#include "cwConcurrent.h"
#include "cwTask.h"

//Qt includes
#include <QFuture>
#include <QSemaphore>
#include <QString>
#include <QThreadPool>

//Std includes
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>

namespace {
constexpr int kSingleThread = 1;
constexpr int kWaitTimeoutMs = 10000;
constexpr int kPollSliceMs = 1;
constexpr qint64 kStartMs = 1000;
constexpr qint64 kJustBeforeWindowMs = kTaskTraceQuietWindowMs - 1;
constexpr quint64 kFootprintBytes = 1048576;
constexpr quint64 kResidentBytes = 2097152;
constexpr quint64 kGpuBytes = 3145728;
constexpr quint64 kAvailableBytes = 4194304;

//! Restores the shared worker pool's thread count on every path out of a test
struct ThreadCountRestorer {
    explicit ThreadCountRestorer(int threadCount) :
        m_previous(cwTask::threadPool()->maxThreadCount())
    {
        cwTask::threadPool()->setMaxThreadCount(threadCount);
    }

    ~ThreadCountRestorer()
    {
        cwTask::threadPool()->setMaxThreadCount(m_previous);
    }

    ThreadCountRestorer(const ThreadCountRestorer&) = delete;
    ThreadCountRestorer& operator=(const ThreadCountRestorer&) = delete;

private:
    int m_previous;
};

//! Polls without QFuture::waitForFinished, which can steal a queued task and run it on this thread
bool waitUntil(const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kWaitTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollSliceMs));
    }
    return condition();
}

cwTaskTrace::ContextCounts countsFor(const QString& context)
{
    const QList<cwTaskTrace::ContextCounts> all = cwTaskTrace::contextCounts();
    for (const cwTaskTrace::ContextCounts& counts : all) {
        if (counts.context == context) {
            return counts;
        }
    }
    return cwTaskTrace::ContextCounts{context, 0, 0};
}

bool countsEqual(const QString& context, int queued, int running)
{
    const cwTaskTrace::ContextCounts counts = countsFor(context);
    return counts.queued == queued && counts.running == running;
}

cwTaskQuietState quietState()
{
    cwTaskQuietState state;
    state.projectLoaded = true;
    state.coordinatorRunning = false;
    state.automaticUpdate = true;
    state.needsUpdate = false;
    state.futureCount = 0;
    state.activeThreadCount = 0;
    state.queuedTaskCount = 0;
    state.runningTaskCount = 0;
    return state;
}
}

TEST_CASE("Task context carries across a nested cwConcurrent::run", "[cwTaskTrace]")
{
    const QString context = QStringLiteral("trace-nested");

    QFuture<QFuture<QString>> outer;
    {
        cwTaskContextScope scope(context);
        CHECK(cwTaskContextScope::current() == context);
        outer = cwConcurrent::run([]() {
            return cwConcurrent::run([]() {
                return cwTaskContextScope::current();
            });
        });
    }

    CHECK(cwTaskContextScope::current() == QStringLiteral("untagged"));

    REQUIRE(waitUntil([&]() { return outer.isFinished(); }));
    const QFuture<QString> inner = outer.result();
    REQUIRE(waitUntil([&]() { return inner.isFinished(); }));
    CHECK(inner.result() == context);
}

TEST_CASE("Task context scopes nest and restore the outer context", "[cwTaskTrace]")
{
    const QString outerContext = QStringLiteral("trace-outer");
    const QString innerContext = QStringLiteral("trace-inner");

    cwTaskContextScope outerScope(outerContext);
    {
        cwTaskContextScope innerScope(innerContext);
        CHECK(cwTaskContextScope::current() == innerContext);
    }
    CHECK(cwTaskContextScope::current() == outerContext);
}

TEST_CASE("Untagged submission reports untagged", "[cwTaskTrace]")
{
    ThreadCountRestorer restorer(kSingleThread);

    const QString context = QStringLiteral("trace-before-untagged");

    QFuture<QString> tagged;
    {
        cwTaskContextScope scope(context);
        tagged = cwConcurrent::run([]() {
            return cwTaskContextScope::current();
        });
    }
    REQUIRE(waitUntil([&]() { return tagged.isFinished(); }));
    CHECK(tagged.result() == context);

    // The single worker just ran a tagged task, so this also checks the worker's tag is cleared.
    QSemaphore release;
    QFuture<QString> untagged = cwConcurrent::run([&release]() {
        const QString current = cwTaskContextScope::current();
        release.acquire();
        return current;
    });

    CHECK(waitUntil([]() { return countsFor(QStringLiteral("untagged")).running >= 1; }));

    release.release();
    REQUIRE(waitUntil([&]() { return untagged.isFinished(); }));
    CHECK(untagged.result() == QStringLiteral("untagged"));
}

TEST_CASE("Queued and running counts follow a saturated pool", "[cwTaskTrace]")
{
    REQUIRE(waitUntil([]() { return cwTask::threadPool()->activeThreadCount() == 0; }));
    ThreadCountRestorer restorer(kSingleThread);

    const QString context = QStringLiteral("trace-saturated");
    CHECK(countsEqual(context, 0, 0));

    QSemaphore releaseFirst;
    QSemaphore releaseSecond;
    QFuture<void> first;
    QFuture<void> second;
    {
        cwTaskContextScope scope(context);
        first = cwConcurrent::run([&releaseFirst]() { releaseFirst.acquire(); });
        second = cwConcurrent::run([&releaseSecond]() { releaseSecond.acquire(); });
    }

    CHECK(waitUntil([&]() { return countsEqual(context, 1, 1); }));

    releaseFirst.release();
    CHECK(waitUntil([&]() { return countsEqual(context, 0, 1); }));
    CHECK(waitUntil([&]() { return first.isFinished(); }));

    releaseSecond.release();
    CHECK(waitUntil([&]() { return second.isFinished(); }));
    CHECK(waitUntil([&]() { return countsEqual(context, 0, 0); }));
}

TEST_CASE("A task canceled before it runs leaves the queue", "[cwTaskTrace]")
{
    REQUIRE(waitUntil([]() { return cwTask::threadPool()->activeThreadCount() == 0; }));
    ThreadCountRestorer restorer(kSingleThread);

    const QString context = QStringLiteral("trace-canceled");

    QSemaphore releaseBlocker;
    std::atomic<bool> canceledRan{false};
    QFuture<void> blocker;
    QFuture<void> canceled;
    {
        cwTaskContextScope scope(context);
        blocker = cwConcurrent::run([&releaseBlocker]() { releaseBlocker.acquire(); });
        canceled = cwConcurrent::run([&canceledRan]() { canceledRan = true; });
    }

    CHECK(waitUntil([&]() { return countsEqual(context, 1, 1); }));

    canceled.cancel();
    releaseBlocker.release();

    CHECK(waitUntil([&]() { return blocker.isFinished() && canceled.isFinished(); }));
    CHECK(waitUntil([&]() { return countsEqual(context, 0, 0); }));
    CHECK_FALSE(canceledRan);
}

TEST_CASE("Quiet state requires all four idle conditions", "[cwTaskTrace]")
{
    CHECK(quietState().isQuiet());

    SECTION("Project still loading") {
        cwTaskQuietState state = quietState();
        state.projectLoaded = false;
        CHECK_FALSE(state.isQuiet());
    }

    SECTION("Coordinator running") {
        cwTaskQuietState state = quietState();
        state.coordinatorRunning = true;
        CHECK_FALSE(state.isQuiet());
    }

    SECTION("Needs update with automatic update on") {
        cwTaskQuietState state = quietState();
        state.needsUpdate = true;
        CHECK_FALSE(state.isQuiet());
    }

    SECTION("Needs update with automatic update off") {
        cwTaskQuietState state = quietState();
        state.automaticUpdate = false;
        state.needsUpdate = true;
        CHECK(state.isQuiet());
    }

    SECTION("Futures in the future manager") {
        cwTaskQuietState state = quietState();
        state.futureCount = 1;
        CHECK_FALSE(state.isQuiet());
    }

    SECTION("Active pool threads") {
        cwTaskQuietState state = quietState();
        state.activeThreadCount = 1;
        CHECK_FALSE(state.isQuiet());
    }

    SECTION("Queued tasks") {
        cwTaskQuietState state = quietState();
        state.queuedTaskCount = 1;
        CHECK_FALSE(state.isQuiet());
    }

    SECTION("Running tasks") {
        cwTaskQuietState state = quietState();
        state.runningTaskCount = 1;
        CHECK_FALSE(state.isQuiet());
    }
}

TEST_CASE("Idle holds only after the quiet window", "[cwTaskTrace]")
{
    cwTaskIdleWindow window;

    CHECK_FALSE(window.update(quietState(), kStartMs));
    CHECK_FALSE(window.update(quietState(), kStartMs + kJustBeforeWindowMs));
    CHECK(window.update(quietState(), kStartMs + kTaskTraceQuietWindowMs));

    SECTION("Work during the window restarts it") {
        cwTaskQuietState busy = quietState();
        busy.queuedTaskCount = 1;
        const qint64 busyMs = kStartMs + kTaskTraceQuietWindowMs + 1;
        CHECK_FALSE(window.update(busy, busyMs));

        const qint64 quietAgainMs = busyMs + 1;
        CHECK_FALSE(window.update(quietState(), quietAgainMs));
        CHECK_FALSE(window.update(quietState(), quietAgainMs + kJustBeforeWindowMs));
        CHECK(window.update(quietState(), quietAgainMs + kTaskTraceQuietWindowMs));
    }
}

TEST_CASE("Memory event message lists gpu only when the sample has it", "[cwTaskTrace]")
{
    cwTaskMemorySample sample;
    sample.footprint = kFootprintBytes;
    sample.resident = kResidentBytes;

    CHECK(sample.message() == QStringLiteral("mem footprint=%1 resident=%2")
                                  .arg(kFootprintBytes)
                                  .arg(kResidentBytes));

    SECTION("With a GPU reading") {
        sample.gpu = kGpuBytes;
        CHECK(sample.message() == QStringLiteral("mem footprint=%1 resident=%2 gpu=%3")
                                      .arg(kFootprintBytes)
                                      .arg(kResidentBytes)
                                      .arg(kGpuBytes));
    }

    SECTION("With a GPU reading and the iOS budget") {
        sample.gpu = kGpuBytes;
        sample.available = kAvailableBytes;
        CHECK(sample.message() == QStringLiteral("mem footprint=%1 resident=%2 gpu=%3 available=%4")
                                      .arg(kFootprintBytes)
                                      .arg(kResidentBytes)
                                      .arg(kGpuBytes)
                                      .arg(kAvailableBytes));
    }
}

TEST_CASE("Memory sample carries the GPU reading the sampler reports", "[cwTaskTrace]")
{
    const std::optional<cwTaskMemorySample> sample = cwTaskTrace::memorySample();

#ifdef Q_OS_APPLE
    REQUIRE(sample.has_value());
    CHECK(sample->footprint > 0);
    CHECK(sample->resident > 0);
    // A host without a Metal device (a bare VM) reports no GPU reading, which the sampler tolerates
    if (sample->gpu.has_value()) {
        CHECK(sample->message().contains(QStringLiteral(" gpu=%1").arg(*sample->gpu)));
    } else {
        CHECK_FALSE(sample->message().contains(QStringLiteral(" gpu=")));
        SUCCEED("No Metal device on this host, so the sample carries no GPU reading");
    }
#else
    CHECK_FALSE(sample.has_value());
#endif
}
