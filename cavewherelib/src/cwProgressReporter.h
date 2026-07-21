#ifndef CWPROGRESSREPORTER_H
#define CWPROGRESSREPORTER_H

//Qt includes
#include <QtGlobal>

//Std includes
#include <algorithm>
#include <atomic>

// Thread-safe, throttled progress reporter for cwConcurrent tasks.
//
// It turns a real "work done" count (points, primitives, bytes) into the
// int-typed QPromise progress API without the two failure modes hand-rolled
// reporters keep re-hitting:
//   - A total larger than int is *scaled down* to fit the range, never clamped
//     to INT_MAX, so the bar keeps advancing for multi-billion-item work
//     instead of sticking near full while the task runs on.
//   - setProgressValue is pushed only when the reported unit advances by a full
//     stride, so a per-item hot loop (or a fleet of workers) doesn't hammer the
//     promise mutex — roughly `updateBudget` pushes across the whole run.
//
// Reports are monotonic and safe from any number of worker threads: each
// advance elects a single reporter via a compare-exchange, and QFutureInterface
// itself drops any value that doesn't move progress forward.
//
// Completion is structural: the destructor snaps the bar to full so a task that
// simply returns can't leave it stuck a stride short (the throttle can drop the
// last report). A cancelled promise is left alone, and a non-cancel failure
// exit calls dismiss() to opt out. This makes the common success path
// zero-ceremony; forgetting the annotation on a rare failure path only flashes
// a full bar for a frame (the task row is removed the moment the future ends),
// never the stuck-bar bug this class exists to kill.
//
// Templated on the promise type so it serves any QPromise<T>; setProgressRange,
// setProgressValue and isCanceled are used. A rawTotal <= 0 means "unknown
// size": the range is set indeterminate (0, 0) and every report is a no-op.
template <typename Promise>
class cwProgressReporter
{
public:
    // Target number of setProgressValue calls across the whole run, whatever
    // the total — keeps the promise mutex cold under a per-item hot loop.
    static constexpr qsizetype kDefaultUpdateBudget = 10000;

    cwProgressReporter(Promise& promise,
                       qsizetype rawTotal,
                       qsizetype updateBudget = kDefaultUpdateBudget) :
        m_promise(promise),
        m_rawTotal(rawTotal)
    {
        if (rawTotal <= 0) {
            // Unknown size — indeterminate bar; report()/complete()/finish()
            // all no-op.
            m_promise.setProgressRange(0, 0);
            return;
        }

        // Scale the maximum down (never clamp) so it fits int with headroom;
        // kMaxInt < INT_MAX guarantees the static_cast in report() can't
        // overflow, and unit = done/scale <= rawTotal/scale <= scaledMax.
        constexpr qsizetype kMaxInt = qsizetype(1) << 30;
        // Ceil-divide without a (rawTotal + kMaxInt - 1) pre-add: that addition
        // would overflow signed 64-bit for a rawTotal within kMaxInt of
        // qsizetype's max. This form never overflows for any rawTotal.
        m_scale = (std::max)(qsizetype(1),
                             rawTotal / kMaxInt + (rawTotal % kMaxInt != 0 ? 1 : 0));
        m_scaledMax = (std::max)(1, static_cast<int>(rawTotal / m_scale));
        m_stride = (std::max)(qsizetype(1),
                              qsizetype(m_scaledMax) / (std::max)(qsizetype(1), updateBudget));
        m_promise.setProgressRange(0, m_scaledMax);
        m_promise.setProgressValue(0);
    }

    // Report an absolute running total — e.g. an atomic counter a poll loop
    // reads. Thread-safe, throttled, monotonic. Values past the total are
    // clamped, and a value that doesn't advance the reported unit is dropped.
    void report(qsizetype absoluteDone)
    {
        if (m_rawTotal <= 0) {
            return;
        }
        const qsizetype clamped = (std::min)(absoluteDone, m_rawTotal);
        const qsizetype unit = clamped / m_scale;
        qsizetype prev = m_lastReportedUnit.load(std::memory_order_relaxed);
        // qsizetype is signed, so a stale lower `unit` yields a negative
        // difference that fails the stride test and is skipped.
        if (unit - prev >= m_stride
            && m_lastReportedUnit.compare_exchange_strong(
                   prev, unit, std::memory_order_relaxed)) {
            m_promise.setProgressValue(static_cast<int>(unit));
        }
    }

    // Accumulate `delta` into the running total, then report it. Use this when
    // each worker contributes a piece and there is no external counter to read.
    void complete(qsizetype delta)
    {
        report(m_done.fetch_add(delta, std::memory_order_relaxed) + delta);
    }

    // Snap to full now, rather than letting the destructor do it. Useful when
    // progress must read complete before a later step in the same scope. Marks
    // the reporter done so the destructor won't push again.
    void finish()
    {
        m_armed = false;
        if (m_rawTotal <= 0) {
            return;
        }
        m_promise.setProgressValue(m_scaledMax);
    }

    // Opt out of the destructor's snap-to-full. Call on a non-cancel failure
    // exit so a bar that never reached completion isn't shown as full.
    void dismiss() { m_armed = false; }

    // The destructor completes the bar for a normally-exiting scope. Skipped
    // when finish()/dismiss() already fired, when the promise was cancelled, or
    // when the size was unknown (indeterminate bar).
    ~cwProgressReporter()
    {
        if (m_armed && m_rawTotal > 0 && !m_promise.isCanceled()) {
            m_promise.setProgressValue(m_scaledMax);
        }
    }

    cwProgressReporter(const cwProgressReporter&) = delete;
    cwProgressReporter& operator=(const cwProgressReporter&) = delete;

    int scaledMaximum() const { return m_scaledMax; }

private:
    Promise& m_promise;
    qsizetype m_rawTotal = 0;
    qsizetype m_scale = 1;
    qsizetype m_stride = 1;
    int m_scaledMax = 1;
    bool m_armed = true;
    std::atomic<qsizetype> m_done{0};
    std::atomic<qsizetype> m_lastReportedUnit{0};
};

#endif // CWPROGRESSREPORTER_H
