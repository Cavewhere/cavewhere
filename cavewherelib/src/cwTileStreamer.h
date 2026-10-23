/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTILESTREAMER_H
#define CWTILESTREAMER_H

// Qt includes
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QVector>
#include <QWaitCondition>

// Std includes
#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

// Our includes
#include "cwConcurrent.h"
#include "cwRenderMemoryLedger.h"

// Monad includes
#include "Monad/Result.h"

/**
 * Cancellable, prioritized loading of one payload per item id. The owner asks
 * for work with request(), the work runs on cwConcurrent, and the owner drains
 * finished loads with takeReady().
 *
 * A plain thread-safe class guarded by a single QMutex — no QObject, no thread
 * affinity — so any thread may call into it, the same model cwRenderMemoryLedger
 * uses.
 *
 * There is one slot per item id. Asking for something different than what is
 * queued or in flight bumps that item's generation: the stale worker's result is
 * dropped when it tries to publish, and results carry the generation so the
 * caller can check them against what it wants by the time they land.
 *
 * Source describes what to load and must provide operator==: the dedup against
 * what is already queued, in flight, or waiting to be drained compares sources
 * with it. Payload is what the loader produces.
 *
 * cancel(itemId) returns without waiting — a load already in flight for that
 * item finishes into the void — while cancelAll() and the destructor block until
 * the loads in flight have finished.
 */
template <typename Source, typename Payload>
class cwTileStreamer
{
public:
    /**
     * Loads level and whatever else that level implies for source. Runs on a
     * cwConcurrent worker, so it may touch only what it captured by value or
     * what is documented thread-safe.
     */
    using Loader = std::function<Monad::Result<Payload>(const Source&, int level)>;

    /**
     * How many bytes the payload for (source, level) will hold. Used for the CPU
     * cap and the ledger before the load has run.
     */
    using ByteEstimator = std::function<qint64(const Source&, int level)>;

    //How many loads run on cwConcurrent at once
    static constexpr int kMaxConcurrentLoads = 4;

    cwTileStreamer(Loader loader,
                   ByteEstimator estimator,
                   cwRenderMemoryLedger::Category category);
    ~cwTileStreamer();

    cwTileStreamer(const cwTileStreamer&) = delete;
    cwTileStreamer& operator=(const cwTileStreamer&) = delete;

    /**
     * One finished load. payload is default constructed and error is set when
     * the load failed.
     */
    struct Result
    {
        quint32 itemId = 0;
        quint64 generation = 0;
        int level = 0;
        Payload payload;
        QString error;
    };

    /**
     * Asks for level of source. Higher priority runs first — callers pass their
     * screen-space-error deficit, and pinned loads pass
     * std::numeric_limits<quint64>::max().
     *
     * Asking for what is already queued or in flight for itemId does nothing.
     */
    void request(quint32 itemId, const Source& source, int level, quint64 priority);

    /**
     * Forgets everything for itemId — for an item that has been removed. Returns
     * without waiting: a load already in flight finishes into the void.
     */
    void cancel(quint32 itemId);

    /**
     * Forgets every item and blocks until the loads in flight have finished.
     */
    void cancelAll();

    /**
     * The finished loads, handing their payload bytes to the caller.
     */
    QVector<Result> takeReady();

    /**
     * True while anything is queued, in flight, or waiting to be drained.
     */
    bool hasWork() const;

    /**
     * What the streamer is holding alive right now, for the render stats HUD.
     */
    struct Pending
    {
        int loads = 0;       //!< queued, in flight, or waiting to be drained
        qint64 cpuBytes = 0; //!< payload bytes those loads hold
    };

    Pending pending() const;

    /**
     * The most payload bytes to keep alive at once, counting loads in flight and
     * results waiting to be drained. Loads stay queued while the cap is met.
     * Unlimited by default.
     */
    void setMaxPendingCpuBytes(qint64 maxBytes);

private:
    struct Request
    {
        quint32 itemId = 0;
        quint64 generation = 0;
        Source source;
        int level = 0;
        quint64 priority = 0;
        quint64 sequence = 0;
        qint64 estimatedBytes = 0;

        bool matches(const Source& otherSource, int otherLevel) const
        {
            return level == otherLevel && source == otherSource;
        }
    };

    struct ReadyResult
    {
        Result result;
        Request request;      //What was asked for, so a repeat ask is a no-op
        qint64 bytes = 0;     //0 for a failed load, which holds no payload
    };

    void enqueue(const Request& request);
    void launchReadyJobs();
    void publish(const Request& request, const Monad::Result<Payload>& loaded);
    void forget(quint32 itemId);
    void updateLedger();
    void waitForInFlight();
    const Request* currentInFlight(quint32 itemId) const;

    const Loader m_loader;
    const ByteEstimator m_estimator;

    mutable QMutex m_mutex;
    QWaitCondition m_inFlightFinished;

    QList<Request> m_pending;                  //Highest priority first
    //At most kMaxConcurrentLoads entries. A superseded load stays here until it
    //finishes, so one item id can hold both a stale and a current entry.
    QList<Request> m_inFlight;

    QList<ReadyResult> m_ready;
    QHash<quint32, quint64> m_generations;

    quint64 m_sequence = 0;
    qint64 m_inFlightBytes = 0;
    qint64 m_readyBytes = 0;
    qint64 m_maxPendingCpuBytes = std::numeric_limits<qint64>::max();

    cwLedgeredBytes m_cpuBytes;
};

template <typename Source, typename Payload>
cwTileStreamer<Source, Payload>::cwTileStreamer(Loader loader,
                                                ByteEstimator estimator,
                                                cwRenderMemoryLedger::Category category) :
    m_loader(std::move(loader)),
    m_estimator(std::move(estimator)),
    m_cpuBytes(category, cwRenderMemoryLedger::Residency::Cpu)
{
}

template <typename Source, typename Payload>
cwTileStreamer<Source, Payload>::~cwTileStreamer()
{
    cancelAll();
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::request(quint32 itemId,
                                              const Source& source,
                                              int level,
                                              quint64 priority)
{
    QMutexLocker locker(&m_mutex);

    const Request* inFlight = currentInFlight(itemId);
    if (inFlight != nullptr && inFlight->matches(source, level)) {
        return;
    }

    const auto queued = std::find_if(m_pending.constBegin(), m_pending.constEnd(),
                                     [itemId](const Request& request) {
        return request.itemId == itemId;
    });

    if (queued != m_pending.constEnd() && queued->matches(source, level)) {
        return;
    }

    const auto ready = std::find_if(m_ready.constBegin(), m_ready.constEnd(),
                                    [itemId](const ReadyResult& ready) {
        return ready.result.itemId == itemId;
    });

    if (ready != m_ready.constEnd() && ready->request.matches(source, level)) {
        return;
    }

    forget(itemId);

    Request request;
    request.itemId = itemId;
    request.generation = ++m_generations[itemId];
    request.source = source;
    request.level = level;
    request.priority = priority;
    request.sequence = ++m_sequence;
    request.estimatedBytes = m_estimator(source, level);

    enqueue(request);
    launchReadyJobs();
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::cancel(quint32 itemId)
{
    QMutexLocker locker(&m_mutex);

    forget(itemId);
    ++m_generations[itemId];

    launchReadyJobs();
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::cancelAll()
{
    QMutexLocker locker(&m_mutex);

    for (const Request& request : std::as_const(m_pending)) {
        ++m_generations[request.itemId];
    }

    for (const Request& request : std::as_const(m_inFlight)) {
        ++m_generations[request.itemId];
    }

    for (const ReadyResult& ready : std::as_const(m_ready)) {
        ++m_generations[ready.result.itemId];
    }

    m_pending.clear();
    m_ready.clear();
    m_readyBytes = 0;
    updateLedger();

    waitForInFlight();
}

template <typename Source, typename Payload>
QVector<typename cwTileStreamer<Source, Payload>::Result>
cwTileStreamer<Source, Payload>::takeReady()
{
    QMutexLocker locker(&m_mutex);

    QVector<Result> results;
    results.reserve(m_ready.size());
    for (ReadyResult& ready : m_ready) {
        results.append(std::move(ready.result));
    }

    m_ready.clear();
    m_readyBytes = 0;

    updateLedger();
    launchReadyJobs();

    return results;
}

template <typename Source, typename Payload>
bool cwTileStreamer<Source, Payload>::hasWork() const
{
    QMutexLocker locker(&m_mutex);
    return !m_pending.isEmpty() || !m_inFlight.isEmpty() || !m_ready.isEmpty();
}

template <typename Source, typename Payload>
typename cwTileStreamer<Source, Payload>::Pending
cwTileStreamer<Source, Payload>::pending() const
{
    QMutexLocker locker(&m_mutex);
    return {static_cast<int>(m_pending.size() + m_inFlight.size() + m_ready.size()),
            m_inFlightBytes + m_readyBytes};
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::setMaxPendingCpuBytes(qint64 maxBytes)
{
    QMutexLocker locker(&m_mutex);

    m_maxPendingCpuBytes = maxBytes;
    launchReadyJobs();
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::enqueue(const Request& request)
{
    const auto after = std::upper_bound(m_pending.constBegin(), m_pending.constEnd(), request,
                                        [](const Request& left, const Request& right) {
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return left.sequence < right.sequence;
    });

    m_pending.insert(after, request);
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::launchReadyJobs()
{
    while (!m_pending.isEmpty() && m_inFlight.size() < kMaxConcurrentLoads) {
        const Request request = m_pending.constFirst();
        const qint64 outstandingBytes = m_inFlightBytes + m_readyBytes;

        //Always let one load through, so a cap smaller than a single payload
        //still makes progress
        if (outstandingBytes > 0
            && outstandingBytes + request.estimatedBytes > m_maxPendingCpuBytes) {
            return;
        }

        m_pending.removeFirst();
        m_inFlight.append(request);
        m_inFlightBytes += request.estimatedBytes;
        updateLedger();

        cwConcurrent::run([this, request]() {
            publish(request, m_loader(request.source, request.level));
        });
    }
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::publish(const Request& request,
                                              const Monad::Result<Payload>& loaded)
{
    QMutexLocker locker(&m_mutex);

    //Keyed by sequence: a superseding load for the same item is a separate entry
    m_inFlight.removeIf([&request](const Request& inFlight) {
        return inFlight.sequence == request.sequence;
    });
    m_inFlightBytes -= request.estimatedBytes;

    if (m_generations.value(request.itemId) == request.generation) {
        ReadyResult ready;
        ready.request = request;
        ready.result.itemId = request.itemId;
        ready.result.generation = request.generation;
        ready.result.level = request.level;

        if (loaded.hasError()) {
            ready.result.error = loaded.errorMessage();
        } else {
            ready.result.payload = loaded.value();
            ready.bytes = request.estimatedBytes;
            m_readyBytes += ready.bytes;
        }

        m_ready.append(ready);
    }

    updateLedger();
    launchReadyJobs();

    m_inFlightFinished.wakeAll();
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::forget(quint32 itemId)
{
    m_pending.removeIf([itemId](const Request& request) {
        return request.itemId == itemId;
    });

    m_ready.removeIf([this, itemId](const ReadyResult& ready) {
        if (ready.result.itemId != itemId) {
            return false;
        }

        m_readyBytes -= ready.bytes;
        return true;
    });

    updateLedger();
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::updateLedger()
{
    m_cpuBytes.setBytes(m_inFlightBytes + m_readyBytes);
}

template <typename Source, typename Payload>
const typename cwTileStreamer<Source, Payload>::Request*
cwTileStreamer<Source, Payload>::currentInFlight(quint32 itemId) const
{
    const quint64 generation = m_generations.value(itemId);
    const auto found = std::find_if(m_inFlight.constBegin(), m_inFlight.constEnd(),
                                    [itemId, generation](const Request& request) {
        return request.itemId == itemId && request.generation == generation;
    });

    return found == m_inFlight.constEnd() ? nullptr : &*found;
}

template <typename Source, typename Payload>
void cwTileStreamer<Source, Payload>::waitForInFlight()
{
    while (!m_inFlight.isEmpty()) {
        m_inFlightFinished.wait(&m_mutex);
    }
}

#endif // CWTILESTREAMER_H
