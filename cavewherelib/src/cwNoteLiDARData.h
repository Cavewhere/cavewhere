#ifndef CWNOTELIDARDATA_H
#define CWNOTELIDARDATA_H

//Our includes
#include "cwNoteLiDARStation.h"

//Qt includes
#include <QString>

struct cwNoteLiDARData {
    QString name;
    QString filename;
    QList<cwNoteLiDARStation> stations;
};

#endif // CWNOTELIDARDATA_H
