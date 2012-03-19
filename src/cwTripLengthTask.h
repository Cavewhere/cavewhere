#ifndef CWTRIPLENGTHTASK_H
#define CWTRIPLENGTHTASK_H

//Our includes
#include "cwTask.h"
class cwTrip;
class cwSurveyChunk;

/**
  This class isn't thread safe!

  This calculates the leghth of the current trip
  */
class cwTripLengthTask : public cwTask
{
    Q_OBJECT

    Q_PROPERTY(double length READ length NOTIFY lengthChanged)
    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)

public:
    explicit cwTripLengthTask(QObject *parent = 0);

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    double length() const;

protected:
    void runTask();

signals:
   void lengthChanged();
    void tripChanged();

private slots:
   void chunkRemoved(int begin, int end);
   void chunkAdded(int begin, int end);

private:
   cwTrip* Trip;
   double Length; //!< The length of the trip

   void connectChunks();
   void connectChunk(cwSurveyChunk* chunk);

   void disconnectChunks();
   void disconnectChunk(cwSurveyChunk* chunk);

   double distanceOfChunk(const cwSurveyChunk *chunk) const;
};

/**
Gets length
*/
inline double cwTripLengthTask::length() const {
    return Length;
}

/**
Gets trip
*/
inline cwTrip* cwTripLengthTask::trip() const {
    return Trip;
}



#endif // CWTRIPLENGTHTASK_H
