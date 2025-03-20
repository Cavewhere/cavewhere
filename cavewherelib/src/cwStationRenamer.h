/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSTATIONRENAMER_H
#define CWSTATIONRENAMER_H

//Our includes
#include "cwStation.h"

//Qt includes
#include <QString>
#include <QHash>
#include <QSet>

/**
 * @brief The cwStationRenamer class
 *
 * This class will rename invalid stations in a cave. This is useful for renaming bad stations
 * that come from an external source like compass or survex.
 */
class cwStationRenamer
{
public:
    cwStationRenamer();

    cwStation createStation(QString stationName);

    void clear();

private:
    QHash<QString, QString> OriginalToRenamedStations;
    QHash<QString, QString> RenamedToOriginalStations;
    QSet<QString> UpperCaseRenamedStations;

    const QRegularExpression invalidCharRegExp() { return QRegularExpression("[^-_a-zA-Z0-9]"); }
};

#endif // CWSTATIONRENAMER_H
