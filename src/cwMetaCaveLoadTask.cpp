/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwMetaCaveLoadTask.h"
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
#include <QVariant>
#include <QVariantMap>
#include <QMapIterator>

cwMetaCaveLoadTask::cwMetaCaveLoadTask()
{

}

void cwMetaCaveLoadTask::runTask()
{
    QFile file(databaseFilename());
    bool okay = file.open(QFile::ReadOnly);
    if(!okay) {
        qDebug() << "Failed to open " << databaseFilename() << "for reading:" << file.errorString();
        stop();
        done();
        return;
    }

    QByteArray fileData = file.readAll();

    QJsonParseError parserError;
    QJsonDocument document = QJsonDocument::fromJson(fileData, &parserError);

    if(parserError.error != QJsonParseError::NoError) {
        //Parsed json correctly
        QVariant variant = document.toVariant();
        QVariantMap map = variant.toMap();

        try {
            loadCavingRegion(map);
        } catch(QString message) {
            qDebug() << "JSON loading error:" << message;
        }

    } else {
        //There was a parsing error
        qDebug() << "JSON Syntax error loading file " << databaseFilename() << parserError.errorString();
    }
}

/**
 * @brief cwMetaCaveLoadTask::loadCavingRegion
 * @param map
 */
void cwMetaCaveLoadTask::loadCavingRegion(const QVariantMap &map)
{
    hasProperty(map, "caves", "root");
    QVariantMap cavesMap = map.value("caves").toMap();

    for(auto caveIter = cavesMap.begin(); caveIter != cavesMap.end(); caveIter++) {
        QString caveName = caveIter.key();

        cwCave* cave = new cwCave();
        cave->setName(caveName);

        QVariantMap caveJsonMap = caveIter.value().toMap();
        loadCave(caveJsonMap, cave);

        Region->addCave(cave);
    }
}

/**
 * @brief cwMetaCaveLoadTask::loadCave
 * @param map
 * @param cave
 */
void cwMetaCaveLoadTask::loadCave(const QVariantMap &map, cwCave *cave)
{
    QVariantList tripsList = map.value("trips").toList();
    for(int i = 0; i < tripsList.size(); i++) {
        QVariantMap tripMap = tripsList.at(i).toMap();
        cwTrip* trip = new cwTrip();

        loadTrip(tripMap, trip);

        cave->addTrip(trip);
    }
}

/**
 * @brief cwMetaCaveLoadTask::loadTrip
 * @param map
 * @param trip
 */
void cwMetaCaveLoadTask::loadTrip(const QVariantMap &map, cwTrip *trip)
{
    trip->setName(map.value("name").toString());
    trip->setDate(map.value("date").toDate());

    QVariantMap surveyors = map.value("surveyors").toMap();
    loadTeam(surveyors, trip->team());

    loadTripCalibration(map, trip->calibrations());



}

/**
 * @brief cwMetaCaveLoadTask::loadTripCalibration
 * @param map
 * @param tripCalibration
 */
void cwMetaCaveLoadTask::loadTripCalibration(const QVariantMap &map, cwTripCalibration *tripCalibration)
{
    Q_UNUSED(tripCalibration);

    hasProperty(map, "distUnit", "trip");
    hasProperty(map, "angleUnit", "trip");
    hasProperty(map, "backsightsCorrected", "trip");



}

/**
 * @brief cwMetaCaveLoadTask::loadTeam
 * @param map
 * @param team
 */
void cwMetaCaveLoadTask::loadTeam(const QVariantMap &map, cwTeam *team)
{
    Q_UNUSED(map);
    Q_UNUSED(team);
}

/**
 * @brief cwMetaCaveLoadTask::hasProperty
 * @param map
 * @param property
 *
 * Checks to see if the map has property, if it doesn't this throws an exception
 */
void cwMetaCaveLoadTask::hasProperty(const QVariantMap &map, QString property, QString structureLocation) const
{
    if(!map.contains(property)) {
        throw QString("Can't find property \"%1\" in %2 of JSON file").arg(property).arg(structureLocation);
    }
}

