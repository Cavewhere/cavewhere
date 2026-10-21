/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwTextureStreamer.h"
#include "cwMipMath.h"
#include "cwRenderMemoryLedger.h"

// Std includes
#include <utility>

cwTextureStreamer::cwTextureStreamer(Loader loader) :
    m_streamer([loader = std::move(loader)](const TextureRequest& request, int level) {
                   return loader(request.source, request.target, level);
               },
               [](const TextureRequest& request, int level) {
                   return cw::mip::chainBytes(request.target, request.source.size, level);
               },
               cwRenderMemoryLedger::Category::TexturedItemTexture)
{
}

void cwTextureStreamer::request(quint32 itemId,
                                const cwStreamedTexture& source,
                                QRhiTexture::Format target,
                                int topLevel,
                                quint64 priority)
{
    m_streamer.request(itemId, TextureRequest {source, target}, topLevel, priority);
}

void cwTextureStreamer::cancel(quint32 itemId)
{
    m_streamer.cancel(itemId);
}

void cwTextureStreamer::cancelAll()
{
    m_streamer.cancelAll();
}

QVector<cwTextureStreamer::Result> cwTextureStreamer::takeReady()
{
    QVector<Streamer::Result> streamed = m_streamer.takeReady();

    QVector<Result> results;
    results.reserve(streamed.size());
    for (Streamer::Result& result : streamed) {
        results.append({result.itemId, result.generation, result.level,
                        std::move(result.payload), std::move(result.error)});
    }

    return results;
}

bool cwTextureStreamer::hasWork() const
{
    return m_streamer.hasWork();
}

cwTextureStreamer::Pending cwTextureStreamer::pending() const
{
    return m_streamer.pending();
}

void cwTextureStreamer::setMaxPendingCpuBytes(qint64 maxBytes)
{
    m_streamer.setMaxPendingCpuBytes(maxBytes);
}
