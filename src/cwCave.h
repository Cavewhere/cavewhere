/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAVE_H
#define CWCAVE_H

//Our include
class cwTrip;
class cwLength;
#include "cwStation.h"
#include "cwUndoer.h"
#include "cwStationPositionLookup.h"

//Qt includes
#include <QObject>
#include <QList>
#include <QSharedPointer>
#include <QUndoCommand>
#include <QDebug>
#include <QWeakPointer>
#include <QVariant>

class cwCave : public QObject, public cwUndoer
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(cwLength* length READ length CONSTANT)
    Q_PROPERTY(cwLength* depth READ depth CONSTANT)

public:
    explicit cwCave(QObject* parent = nullptr);
    cwCave(const cwCave& object);
    cwCave& operator=(const cwCave& object);
    ~cwCave();

    QString name() const;
    void setName(QString name);

    cwLength* length() const;
    cwLength* depth() const;

    int tripCount() const;
    cwTrip* trip(int index) const;
    bool hasTrips() const { return tripCount() > 0; }
    QList<cwTrip*> trips() const;

    Q_INVOKABLE void addTrip(cwTrip* trip = nullptr);
    void insertTrip(int i, cwTrip* trip);
    void removeTrip(int i);
    int indexOf(cwTrip* trip) const;

    cwStationPositionLookup stationPositionLookup() const;
    void setStationPositionLookup(const cwStationPositionLookup& model);

    void setStationPositionLookupStale(bool isStale);
    bool isStationPositionLookupStale() const;

    QList< cwStation > stations() const;

signals:
    void beginInsertTrips(int begin, int end);
    void insertedTrips(int begin, int end);

    void beginRemoveTrips(int begin, int end);
    void removedTrips(int begin, int end);

    void nameChanged(QString name);

    void stationPositionPositionChanged();

protected:
    QList<cwTrip*> Trips;
    QString Name;

    cwLength* Length;
    cwLength* Depth;

    cwStationPositionLookup StationPositionModel;
    bool StationPositionModelStale;

    virtual void setUndoStackForChildren();
private:
    cwCave& Copy(const cwCave& object);
    void addTripNullHelper();

////////////////////// Undo Redo commands ///////////////////////////////////
    class NameCommand : public QUndoCommand {
    public:
        NameCommand(cwCave* cave, QString name);
        void redo();
        void undo();
    private:
        cwCave* CavePtr;
        QString newName;
        QString oldName;
    };

    class InsertRemoveTrip : public QUndoCommand {
    public:
        InsertRemoveTrip(cwCave* cave, int beginIndex, int endIndex);
        ~InsertRemoveTrip();

    protected:
        void insertTrips();
        void removeTrips();

        QList<cwTrip*> Trips;
    private:
        cwCave* CavePtr;
        int BeginIndex;
        int EndIndex;
        bool OwnsTrips;
    };

    class InsertTripCommand : public InsertRemoveTrip {
    public:
        InsertTripCommand(cwCave* cave, cwTrip* Trip, int index);
        InsertTripCommand(cwCave* cave, QList<cwTrip*> Trip, int index);
        virtual void redo();
        virtual void undo();
    };

    class RemoveTripCommand : public InsertRemoveTrip {
    public:
        RemoveTripCommand(cwCave* cave, int beginIndex, int endIndex);
        virtual void redo();
        virtual void undo();
    };

};

Q_DECLARE_METATYPE(cwCave*)

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
  \brief Get's all the trips in the cave
  */
inline QList<cwTrip*> cwCave::trips() const {
    return Trips;
}

/**
  \brief Get's the trip at an index

  If the index is out of bounds this return nullptr
  */
inline cwTrip* cwCave::trip(int index) const {
    if(index < 0 || index >= Trips.size()) { return nullptr; }
    return Trips[index];
}

/**
 * @brief cwCave::length
 * @return The cave's current length
 */
inline cwLength *cwCave::length() const
{
   return Length;
}

/**
 * @brief cwCave::depth
 * @return The cave's current depth
 */
inline cwLength *cwCave::depth() const
{
    return Depth;
}

/**
  \brief Gets the index of the trip inside of the cave
  */
inline int cwCave::indexOf(cwTrip* trip) const {
    return Trips.indexOf(trip);
}

/**
  \brief Gets the station position model for the cave
  */
inline cwStationPositionLookup cwCave::stationPositionLookup() const
{
    return StationPositionModel;
}




#endif // CWCAVE_H
