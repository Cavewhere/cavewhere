#ifndef CWTRIPDATA_H
#define CWTRIPDATA_H

#include <QString>
#include <QDateTime>

//Our includes
#include "cwTeamData.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunkData.h"
#include "cwSurveyNoteModelData.h"
#include "cwSurveyNoteLiDARModelData.h"

struct cwTripData {
    QString name;
    QDateTime date;
    cwTeamData team;
    cwTripCalibrationData calibrations;
    QList<cwSurveyChunkData> chunks;
    cwSurveyNoteModelData noteModel;
    cwSurveyNoteLiDARModelData noteLiDARModel;

};


#endif // CWTRIPDATA_H
