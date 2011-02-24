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

class cwCave : public QObject
{
    Q_OBJECT

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

signals:
    void insertedTrips(int begin, int end);
    void removedTrips(int begin, int end);

    void nameChanged(QString name);

protected:
    QList<cwTrip*> Trips;
    QString Name;

    QMap<QString, QWeakPointer<cwStation> > StationLookup;

private:
    cwCave& Copy(const cwCave& object);

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
        qDebug() << "Station lookup: " << StationLookup;
    }
}



#endif // CWCAVE_H
