/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIPLENGTHTASK_H
#define CWTRIPLENGTHTASK_H

//Our includes
#include "cwTask.h"
class cwTrip;
class cwSurveyChunk;

//Qt includes
#include <QPair>
#include <QPointer>

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
   void chunkAdded(int begin, int end);

   void disconnectTrip();

private:
   QPointer<cwTrip> Trip;
   double Length; //!< The length of the trip

   void connectChunks();
   void connectChunk(cwSurveyChunk* chunk);

   void disconnectChunks();
   void disconnectChunk(cwSurveyChunk* chunk);

   QPair<double, int> distanceOfChunk(const cwSurveyChunk *chunk) const;
};

/**
Gets length
*/
inline double cwTripLengthTask::length() const {
    return Length;
}




#endif // CWTRIPLENGTHTASK_H
