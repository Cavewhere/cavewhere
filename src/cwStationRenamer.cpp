/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStationRenamer.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStationValidator.h"

//Qt includes
#include <QUuid>

cwStationRenamer::cwStationRenamer()
{
}

/**
 * @brief cwStationRenamer::setCave
 * @param cave
 *
 * Sets the cave where the invalid station will live.
 */
void cwStationRenamer::setCave(cwCave *cave)
{
    if(Cave != cave) {
        Cave = cave;
        clear();
    }
}

/**
 * @brief cwStationRenamer::cave
 * @return Returns the cave
 */
cwCave *cwStationRenamer::cave() const
{
    return Cave;
}

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
 *
 * If the cave is null, this will assert.
 */
cwStation cwStationRenamer::createStation(QString stationName)
{
    Q_ASSERT(!Cave.isNull());

    if(cwStation::nameIsValid(stationName)) {
        ValidStations.insert(stationName);
        return cwStation(stationName);
    } else {
        QString validName = InvalidToValidStations.value(stationName);

        if(validName.isEmpty()) {
            QUuid uuid = QUuid::createUuid();
            QString uuidString = uuid.toString();
            QStringRef removeBracets = QStringRef(&uuidString, 1, uuidString.size() - 2);
            validName = removeBracets.toString();
            Q_ASSERT(cwStation::nameIsValid(validName));

            InvalidToValidStations.insert(stationName, validName);
            ValidToInvalidStations.insert(validName, stationName);
        }

        return cwStation(validName);
    }
}

/**
 * @brief cwStationRenamer::renameInvalidStations
 *
 * This will go through the cave (see setCave) and rename invalid station names with valid ones.
 *
 * Invalid characters will be remove from the invalid stations. The new station will be checked
 * agianst the validStations list. If it exist in this list, '_' will be appended to the name
 * until it's unique.
 *
 * If the cave is null, this will assert.
 */
void cwStationRenamer::renameInvalidStations()
{
    Q_ASSERT(!Cave.isNull());

    QRegExp removeInvalidCharsRegex = cwStationValidator::invalidCharactersRegex();

    foreach(cwTrip* trip, Cave->trips()) {
        foreach(cwSurveyChunk* chunk, trip->chunks()) {
            for(int i = 0; i < chunk->stations().size(); i++) {
                renameInvalidStationsInChunk(chunk, i, removeInvalidCharsRegex);
            }
        }
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

/**
 * @brief cwStationRenamer::renameInvalidStationsInChunk
 * @param chunk
 * @param stationIndex
 *
 * This is a helper function to renameInvalidStations()
 */
void cwStationRenamer::renameInvalidStationsInChunk(cwSurveyChunk *chunk,
                                                    int stationIndex,
                                                    const QRegExp& removeInvalidCharsRegex)
{
    cwStation station = chunk->stations().at(stationIndex);
    if(ValidToInvalidStations.contains(station.name())) {
        QString invalidName = ValidToInvalidStations.value(station.name());
        QString validName;
        if(cwStation::nameIsValid(invalidName)) {
            //This name has been update with a valid name, so just use as the valid name
            validName = invalidName;
        } else {
            //Remove all invalid characters
            invalidName.remove(removeInvalidCharsRegex);

            //Append '_' until the invalidName is valid
            while(ValidStations.contains(invalidName)) {
                invalidName.append("_");
                Q_ASSERT(cwStation::nameIsValid(invalidName)); //Make sure we're generating a valid name
            }

            //ValidStation doesn't contain invalidName;
            validName = invalidName;
            ValidStations.insert(validName);
            ValidToInvalidStations.insert(station.name(), validName);
        }

        //Update the station
        Q_ASSERT(cwStation::nameIsValid(validName));
        station.setName(validName);
        chunk->setStation(station, stationIndex);
    }
}
