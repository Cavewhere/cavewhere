//Our includes
#include "cwTripLengthTask.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"

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
            disconnectChunks();
        }

        Trip = trip;

        if(Trip != NULL) {
            connect(Trip, SIGNAL(chunksInserted(int,int)), SLOT(chunkAdded(int,int)));
            connect(Trip, SIGNAL(chunksRemoved(int,int)), SLOT(chunkRemoved(int,int)));
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

double cwTripLengthTask::distanceOfChunk(const cwSurveyChunk *chunk) const
{
    double distance = 0.0;
    foreach(cwShot shot, chunk->shots()) {
        distance += shot.distance();
    }
    return distance;
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
    foreach(cwSurveyChunk* chunk, Trip->chunks()) {
        distance += distanceOfChunk(chunk);
    }
    Length = distance;
    lengthChanged();

    done();
}
