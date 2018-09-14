/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUSEDSTATIONTASKMANAGER_H
#define CWUSEDSTATIONTASKMANAGER_H

//Qt includes
#include <QObject>
#include <QStringList>
#include <QPointer>
#include <QModelIndex>

//Our includes
class cwCave;
class cwTrip;
#include "cwSurveyChunk.h"
#include "cwUsedStationsTask.h"

/**
 * @brief The cwUsedStationTaskManager class
 *
 * This will create a list of used stations from either a whole cave or a single trip. If you want
 * to find the list of the station for a cave, use the setCave(). For a trip, use setTrip(). This
 * class will assert if both the cave and trip are set at the same time. Make sure you call setCave
 * or setTrip with null before switching between them.
 *
 * The manager will also do the calculation in another thread or the current thread by using the
 * threaded property. It's best to setup the threaded property before setting the cave or trip.
 * By default, threading is turned off.
 *
 * After the manager has completed calculating the usedStations for either the cave or the trip, it
 * will emit the usedStationChanged signal. And the results are avaliable in usedStations().
 */
class cwUsedStationTaskManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCave* cave READ cave WRITE setCave NOTIFY caveChanged)
    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)

    Q_PROPERTY(bool listenToChanges READ listenToChanges WRITE setListenToChanges NOTIFY listenToChangesChanged)
    Q_PROPERTY(QStringList usedStations READ usedStations NOTIFY usedStationsChanged)

    //Settings for how the data is displayed
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY boldChanged)
    Q_PROPERTY(bool abbreviated READ abbreviated WRITE setAbbreviated NOTIFY abbreviatedChanged)
    Q_PROPERTY(bool onlyLargestRange READ onlyLargestRange WRITE setOnlyLargestRange NOTIFY onlyLargestRangeChanged)

public:
    explicit cwUsedStationTaskManager(QObject *parent = 0);
    ~cwUsedStationTaskManager();

    bool threaded() const;
    void setThreaded(bool threaded);

    void setListenToChanges(bool listen);
    bool listenToChanges() const;

    void setCave(cwCave* cave);
    cwCave* cave();

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    bool bold() const;
    void setBold(bool bold);

    bool abbreviated() const;
    void setAbbreviated(bool abbreviated);

    bool onlyLargestRange() const;
    void setOnlyLargestRange(bool onlyLargestRange);

    QStringList usedStations() const;

public slots:
    void calculateUsedStations();

signals:
    void usedStationsChanged();
    void threadedChanged();
    void caveChanged();
    void tripChanged();
    void listenToChangesChanged();
    void boldChanged();
    void abbreviatedChanged();
    void onlyLargestRangeChanged();

private slots:
    void setUsedStations(QList<QString> stations);

    void connectAddedTrips(QModelIndex parent, int begin, int end);
    void connectAddedChunks(int begin, int end);

    void disconnectRemovedTrips(QModelIndex parent, int begin, int end);
    void disconnectRemovedChunks(int begin, int end);

    void filterChunkDataChanged(cwSurveyChunk::DataRole role, int index);

private:
    QPointer<cwCave> Cave; //The cave where all the stations live
    QPointer<cwTrip> Trip;

    cwUsedStationsTask* Task; //The task that creates the used stations
    QStringList UsedStations; //List of used stations
    bool ListenToChanges; //This flag connects the cave to task to rerun it

    cwUsedStationsTask::Settings TaskSettings;

    QList<QString> uoallCaveStationNames() const;
    void hookupCaveToTask();

    void connectCave(cwCave* cave);
    void connectTrip(cwTrip* trip);
    void connectChunk(cwSurveyChunk* chunk);

    void disconnectCave(cwCave* cave);
    void disconnectTrip(cwTrip* trip);
    void disconnectChunk(cwSurveyChunk* chunk);
};




/**
  Get's the used stations for the manager.  This wont be valid until calculateUsedStations
  is called and usedStationChanged is emitted
  */
inline QStringList cwUsedStationTaskManager::usedStations() const {
    return UsedStations;
}

/**
  This flag connects the cave to the task to rerun it when the cave's data has
  changed
  */
inline bool cwUsedStationTaskManager::listenToChanges() const {
    return ListenToChanges;
}


/**
* @brief cwUsedStationTaskManager::bold
* @return
*/
inline bool cwUsedStationTaskManager::bold() const {
    return TaskSettings.bold();
}

/**
* @brief cwUsedStationTaskManager::abbreviated
* @return
*/
inline bool cwUsedStationTaskManager::abbreviated() const {
    return TaskSettings.abbreviated();
}

/**
* @brief cwUsedStationTaskManager::onlyLargestRange
* @return
*/
inline bool cwUsedStationTaskManager::onlyLargestRange() const {
    return TaskSettings.onlyLargestRange();
}

#endif // CWUSEDSTATIONTASKMANAGER_H
