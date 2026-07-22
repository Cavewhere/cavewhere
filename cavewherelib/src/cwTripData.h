#ifndef CWTRIPDATA_H
#define CWTRIPDATA_H

#include <QString>
#include <QDateTime>
#include <QUuid>

//Our includes
#include "cwExternalCenterline.h"
#include "cwTeamData.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunkData.h"
#include "cwSurveyNoteModelData.h"
#include "cwSurveyNoteLiDARModelData.h"
#include "cwSurveyNoteSketchModelData.h"

struct cwTripData {
    QString name;
    QDateTime date;
    cwTeamData team;
    cwTripCalibrationData calibrations;
    QList<cwSurveyChunkData> chunks;
    cwSurveyNoteModelData noteModel;
    cwSurveyNoteLiDARModelData noteLiDARModel;
    cwSurveyNoteSketchModelData sketchModel;
    QUuid id;
    cwExternalCenterline externalCenterline;
    QString stationPrefix;
};


#endif // CWTRIPDATA_H
