/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStationRenamer.h"

cwStationRenamer::cwStationRenamer()
{
}

// these don't allow lowercase like cwStationValidator does,
// because we want to convert lowercase chars in case-sensitive
// external formats to uppercase + '-' (e.g. ld40 -> L-D-40)

const QRegExp cwStationRenamer::validUcaseRx("^[-_A-Z0-9]+$");
const QRegExp cwStationRenamer::invalidCharRx("[^-_A-Z0-9]");

/**
 * @brief cwStationRenamer::createStation
 * @param stationName
 * @return A station with a valid name.
 *
 * If stationName is invalid (see cwStationValidator), this will create a station with a station
 * name generated from uuid. This will most likely be a unique number for the station. The
 * chance of collision is extremely small, look up uuid for more details. Once all the stations
 * have been added to the cave, call renameInvalidStations(). This will try to keep the original
 * invalid name, without conflicting with valid names.  This class will keep track of valid names
 * when creating the station through this function.
 */
cwStation cwStationRenamer::createStation(QString stationName)
{
    if (validUcaseRx.exactMatch(stationName)) {
        ValidStations.insert(stationName);
        return cwStation(stationName);
    } else {
        QString validName = InvalidToValidStations.value(stationName);

        if(validName.isEmpty()) {
            // replace lowercase chars with uppercase + '-'
            // e.g. ld40 -> L-D-40
            validName.reserve(stationName.size());
            for (auto i = stationName.cbegin(); i < stationName.cend(); i++) {
                QChar c = *i;
                validName += c.toUpper();
                if (c.isLower()) {
                    validName += '-';
                }
            }

            // replace invalid chars with "_"
            // e.g. FR:WB$ -> FR_WB_
            validName.replace(invalidCharRx, "_");

            // if transformed name already exists try adding _1, _2, etc.
            // until that doesn't exist
            if (ValidStations.contains(validName)) {
                int num = 1;
                QString numberedName;
                do {
                    numberedName = QString("%1_%2").arg(validName).arg(num);
                } while (ValidStations.contains(numberedName));
                validName = numberedName;
            }

            Q_ASSERT(cwStation::nameIsValid(validName));

            ValidStations << validName;
            InvalidToValidStations.insert(stationName, validName);
            ValidToInvalidStations.insert(validName, stationName);
        }

        return cwStation(validName);
    }
}

/**
 * @brief cwStationRenamer::clear
 *
 * This clears all the history for this station renamer. All created stations will be forgotten.
 */
void cwStationRenamer::clear()
{
    InvalidToValidStations.clear();
    ValidToInvalidStations.clear();
    ValidStations.clear();
}
