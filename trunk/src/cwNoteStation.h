#ifndef CWNOTESTATION_H
#define CWNOTESTATION_H

//Qt includes
#include <QSharedData>
#include <QSharedDataPointer>
#include <QPointF>

//Our includes
#include "cwStationReference.h"

class cwNoteStation
{
public:
    cwNoteStation();

    void setStation(const cwStationReference& referance);
    cwStationReference station() const;

    void setPositionOnNote(QPointF point);
    QPointF positionOnNote() const;

private:
    class PrivateData : public QSharedData {

    public:
        PrivateData() { }

        QPointF PositionOnNote;
        cwStationReference Station;
    };

    QSharedDataPointer<PrivateData> Data;
};


/**
  Gets the station
  */
inline cwStationReference cwNoteStation::station() const {
    return Data->Station;
}

/**
  Gets the position of the station on the page of notes.

  The point is normalized from 0.0 to 1.0.
  */
inline QPointF cwNoteStation::positionOnNote() const {
    return Data->PositionOnNote;
}


#endif // CWNOTESTATION_H
