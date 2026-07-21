//Catch includes
#include <catch2/catch_test_macros.hpp>

// Std
#include <limits>

// Qt
#include <QFuture>
#include <QPromise>

// SUT
#include "cwProgressReporter.h"

// A running total just past INT_MAX — the size where a clamp-to-INT_MAX
// reporter would stall near full while the work continues.
static constexpr qsizetype kHugeTotal = (qsizetype(3) << 30); // ~3.2 billion

TEST_CASE("cwProgressReporter reports real counts for an int-sized total",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    cwProgressReporter<QPromise<void>> reporter(promise, 5000);

    // The range is the real total, not a normalized 0..1000.
    REQUIRE(reporter.scaledMaximum() == 5000);
    REQUIRE(promise.future().progressMaximum() == 5000);

    reporter.report(2500);
    REQUIRE(promise.future().progressValue() == 2500);

    reporter.finish();
    REQUIRE(promise.future().progressValue() == 5000);
}

TEST_CASE("cwProgressReporter scales a huge total down to fit int, never clamps",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    cwProgressReporter<QPromise<void>> reporter(promise, kHugeTotal);

    // The maximum must fit int with headroom — the whole point of scaling
    // instead of clamping to INT_MAX.
    const int max = reporter.scaledMaximum();
    REQUIRE(max > 0);
    REQUIRE(max < (std::numeric_limits<int>::max)());

    // Halfway through the (huge) work the bar sits near halfway — it keeps
    // advancing rather than sticking pinned at full.
    reporter.report(kHugeTotal / 2);
    const int mid = promise.future().progressValue();
    REQUIRE(mid > max / 4);
    REQUIRE(mid < max);

    reporter.finish();
    REQUIRE(promise.future().progressValue() == max);
}

TEST_CASE("cwProgressReporter is indeterminate for an unknown total",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    cwProgressReporter<QPromise<void>> reporter(promise, 0);

    // A (0, 0) range is the indeterminate-bar convention; reports are no-ops.
    REQUIRE(promise.future().progressMinimum() == 0);
    REQUIRE(promise.future().progressMaximum() == 0);

    reporter.report(5);
    reporter.complete(5);
    reporter.finish();
    REQUIRE(promise.future().progressValue() == 0);
}

TEST_CASE("cwProgressReporter clamps values past the total",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    cwProgressReporter<QPromise<void>> reporter(promise, 100);

    reporter.report(500);
    REQUIRE(promise.future().progressValue() == 100);
}

TEST_CASE("cwProgressReporter::complete accumulates deltas",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    cwProgressReporter<QPromise<void>> reporter(promise, 100);

    reporter.complete(30);
    reporter.complete(30);
    REQUIRE(promise.future().progressValue() == 60);

    reporter.finish();
    REQUIRE(promise.future().progressValue() == 100);
}

TEST_CASE("cwProgressReporter snaps to full when it goes out of scope",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    {
        cwProgressReporter<QPromise<void>> reporter(promise, 100);
        reporter.report(40);
        REQUIRE(promise.future().progressValue() == 40);
    }
    // The destructor completes the bar so a forgotten finish() can't leave it
    // stuck a stride short.
    REQUIRE(promise.future().progressValue() == 100);
}

TEST_CASE("cwProgressReporter does not complete a cancelled promise on destruction",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    QFuture<void> future = promise.future();
    {
        cwProgressReporter<QPromise<void>> reporter(promise, 100);
        reporter.report(40);
        future.cancel();
    }
    // A cancelled operation must not read complete.
    REQUIRE(promise.future().progressValue() == 40);
}

TEST_CASE("cwProgressReporter::dismiss suppresses the destructor snap",
          "[cwProgressReporter]")
{
    QPromise<void> promise;
    {
        cwProgressReporter<QPromise<void>> reporter(promise, 100);
        reporter.report(40);
        reporter.dismiss();
    }
    // A non-cancel failure exit opted out of the completion.
    REQUIRE(promise.future().progressValue() == 40);
}
