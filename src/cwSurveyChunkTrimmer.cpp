#include "cwSurveyChunkTrimmer.h"

cwSurveyChunkTrimmer::cwSurveyChunkTrimmer(QObject *parent) :
    QObject(parent),
    Chunk(NULL)
{
}

void cwSurveyChunkTrimmer::setChunk(cwSurveyChunk *chunk) {
    if(Chunk == chunk) { return; }

    if(Chunk != NULL) {
        trim(FullTrim);
        disconnect(Chunk, NULL, this, NULL);
    }

    Chunk = chunk;

    if(Chunk != NULL) {
        connect(Chunk, SIGNAL(stationsAdded(int,int)), SLOT(addLastEmptyStation()));
        connect(Chunk, SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)), SLOT(addLastEmptyStation()));

        addLastEmptyStation();
    }

    emit chunkChanged();
}

/**
  This removes the last empty stations and shots from the chunk

  chunk - The that's will be trimmed
  fullTrim - If true, this will fully trim, else it'll leave just the last
  empty shot and station
  */
void cwSurveyChunkTrimmer::trim(TrimType fullTrim) {
    if(Chunk->stationCount() <= 2) {
        return;
    }

    switch(fullTrim) {
    case FullTrim: {
        for(int i = Chunk->stationCount() - 1; i > 1; i--) {
            if(isStationShotEmpty(Chunk, i)) {
                Chunk->removeStation(i, cwSurveyChunk::Above);
            }
        }
        break;
    }
    case PreserveLastEmptyOne: {
        for(int i = Chunk->stationCount() - 1; i > 1; i--) {
            //Look a head one
            if(isStationShotEmpty(Chunk, i - 1)) {
                //Remove the current
                if(isStationShotEmpty(Chunk, i)) {
                    Chunk->removeStation(i, cwSurveyChunk::Above);
                }
            }
        }
        break;
    }
    }
}

/**
  This addes an empty station and shot to the end of the surveyChunk
  */

void cwSurveyChunkTrimmer::addLastEmptyStation() {
    trim(PreserveLastEmptyOne);

    //If last station isn't empty, add new one
    if(!isStationShotEmpty(Chunk, Chunk->stationCount() - 1)) {
        connect(Chunk, SIGNAL(stationsAdded(int,int)), this, SLOT(addLastEmptyStation()));
        Chunk->appendNewShot();
        connect(Chunk, SIGNAL(stationsAdded(int,int)), SLOT(addLastEmptyStation()));
    }
}

/**
    Checks to see if the last station and shot is empty

    This function is invalid if the stationIndex is equal to zero, always returns false

    chunk - The chunk that's checked
    stationIndex - The station that's chuck.
  */
bool cwSurveyChunkTrimmer::isStationShotEmpty(cwSurveyChunk *chunk, int stationIndex) const {
    if(stationIndex == 0) { return false; }

    cwStation station = chunk->station(stationIndex);
    cwShot shot = chunk->shot(stationIndex - 1);

    return station.name().isEmpty() &&
            station.leftInputState() == cwDistanceStates::Empty &&
            station.rightInputState() == cwDistanceStates::Empty &&
            station.downInputState() == cwDistanceStates::Empty &&
            station.upInputState() == cwDistanceStates::Empty &&
            shot.distanceState() == cwDistanceStates::Empty &&
            shot.compassState() == cwCompassStates::Empty &&
            shot.clinoState() == cwClinoStates::Empty &&
            shot.backCompassState() == cwCompassStates::Empty &&
            shot.backClinoState() == cwClinoStates::Empty;
}
