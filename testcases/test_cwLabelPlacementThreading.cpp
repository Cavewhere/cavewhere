// Our includes
#include "cwCaptureLabelPlacer.h"
#include "cwConcurrent.h"
#include "cwLabelPlacementControl.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Test includes
#include "cwSignalSpy.h"

// Qt includes
#include <QDeadlineTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <QLineF>
#include <QPointF>
#include <QPromise>
#include <QRectF>
#include <QSemaphore>
#include <QSizeF>
#include <QString>
#include <QThread>
#include <QVector>

// Std includes
#include <atomic>
#include <cmath>

// Phase 5a moves the export label-placement stage off the GUI thread: the
// distance-transform build and the per-label placement loop run on a worker
// thread (cwConcurrent::run), reporting progress and honoring cancelation
// through a cwLabelPlacementControl backed by the run's QPromise. The
// production loop is cwCaptureLabelPlacer::placeAll, fed by
// cwCaptureCenterline/cwCaptureLeads::buildLabelRequests — which need a
// camera + survey network + RHI tiles to drive; this test exercises the same
// control contract and the same cwConcurrent/QPromise machinery directly
// against the placer, so the off-thread/progress/cancel behavior is verified
// without that harness.

namespace {

constexpr qreal StationSpacingPaperPx   = 40.0;
constexpr qreal CellSizePaperPx         = 2.0;
constexpr qreal LabelMarginPaperPx      = 1.0;
constexpr qreal StationDotHalfPaperPx   = 3.0;
constexpr qreal SoftLegThicknessPaperPx = 1.0;
constexpr qreal LeaderThicknessPaperPx  = 1.0;
constexpr qreal LabelWidthPaperPx       = 24.0;
constexpr qreal LabelHeightPaperPx      = 10.0;
constexpr qreal PageMarginPaperPx       = StationSpacingPaperPx;
constexpr int   TestStationCount        = 300;

// Generous ceiling for waiting on the worker to signal; a healthy worker
// signals in milliseconds, so hitting this means it never started (e.g. a
// saturated pool) and the test should fail rather than hang the suite.
constexpr int   WorkerSignalTimeoutMs   = 30000;

struct SyntheticCave {
    QRectF           bounds;
    QVector<QPointF> stations;
    QVector<QLineF>  legs;
};

// A serpentine survey packed onto one sheet — same generator as the placement
// benchmark, so the placement cost per label matches a real export.
SyntheticCave makeSerpentineCave(int stationCount)
{
    SyntheticCave cave;
    cave.stations.reserve(stationCount);
    cave.legs.reserve(qMax(0, stationCount - 1));

    const int perRow = qMax(1, int(std::ceil(std::sqrt(double(stationCount)))));
    for(int i = 0; i < stationCount; i++) {
        const int row = i / perRow;
        int       col = i % perRow;
        if(row % 2 == 1) {
            col = perRow - 1 - col;
        }
        const QPointF pos((col + 1) * StationSpacingPaperPx,
                          (row + 1) * StationSpacingPaperPx);
        cave.stations.append(pos);
        if(i > 0) {
            cave.legs.append(QLineF(cave.stations.at(i - 1), pos));
        }
    }

    const int rows = (stationCount + perRow - 1) / perRow;
    const qreal width  = (perRow + 1) * StationSpacingPaperPx + PageMarginPaperPx;
    const qreal height = (rows + 1)   * StationSpacingPaperPx + PageMarginPaperPx;
    cave.bounds = QRectF(0.0, 0.0, width, height);
    return cave;
}

// Builds the placer for `cave` exactly as the export worker does: seed station
// dots as hard obstacles, finalize, then register legs as soft obstacles.
void buildPlacer(cwCaptureLabelPlacer& placer, const SyntheticCave& cave)
{
    placer.setObstacleBounds(cave.bounds, CellSizePaperPx);
    placer.setViewportBounds(cave.bounds);
    placer.setLabelMarginPaperPx(LabelMarginPaperPx);

    for(const QPointF& pos : cave.stations) {
        placer.addObstacleRect(QRectF(pos.x() - StationDotHalfPaperPx,
                                      pos.y() - StationDotHalfPaperPx,
                                      StationDotHalfPaperPx * 2.0,
                                      StationDotHalfPaperPx * 2.0));
    }

    placer.finalize();

    for(const QLineF& leg : cave.legs) {
        placer.addSoftLineObstacle(leg, SoftLegThicknessPaperPx);
    }
}

// Runs the per-label placement loop threading `control`, mirroring the contract
// the production item placement loops implement: poll isCanceled() once per
// label and report progress via labelProcessed(), both BEFORE placing the
// label, so a cancel bails mid-run and progress covers every attempt. Only the
// control contract mirrors production — the unconditional leader
// addLineObstacle below is the benchmark's stress setup, not the production
// loops (which register leaders conditionally or not at all).
// QFuture has no timed wait; poll so a placement-loop hang regression fails
// the test after WorkerSignalTimeoutMs instead of hanging the whole suite.
bool waitForFinishedWithTimeout(const QFuture<void>& future,
                                int timeoutMs = WorkerSignalTimeoutMs)
{
    QDeadlineTimer deadline(timeoutMs);
    while(!future.isFinished() && !deadline.hasExpired()) {
        QThread::msleep(1);
    }
    return future.isFinished();
}

// Failure-path teardown: the worker lambdas capture this test's stack by
// reference, and a QFuture destructor neither cancels nor waits. If the
// worker never signaled in time (saturated pool), cancel it — a queued,
// not-yet-started QtConcurrent run is then never executed — and wait out a
// worker that did start (its gate semaphore has already been released), so
// REQUIRE can't unwind the stack under a live or late-starting worker.
void cancelAndWait(QFuture<void>& future)
{
    future.cancel();
    future.waitForFinished();
}

int placeWithControl(cwCaptureLabelPlacer& placer, const SyntheticCave& cave,
                     const cwLabelPlacementControl& control)
{
    const QSizeF labelSize(LabelWidthPaperPx, LabelHeightPaperPx);
    int placed = 0;
    for(int i = 0; i < cave.stations.size(); i++) {
        if(control.isCanceled && control.isCanceled()) {
            break;
        }
        if(control.labelProcessed) {
            control.labelProcessed();
        }

        const cwCaptureLabelPlacer::LabelRequest request{
            QString::number(i), cave.stations.at(i), labelSize};
        const cwCaptureLabelPlacer::Placement placement = placer.placeLabel(request);
        if(placement.placed) {
            placed++;
            placer.addLineObstacle(QLineF(placement.leaderStart, placement.leaderEnd),
                                   LeaderThicknessPaperPx);
        }
    }
    return placed;
}

} // namespace

TEST_CASE("cwLabelPlacementControl placement runs off-thread and reports progress",
          "[cwLabelPlacementControl]")
{
    const SyntheticCave cave = makeSerpentineCave(TestStationCount);
    QThread* const callerThread = QThread::currentThread();

    std::atomic<QThread*> workerThread{nullptr};
    QSemaphore workerStarted;
    QSemaphore mayFinish;

    QFuture<void> future = cwConcurrent::run([&](QPromise<void>& promise) {
        // Record and announce which thread we're on, then hold it until the
        // test has checked. Because the test is parked in workerStarted.acquire()
        // (not waitForFinished), QThreadPool won't run this inline on the caller
        // — so this is guaranteed to be a real pool thread.
        workerThread.store(QThread::currentThread());
        workerStarted.release();
        mayFinish.acquire();

        cwCaptureLabelPlacer placer;
        buildPlacer(placer, cave);

        promise.setProgressRange(0, cave.stations.size());
        int processed = 0;
        cwLabelPlacementControl control;
        control.isCanceled = [&promise]() { return promise.isCanceled(); };
        control.labelProcessed = [&promise, &processed]() {
            processed++;
            promise.setProgressValue(processed);
        };

        placeWithControl(placer, cave, control);
    });

    // Timed acquire so a worker that never starts fails the test instead of
    // hanging the suite; unblock the (possibly late-arriving) worker before
    // failing so it can't strand a pool thread.
    const bool workerReady = workerStarted.tryAcquire(1, WorkerSignalTimeoutMs);
    if(!workerReady) {
        mayFinish.release();
        cancelAndWait(future); // see helper: REQUIRE must not unwind our stack under a live worker
    }
    REQUIRE(workerReady);

    // The placement genuinely ran on a pooled worker thread, not the caller's.
    CHECK(workerThread.load() != nullptr);
    CHECK(workerThread.load() != callerThread);

    mayFinish.release();
    REQUIRE(waitForFinishedWithTimeout(future));

    CHECK_FALSE(future.isCanceled());

    // Progress advanced to one tick per station.
    CHECK(future.progressValue() == cave.stations.size());
}

TEST_CASE("cwLabelPlacementControl placement is cancelable mid-run",
          "[cwLabelPlacementControl]")
{
    const SyntheticCave cave = makeSerpentineCave(TestStationCount);

    // Two semaphores make the cancel deterministic regardless of how fast
    // placement runs: the worker blocks after its first label until the test
    // has issued the cancel, so the next isCanceled() poll is guaranteed true.
    QSemaphore workerStarted;
    QSemaphore mayContinue;
    std::atomic<int>  processedCount{0};
    std::atomic<bool> workerSawCancel{false};

    QFuture<void> future = cwConcurrent::run([&](QPromise<void>& promise) {
        cwCaptureLabelPlacer placer;
        buildPlacer(placer, cave);

        cwLabelPlacementControl control;
        control.isCanceled = [&promise]() { return promise.isCanceled(); };
        control.labelProcessed = [&]() {
            const int n = ++processedCount;
            if(n == 1) {
                // Placement is underway. Let the test cancel, then wait for it
                // so the loop's next cancel poll definitely sees the cancel.
                workerStarted.release();
                mayContinue.acquire();
            }
        };

        placeWithControl(placer, cave, control);
        workerSawCancel.store(promise.isCanceled());
    });

    // Timed acquire so a worker that never reaches its first label (e.g. a
    // saturated pool or a throw in buildPlacer) fails the test instead of
    // hanging the suite; unblock a late-arriving worker before failing.
    const bool workerReady = workerStarted.tryAcquire(1, WorkerSignalTimeoutMs);
    if(!workerReady) {
        mayContinue.release();
        cancelAndWait(future); // see helper: REQUIRE must not unwind our stack under a live worker
    }
    REQUIRE(workerReady);      // worker is blocked one label into the loop

    future.cancel();           // cancel while it is definitely mid-run
    mayContinue.release();     // release it; the next isCanceled() poll is true
    REQUIRE(waitForFinishedWithTimeout(future));

    CHECK(future.isCanceled());
    // The loop observed the cancel through the QPromise and bailed out before
    // placing every station.
    CHECK(workerSawCancel.load());
    CHECK(processedCount.load() > 0);
    CHECK(processedCount.load() < cave.stations.size());
}

// cwCaptureViewport's GUI-thread continuation must run only after the worker
// has actually unwound: it clears the CapturingImages guard that keeps the
// label items alive under the worker. This test pins the mechanism it relies
// on — QFutureWatcher::canceled fires as soon as cancel() is called (while the
// worker is still running!), but QFutureWatcher::finished fires only once the
// worker exits. Keying the continuation off canceled (as an
// AsyncFuture-observe cancel callback does) would drop the guard mid-worker.
TEST_CASE("cwLabelPlacementControl continuation must key off finished, not canceled",
          "[cwLabelPlacementControl]")
{
    QSemaphore workerStarted;
    QSemaphore mayFinish;
    std::atomic<bool> workerExited{false};

    QFutureWatcher<void> watcher;
    cwSignalSpy canceledSpy(&watcher, &QFutureWatcher<void>::canceled);
    cwSignalSpy finishedSpy(&watcher, &QFutureWatcher<void>::finished);

    QFuture<void> future = cwConcurrent::run([&](QPromise<void>& promise) {
        Q_UNUSED(promise);
        workerStarted.release();
        mayFinish.acquire();
        workerExited.store(true);
    });
    watcher.setFuture(future);

    // Timed acquire + unblock-on-failure, same rationale as the tests above.
    const bool workerReady = workerStarted.tryAcquire(1, WorkerSignalTimeoutMs);
    if(!workerReady) {
        mayFinish.release();
        cancelAndWait(future); // see helper: REQUIRE must not unwind our stack under a live worker
    }
    REQUIRE(workerReady);

    future.cancel();

    // The canceled notification is delivered promptly — while the worker is
    // provably still running (it is parked on mayFinish).
    const bool canceledArrived = !canceledSpy.isEmpty()
                                 || canceledSpy.wait(WorkerSignalTimeoutMs);
    CHECK(canceledArrived);
    CHECK_FALSE(workerExited.load());
    CHECK(finishedSpy.isEmpty());

    // Only once the worker unwinds does finished fire.
    mayFinish.release();
    const bool finishedArrived = !finishedSpy.isEmpty()
                                 || finishedSpy.wait(WorkerSignalTimeoutMs);
    CHECK(finishedArrived);
    CHECK(workerExited.load());
    CHECK(future.isCanceled());
    CHECK(future.isFinished());
}
