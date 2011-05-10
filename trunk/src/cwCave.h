#ifndef CWCAVE_H
#define CWCAVE_H

//Our include
class cwTrip;
#include <cwStation.h>

//Qt includes
#include <QObject>
#include <QList>
#include <QSharedPointer>
#include <QWeakPointer>
#include <QUndoCommand>

class cwCave : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

    friend class NameCommand;

public:
    explicit cwCave(QObject* parent = NULL);
    cwCave(const cwCave& object);
    cwCave& operator=(const cwCave& object);

    QString name() const;
    void setName(QString name);

    int tripCount() const;
    cwTrip* trip(int index) const;
    void addTrip(cwTrip* trip);
    void insertTrip(int i, cwTrip* trip);
    void removeTrip(int i);

    bool hasStation(QString name);
    QWeakPointer<cwStation> station(QString name);
    void addStation(QSharedPointer<cwStation> station);
    void removeStation(QString name);

    QList< QWeakPointer<cwStation> > stations() const;

signals:
    void insertedTrips(int begin, int end);
    void removedTrips(int begin, int end);

    void nameChanged(QString name);

    void stationAddedToCave(QString name);
    void stationRemovedFromCave(QString name);

protected:
    QList<cwTrip*> Trips;
    QString Name;

    QMap<QString, QWeakPointer<cwStation> > StationLookup;

private:
    cwCave& Copy(const cwCave& object);




    class NameCommand : public QUndoCommand {
    public:
        NameCommand(cwCave* cave, QString name);
        void redo();
        void undo();
    private:
        cwCave* Cave;
        QString newName;
        QString oldName;
    };

};

/**
  \brief Get's the name of the cave
  */
inline QString cwCave::name() const {
    return Name;
}

/**
  \brief Get's the number of survey trips in the cave
  */
inline int cwCave::tripCount() const {
    return Trips.count();
}

/**
  \brief Get's the trip at an index

  If the index is out of bounds this return NULL
  */
inline cwTrip* cwCave::trip(int index) const {
    if(index < 0 || index >= Trips.size()) { return NULL; }
    return Trips[index];
}

/**
  \brief Test if this caves has a station with the name, name.

  Returns true if it has a station with that name
  */
inline bool cwCave::hasStation(QString name) {
    QWeakPointer<cwStation> pointer = station(name);
    return !pointer.isNull();
}

/**
  \brief Get's the station from it's name
  */
inline QWeakPointer<cwStation> cwCave::station(QString name) {
    return StationLookup.value(name, QWeakPointer<cwStation>());
}

/**
  \brief Adds the station to the cave
  */
inline void cwCave::addStation(QSharedPointer<cwStation> station) {
    if(!station->name().isEmpty()) {
        StationLookup[station->name()] = station.toWeakRef();
    }
}

/**
  \brief Gets all the stations in the cave

  To figure out how stations are structured see cwTrip and cwSurveyChunk

  Shots and stations are stored in the cwSurveyChunk
  */
inline QList< QWeakPointer<cwStation> > cwCave::stations() const {
    return StationLookup.values();
}



#endif // CWCAVE_H
