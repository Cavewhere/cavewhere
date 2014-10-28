/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSTATIONRENAMER_H
#define CWSTATIONRENAMER_H

//Our includes
class cwCave;
class cwSurveyChunk;
#include "cwStation.h"

//Qt includes
#include <QString>
#include <QPointer>

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

    void setCave(cwCave* cave);
    cwCave* cave() const;

    cwStation createStation(QString stationName);

    void renameInvalidStations();

    void clear();

private:
    QHash<QString, QString> InvalidToValidStations;
    QHash<QString, QString> ValidToInvalidStations;
    QSet<QString> ValidStations;

    QPointer<cwCave> Cave;

    void renameInvalidStationsInChunk(cwSurveyChunk* chunk,
                                      int stationIndex,
                                      const QRegExp& removeInvalidCharsRegex);
};

#endif // CWSTATIONRENAMER_H
