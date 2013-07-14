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
#include <QWeakPointer>
#include <QStringList>

//Our includes
class cwCave;
class cwUsedStationsTask;

class cwUsedStationTaskManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCave* cave READ cave WRITE setCave)
    Q_PROPERTY(bool listenToCaveChanges READ listenToCaveChanges WRITE setListenToCaveChanges)
    Q_PROPERTY(QStringList usedStations READ usedStations NOTIFY usedStationsChanged)

public:
    explicit cwUsedStationTaskManager(QObject *parent = 0);
    ~cwUsedStationTaskManager();

    void setListenToCaveChanges(bool listen);
    bool listenToCaveChanges() const;

    void setCave(cwCave* cave);
    cwCave* cave();

    QStringList usedStations() const;

public slots:
    void calculateUsedStations();

signals:
    void usedStationsChanged();

private slots:
    void setUsedStations(QList<QString> stations);
    void caveDestroyed();

private:
    cwCave* Cave; //The cave where all the stations live
    cwUsedStationsTask* Task; //The task that creates the used stations
    QStringList UsedStations; //List of used stations
    bool ListenToCaveChanges; //This flag connects the cave to task to rerun it

    QThread* Thread; //The thread that the task will use

    QList<QString> allCaveStationNames() const;
    void hookupCaveToTask();
};

/**
  Get's the cave for this task
  */
inline cwCave* cwUsedStationTaskManager::cave() {
    return Cave;
}

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
inline bool cwUsedStationTaskManager::listenToCaveChanges() const {
    return ListenToCaveChanges;
}

#endif // CWUSEDSTATIONTASKMANAGER_H
