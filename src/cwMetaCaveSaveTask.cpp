/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwMetaCaveSaveTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModel.h"
#include "cwTeam.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwTriangulatedData.h"
#include "cwNoteTranformation.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwProject.h"
#include "cwStationPositionLookup.h"
#include "cwLength.h"
#include "cwImageResolution.h"
#include "cwDebug.h"
#include "cwSQLManager.h"
#include "cwLead.h"

//Qt includes
#include <QFile>
#include <QJsonDocument>

cwMetaCaveSaveTask::cwMetaCaveSaveTask()
{

}

void cwMetaCaveSaveTask::runTask()
{
    QVariantMap regionMap = saveRegion();

    QJsonDocument jsonDocument = QJsonDocument::fromVariant(regionMap);

    QFile file(databaseFilename());
    bool okay = file.open(QFile::WriteOnly);
    if(!okay) {
        qDebug() << "Failed to open " << databaseFilename() << "for writting:" << file.errorString();
        stop();
        done();
        return;
    }

    file.write(jsonDocument.toJson(QJsonDocument::Indented));
    file.close();
    done();
}

/**
 * @brief cwMetaCaveSaveTask::saveRegion
 * @return The fully saved caving region into a variant map
 */
QVariantMap cwMetaCaveSaveTask::saveRegion()
{
    QVariantMap regionMap;

    QVariantList cavesList;
    foreach(const cwCave* cave, Region->caves()) {
        QVariantMap caveAsVariantMap = saveCave(cave);
        cavesList.append(caveAsVariantMap);
    }

    regionMap.insert("caves", cavesList);

    return regionMap;
}

/**
 * @brief cwMetaCaveSaveTask::saveCave
 * @param cave - The cave that will be converted into a variant map
 * @return The variant map of a cave
 */
QVariantMap cwMetaCaveSaveTask::saveCave(const cwCave *cave)
{
    QVariantMap caveAsVariantMap;

    QVariantList trips;
    foreach(const cwTrip* trip, cave->trips()) {
        QVariantMap tripMap = saveTrip(trip);
        trips.append(tripMap);
    }

    caveAsVariantMap.insert("name", cave->name());
    caveAsVariantMap.insert("fixedStations", QVariantList()); //Fixed stations aren't supported yet
    caveAsVariantMap.insert("trips", trips);

    return caveAsVariantMap;
}

/**
 * @brief cwMetaCaveSaveTask::saveTrip
 * @param trip
 * @return
 */
QVariantMap cwMetaCaveSaveTask::saveTrip(const cwTrip *trip)
{
    QVariantMap tripMap;

    tripMap.insert("name", trip->name());
    tripMap.insert("date", trip->date().toString(Qt::ISODate));
    tripMap.insert("surveyors", saveTeam(trip->team()));

    saveCalibration(tripMap, trip->calibrations());

    QVariantList surveyData = saveSurveyData(trip);

    tripMap.insert("survey", surveyData);
    return tripMap;
}

/**
 * @brief cwMetaCaveSaveTask::saveSurveyData
 * @param trip
 * @return
 */
QVariantList cwMetaCaveSaveTask::saveSurveyData(const cwTrip *trip)
{
    QVariantList surveyList;

    QList<cwSurveyChunk*> chunks = trip->chunks();
    for(int i = 0; i < chunks.size(); i++) {
        QVariantList chunkData = saveSurveyChunkData(chunks.at(i));
        surveyList.append(chunkData);

        //If i isn't the last index
        if(i != chunks.size() - 1) {
            //Add null to seperate chunks
            surveyList.append(QVariant());
        }
    }

    return surveyList;
}

/**
 * @brief cwMetaCaveSaveTask::saveTeam
 * @param team
 * @return
 */
QVariantMap cwMetaCaveSaveTask::saveTeam(const cwTeam *team)
{
    QVariantMap teamMap;
    foreach(cwTeamMember teamMember, team->teamMembers()) {
        teamMap.insert(teamMember.name(), saveTeamMember(teamMember));
    }
    return teamMap;
}

/**
 * @brief cwMetaCaveSaveTask::saveTeamMember
 * @param teamMember
 * @return
 */
QVariantMap cwMetaCaveSaveTask::saveTeamMember(const cwTeamMember &teamMember)
{
   QVariantMap teamMemberMap;
   teamMemberMap.insert("roles", teamMember.jobs());
   return teamMemberMap;
}

/**
 * @brief cwMetaCaveSaveTask::saveSurveyChunkData
 * @param chunk
 * @return
 */
QVariantList cwMetaCaveSaveTask::saveSurveyChunkData(const cwSurveyChunk *chunk)
{
    QVariantList stationShotList;

    int totalEntries = chunk->stationCount() + chunk->shotCount();

    int stationIndex = 0;
    int shotIndex = 0;

    for(int i = 0; i < totalEntries; i++) {
        QVariantMap map;
        if(i % 2 == 0) {
            //Station
            cwStation station = chunk->station(stationIndex);
            map = saveStation(station);
            stationIndex++;
        } else {
            //Shot
            cwShot shot = chunk->shot(shotIndex);
            map = saveShot(shot);
            shotIndex++;
        }
        stationShotList.append(map);
    }

    return stationShotList;
}

/**
 * @brief cwMetaCaveSaveTask::saveStation
 * @param station
 * @return
 */
QVariantMap cwMetaCaveSaveTask::saveStation(const cwStation &station)
{
    QVariantMap stationMap;
    stationMap.insert("station", station.name());

    QVariantList lrud;
    lrud.append(station.left());
    lrud.append(station.right());
    lrud.append(station.up());
    lrud.append(station.down());

    stationMap.insert("lrud", lrud);

    return stationMap;
}

/**
 * @brief cwMetaCaveSaveTask::saveShot
 * @param shot
 * @return
 */
QVariantMap cwMetaCaveSaveTask::saveShot(const cwShot &shot)
{
    QVariantMap shotMap;
    shotMap.insert("dist", shot.distance());
    shotMap.insert("excludeDist", !shot.isDistanceIncluded());

    saveCompassReading(shotMap, "fsAzm", shot.compass(), shot.compassState());
    saveCompassReading(shotMap, "bzAzm", shot.backCompass(), shot.backCompassState());

    saveClinoReading(shotMap, "fsInc", shot.clino(), shot.clinoState());
    saveClinoReading(shotMap, "bsInc", shot.backClino(), shot.backClinoState());

    return shotMap;
}

/**
 * @brief cwMetaCaveSaveTask::saveCalibration
 * @param map
 * @param team
 */
void cwMetaCaveSaveTask::saveCalibration(QVariantMap &map, const cwTripCalibration *calibration)
{
    map.insert("distUnit", cwUnits::unitName(calibration->distanceUnit()));
}

/**
 * @brief cwMetaCaveSaveTask::saveClinoReading
 * @param map
 * @param name
 * @param value
 * @param state
 */
void cwMetaCaveSaveTask::saveClinoReading(QVariantMap &map,
                                          QString name,
                                          double value,
                                          cwClinoStates::State state)
{
    switch(state) {
    case cwClinoStates::Down:
        map.insert(name, "down");
        break;
    case cwClinoStates::Up:
        map.insert(name, "up");
        break;
    case cwClinoStates::Valid:
        map.insert(name, value);
        break;
    case cwClinoStates::Empty:
        break;
    }
}

/**
 * @brief cwMetaCaveSaveTask::saveCompassReading
 * @param map
 * @param name
 * @param value
 * @param state
 */
void cwMetaCaveSaveTask::saveCompassReading(QVariantMap &map,
                                            QString name,
                                            double value,
                                            cwCompassStates::State state)
{
    switch(state) {
    case cwCompassStates::Valid:
        map.insert(name, value);
    case cwCompassStates::Empty:
        break;
    }
}


