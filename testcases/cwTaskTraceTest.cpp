#if CAVEWHERE_TASK_TRACE

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTaskTrace.h"
#include "cwConcurrent.h"
#include "cwTask.h"
#include "cwThreadCountRestorer.h"
#include "cwGeometryItersecter.h"
#include "TestGeometryBuilders.h"
#include "cwSaveLoad.h"
#include "cwRenderBudgets.h"

//Qt includes
#include <QCoreApplication>
#include <QFuture>
#include <QSemaphore>
#include <QString>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QVector3D>

//Std includes
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>

namespace {
constexpr int kWaitTimeoutMs = 10000;
constexpr int kPollSliceMs = 1;
constexpr qint64 kStartMs = 1000;
constexpr qint64 kJustBeforeWindowMs = kTaskTraceQuietWindowMs - 1;
constexpr quint64 kFootprintBytes = 1048576;
constexpr quint64 kResidentBytes = 2097152;
constexpr quint64 kGpuBytes = 3145728;
constexpr quint64 kAvailableBytes = 4194304;
constexpr int kWrappedArgument = 7;
constexpr int kWrappedIncrement = 1;
constexpr int kSecondCall = 2;
constexpr uint64_t kTriangleObjectId = 1;
constexpr float kTriangleEdge = 1.0f;
constexpr qint64 kChargedBytes = 128 * cw::budgets::kBytesPerMegabyte;
constexpr qint64 kWaitUs = 5;
constexpr quint64 kGeneration = 3;

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

//! Like waitUntil, but pumps the event loop so main-thread continuations and restarters run
bool waitUntilProcessingEvents(const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kWaitTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, kPollSliceMs);
    }
    return condition();
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
    cwThreadCountRestorer restorer(kSingleThread);

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
    cwThreadCountRestorer restorer(kSingleThread);

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
    cwThreadCountRestorer restorer(kSingleThread);

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

TEST_CASE("A wrapped callable reinstalls its context after the scope ends", "[cwTaskTrace]")
{
    const QString context = QStringLiteral("trace-wrap");

    std::function<QString()> wrapped;
    {
        cwTaskContextScope scope(context);
        wrapped = cwTaskContextScope::wrap([]() {
            return cwTaskContextScope::current();
        });
    }

    CHECK(cwTaskContextScope::current() == QStringLiteral("untagged"));
    CHECK(wrapped() == context);
    CHECK(cwTaskContextScope::current() == QStringLiteral("untagged"));

    SECTION("Arguments and the result pass through") {
        std::function<int(int)> increment;
        {
            cwTaskContextScope scope(context);
            increment = cwTaskContextScope::wrap([](int value) {
                return value + kWrappedIncrement;
            });
        }
        CHECK(increment(kWrappedArgument) == kWrappedArgument + kWrappedIncrement);
    }

    SECTION("Work submitted from the wrapped callable carries the context") {
        QFuture<QString> submitted;
        const auto submit = [&]() {
            submitted = cwConcurrent::run([]() {
                return cwTaskContextScope::current();
            });
        };

        std::function<void()> wrappedSubmit;
        {
            cwTaskContextScope scope(context);
            wrappedSubmit = cwTaskContextScope::wrap(submit);
        }
        wrappedSubmit();

        REQUIRE(waitUntil([&]() { return submitted.isFinished(); }));
        CHECK(submitted.result() == context);
    }

    SECTION("A mutable lambda keeps its state between calls") {
        std::function<int()> counter;
        {
            cwTaskContextScope scope(context);
            counter = cwTaskContextScope::wrap([calls = 0]() mutable {
                return ++calls;
            });
        }
        counter();
        CHECK(counter() == kSecondCall);
    }

    SECTION("The context reaches another thread") {
        QString seen;
        std::thread worker([&]() { seen = wrapped(); });
        worker.join();
        CHECK(seen == context);
    }
}

TEST_CASE("Nested wraps keep the inner context", "[cwTaskTrace]")
{
    const QString outerContext = QStringLiteral("trace-wrap-outer");
    const QString innerContext = QStringLiteral("trace-wrap-inner");

    std::function<QString()> inner;
    {
        cwTaskContextScope scope(innerContext);
        inner = cwTaskContextScope::wrap([]() {
            return cwTaskContextScope::current();
        });
    }

    std::function<QString()> outer;
    {
        cwTaskContextScope scope(outerContext);
        outer = cwTaskContextScope::wrap([inner]() {
            return inner();
        });
    }

    CHECK(outer() == innerContext);

    SECTION("A wrap made inside a wrapped call captures the outer context") {
        std::function<std::function<QString()>()> makeInner;
        {
            cwTaskContextScope scope(outerContext);
            makeInner = cwTaskContextScope::wrap([]() {
                return std::function<QString()>(cwTaskContextScope::wrap([]() {
                    return cwTaskContextScope::current();
                }));
            });
        }

        const std::function<QString()> madeInside = makeInner();
        CHECK(cwTaskContextScope::current() == QStringLiteral("untagged"));
        CHECK(madeInside() == outerContext);
    }

    SECTION("The outer context returns after the inner call") {
        std::function<QStringList()> both;
        {
            cwTaskContextScope scope(outerContext);
            both = cwTaskContextScope::wrap([inner]() {
                const QString during = inner();
                return QStringList{during, cwTaskContextScope::current()};
            });
        }
        CHECK(both() == QStringList{innerContext, outerContext});
    }
}

TEST_CASE("Geometry intersecter builds count as intersect work", "[cwTaskTrace]")
{
    REQUIRE(waitUntil([]() { return cwTask::threadPool()->activeThreadCount() == 0; }));
    cwThreadCountRestorer restorer(kSingleThread);

    const QString intersect = QStringLiteral("intersect");
    CHECK(countsEqual(intersect, 0, 0));

    // Holding the only worker keeps the build queued long enough to read its tag
    QSemaphore releaseBlocker;
    QFuture<void> blocker;
    {
        cwTaskContextScope scope(QStringLiteral("trace-intersect-blocker"));
        blocker = cwConcurrent::run([&releaseBlocker]() { releaseBlocker.acquire(); });
    }

    cwGeometryItersecter intersecter;
    const cwGeometryItersecter::Object triangle(nullptr, kTriangleObjectId,
                                                cwTestGeometry::triangles({QVector3D(),
                                                                           QVector3D(kTriangleEdge, 0.0f, 0.0f),
                                                                           QVector3D(0.0f, kTriangleEdge, 0.0f)},
                                                                          {0, 1, 2}));
    const QFuture<void> ready = intersecter.addObject(triangle);

    CHECK(waitUntilProcessingEvents([&]() { return countsFor(intersect).queued >= 1; }));
    CHECK(countsFor(QStringLiteral("untagged")).queued == 0);

    releaseBlocker.release();
    CHECK(waitUntil([&]() { return blocker.isFinished(); }));
    CHECK(waitUntilProcessingEvents([&]() { return ready.isFinished(); }));
    intersecter.waitForFinish();

    CHECK(intersecter.isObjectPickReady(triangle.key()));
    CHECK(waitUntil([&]() { return countsEqual(intersect, 0, 0); }));
}

TEST_CASE("Loading a project counts its worker tasks as load work", "[cwTaskTrace]")
{
    REQUIRE(waitUntil([]() { return cwTask::threadPool()->activeThreadCount() == 0; }));
    cwThreadCountRestorer restorer(kSingleThread);

    const QString load = QStringLiteral("load");
    CHECK(countsEqual(load, 0, 0));

    // Holding the only worker keeps the load's tasks queued long enough to read their tag
    QSemaphore releaseBlocker;
    QFuture<void> blocker;
    {
        cwTaskContextScope scope(QStringLiteral("trace-load-blocker"));
        blocker = cwConcurrent::run([&releaseBlocker]() { releaseBlocker.acquire(); });
    }

    // The project reads run in continuations after load() returns, so their tag shows the wraps held
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    cwSaveLoad saveLoad;
    const QFuture<Monad::ResultBase> loaded = saveLoad.load(tempDir.filePath(QStringLiteral("trace-load.cwproj")));

    CHECK(waitUntilProcessingEvents([&]() { return countsFor(load).queued >= 1; }));

    releaseBlocker.release();
    CHECK(waitUntil([&]() { return blocker.isFinished(); }));
    CHECK(waitUntilProcessingEvents([&]() { return loaded.isFinished(); }));
    CHECK(waitUntil([&]() { return countsEqual(load, 0, 0); }));
}

TEST_CASE("A charged interval carries its byte count in the metadata", "[cwTaskTrace]")
{
    const cwTaskContext context {QStringLiteral("trace-charged"), kGeneration, nullptr};
    const QString unchargedMetadata = QStringLiteral("ctx=trace-charged name=run wait_us=%1 gen=%2")
                                          .arg(kWaitUs)
                                          .arg(kGeneration);

    CHECK(cwTaskInterval::metadata(context, QStringLiteral("run"), kWaitUs, kChargedBytes)
          == unchargedMetadata + QStringLiteral(" charged_bytes=%1").arg(kChargedBytes));

    SECTION("An uncharged interval leaves the field out") {
        CHECK(cwTaskInterval::metadata(context, QStringLiteral("run"), kWaitUs, 0) == unchargedMetadata);
    }
}

#endif // CAVEWHERE_TASK_TRACE
