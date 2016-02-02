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
#include <QThread>

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

    if(parserError.error == QJsonParseError::NoError) {
        //Parsed json correctly
        QVariant variant = document.toVariant();
        QVariantMap map = variant.toMap();

        try {
            moveObjectToThread(Region, QThread::currentThread());
            loadCavingRegion(map);
            moveObjectToThread(Region, thread(), this);
        } catch(QString message) {
            qDebug() << "JSON loading error:" << message;
        }

    } else {
        //There was a parsing error
        qDebug() << "JSON Syntax error loading file " << databaseFilename() << parserError.errorString();
    }

    done();
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

    QVariantList survey = map.value("survey").toList();
    loadSurvey(survey, trip);
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
    hasProperty(map, "azmBacksightsCorrected", "trip");
    hasProperty(map, "incBacksightsCorrected", "trip");

    cwUnits::LengthUnit distanceUnit = cwUnits::toLengthUnit(map.value("distUnit").toString());
    tripCalibration->setDistanceUnit(distanceUnit);

    cwUnits::AngleUnit angleUnit = cwUnits::toAngleUnit(map.value("angleUnit").toString());
    tripCalibration->setAngleUnit(angleUnit);
    tripCalibration->setFrontCompassUnit(angleUnit);
    tripCalibration->setFrontClinoUnit(angleUnit);
    tripCalibration->setBackCompassUnit(angleUnit);
    tripCalibration->setBackClinoUnit(angleUnit);

    bool azmBacksightCorrected = map.value("backsightsCorrected").toBool();
    bool incBacksightCorrected = map.value("incBacksightsCorrected").toBool();
    tripCalibration->setCorrectedCompassBacksight(azmBacksightCorrected);
    tripCalibration->setCorrectedClinoBacksight(incBacksightCorrected);

    loadOptionalData(map, "declination", tripCalibration, "declination", 0.0);
    loadOptionalData(map, "azmFsUnit", tripCalibration, "frontCompassUnit", cwUnits::Degrees);
    loadOptionalData(map, "azmBsUnit", tripCalibration, "backCompassUnit", cwUnits::Degrees);
    loadOptionalData(map, "incFsUnit", tripCalibration, "frontClinoUnit", cwUnits::Degrees);
    loadOptionalData(map, "incBsUnit", tripCalibration, "backClinoUnit", cwUnits::Degrees);
    loadOptionalData(map, "distCorrection", tripCalibration, "tapeCalibration", 0.0);
    loadOptionalData(map, "azmFsCorrection", tripCalibration, "frontCompassCalibration", 0.0);
    loadOptionalData(map, "azmBsCorrection", tripCalibration, "backCompassCalibration", 0.0);
    loadOptionalData(map, "incFsCorrection", tripCalibration, "frontClinoCalibration", 0.0);
    loadOptionalData(map, "incBsCorrection", tripCalibration, "backClinoCalibration", 0.0);
}

/**
 * @brief cwMetaCaveLoadTask::loadSurvey
 * @param surveyList - The list of stations and shots from the json
 * @param trip - The trip where the stations and shots will be added to
 *
 * This will load shot and station data from the surveyList into the trip.
 */
void cwMetaCaveLoadTask::loadSurvey(const QVariantList &surveyList, cwTrip *trip)
{
    Q_UNUSED(trip);

    for(int i = 0; i < surveyList.size(); i++) {
        const QVariantMap& stationShot = surveyList.at(i).toMap();
        Q_UNUSED(stationShot)

        switch(i % 2) {
        case 0:

//            createStation(surveyList.at(i));
            break;
        case 1:

            break;
        }
    }






}

/**
 * @brief cwMetaCaveLoadTask::loadTeam
 * @param map
 * @param team
 */
void cwMetaCaveLoadTask::loadTeam(const QVariantMap &map, cwTeam *team)
{
    for(auto iter = map.begin(); iter != map.end(); iter++) {
        QString memberName = iter.key();
        QStringList roles;

        QVariantMap memberMap = iter.value().toMap();

        QVariant rolesVariant = memberMap.value("roles");
        if(rolesVariant.canConvert<QVariantList>()) {
            QVariantList roleList = rolesVariant.toList();
            foreach(QVariant role, roleList) {
                roles.append(role.toString());
            }
        } else {
            roles.append(rolesVariant.toString());
        }

        cwTeamMember teamMember;
        teamMember.setName(memberName);
        teamMember.setJobs(roles);
        team->addTeamMember(teamMember);
    }
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

/**
 * @brief cwMetaCaveLoadTask::loadOptionalData
 * @param map
 * @param property
 * @param object
 * @param objectProperty
 * @param defaultValue
 */
void cwMetaCaveLoadTask::loadOptionalData(const QVariantMap &map,
                                          QString property,
                                          QObject *object,
                                          QByteArray objectProperty,
                                          QVariant defaultValue) const
{
    if(map.contains(property)) {
        QVariant value = map.value(property);
        object->setProperty(objectProperty.constData(), value);
    } else if(defaultValue != QVariant()) {
        object->setProperty(objectProperty.constData(), defaultValue);
    }
}
