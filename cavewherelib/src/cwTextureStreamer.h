/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEXTURESTREAMER_H
#define CWTEXTURESTREAMER_H

// Qt includes
#include <QString>
#include <QVector>

// Qt RHI
#include <rhi/qrhi.h>

// Std includes
#include <functional>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwKtx2Codec.h"
#include "cwStreamedTexture.h"
#include "cwTileStreamer.h"

// Monad includes
#include "Monad/Result.h"

/**
 * Cancellable, prioritized loading of streamed mip levels. One instance belongs
 * to one cwRhiTexturedItems: the render thread asks for levels with request(),
 * the work runs on cwConcurrent, and the render thread drains finished loads
 * with takeReady().
 *
 * The queueing, generation, and budget policy all live in cwTileStreamer; this
 * class pairs the source texture with its target format so one cwTileStreamer
 * slot describes a whole transcode.
 */
class CAVEWHERE_LIB_EXPORT cwTextureStreamer
{
private:
    //One streamer slot asks for a source transcoded to a target format
    struct TextureRequest
    {
        cwStreamedTexture source;
        QRhiTexture::Format target = QRhiTexture::UnknownFormat;

        bool operator==(const TextureRequest& other) const
        {
            return target == other.target && source == other.source;
        }
    };

    using Streamer = cwTileStreamer<TextureRequest, cwCompressedTexture>;

public:
    /**
     * Reads and transcodes levels topLevel through the 1x1 tail. The default is
     * cw::ktx2::loadStreamedLevels(); tests substitute a synchronous fake.
     */
    using Loader = std::function<Monad::Result<cwCompressedTexture>(const cwStreamedTexture&,
                                                                    QRhiTexture::Format,
                                                                    int firstLevel)>;

    //How many loads run on cwConcurrent at once
    static constexpr int kMaxConcurrentLoads = Streamer::kMaxConcurrentLoads;

    explicit cwTextureStreamer(Loader loader = &cw::ktx2::loadStreamedLevels);

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
    using Pending = Streamer::Pending;

    Pending pending() const;

    /**
     * The most payload bytes to keep alive at once, counting loads in flight and
     * results waiting to be drained. Loads stay queued while the cap is met.
     * Unlimited by default.
     */
    void setMaxPendingCpuBytes(qint64 maxBytes);

private:
    Streamer m_streamer;
};

#endif // CWTEXTURESTREAMER_H
