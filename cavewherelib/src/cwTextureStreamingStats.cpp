/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTextureStreamingStats.h"

//Qt includes
#include <QMutexLocker>

cwTextureStreamingStats* cwTextureStreamingStats::instance()
{
    static cwTextureStreamingStats stats;
    return &stats;
}

void cwTextureStreamingStats::publish(const Counts& counts)
{
    QMutexLocker locker(&m_mutex);
    m_counts = counts;
    ++m_revision;
}

cwTextureStreamingStats::Counts cwTextureStreamingStats::counts() const
{
    QMutexLocker locker(&m_mutex);
    return m_counts;
}

quint64 cwTextureStreamingStats::revision() const
{
    QMutexLocker locker(&m_mutex);
    return m_revision;
}
