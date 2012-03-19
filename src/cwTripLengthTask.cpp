//Our includes
#include "cwTripLengthTask.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwTripCalibration.h"

cwTripLengthTask::cwTripLengthTask(QObject *parent) :
    cwTask(parent),
    Trip(NULL),
    Length(0.0)
{
}

/**
  Sets the trip for the lenght task
  */
void cwTripLengthTask::setTrip(cwTrip *trip) {

    if(Trip != trip) {

        if(Trip != NULL) {
            disconnect(Trip, NULL, this, NULL);
            disconnect(Trip->calibrations(), SIGNAL(tapeCalibrationChanged(double)), this, SLOT(restart()));
            disconnectChunks();
        }

        Trip = trip;

        if(Trip != NULL) {
            connect(Trip, SIGNAL(chunksInserted(int,int)), SLOT(chunkAdded(int,int)));
            connect(Trip, SIGNAL(chunksRemoved(int,int)), SLOT(chunkRemoved(int,int)));
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
    foreach(cwSurveyChunk* chunk, Trip->chunks()) {
        disconnectChunk(chunk);
    }
}

void cwTripLengthTask::disconnectChunk(cwSurveyChunk *chunk)
{
    disconnect(chunk, NULL, this, NULL);
}

/**
  Returns the distance of the chunk and the number of shots in the chunk
  */
QPair<double, int> cwTripLengthTask::distanceOfChunk(const cwSurveyChunk *chunk) const
{
    double distance = 0.0;
    int numberOfShots = 0;
    foreach(cwShot shot, chunk->shots()) {
        if(shot.distanceState() == cwDistanceStates::Valid) {
            distance += shot.distance();
            numberOfShots++;
        }
    }
    return QPair<double, int>(distance, numberOfShots);
}

/**

  */
void cwTripLengthTask::chunkRemoved(int begin, int end)
{
    for(int i = begin; i <= end; i++) {
        cwSurveyChunk* chunk = Trip->chunk(i);
        disconnectChunk(chunk);
    }
}

void cwTripLengthTask::chunkAdded(int begin, int end)
{
    for(int i = begin; i <= end; i++) {
        cwSurveyChunk* chunk = Trip->chunk(i);
        connectChunk(chunk);
    }
}

void cwTripLengthTask::runTask()
{
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
