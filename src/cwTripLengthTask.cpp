/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTripLengthTask.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwTripCalibration.h"

cwTripLengthTask::cwTripLengthTask(QObject *parent) :
    cwTask(parent),
    Trip(nullptr),
    Length(0.0)
{
    setUsingThreadPool(false);
}

/**
  Sets the trip for the lenght task
  */
void cwTripLengthTask::setTrip(cwTrip *trip) {
    if(Trip != trip) {

        if(Trip != nullptr) {
            disconnectTrip();
        }

        Trip = trip;

        if(Trip != nullptr) {
            connect(Trip, SIGNAL(destroyed()), SLOT(disconnectTrip()));
            connect(Trip, SIGNAL(chunksInserted(int,int)), SLOT(chunkAdded(int,int)));
            connect(Trip->calibrations(), SIGNAL(tapeCalibrationChanged(double)), SLOT(restart()));
            connectChunks();

            restart();
        }

        tripChanged();
    }
}

void cwTripLengthTask::connectChunks()
{
    foreach(cwSurveyChunk* chunk, Trip->chunks()) {
        connectChunk(chunk);
    }
}

void cwTripLengthTask::connectChunk(cwSurveyChunk *chunk)
{
    connect(chunk, SIGNAL(shotsAdded(int,int)), SLOT(restart()));
    connect(chunk, SIGNAL(shotsRemoved(int,int)), SLOT(restart()));
    connect(chunk, SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)), SLOT(restart()));
}

void cwTripLengthTask::disconnectChunks()
{
    if(!Trip.isNull()) {
        foreach(cwSurveyChunk* chunk, Trip->chunks()) {
            disconnectChunk(chunk);
        }
    }
}

void cwTripLengthTask::disconnectChunk(cwSurveyChunk *chunk)
{
    disconnect(chunk, nullptr, this, nullptr);
}

/**
  Returns the distance of the chunk and the number of shots in the chunk
  */
QPair<double, int> cwTripLengthTask::distanceOfChunk(const cwSurveyChunk *chunk) const
{
    double distance = 0.0;
    int numberOfShots = 0;
    foreach(cwShot shot, chunk->shots()) {
        if(shot.distanceState() == cwDistanceStates::Valid &&
                shot.isDistanceIncluded()) {
            distance += shot.distance();
            numberOfShots++;
        }
    }
    return QPair<double, int>(distance, numberOfShots);
}


void cwTripLengthTask::chunkAdded(int begin, int end)
{
    for(int i = begin; i <= end; i++) {
        cwSurveyChunk* chunk = Trip->chunk(i);
        connectChunk(chunk);
    }
}

/**
  Disconnects the trip from the task
  */
void cwTripLengthTask::disconnectTrip() {
    if(!Trip.isNull()) {
        disconnect(Trip, nullptr, this, nullptr);
        disconnect(Trip->calibrations(), SIGNAL(tapeCalibrationChanged(double)), this, SLOT(restart()));
        disconnectChunks();
        Trip = nullptr;
    }
}

void cwTripLengthTask::runTask()
{
    if(Trip.isNull()) {
        Length = 0;
        done();
    }

    double distance = 0.0;
    int numberOfShots = 0;

    foreach(cwSurveyChunk* chunk, Trip->chunks()) {
        QPair<double, int> distanceAndNumberShots = distanceOfChunk(chunk);
        distance += distanceAndNumberShots.first;
        numberOfShots += distanceAndNumberShots.second;
    }

    //Calibrate for broken tapes
    double totalTapeError = Trip->calibrations()->tapeCalibration() * (double)numberOfShots;

    Length = distance + totalTapeError;
    lengthChanged();

    done();
}

/**
Gets trip
*/
cwTrip* cwTripLengthTask::trip() const {
    return Trip;
}
