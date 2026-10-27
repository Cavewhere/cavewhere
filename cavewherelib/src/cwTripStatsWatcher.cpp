/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTripStatsWatcher.h"
#include "cwTrip.h"
#include "cwTripLengthTask.h"
#include "cwUsedStationTaskManager.h"

/**
  \brief Watches \a trip, starting both tasks at once

  The length task is started ahead of the used-station manager, so a trip whose
  shots change reports its length first and its stations second.
  */
cwTripStatsWatcher::cwTripStatsWatcher(cwTrip* trip, QObject* parent) :
    QObject(parent),
    m_trip(trip),
    m_lengthTask(new cwTripLengthTask(this)),
    m_usedStationsManager(new cwUsedStationTaskManager(this))
{
    m_usedStationsManager->setAbbreviated(true);
    m_usedStationsManager->setBold(false);
    m_usedStationsManager->setOnlyLargestRange(true);

    connect(m_lengthTask, &cwTripLengthTask::finished, this, [this]() {
        m_length = m_lengthTask->length();
        emit lengthChanged();
    });

    connect(m_usedStationsManager, &cwUsedStationTaskManager::usedStationsChanged, this, [this]() {
        m_usedStations = m_usedStationsManager->usedStations();

        //The manager reports ranges rather than a count, and the ranges are the
        //form a table shows, so the count comes from the trip itself.
        m_stationCount = m_trip.isNull() ? 0 : m_trip->uniqueStations().size();

        emit usedStationsChanged();
    });

    m_lengthTask->setTrip(trip);
    m_usedStationsManager->setTrip(trip);
}

//! Runs the used-station task to completion. Test-only.
void cwTripStatsWatcher::waitForFinished()
{
    m_usedStationsManager->waitForFinished();
}
