#ifndef CWNOTELIDARDATA_H
#define CWNOTELIDARDATA_H

//Our includes
#include "cwNoteLiDARStation.h"
#include "cwNoteLiDARTransformationData.h"

//Qt includes
#include <QString>
#include <QUuid>

struct cwNoteLiDARData {
    QString name;
    QString filename;
    QList<cwNoteLiDARStation> stations;
    cwNoteLiDARTransformationData transfrom;
    bool autoCalculateNorth = true;
    QUuid id;
};

#endif // CWNOTELIDARDATA_H
