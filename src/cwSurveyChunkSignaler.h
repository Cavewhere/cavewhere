/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSURVEYCHUNKSIGNALER_H
#define CWSURVEYCHUNKSIGNALER_H

//Qt includes
#include <QObject>
#include <QPointer>

//Cavewhere includes
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyChunk;
#include "cwGlobals.h"

/**
 * @brief The cwSurveyChunkSignaler class
 *
 * This class saves the programmer typing when trying to connect to all the caves, trips, or
 * cwSurveyChunks in a cwCavingRegion. This is usually to listen for when data changes. For examlpe
 * the cwLinePlotManager class re-runs the line plot when survey data changes. Calling addConnectionTo*()
 * will setup a signal slot connection between caves, trips, or chunks in the caving region. Recieving
 * slot can use QObject::sender() to figure out what object emited the signal.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyChunkSignaler : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)

public:
    explicit cwSurveyChunkSignaler(QObject *parent = 0);

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

    void addConnectionToCaves(const char* signal, QObject* reciever, const char* slot);
    void addConnectionToTrips(const char* signal, QObject* reciever, const char* slot);
    void addConnectionToTripCalibrations(const char* signal, QObject* reciever, const char* slot);
    void addConnectionToChunks(const char* signal, QObject* reciever, const char* slot);


signals:
    void regionChanged();

public slots:

private:
    class Connection {
    public:
        Connection();
        Connection(QByteArray signal,
                   QObject* reciever,
                   QByteArray slot) :
            Signal(signal),
            Reciever(reciever),
            Slot(slot) {}

        QByteArray Signal;
        QPointer<QObject> Reciever;
        QByteArray Slot;

        bool operator==(const Connection& other) const {
            return Signal == other.Signal && Reciever == other.Reciever && Slot == other.Slot;
        }

        void connect(QObject* sender) const;
        void disconnect(QObject* sender) const;
    };

   QPointer<cwCavingRegion> Region;
   QList<Connection> CaveConnections;
   QList<Connection> TripConnections;
   QList<Connection> ChunkConnections;
   QList<Connection> TripCalibrationConnections;

   void connectCaves(cwCavingRegion* region);
   void connectCave(cwCave* cave);
   void connectTrips(cwCave* cave);
   void connectTrip(cwTrip* trip);
   void connectChunks(cwTrip* trip);
   void connectChunk(cwSurveyChunk* chunk);

   void disconnectCave(cwCave* cave);
   void disconnectTrips(cwCave* cave, int beginIndex, int endIndex);
   void disconnectTrip(cwTrip* trip);
   void disconnectSurveyChunks(cwTrip* trip, int beginIndex, int endIndex);
   void disconnectSurveyChunk(cwSurveyChunk* chunk);

   void connectAll(QObject* sender, const QList<Connection>& connections) const;
   void disconnectAll(QObject* sender, const QList<Connection>& connections) const;

private slots:
   void connectAddedCaves(int beginIndex, int endIndex);
   void connectAddedTrips(int beginIndex, int endIndex);
   void connectAddedChunks(int beginIndex, int endIndex);

   void disconnectRemovedCaves(int beginIndex, int endIndex);
   void disconnectRemovedTrips(int beginIndex, int endIndex);
   void disconnectRemovedChunks(int beginIndex, int endIndex);

};

#endif // CWSURVEYCHUNKSIGNALER_H
