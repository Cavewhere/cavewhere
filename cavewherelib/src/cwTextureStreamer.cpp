/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwTextureStreamer.h"
#include "cwConcurrent.h"
#include "cwTextureResidency.h"

// Qt includes
#include <QMutexLocker>

// Std includes
#include <algorithm>

cwTextureStreamer::cwTextureStreamer(Loader loader) :
    m_loader(std::move(loader))
{
}

cwTextureStreamer::~cwTextureStreamer()
{
    cancelAll();
}

void cwTextureStreamer::request(quint32 itemId,
                                const cwStreamedTexture& source,
                                QRhiTexture::Format target,
                                int topLevel,
                                quint64 priority)
{
    QMutexLocker locker(&m_mutex);

    const Request* inFlight = currentInFlight(itemId);
    if (inFlight != nullptr && inFlight->matches(source, target, topLevel)) {
        return;
    }

    const auto queued = std::find_if(m_pending.constBegin(), m_pending.constEnd(),
                                     [itemId](const Request& request) {
        return request.itemId == itemId;
    });

    if (queued != m_pending.constEnd() && queued->matches(source, target, topLevel)) {
        return;
    }

    const auto ready = std::find_if(m_ready.constBegin(), m_ready.constEnd(),
                                    [itemId](const ReadyResult& ready) {
        return ready.result.itemId == itemId;
    });

    if (ready != m_ready.constEnd() && ready->request.matches(source, target, topLevel)) {
        return;
    }

    forget(itemId);

    Request request;
    request.itemId = itemId;
    request.generation = ++m_generations[itemId];
    request.source = source;
    request.target = target;
    request.topLevel = topLevel;
    request.priority = priority;
    request.sequence = ++m_sequence;
    request.estimatedBytes = cw::residency::chainBytes(target, source.size, topLevel);

    enqueue(request);
    launchReadyJobs();
}

void cwTextureStreamer::cancel(quint32 itemId)
{
    QMutexLocker locker(&m_mutex);

    forget(itemId);
    ++m_generations[itemId];

    launchReadyJobs();
}

void cwTextureStreamer::cancelAll()
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

QVector<cwTextureStreamer::Result> cwTextureStreamer::takeReady()
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

bool cwTextureStreamer::hasWork() const
{
    QMutexLocker locker(&m_mutex);
    return !m_pending.isEmpty() || !m_inFlight.isEmpty() || !m_ready.isEmpty();
}

void cwTextureStreamer::setMaxPendingCpuBytes(qint64 maxBytes)
{
    QMutexLocker locker(&m_mutex);

    m_maxPendingCpuBytes = maxBytes;
    launchReadyJobs();
}

void cwTextureStreamer::enqueue(const Request& request)
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

void cwTextureStreamer::launchReadyJobs()
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
            publish(request, m_loader(request.source, request.target, request.topLevel));
        });
    }
}

void cwTextureStreamer::publish(const Request& request,
                                const Monad::Result<cwCompressedTexture>& loaded)
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
        ready.result.topLevel = request.topLevel;

        if (loaded.hasError()) {
            ready.result.error = loaded.errorMessage();
        } else {
            ready.result.texture = loaded.value();
            ready.bytes = request.estimatedBytes;
            m_readyBytes += ready.bytes;
        }

        m_ready.append(ready);
    }

    updateLedger();
    launchReadyJobs();

    m_inFlightFinished.wakeAll();
}

void cwTextureStreamer::forget(quint32 itemId)
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

void cwTextureStreamer::updateLedger()
{
    m_cpuBytes.setBytes(m_inFlightBytes + m_readyBytes);
}

const cwTextureStreamer::Request* cwTextureStreamer::currentInFlight(quint32 itemId) const
{
    const quint64 generation = m_generations.value(itemId);
    const auto found = std::find_if(m_inFlight.constBegin(), m_inFlight.constEnd(),
                                    [itemId, generation](const Request& request) {
        return request.itemId == itemId && request.generation == generation;
    });

    return found == m_inFlight.constEnd() ? nullptr : &*found;
}

void cwTextureStreamer::waitForInFlight()
{
    while (!m_inFlight.isEmpty()) {
        m_inFlightFinished.wait(&m_mutex);
    }
}
