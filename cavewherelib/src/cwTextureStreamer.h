/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEXTURESTREAMER_H
#define CWTEXTURESTREAMER_H

// Qt includes
#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QVector>
#include <QWaitCondition>

// Qt RHI
#include <rhi/qrhi.h>

// Std includes
#include <functional>
#include <limits>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwKtx2Codec.h"
#include "cwRenderMemoryLedger.h"
#include "cwStreamedTexture.h"

// Monad includes
#include "Monad/Result.h"

/**
 * Cancellable, prioritized loading of streamed mip levels. One instance belongs
 * to one cwRhiTexturedItems: the render thread asks for levels with request(),
 * the work runs on cwConcurrent, and the render thread drains finished loads
 * with takeReady().
 *
 * A plain thread-safe class guarded by a single QMutex — no QObject, no thread
 * affinity — so any thread may call into it, the same model cwRenderMemoryLedger
 * uses.
 *
 * There is one slot per item id. Asking for something different than what is
 * queued or in flight bumps that item's generation: the stale worker's result is
 * dropped when it tries to publish, and results carry the generation so the
 * caller can check them against what it wants by the time they land.
 */
class CAVEWHERE_LIB_EXPORT cwTextureStreamer
{
public:
    /**
     * Reads and transcodes levels topLevel through the 1x1 tail. The default is
     * cw::ktx2::loadStreamedLevels(); tests substitute a synchronous fake.
     */
    using Loader = std::function<Monad::Result<cwCompressedTexture>(const cwStreamedTexture&,
                                                                    QRhiTexture::Format,
                                                                    int firstLevel)>;

    //How many loads run on cwConcurrent at once
    static constexpr int kMaxConcurrentLoads = 4;

    explicit cwTextureStreamer(Loader loader = &cw::ktx2::loadStreamedLevels);
    ~cwTextureStreamer();

    cwTextureStreamer(const cwTextureStreamer&) = delete;
    cwTextureStreamer& operator=(const cwTextureStreamer&) = delete;

    /**
     * One finished load. texture is null and error is set when the load failed.
     */
    struct Result
    {
        quint32 itemId = 0;
        quint64 generation = 0;
        int topLevel = 0;
        cwCompressedTexture texture;
        QString error;
    };

    /**
     * Asks for levels topLevel and coarser of source, transcoded to target.
     * Higher priority runs first — callers pass their screen-space-error
     * deficit, and pinned-base loads pass std::numeric_limits<quint64>::max().
     *
     * Asking for what is already queued or in flight for itemId does nothing.
     */
    void request(quint32 itemId,
                 const cwStreamedTexture& source,
                 QRhiTexture::Format target,
                 int topLevel,
                 quint64 priority);

    /**
     * Forgets everything for itemId — for an item that has been removed. A load
     * already in flight finishes into the void.
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
        cwStreamedTexture source;
        QRhiTexture::Format target = QRhiTexture::UnknownFormat;
        int topLevel = 0;
        quint64 priority = 0;
        quint64 sequence = 0;
        qint64 estimatedBytes = 0;

        bool matches(const cwStreamedTexture& otherSource,
                     QRhiTexture::Format otherTarget,
                     int otherTopLevel) const
        {
            return topLevel == otherTopLevel && target == otherTarget && source == otherSource;
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
    void publish(const Request& request, const Monad::Result<cwCompressedTexture>& loaded);
    void forget(quint32 itemId);
    void updateLedger();
    void waitForInFlight();
    const Request* currentInFlight(quint32 itemId) const;

    const Loader m_loader;

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

    cwLedgeredBytes m_cpuBytes {cwRenderMemoryLedger::Category::TexturedItemTexture,
                                cwRenderMemoryLedger::Residency::Cpu};
};

#endif // CWTEXTURESTREAMER_H
