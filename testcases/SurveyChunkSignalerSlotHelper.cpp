/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "SurveyChunkSignalerSlotHelper.h"

//Qt includes
#include <QDebug>

SurveyChunkSignalerSlotHelper::SurveyChunkSignalerSlotHelper(QObject *parent) : QObject(parent)
{
    CaveNameChanged = nullptr;
    TripNameChanged = nullptr;
    ChunkSender = nullptr;
    StationAddedIndexes = QPair<int, int>(0, 0);
}

void SurveyChunkSignalerSlotHelper::caveNameChangedCalled()
{
    CaveNameChanged = sender();
}

void SurveyChunkSignalerSlotHelper::tripNameChangedCalled()
{
    TripNameChanged = sender();
}

void SurveyChunkSignalerSlotHelper::calibrationChangedCalled()
{
    CalibrationSender = sender();
}

void SurveyChunkSignalerSlotHelper::chunkStationAdded(int begin, int end)
{
    ChunkSender = sender();
    StationAddedIndexes = QPair<int, int>(begin, end);
}

