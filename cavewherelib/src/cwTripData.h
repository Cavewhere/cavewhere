#ifndef CWTRIPDATA_H
#define CWTRIPDATA_H

#include <QString>
#include <QStringList>
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

    //! Snapshot of cwTrip::externalStations() so a worker pass (the line-plot
    //! solve, the floating-survey search) sees an attachment's station names
    //! without touching the live trip. Derived scan output, so it is filled by
    //! cwTrip::data() and never persisted — see cwTrip::setData for why nothing
    //! reads it back.
    QStringList externalStations;
};


#endif // CWTRIPDATA_H
