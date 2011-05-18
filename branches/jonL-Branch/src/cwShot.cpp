//Our includes
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwStationReference.h"


cwShot::cwShot()
{
    Distance = "";
    Compass = "";
    BackCompass = "";
    Clino = "";
    BackClino = "";
    ParentChunk = NULL;

}

cwShot::cwShot(QString distance,
               QString compass,
               QString backCompass,
               QString clino,
               QString backClino)
{

    Distance = distance;
    Compass = compass;
    BackCompass = backCompass;
    Clino = clino;
    BackClino = backClino;
    ParentChunk = NULL;
}

cwShot::cwShot(const cwShot& shot) {
    Distance = shot.Distance;
    Compass = shot.Compass;
    BackCompass = shot.BackCompass;
    Clino = shot.Clino;
    BackClino = shot.BackClino;
}


void cwShot::SetDistance(QString distance) {
    Distance = distance;
}

void cwShot::SetCompass(QString compass) {
    Compass = compass;
}

void cwShot::SetBackCompass(QString backCompass) {
    BackCompass = backCompass;
}

void cwShot::SetClino(QString clino) {
    Clino = clino;
}

void cwShot::SetBackClino(QString backClino) {
    BackClino = backClino;
}

/**
  Sets the parent chunk
  */
void cwShot::setParentChunk(cwSurveyChunk* parentChunk) {
    ParentChunk = parentChunk;
}

/**
  \brief The parent chunk that this shot is connected to
  */
cwSurveyChunk* cwShot::parentChunk() const {
    return ParentChunk;
}

/**
  \brief The to station of this shot
  */
cwStationReference cwShot::toStation() const {
    cwSurveyChunk* chunk = parentChunk();
    if(chunk != NULL) {
        return chunk->ToFromStations(this).second;
    }
    return cwStationReference();
}

/**
  \brief The from station of these shot
  */
cwStationReference cwShot::fromStation() const {
    cwSurveyChunk* chunk = parentChunk();
    if(chunk != NULL) {
        return chunk->ToFromStations(this).first;
    }
    return cwStationReference();
}
