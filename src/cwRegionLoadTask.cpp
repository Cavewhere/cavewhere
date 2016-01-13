/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRegionLoadTask.h"
#include "cwImageCleanupTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwUnits.h"
#include "cwLength.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwNote.h"
#include "cwTripCalibration.h"
#include "cwTeam.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwImageResolution.h"
#include "cwSQLManager.h"
#include "cwDebug.h"

////Serielization includes
//#include "cwSerialization.h"
//#include "cwQtSerialization.h"

//Boost includes
//#include <boost/archive/xml_archive_exception.hpp>

//Qt includes
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

//Std includes
#include <sstream>

cwRegionLoadTask::cwRegionLoadTask(QObject *parent) :
    cwRegionIOTask(parent)
{

}

/**
  Loads the region data
  */
void cwRegionLoadTask::runTask() {
    //Clear region
    bool connected = connectToDatabase("loadRegionTask");
    if(connected) {

        //This makes sure that sqlite is clean up after it self
        insureVacuuming();

        //Try loading Proto Buffer
        bool success = loadFromProtoBuffer();

//        if(!success) {
//            qDebug() << "Warning loading from XML serialization!" << LOCATION;
//            success = loadFromBoostSerialization();
//        }

        if(!success) {
            qDebug() << "Couldn't load from any format!";
            stop();
        }
    }

    if(isRunning()) {
        emit finishedLoading();
    }

    done();
}

/**
 * @brief cwRegionLoadTask::loadFromProtoBuffer
 * @return
 */
bool cwRegionLoadTask::loadFromProtoBuffer()
{
    bool okay;
    QByteArray protoBufferData = readProtoBufferFromDatabase(&okay);

    if(!okay) {
        return false;
    }

    CavewhereProto::CavingRegion region;
    bool couldParse = region.ParseFromArray(protoBufferData.data(), protoBufferData.size());

    if(!couldParse) {
        qDebug() << "Couldn't read proto buffer. Corrupted?!";
        //Don't close the Database here because, the xml loader needs it
        return false;
    }

    loadCavingRegion(region);

    //Clean up old images
    cwImageCleanupTask imageCleanupTask;
    imageCleanupTask.setDatabaseFilename(databaseFilename());
    imageCleanupTask.setRegion(Region);
    imageCleanupTask.start();

    Database.close();
    return true;
}

/**
 * @brief cwRegionLoadTask::readProtoBufferFromDatabase
 * @return This reads the proto buffer from the database
 */
QByteArray cwRegionLoadTask::readProtoBufferFromDatabase(bool* okay)
{
    cwSQLManager::Transaction transaction(&Database, cwSQLManager::ReadOnly);

    QSqlQuery selectObjectData(Database);
    QString queryStr =
            QString("SELECT protoBuffer FROM ObjectData where id = 1");

    bool couldPrepare = selectObjectData.prepare(queryStr);
    if(!couldPrepare) {
        qDebug() << "Couldn't prepare select Caving Region:" << selectObjectData.lastError().databaseText() << queryStr;
    }

    //Extract the data
    selectObjectData.exec();
    QSqlRecord record = selectObjectData.record();

    if(record.isEmpty()) {
        qDebug() << "Hmmmm, no caving regions to load from protoBuffer";
        *okay = false;
        return QByteArray();
    }

    //Get the first row
    selectObjectData.next();

    *okay = true;
    QByteArray data = selectObjectData.value(0).toByteArray();
    return data;
}

/**
 * @brief cwRegionLoadTask::loadCavingRegion
 * @param region
 */
void cwRegionLoadTask::loadCavingRegion(const CavewhereProto::CavingRegion &region)
{

    Region->clearCaves();

    QList<cwCave*> caves;
    caves.reserve(region.caves_size());

    for(int i = 0; i < region.caves_size(); i++) {
        const CavewhereProto::Cave& protoCave = region.caves(i);
        cwCave* cave = new cwCave();
        loadCave(protoCave, cave);

        caves.append(cave);
    }

    Region->addCaves(caves);
}

/**
 * @brief cwRegionLoadTask::loadCave
 * @param protoCave
 * @param cave
 */
void cwRegionLoadTask::loadCave(const CavewhereProto::Cave& protoCave, cwCave *cave)
{
    QString name = loadString(protoCave.name());
    cwUnits::LengthUnit lengthUnit = (cwUnits::LengthUnit)protoCave.lengthunit();
    cwUnits::LengthUnit depthUnit = (cwUnits::LengthUnit)protoCave.depthunit();

    QList<cwTrip*> trips;
    trips.reserve(protoCave.trips_size());

    cave->setName(name);
    cave->length()->setUnit(lengthUnit);
    cave->depth()->setUnit(depthUnit);

    for(int i = 0; i < protoCave.trips_size(); i++) {
        cwTrip* trip = new cwTrip();
        loadTrip(protoCave.trips(i), trip);
        cave->addTrip(trip);
    }

    cwStationPositionLookup stationLookup = loadStationPositionLookup(protoCave.stationpositionlookup());
    cave->setStationPositionLookup(stationLookup);

    if(protoCave.has_stationpositionlookup()) {
        cave->setStationPositionLookupStale(protoCave.stationpositionlookupstale());
    }
}

/**
 * @brief cwRegionLoadTask::loadTrip
 * @param protoTrip
 * @param trip
 */
void cwRegionLoadTask::loadTrip(const CavewhereProto::Trip& protoTrip, cwTrip *trip)
{
    QString tripName = loadString(protoTrip.name());
    QDate tripDate = loadDate(protoTrip.date());

    trip->setName(tripName);
    trip->setDate(tripDate);

    loadSurveyNoteModel(protoTrip.notemodel(), trip->notes());
    loadTripCalibration(protoTrip.tripcalibration(), trip->calibrations());
    loadTeam(protoTrip.team(), trip->team());

    QList<cwSurveyChunk*> chunks;
    chunks.reserve(protoTrip.chunks_size());
    for(int i = 0; i < protoTrip.chunks_size(); i++) {
        cwSurveyChunk* chunk = new cwSurveyChunk();
        loadSurveyChunk(protoTrip.chunks(i), chunk);
        chunks.append(chunk);
    }

    trip->setChucks(chunks);
}

/**
 * @brief cwRegionLoadTask::loadSurveyNoteModel
 * @param protoNoteModel
 * @param noteModel
 */
void cwRegionLoadTask::loadSurveyNoteModel(const CavewhereProto::SurveyNoteModel& protoNoteModel, cwSurveyNoteModel *noteModel)
{
    QList<cwNote*> notes;
    notes.reserve(protoNoteModel.notes_size());
    for(int i = 0; i < protoNoteModel.notes_size(); i++) {
        cwNote* note = new cwNote();
        loadNote(protoNoteModel.notes(i), note);
        notes.append(note);
    }

    noteModel->addNotes(notes);
}

/**
 * @brief cwRegionLoadTask::loadTripCalibration
 * @param protoTripCalibration
 * @param tripCalibration
 */
void cwRegionLoadTask::loadTripCalibration(const CavewhereProto::TripCalibration& protoTripCalibration, cwTripCalibration *tripCalibration)
{
    tripCalibration->setCorrectedCompassBacksight(protoTripCalibration.correctedcompassbacksight());
    tripCalibration->setCorrectedClinoBacksight(protoTripCalibration.correctedclinobacksight());
    tripCalibration->setCorrectedCompassFrontsight(protoTripCalibration.correctedcompassfrontsight());
    tripCalibration->setCorrectedClinoFrontsight(protoTripCalibration.correctedclinofrontsight());
    tripCalibration->setTapeCalibration(protoTripCalibration.tapecalibration());
    tripCalibration->setFrontCompassCalibration(protoTripCalibration.frontcompasscalibration());
    tripCalibration->setFrontClinoCalibration(protoTripCalibration.frontclinocalibration());
    tripCalibration->setBackCompassCalibration(protoTripCalibration.backcompassscalibration());
    tripCalibration->setBackClinoCalibration(protoTripCalibration.backclinocalibration());
    tripCalibration->setDeclination(protoTripCalibration.declination());
    tripCalibration->setDistanceUnit((cwUnits::LengthUnit)protoTripCalibration.distanceunit());
    tripCalibration->setFrontSights(protoTripCalibration.frontsights());
    tripCalibration->setBackSights(protoTripCalibration.backsights());
}

/**
 * @brief cwRegionLoadTask::loadSurveyChunk
 * @param protoChunk
 * @param chunk
 */
void cwRegionLoadTask::loadSurveyChunk(const CavewhereProto::SurveyChunk& protoChunk, cwSurveyChunk *chunk)
{
    QList<cwStation> stations;
    stations.reserve(protoChunk.stations_size());
    for(int i = 0 ; i < protoChunk.stations_size(); i++) {
        cwStation station = loadStation(protoChunk.stations(i));
        stations.append(station);
    }

    QList<cwShot> shots;
    shots.reserve(protoChunk.shots_size());
    for(int i = 0; i < protoChunk.shots_size(); i++) {
        cwShot shot = loadShot(protoChunk.shots(i));
        shots.append(shot);
    }

    if(stations.count() - 1 != shots.count()) {
        qDebug() << "Shot, station count mismatch, survey chunk invalid:" << stations.count() << shots.count();
        return;
    }

    for(int i = 0; i < stations.count() - 1; i++) {
        cwStation fromStation = stations[i];
        cwStation toStation = stations[i + 1];
        cwShot shot = shots[i];

        chunk->appendShot(fromStation, toStation, shot);
    }
}

/**
 * @brief cwRegionLoadTask::loadTeam
 * @param protoTeam
 * @param team
 */
void cwRegionLoadTask::loadTeam(const CavewhereProto::Team& protoTeam, cwTeam *team)
{
    QList<cwTeamMember> members;
    members.reserve(protoTeam.teammembers_size());
    for(int i = 0; i < protoTeam.teammembers_size(); i++) {
        cwTeamMember member = loadTeamMember(protoTeam.teammembers(i));
        members.append(member);
    }
    team->setTeamMembers(members);
}

/**
 * @brief cwRegionLoadTask::loadNote
 * @param protoNote
 * @param note
 */
void cwRegionLoadTask::loadNote(const CavewhereProto::Note& protoNote, cwNote *note)
{
    cwImage image = loadImage(protoNote.image());
    double rotation = protoNote.rotation();

    note->setImage(image);
    note->setRotate(rotation);

    loadImageResolution(protoNote.imageresolution(), note->imageResolution());

    QList<cwScrap*> scraps;
    scraps.reserve(protoNote.scraps_size());
    for(int i = 0; i < protoNote.scraps_size(); i++) {
        cwScrap* scrap = new cwScrap();
        loadScrap(protoNote.scraps(i), scrap);
        scraps.append(scrap);
    }

    note->setScraps(scraps);
}

/**
 * @brief cwRegionLoadTask::loadImage
 * @param protoImage
 * @return
 */
cwImage cwRegionLoadTask::loadImage(const CavewhereProto::Image& protoImage)
{
    cwImage image;

    image.setOriginal(protoImage.originalid());
    image.setIcon(protoImage.iconid());
    image.setOriginalDotsPerMeter(protoImage.dotpermeter());
    image.setOriginalSize(loadSize(protoImage.size()));

    QList<int> mipmaps;
    mipmaps.reserve(protoImage.mipmapids_size());
    for(int i = 0; i < protoImage.mipmapids_size(); i++) {
        mipmaps.append(protoImage.mipmapids(i));
    }

    image.setMipmaps(mipmaps);

    return image;
}

/**
 * @brief cwRegionLoadTask::loadScrap
 * @param protoScrap
 * @param scrap
 */
void cwRegionLoadTask::loadScrap(const CavewhereProto::Scrap& protoScrap, cwScrap *scrap)
{
    QVector<QPointF> outlinePoint;
    outlinePoint.resize(protoScrap.outlinepoints_size());
    for(int i = 0; i < protoScrap.outlinepoints_size(); i++) {
        outlinePoint[i] = loadPointF(protoScrap.outlinepoints(i));
    }
    scrap->setPoints(outlinePoint);

    QList<cwNoteStation> stations;
    stations.reserve(protoScrap.notestations_size());
    for(int i = 0; i < protoScrap.notestations_size(); i++) {
        stations.append(loadNoteStation(protoScrap.notestations(i)));
    }
    scrap->setStations(stations);

    QList<cwLead> leads;
    leads.reserve(protoScrap.leads_size());
    for(int i = 0; i < protoScrap.leads_size(); i++) {
        leads.append(loadLead(protoScrap.leads(i)));
    }
    scrap->setLeads(leads);

    loadNoteTranformation(protoScrap.notetransformation(), scrap->noteTransformation());
    scrap->setCalculateNoteTransform(protoScrap.calculatenotetransform());
    scrap->setTriangulationData(loadTriangulatedData(protoScrap.triangledata()));
}

/**
 * @brief cwRegionLoadTask::loadImageResolution
 * @param protoImageRes
 * @param imageResolution
 */
void cwRegionLoadTask::loadImageResolution(const CavewhereProto::ImageResolution& protoImageRes, cwImageResolution *imageResolution)
{
    imageResolution->setValue(protoImageRes.value());
    imageResolution->setUnit((cwUnits::ImageResolutionUnit)protoImageRes.unit());
}

/**
 * @brief cwRegionLoadTask::loadNoteStation
 * @param protoNoteStation
 * @return
 */
cwNoteStation cwRegionLoadTask::loadNoteStation(const CavewhereProto::NoteStation& protoNoteStation)
{
    cwNoteStation noteStation;
    noteStation.setName(loadString(protoNoteStation.name()));
    noteStation.setPositionOnNote(loadPointF(protoNoteStation.positiononnote()));
    return noteStation;
}

/**
 * @brief cwRegionLoadTask::loadNoteTranformation
 * @param protoNoteTransformation
 * @param noteTransformation
 */
void cwRegionLoadTask::loadNoteTranformation(const CavewhereProto::NoteTranformation& protoNoteTransformation, cwNoteTranformation *noteTransformation)
{
    noteTransformation->setNorthUp(protoNoteTransformation.northup());
    loadLength(protoNoteTransformation.scalenumerator(), noteTransformation->scaleNumerator());
    loadLength(protoNoteTransformation.scaledenominator(), noteTransformation->scaleDenominator());
}

/**
 * @brief cwRegionLoadTask::loadTriangulatedData
 * @param protoTriangulatedData
 * @return
 */
cwTriangulatedData cwRegionLoadTask::loadTriangulatedData(const CavewhereProto::TriangulatedData& protoTriangulatedData)
{
    cwTriangulatedData data;

    cwImage image = loadImage(protoTriangulatedData.croppedimage());
    data.setCroppedImage(image);

    QVector<QVector3D> points;
    points.resize(protoTriangulatedData.points_size());
    for(int i = 0; i < protoTriangulatedData.points_size(); i++) {
        points[i] = loadVector3D(protoTriangulatedData.points(i));
    }

    QVector<QVector2D> texCoords;
    texCoords.resize(protoTriangulatedData.texcoords_size());
    for(int i = 0; i < protoTriangulatedData.texcoords_size(); i++) {
        texCoords[i] = loadVector2D(protoTriangulatedData.texcoords(i));
    }

    QVector<uint> indexes;
    indexes.resize(protoTriangulatedData.indices_size());
    for(int i = 0; i < protoTriangulatedData.indices_size(); i++) {
        indexes[i] = protoTriangulatedData.indices(i);
    }

    QVector<QVector3D> leadPositions;
    leadPositions.resize(protoTriangulatedData.leadpositions_size());
    for(int i = 0; i < protoTriangulatedData.leadpositions_size(); i++) {
        leadPositions[i] = loadVector3D(protoTriangulatedData.leadpositions(i));
    }

    bool stale = protoTriangulatedData.stale();

    data.setPoints(points);
    data.setTexCoords(texCoords);
    data.setIndices(indexes);
    data.setLeadPoints(leadPositions);
    data.setStale(stale);

    return data;
}

/**
 * @brief cwRegionLoadTask::loadLength
 * @param protoLength
 * @param length
 */
void cwRegionLoadTask::loadLength(const CavewhereProto::Length& protoLength, cwLength *length)
{
    length->setValue(protoLength.value());
    length->setUnit((cwUnits::LengthUnit)protoLength.unit());
}

/**
 * @brief cwRegionLoadTask::loadTeamMember
 * @param protoTeamMember
 * @return
 */
cwTeamMember cwRegionLoadTask::loadTeamMember(const CavewhereProto::TeamMember& protoTeamMember)
{
    cwTeamMember teamMember;
    teamMember.setName(loadString(protoTeamMember.name()));
    teamMember.setJobs(loadStringList(protoTeamMember.jobs()));
    return teamMember;
}

/**
 * @brief cwRegionLoadTask::loadStation
 * @param protoStation
 * @param station
 */
cwStation cwRegionLoadTask::loadStation(const CavewhereProto::Station& protoStation)
{
    cwStation station;

    station.setName(loadString(protoStation.name()));
    station.setLeft(protoStation.left());
    station.setRight(protoStation.right());
    station.setUp(protoStation.up());
    station.setDown(protoStation.down());
    station.setLeftInputState((cwDistanceStates::State)protoStation.leftstate());
    station.setRightInputState((cwDistanceStates::State)protoStation.rightstate());
    station.setUpInputState((cwDistanceStates::State)protoStation.upstate());
    station.setDownInputState((cwDistanceStates::State)protoStation.downstate());
    return station;
}

/**
 * @brief cwRegionLoadTask::loadShot
 * @param protoShot
 * @param shot
 */
cwShot cwRegionLoadTask::loadShot(const CavewhereProto::Shot& protoShot)
{
    cwShot shot;
    shot.setDistance(protoShot.distance());
    shot.setCompass(protoShot.compass());
    shot.setBackCompass(protoShot.backcompass());
    shot.setClino(protoShot.clino());
    shot.setBackClino(protoShot.backclino());
    shot.setDistanceState((cwDistanceStates::State)protoShot.distancestate());
    shot.setCompassState((cwCompassStates::State)protoShot.compassstate());
    shot.setBackCompassState((cwCompassStates::State)protoShot.backcompassstate());
    shot.setClinoState((cwClinoStates::State)protoShot.clinostate());
    shot.setBackClinoState((cwClinoStates::State)protoShot.backclinostate());
    shot.setDistanceIncluded(protoShot.includedistance());

    return shot;
}

/**
 * @brief cwRegionLoadTask::loadStationPositionLookup
 * @param protoStationLookup
 * @return Return's the station lookup for the cave
 */
cwStationPositionLookup cwRegionLoadTask::loadStationPositionLookup(const CavewhereProto::StationPositionLookup &protoStationLookup)
{
    cwStationPositionLookup stationLookup;
    for(int i = 0; i < protoStationLookup.stationpositions_size(); i++) {
        const CavewhereProto::StationPositionLookup_NamePosition& namePosition = protoStationLookup.stationpositions(i);
        QString name = loadString(namePosition.stationname());
        QVector3D position = loadVector3D(namePosition.position());
        stationLookup.setPosition(name, position);
    }
    return stationLookup;
}

/**
 * @brief cwRegionLoadTask::loadLead
 * @param protoLead
 * @return Return's a lead from the a protoLead
 */
cwLead cwRegionLoadTask::loadLead(const CavewhereProto::Lead &protoLead)
{
    cwLead lead;
    lead.setPositionOnNote(loadPointF(protoLead.positiononnote()));
    lead.setDescription(loadString(protoLead.description()));
    lead.setSize(loadSizeF(protoLead.size()));
    lead.setCompleted(protoLead.completed());
    return lead;
}

/**
 * @brief cwRegionLoadTask::loadString
 * @param protoString
 * @return
 */
QString cwRegionLoadTask::loadString(const QtProto::QString& protoString)
{
    const std::string& string = protoString.stringdata();
    return QString::fromUtf8(string.c_str(), string.length());
}

/**
 * @brief cwRegionLoadTask::loadDate
 * @param protoDate
 * @return
 */
QDate cwRegionLoadTask::loadDate(const QtProto::QDate& protoDate)
{
    return QDate(protoDate.year(), protoDate.month(), protoDate.day());
}

/**
 * @brief cwRegionLoadTask::loadSize
 * @param protoSize
 * @return
 */
QSize cwRegionLoadTask::loadSize(const QtProto::QSize &protoSize)
{
    QSize size;
    size.setWidth(protoSize.width());
    size.setHeight(protoSize.height());
    return size;
}

/**
 * @brief cwRegionLoadTask::loadSizeF
 * @param protoSize
 * @return
 */
QSizeF cwRegionLoadTask::loadSizeF(const QtProto::QSizeF &protoSize)
{
    QSizeF size;
    size.setWidth(protoSize.width());
    size.setHeight(protoSize.height());
    return size;
}

/**
 * @brief cwRegionLoadTask::loadPointF
 * @param protoPointF
 * @return
 */
QPointF cwRegionLoadTask::loadPointF(const QtProto::QPointF& protoPointF)
{
    return QPointF (protoPointF.x(), protoPointF.y());
}

/**
 * @brief cwRegionLoadTask::loadVector3D
 * @param protoVector3D
 * @return
 */
QVector3D cwRegionLoadTask::loadVector3D(const QtProto::QVector3D &protoVector3D)
{
    return QVector3D(protoVector3D.x(), protoVector3D.y(), protoVector3D.z());
}

/**
 * @brief cwRegionLoadTask::loadVector2D
 * @param protoVector2D
 * @return
 */
QVector2D cwRegionLoadTask::loadVector2D(const QtProto::QVector2D &protoVector2D)
{
    return QVector2D(protoVector2D.x(), protoVector2D.y());
}

/**
 * @brief cwRegionLoadTask::loadStringList
 * @param protoStringList
 * @return
 */
QStringList cwRegionLoadTask::loadStringList(const QtProto::QStringList &protoStringList)
{
    QStringList list;
    list.reserve(protoStringList.strings_size());
    for(int i = 0; i < protoStringList.strings_size(); i++) {
        list.append(loadString(protoStringList.strings(i)));
    }
    return list;
}


/**
  Reads the region data from the database

  This return a QString filed with XML data
  */
//QString cwRegionLoadTask::readXMLFromDatabase() {
//    QSqlQuery selectCavingRegion(Database);
//    QString queryStr =
//            QString("SELECT qCompress_XML FROM CavingRegion where id = 1");

//    bool couldPrepare = selectCavingRegion.prepare(queryStr);
//    if(!couldPrepare) {
//        qDebug() << "Couldn't prepare select Caving Region:" << selectCavingRegion.lastError().databaseText() << queryStr;
//    }

//    //Extract the data
//    selectCavingRegion.exec();
//    QSqlRecord record = selectCavingRegion.record();

//    if(record.isEmpty()) {
//        qDebug() << "Hmmmm, no caving regions to load";
//        return QString();
//    }

//    //Get the first row
//    selectCavingRegion.next();

//    QString xmlData = selectCavingRegion.value(0).toString();
//    return xmlData;
//}

/**
 * @brief cwRegionLoadTask::loadFromBoostSerialization
 * @return
 */
//bool cwRegionLoadTask::loadFromBoostSerialization()
//{        //Get the xmlData from the database
//    QString xmlData = readXMLFromDatabase();
//    Database.close();

//    //Add the string data to the stream
//    std::stringstream stream(xmlData.toStdString());

//    //Parse xml the string data
//    try {
//        boost::archive::xml_iarchive xmlLoadArchive(stream);

//        cwCavingRegion region;
//        xmlLoadArchive >> BOOST_SERIALIZATION_NVP(region);
//        *Region = region;

//        //Clean up old images
//        cwImageCleanupTask imageCleanupTask;
//        imageCleanupTask.setDatabaseFilename(databaseFilename());
//        imageCleanupTask.setRegion(Region);
//        imageCleanupTask.start();
//        return true;

//    } catch(boost::archive::archive_exception exception) {
//        qDebug() << "Couldn't load data from XML!" << exception.what();
//        return false;
//    }
//}



/**
 * @brief cwRegionLoadTask::insureVacuuming
 *
 * This will make sure that the SQL database is using vacuuming
 *
 * This make sure sqlite is cleaning up after itself
 */
void cwRegionLoadTask::insureVacuuming()
{
//    cwSQLManager::Transaction transaction(&Database);

    int vacuum = -1;

    {
        QString SQL = "PRAGMA auto_vacuum";
        QSqlQuery isVaccumingQuery(SQL, Database);

        if(isVaccumingQuery.next()) {
            vacuum = isVaccumingQuery.value(0).toInt();
        }
    }

    switch(vacuum) {
    case 0: {
        //Vacuum is off
        //Turn on full Vacuum
        QSqlQuery turnOnFullVacuum(Database);

        bool success = turnOnFullVacuum.exec("PRAGMA auto_vacuum = 1");
        if(!success) {
            qDebug() << "Turn on vacuum error:" << turnOnFullVacuum.lastError().text() << LOCATION;
        }

        success = turnOnFullVacuum.exec("VACUUM");
        if(!success) {
            qDebug() << "Vacuum error:" << turnOnFullVacuum.lastError().text();
        }
    }
    case 1:
        //Full Vacuum
        break; //Do nothing
    case 2: {
        //Incremental Vacuum
        QString SQL = "PRAGMA auto_vacuum 1";
        QSqlQuery turnOnFullVacuum(Database);
        turnOnFullVacuum.exec(SQL);
    }
    }
}


