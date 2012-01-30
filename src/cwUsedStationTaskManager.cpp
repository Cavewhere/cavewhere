//Our includes
#include "cwUsedStationTaskManager.h"
#include "cwUsedStationsTask.h"
#include "cwCave.h"

//Qt includes
#include <QThread>

cwUsedStationTaskManager::cwUsedStationTaskManager(QObject *parent) :
    QObject(parent)
{
    Thread = new QThread(this);
    Thread->start();

    Task = new cwUsedStationsTask();
    Task->setThread(Thread);
    connect(Task, SIGNAL(shouldRerun()), SLOT(calculateUsedStations()));
    connect(Task, SIGNAL(usedStations(QList<QString>)), SLOT(setUsedStations(QList<QString>)));
}

cwUsedStationTaskManager::~cwUsedStationTaskManager() {
    Task->stop();

    QMetaObject::invokeMethod(Thread, "quit"); //Quit the event loop
    Thread->wait(); //wait to finish

    delete Task;
}

/**
  If listen to cave changes is set to true,
  */
void cwUsedStationTaskManager::setListenToCaveChanges(bool listen) {
    if(ListenToCaveChanges != listen) {
        ListenToCaveChanges = listen;

        hookupCaveToTask();

        if(ListenToCaveChanges) {
            calculateUsedStations();
        }
    }
}

/**
  Sets the cave for the manager
  */
void cwUsedStationTaskManager::setCave(cwCave* cave) {
    Cave = cave;
    calculateUsedStations();
}

/**
  This starts the running the used station task on an external thread
  */
void cwUsedStationTaskManager::calculateUsedStations() {
    if(Cave.isNull()) { return; }

    if(Task->isReady()) {
        QList<QString> stationNames = allCaveStationNames();

        QMetaObject::invokeMethod(Task, //Object
                                  "setStationNames", //Function
                                  Qt::AutoConnection, //Connection to the function
                                  Q_ARG(QList<QString>, stationNames)); //Arguments to the function

        Task->start();
    } else {
        Task->restart();
    }
}

/**
  Called when the task has completed, stations are set to this object
  */
void cwUsedStationTaskManager::setUsedStations(QList<QString> stations) {
    UsedStations = stations;
    emit usedStationsChanged();
}

/**
  Gets all the station names for the current cave.

  This assumes the cave isn't null
 */
QList<QString> cwUsedStationTaskManager::allCaveStationNames() const {
    cwCave* currentCave = Cave.data();
    QList< QWeakPointer <cwStation> > stations = currentCave->stations();

    QList< QString > stationNames;
    foreach( QWeakPointer <cwStation> currentStation, stations) {
        if(currentStation.isNull()) { continue; }

        cwStation* station = currentStation.data();
        stationNames.append(station->name());
    }

    return stationNames;
}

/**
  \brief Helper function to setListenToCaveChanges

  This hooks up the cave to the task or disconnect it, depending on the value of ListenToCaveCHanges.

  If ListenToCaveChanges is true it connects it, else it disconnects it.

  If Cave is null this function does nothing
  */
void cwUsedStationTaskManager::hookupCaveToTask() {
    if(Cave.isNull()) { return; }
    cwCave* cave = Cave.data();
    if(ListenToCaveChanges) {
        connect(cave, SIGNAL(stationAddedToCave(QString)), SLOT(calculateUsedStations()));
        connect(cave, SIGNAL(stationRemovedFromCave(QString)), SLOT(calculateUsedStations()));
    } else {
        disconnect(cave, 0, this, 0);
    }
}
