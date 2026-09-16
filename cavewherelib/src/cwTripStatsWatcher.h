/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIPSTATSWATCHER_H
#define CWTRIPSTATSWATCHER_H

//Our includes
#include "cwGlobals.h"
class cwTrip;
class cwTripLengthTask;
class cwUsedStationTaskManager;

//Qt includes
#include <QObject>
#include <QPointer>
#include <QStringList>

/**
 * The surveyed length and the used stations of one cwTrip, recalculated as the
 * trip's shots change.
 *
 * The two numbers a trip row shows come from two asynchronous tasks —
 * cwTripLengthTask and cwUsedStationTaskManager — that every model showing
 * trips would otherwise own a copy of. One watcher per trip owns both tasks and
 * announces each result with its own signal, so cwCavePageModel and
 * cwSurveyTreeModel read the same numbers from the same mechanism.
 *
 * The values are empty until the tasks report: a freshly built watcher reads
 * zero length and no stations, and lengthChanged()/usedStationsChanged() say
 * when each has arrived.
 */
class CAVEWHERE_LIB_EXPORT cwTripStatsWatcher : public QObject
{
    Q_OBJECT

public:
    explicit cwTripStatsWatcher(cwTrip* trip, QObject* parent = nullptr);

    //! The trip these statistics belong to, null once the trip is destroyed.
    cwTrip* trip() const;

    //! The trip's surveyed length, in the trip calibration's distance unit.
    double length() const;

    //! The trip's stations as abbreviated ranges, the form a trip table shows.
    QStringList usedStations() const;

    //! The number of stations the trip names, each name counted once.
    int stationCount() const;

    void waitForFinished();

signals:
    void lengthChanged();

    //! Both usedStations() and stationCount() have been recalculated.
    void usedStationsChanged();

private:
    QPointer<cwTrip> m_trip;
    cwTripLengthTask* m_lengthTask;
    cwUsedStationTaskManager* m_usedStationsManager;

    double m_length = 0.0;
    QStringList m_usedStations;
    int m_stationCount = 0;
};

inline cwTrip* cwTripStatsWatcher::trip() const {
    return m_trip;
}

inline double cwTripStatsWatcher::length() const {
    return m_length;
}

inline QStringList cwTripStatsWatcher::usedStations() const {
    return m_usedStations;
}

inline int cwTripStatsWatcher::stationCount() const {
    return m_stationCount;
}

#endif // CWTRIPSTATSWATCHER_H
