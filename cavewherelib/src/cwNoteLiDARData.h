#ifndef CWNOTELIDARDATA_H
#define CWNOTELIDARDATA_H

//Our includes
#include "cwNoteLiDARStation.h"
#include "cwNoteLiDARTransformationData.h"

//Qt includes
#include <QString>

struct cwNoteLiDARData {
    QString name;
    QString filename;
    QList<cwNoteLiDARStation> stations;
    cwNoteLiDARTransformationData transfrom;
    bool autoCalculateNorth = true;
    QString iconImagePath;
};

#endif // CWNOTELIDARDATA_H
