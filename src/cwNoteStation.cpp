#include "cwNoteStation.h"

cwNoteStation::cwNoteStation() :
    Data(new PrivateData())
{

}

/**
  Sets the position of the station on the page of notes.  This is in note coordinate system,
  where the top left corner of the page of notes is the origin.  Should be normalized.

  Any values greater than 1.0 or less than 0.0 will be clamped
  */
void cwNoteStation::setPositionOnNote(QPointF point) {
    if(Data->PositionOnNote != point) {
        Data->PositionOnNote = point;
    }
}

/**
  Sets the station that this note will refer to
  */
void cwNoteStation::setStation(const cwStationReference& reference) {
    if(Data->Station != reference) {
        Data->Station = reference;
    }
}
