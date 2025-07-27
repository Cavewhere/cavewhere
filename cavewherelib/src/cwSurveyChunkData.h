#ifndef CWSURVEYCHUNKDATA_H
#define CWSURVEYCHUNKDATA_H

//Qt includes
#include <QUuid>

//Our includes
#include "cwStation.h"
#include "cwShot.h"

struct cwSurveyChunkData {
    QUuid id;
    QList<cwStation> stations;
    QList<cwShot> shots;
};

#endif // CWSURVEYCHUNKDATA_H
