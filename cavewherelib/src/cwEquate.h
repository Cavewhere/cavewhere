/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEQUATE_H
#define CWEQUATE_H

//Qt includes
#include <QList>
#include <QMetaType>

//Our includes
#include "cwGlobals.h"
#include "cwStationHandle.h"

/**
 * A persistent, user-editable equate: an N-way set (>= 2) of self-contained
 * station handles that all name one physical point. Unlike the import-time
 * flatten (cwSurvexGlobalData), an equate never renames a station - it records
 * the tie and lets emission render a bare *equate line at the right scope
 * (commits 4/5).
 *
 * Value type (copyable, equality-comparable). It lives either in a cwCave's
 * equate list (within-cave ties) or the cwCavingRegion's list (cross-cave
 * ties); the handle carries its own scope, so the same type serves both homes.
 */
class CAVEWHERE_LIB_EXPORT cwEquate
{
    Q_GADGET
    Q_PROPERTY(QList<cwStationHandle> stations READ stations WRITE setStations FINAL)
    Q_PROPERTY(bool isValid READ isValid FINAL)

public:
    cwEquate() = default;
    explicit cwEquate(const QList<cwStationHandle>& stations) : m_stations(stations) {}

    QList<cwStationHandle> stations() const { return m_stations; }
    void setStations(const QList<cwStationHandle>& stations) { m_stations = stations; }

    void addStation(const cwStationHandle& station) { m_stations.append(station); }

    //! Structurally valid when it ties at least two distinct, individually
    //! valid stations. Whether the containers actually live in a given home is
    //! a separate, home-specific check (cwCave::validate).
    bool isValid() const;

    bool operator==(const cwEquate& other) const { return m_stations == other.m_stations; }
    bool operator!=(const cwEquate& other) const { return !(*this == other); }

private:
    QList<cwStationHandle> m_stations;
};

Q_DECLARE_METATYPE(cwEquate)

#endif // CWEQUATE_H
