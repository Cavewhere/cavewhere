/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderCullingStats.h"

//Qt includes
#include <QMutexLocker>

cwRenderCullingStats* cwRenderCullingStats::instance()
{
    static cwRenderCullingStats stats;
    return &stats;
}

void cwRenderCullingStats::publish(const Counts& counts)
{
    QMutexLocker locker(&m_mutex);
    m_counts = counts;
    ++m_revision;
}

cwRenderCullingStats::Counts cwRenderCullingStats::counts() const
{
    QMutexLocker locker(&m_mutex);
    return m_counts;
}

quint64 cwRenderCullingStats::revision() const
{
    QMutexLocker locker(&m_mutex);
    return m_revision;
}
