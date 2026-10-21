/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERMEMORYLEDGER_H
#define CWRENDERMEMORYLEDGER_H

//Qt includes
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QtGlobal>

//Our includes
#include "cwGlobals.h"

// Process-wide byte accounting for render resources. Render objects live on the
// render thread and several 3D views can be alive at once, so a single shared
// instance totals everything and a QMutex guards the map. It has no QObject
// thread affinity on purpose: any thread may report into it.
class CAVEWHERE_LIB_EXPORT cwRenderMemoryLedger
{
public:
    enum class Category {
        PointCloudGeometry,
        TexturedItemGeometry,
        TexturedItemTexture,
        LinePlotGeometry,
        Other
    };

    enum class Residency {
        Gpu,
        Cpu
    };

    static cwRenderMemoryLedger* instance();

    void adjust(Category category, Residency residency, qint64 deltaBytes);
    qint64 bytes(Category category, Residency residency) const;
    qint64 totalBytes(Residency residency) const;
    quint64 revision() const;

private:
    cwRenderMemoryLedger() = default;

    struct Key {
        Category category;
        Residency residency;

        bool operator==(const Key& other) const = default;
    };

    friend size_t qHash(const Key& key, size_t seed)
    {
        return qHashMulti(seed, static_cast<int>(key.category), static_cast<int>(key.residency));
    }

    mutable QMutex m_mutex;
    QHash<Key, qint64> m_bytes;
    QSet<Category> m_underflowWarnedCategories;
    quint64 m_revision = 0;
};

// RAII reporter for one tracked resource. The owner holds one of these per
// resource and calls setBytes() as the resource is sized; destruction returns
// the reported bytes to the ledger. Move-only: moving transfers the reported
// value, leaving the moved-from reporter at zero.
class CAVEWHERE_LIB_EXPORT cwLedgeredBytes
{
public:
    cwLedgeredBytes(cwRenderMemoryLedger::Category category,
                    cwRenderMemoryLedger::Residency residency);
    ~cwLedgeredBytes();

    cwLedgeredBytes(const cwLedgeredBytes&) = delete;
    cwLedgeredBytes& operator=(const cwLedgeredBytes&) = delete;

    cwLedgeredBytes(cwLedgeredBytes&& other) noexcept;
    cwLedgeredBytes& operator=(cwLedgeredBytes&& other) noexcept;

    void setBytes(qint64 bytes);
    qint64 bytes() const { return m_bytes; }

private:
    cwRenderMemoryLedger::Category m_category;
    cwRenderMemoryLedger::Residency m_residency;
    qint64 m_bytes = 0;
};

#endif // CWRENDERMEMORYLEDGER_H
