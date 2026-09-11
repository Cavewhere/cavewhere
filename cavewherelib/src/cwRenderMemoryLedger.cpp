/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderMemoryLedger.h"

//Qt includes
#include <QMutexLocker>

cwRenderMemoryLedger* cwRenderMemoryLedger::instance()
{
    static cwRenderMemoryLedger ledger;
    return &ledger;
}

void cwRenderMemoryLedger::adjust(Category category, Residency residency, qint64 deltaBytes)
{
    QMutexLocker locker(&m_mutex);

    const Key key {category, residency};
    qint64& total = m_bytes[key];
    total += deltaBytes;

    if (total < 0) {
        total = 0;

        if (!m_underflowWarnedCategories.contains(category)) {
            m_underflowWarnedCategories.insert(category);
            qWarning("cwRenderMemoryLedger: byte total for category %d went negative and was "
                     "clamped to 0. A reported allocation was freed twice or never reported.",
                     static_cast<int>(category));
        }
    }

    ++m_revision;
}

qint64 cwRenderMemoryLedger::bytes(Category category, Residency residency) const
{
    QMutexLocker locker(&m_mutex);
    return m_bytes.value(Key {category, residency});
}

qint64 cwRenderMemoryLedger::totalBytes(Residency residency) const
{
    QMutexLocker locker(&m_mutex);

    qint64 total = 0;
    for (auto iter = m_bytes.constBegin(); iter != m_bytes.constEnd(); ++iter) {
        if (iter.key().residency == residency) {
            total += iter.value();
        }
    }
    return total;
}

quint64 cwRenderMemoryLedger::revision() const
{
    QMutexLocker locker(&m_mutex);
    return m_revision;
}

cwLedgeredBytes::cwLedgeredBytes(cwRenderMemoryLedger::Category category,
                                 cwRenderMemoryLedger::Residency residency) :
    m_category(category),
    m_residency(residency)
{
}

cwLedgeredBytes::~cwLedgeredBytes()
{
    setBytes(0);
}

cwLedgeredBytes::cwLedgeredBytes(cwLedgeredBytes&& other) noexcept :
    m_category(other.m_category),
    m_residency(other.m_residency),
    m_bytes(other.m_bytes)
{
    other.m_bytes = 0;
}

cwLedgeredBytes& cwLedgeredBytes::operator=(cwLedgeredBytes&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    setBytes(0);

    m_category = other.m_category;
    m_residency = other.m_residency;
    m_bytes = other.m_bytes;
    other.m_bytes = 0;

    return *this;
}

void cwLedgeredBytes::setBytes(qint64 bytes)
{
    if (bytes == m_bytes) {
        return;
    }

    const qint64 delta = bytes - m_bytes;
    m_bytes = bytes;
    cwRenderMemoryLedger::instance()->adjust(m_category, m_residency, delta);
}
