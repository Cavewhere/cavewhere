#ifndef CWSURVEYCHUNKDATA_H
#define CWSURVEYCHUNKDATA_H

//Qt includes
#include <QUuid>

//Our includes
#include "cwStation.h"
#include "cwShot.h"
#include "cwTripCalibration.h"

struct cwSurveyChunkData {
    QUuid id;
    QList<cwStation> stations;
    QList<cwShot> shots;

    //Pontentially remove
    QMap<int, cwTripCalibrationData> calibrations;
};

#endif // CWSURVEYCHUNKDATA_H
