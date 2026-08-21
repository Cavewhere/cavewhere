/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef SPLAYFIXTUREHELPER_H
#define SPLAYFIXTUREHELPER_H

//Our includes
#include "cwShotMeasurement.h"

//Qt includes
#include <QList>
#include <QString>

inline cwShotMeasurement makeSplay(const QString& distance,
                                   const QString& compass,
                                   const QString& clino)
{
    return cwShotMeasurement(cwDistanceReading(distance),
                             cwCompassReading(compass),
                             cwClinoReading(clino));
}

//The first three splays off a4 in the TopoDroid export used as ground truth
//(~/Desktop/svx/a0-a34.svx). testcases/datasets/survex/splays.svx carries the
//same readings, so the importer and the data model are checked against one
//copy of the numbers.
inline QList<cwShotMeasurement> a4Splays()
{
    return {
        makeSplay("5.88", "124.1", "4.6"),
        makeSplay("5.42", "118.8", "2.9"),
        makeSplay("8.96", "150.9", "17.5")
    };
}

#endif //SPLAYFIXTUREHELPER_H
