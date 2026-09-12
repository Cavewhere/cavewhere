/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderFrameStats.h"

//Qt includes
#include <QMutexLocker>

cwRenderFrameStats* cwRenderFrameStats::instance()
{
    static cwRenderFrameStats stats;
    return &stats;
}

void cwRenderFrameStats::publishCulling(const Culling& culling)
{
    QMutexLocker locker(&m_mutex);
    m_culling = culling;
    ++m_revision;
}

void cwRenderFrameStats::publishStreaming(const Streaming& streaming)
{
    QMutexLocker locker(&m_mutex);
    m_streaming = streaming;
    ++m_revision;
}

cwRenderFrameStats::Culling cwRenderFrameStats::culling() const
{
    QMutexLocker locker(&m_mutex);
    return m_culling;
}

cwRenderFrameStats::Streaming cwRenderFrameStats::streaming() const
{
    QMutexLocker locker(&m_mutex);
    return m_streaming;
}

quint64 cwRenderFrameStats::revision() const
{
    QMutexLocker locker(&m_mutex);
    return m_revision;
}
