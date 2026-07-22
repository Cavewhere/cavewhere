/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTATIONHANDLE_H
#define CWSTATIONHANDLE_H

//Qt includes
#include <QHashFunctions>
#include <QMetaType>
#include <QString>
#include <QUuid>

//Our includes
#include "cwGlobals.h"

/**
 * Location-independent identity of a single station inside an equate
 * (cwEquate). A handle names a station by the scope it lives in plus the
 * tail (the station name relative to that scope), never by a fully-qualified
 * flat string. That keeps the tie valid when a trip is re-parented or a
 * cave/trip is renamed, and lets one handle type serve both the cwCave
 * (within-cave) and cwCavingRegion (cross-cave) equate homes.
 *
 * containerId is always populated (option A in the plan):
 *   - NativeCave: containerId is the owning cave's id; the station is a bare
 *     native station (or a cave-level-externalCenterline station directly
 *     under cave_<hex>.).
 *   - Trip: containerId is the owning trip's id; covers both attached external
 *     trips (trip_<hex>.) and native-prefixed Scope trips (stationPrefix). The
 *     concrete prefix is rendered at emission time via cwTrip::scopePrefix().
 */
class CAVEWHERE_LIB_EXPORT cwStationHandle
{
    Q_GADGET
    Q_PROPERTY(Scope scope READ scope WRITE setScope FINAL)
    Q_PROPERTY(QUuid containerId READ containerId WRITE setContainerId FINAL)
    Q_PROPERTY(QString tail READ tail WRITE setTail FINAL)

public:
    enum Scope {
        NativeCave,
        Trip
    };
    Q_ENUM(Scope)

    cwStationHandle() = default;
    cwStationHandle(Scope scope, const QUuid& containerId, const QString& tail)
        : m_scope(scope), m_containerId(containerId), m_tail(tail) {}

    Scope scope() const { return m_scope; }
    void setScope(Scope scope) { m_scope = scope; }

    QUuid containerId() const { return m_containerId; }
    void setContainerId(const QUuid& containerId) { m_containerId = containerId; }

    QString tail() const { return m_tail; }
    void setTail(const QString& tail) { m_tail = tail; }

    //! A handle is structurally valid once it names a container and a station
    //! within it. Membership (does this container actually live in the owner
    //! cave?) is a separate, home-specific check on cwCave.
    bool isValid() const { return !m_containerId.isNull() && !m_tail.isEmpty(); }

    bool operator==(const cwStationHandle& other) const
    {
        return m_scope == other.m_scope
                && m_containerId == other.m_containerId
                && m_tail == other.m_tail;
    }
    bool operator!=(const cwStationHandle& other) const { return !(*this == other); }

private:
    Scope m_scope = NativeCave;
    QUuid m_containerId;
    QString m_tail;
};

CAVEWHERE_LIB_EXPORT size_t qHash(const cwStationHandle& value, size_t seed = 0) noexcept;

Q_DECLARE_METATYPE(cwStationHandle)

#endif // CWSTATIONHANDLE_H
