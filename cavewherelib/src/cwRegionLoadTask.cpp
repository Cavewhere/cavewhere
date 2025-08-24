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
#include "cwSurveyNetwork.h"
#include "cavewhereVersion.h"
#include "cwProject.h"
#include "cwProjectedProfileScrapViewMatrix.h"

//Qt includes
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QThread>


//Protobuf
#include "cavewhere.pb.h"
#include "qt.pb.h"


cwRegionLoadTask::cwRegionLoadTask(QObject *parent) :
    cwRegionIOTask(parent)
{

}

QByteArray cwRegionLoadTask::readSeralizedData()
{
    QByteArray data;

    runAfterConnected([this, &data](){
        bool okay;
        data = readProtoBufferFromDatabase(&okay);
    });

    return data;
}

void cwRegionLoadTask::setDeleteOldImages(bool deleteImages)
{
    DeleteOldImages = deleteImages;
}

cwRegionLoadResult cwRegionLoadTask::load()
{
    cwRegionLoadResult results;

    QFileInfo info(databaseFilename());

    if(!info.exists()) {
        results.addError(cwError(doesNotExistErrorMessage(databaseFilename()), cwError::Fatal));
        return results;
    }

    if(!info.isReadable()) {
        results.addError(cwError(fileNotReadableErrorMessage(databaseFilename()), cwError::Fatal));
        return results;
    }

    if(!info.isWritable()) {
        results.addError(cwError(QString("'%1' is a ReadOnly file. Copying it to a temporary file").arg(databaseFilename()), cwError::Warning));

        auto createTemporaryFilename = []()
        {
            QDateTime seedTime = QDateTime::currentDateTime();
            return QString("%1/CavewhereTmpProject-%2.cw")
                    .arg(QDir::tempPath())
                    .arg(seedTime.toMSecsSinceEpoch(), 0, 16);
        };


        auto newFilename = createTemporaryFilename();
        QFile::copy(databaseFilename(), newFilename);
        QFile::setPermissions(newFilename, QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::ReadOwner | QFileDevice::ReadUser);
        setDatabaseFilename(newFilename);
        results.setIsTempFile(true);

        QFileInfo newInfo(newFilename);
        if(!newInfo.exists() || !newInfo.isReadable() || !newInfo.isWritable()) {
            results.addError(cwError(QString("Can't copy to temporary file '%1', giving up.").arg(newFilename), cwError::Fatal));
            return results;
        }
    }

    runAfterConnected([this, &results](){
        auto region = cwCavingRegionPtr(new cwCavingRegion);

        //Try loading Proto Buffer
        auto data = loadFromProtoBuffer();

        results.addErrors(errors());
        if(cwError::containsFatal(results.errors())) {
            return results;
        }

        results.setCavingRegion(data.region->data());
        results.setFileVersion(data.fileVersion);
        results.setFileName(databaseFilename());
        return results;
    });

    return results;
}

/**
  Loads the region data
  */
void cwRegionLoadTask::runTask() {
    done();
}

/**
 * @brief cwRegionLoadTask::loadFromProtoBuffer
 * @return
 */
cwRegionLoadTask::LoadData cwRegionLoadTask::loadFromProtoBuffer()
{
    bool okay;
    QByteArray protoBufferData = readProtoBufferFromDatabase(&okay);

    if(!okay) {
        return {};
    }

    CavewhereProto::CavingRegion regionProto;
    bool couldParse = regionProto.ParseFromArray(protoBufferData.data(), protoBufferData.size());

    if(!couldParse) {
        addError(cwError("Couldn't read proto buffer. Corrupted?!", cwError::Fatal));
        return {};
    }

    auto data = loadCavingRegion(regionProto);

    //Clean up old images
    if(DeleteOldImages) {
        cwImageCleanupTask imageCleanupTask;
        imageCleanupTask.setUsingThreadPool(false);
        imageCleanupTask.setDatabaseFilename(databaseFilename());
        imageCleanupTask.setRegion(data.region.data());
        imageCleanupTask.start();
    }

    disconnectToDatabase();
    return data;
}

/**
 * @brief cwRegionLoadTask::readProtoBufferFromDatabase
 * @return This reads the proto buffer from the database
 */
QByteArray cwRegionLoadTask::readProtoBufferFromDatabase(bool* okay)
{
    cwSQLManager::Transaction transaction(database(), cwSQLManager::ReadOnly);

    QSqlQuery selectObjectData(database());
    QString queryStr =
            QString("SELECT protoBuffer FROM ObjectData where id = 1");

    bool couldPrepare = selectObjectData.prepare(queryStr);
    if(!couldPrepare) {
        addError({QString("Couldn't prepare select Caving Region:'%1' sql:'%2'").arg(selectObjectData.lastError().databaseText()).arg(queryStr), cwError::Fatal});
        return QByteArray();
    }

    //Extract the data
    selectObjectData.exec();
    QSqlRecord record = selectObjectData.record();

    if(record.isEmpty()) {
        addError({QString("Hmmmm, no caving regions to load from protoBuffer"), cwError::Fatal});
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
cwRegionLoadTask::LoadData cwRegionLoadTask::loadCavingRegion(const CavewhereProto::CavingRegion &protoRegion)
{
    LoadData data(new cwCavingRegion(), loadFileVersion(protoRegion));

    data.fileVersion = loadFileVersion(protoRegion);
    // auto fileCavewhereVersion = protoRegion.has_cavewhereversion() ? QString::fromStdString(protoRegion.cavewhereversion()) : loadString(protoRegion.cavewhereversion()) : "";
    auto fileCavewhereVersion = LOAD_STRING(protoRegion, cavewhereversion);

    if(data.fileVersion > protoVersion()) {
        //Add a warning
        QString upgradeStr = fileCavewhereVersion.isEmpty() ? QString() : QString(" to %1").arg(fileCavewhereVersion);

        addError(cwError(QString("Upgrade CaveWhere to %1 to load this file! Current file version is %2. CaveWhere %3 supports up to file version %4. You are loading a newer CaveWhere file than this version supports. You will loss data if you save")
                         .arg(fileCavewhereVersion)
                         .arg(data.fileVersion)
                         .arg(CavewhereVersion)
                         .arg(protoVersion())));
    }

    QList<cwCave*> caves;
    caves.reserve(protoRegion.caves_size());

    for(int i = 0; i < protoRegion.caves_size(); i++) {
        const CavewhereProto::Cave& protoCave = protoRegion.caves(i);
        cwCave* cave = new cwCave();
        loadCave(protoCave, cave);

        caves.append(cave);
    }

    data.region->addCaves(caves);

    return data;
}

/**
 * @brief cwRegionLoadTask::loadCave
 * @param protoCave
 * @param cave
 */
void cwRegionLoadTask::loadCave(const CavewhereProto::Cave& protoCave, cwCave *cave)
{
    QString name = loadName(protoCave);
    cwUnits::LengthUnit lengthUnit = static_cast<cwUnits::LengthUnit>(protoCave.lengthunit());
    cwUnits::LengthUnit depthUnit = static_cast<cwUnits::LengthUnit>(protoCave.depthunit());

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

    if(protoCave.has_network()) {
        auto protoNetwork = protoCave.network();
        cwSurveyNetwork network;
        for(int i = 0; i < protoNetwork.stations_size(); i++) {
            auto stationItem = protoNetwork.stations(i);

            auto stationName = loadLegacyString(stationItem.legacy_stationname());
            auto neighbors = loadLegacyStringList(stationItem.legacy_neighbors());

            for(auto neighbor : neighbors) {
                network.addShot(stationName, neighbor);
            }
        }
        cave->setSurveyNetwork(network);
    } else {
        cave->setStationPositionLookupStale(false);
    }
}

/**
 * @brief cwRegionLoadTask::loadTrip
 * @param protoTrip
 * @param trip
 */
void cwRegionLoadTask::loadTrip(const CavewhereProto::Trip& protoTrip, cwTrip *trip)
{    
    QString tripName = loadName(protoTrip);
    QDate tripDate = loadDate(protoTrip.date());

    trip->setName(tripName);
    trip->setDate(QDateTime(tripDate, QTime()));

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
        addError(cwError("Shot, station count mismatch, survey chunk invalid: %1 %2", cwError::Warning));
        return;
    }

    for(int i = 0; i < stations.count() - 1; i++) {
        cwStation fromStation = stations[i];
        cwStation toStation = stations[i + 1];
        cwShot shot = shots[i];

        chunk->appendShot(fromStation, toStation, shot);
    }

    for(int i = 0; i < protoChunk.calibrations_size(); i++) {
        const CavewhereProto::ChunkCalibration& protoCalibration = protoChunk.calibrations(i);
        cwTripCalibration* calibration = new cwTripCalibration();
        loadTripCalibration(protoCalibration.calibration(), calibration);

        chunk->addCalibration(protoCalibration.shotindex(), calibration);
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

    image.setOriginal(protoImage.legacy_originalid());
    image.setIcon(protoImage.legacy_iconid());
    image.setOriginalDotsPerMeter(protoImage.dotpermeter());
    image.setOriginalSize(loadSize(protoImage.size()));

    QList<int> mipmaps;
    mipmaps.reserve(protoImage.legacy_mipmapids_size());
    for(int i = 0; i < protoImage.legacy_mipmapids_size(); i++) {
        mipmaps.append(protoImage.legacy_mipmapids(i));
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
    scrap->setType(static_cast<cwScrap::ScrapType>(protoScrap.type()));

    if(protoScrap.type() == CavewhereProto::Scrap_ScrapType::Scrap_ScrapType_ProjectedProfile) {
        Q_ASSERT(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()));
        auto view = static_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
        if(protoScrap.has_profileviewmatrix()) {
            if(protoScrap.profileviewmatrix().has_azimuth()) {
                view->setAzimuth(protoScrap.profileviewmatrix().azimuth());
            }
            if(protoScrap.profileviewmatrix().has_direction()) {
                view->setDirection(static_cast<cwProjectedProfileScrapViewMatrix::AzimuthDirection>(protoScrap.profileviewmatrix().direction()));
            }
        }


    }
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
    noteStation.setName(loadLegacyString(protoNoteStation.legacy_name()));
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
    data.setCroppedImage(cwTrackedImage::createShared(image, databaseFilename(), cwTrackedImage::NoOwnership));

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
    teamMember.setName(loadLegacyString(protoTeamMember.legacy_name()));
    teamMember.setJobs(loadLegacyStringList(protoTeamMember.legacy_jobs()));
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

    station.setName(loadName(protoStation));

    //Returns the distance based on functors
    // auto distance = [&](auto hasReading,
    //                     auto reading,
    //                     auto state,
    //                     auto value) -> cwDistanceReading {
    //     if (hasReading()) {
    //         //Version 6
    //         return distanceReading(reading());
    //     } else {
    //         auto s = static_cast<cwDistanceStates::State>(state());
    //         switch (s) {
    //         case cwDistanceStates::Valid: {
    //             return cwDistanceReading(value());
    //         }
    //         case cwDistanceStates::Empty: {
    //             return cwDistanceReading();
    //         }
    //         default: {
    //             Q_ASSERT(false);
    //             return cwDistanceReading();
    //         }
    //         }
    //     }
    // };

    station.setLeft(distance(
        [&]() { return protoStation.has_legacy_leftreading(); }, //v6
        [&]() { return protoStation.legacy_leftreading(); }, //v6
        [&]() { return protoStation.legacy_leftstate(); }, //Handle older version 5 attributes
        [&]() { return protoStation.legacy_left(); } //Handle older version 5 attribute
        ));
    station.setRight(distance(
        [&]() { return protoStation.has_legacy_rightreading(); }, //v6
        [&]() { return protoStation.legacy_rightreading(); }, //v6
        [&]() { return protoStation.legacy_rightstate(); }, //Handle older version 5 attributes
        [&]() { return protoStation.legacy_right(); } //Handle older version 5 attribute
        ));
    station.setUp(distance(
        [&]() { return protoStation.has_legacy_upreading(); }, //v6
        [&]() { return protoStation.legacy_upreading(); }, //v6
        [&]() { return protoStation.legacy_upstate(); }, //Handle older version 5 attributes
        [&]() { return protoStation.legacy_up(); } //Handle older version 5 attribute
        ));
    station.setDown(distance(
        [&]() { return protoStation.has_legacy_downreading(); }, //v6
        [&]() { return protoStation.legacy_downreading(); }, // v6
        [&]() { return protoStation.legacy_downstate(); }, //Handle older version 5 attributes
        [&]() { return protoStation.legacy_down(); } //Handle older version 5 attribute
        ));

    //Old version 5 code
    // station.setRight(protoStation.right());
    // station.setUp(protoStation.up());
    // station.setDown(protoStation.down());
    // station.setLeftInputState((cwDistanceStates::State)protoStation.leftstate());
    // station.setRightInputState((cwDistanceStates::State)protoStation.rightstate());
    // station.setUpInputState((cwDistanceStates::State)protoStation.upstate());
    // station.setDownInputState((cwDistanceStates::State)protoStation.downstate());
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
    shot.setDistance(distance(
        [&]() { return protoShot.has_legacy_distancereading(); }, //v6
        [&]() { return protoShot.legacy_distancereading(); }, //v6
        [&]() { return protoShot.legacy_distancestate(); }, //Handle older version 5 attributes
        [&]() { return protoShot.legacy_distance(); } //Handle older version 5 attribute
        ));

    shot.setCompass(compass(
        [&]() { return protoShot.has_legacy_compassreading(); },
        [&]() { return protoShot.legacy_compassreading(); },
        [&]() { return protoShot.legacy_compassstate(); }, //Handle older version 5 attributes
        [&]() { return protoShot.legacy_compass(); } //Handle older version 5 attribute
        ));

    shot.setBackCompass(compass(
        [&]() { return protoShot.has_legacy_backcompassreading(); },
        [&]() { return protoShot.legacy_backcompassreading(); },
        [&]() { return protoShot.legacy_backcompassstate(); }, //Handle older version 5 attributes
        [&]() { return protoShot.legacy_backcompass(); } //Handle older version 5 attribute
        ));

    shot.setClino(clino(
        [&]() { return protoShot.has_legacy_clinoreading(); },
        [&]() { return protoShot.legacy_clinoreading(); },
        [&]() { return protoShot.legacy_clinostate(); }, //Handle older version 5 attributes
        [&]() { return protoShot.legacy_clino(); } //Handle older version 5 attribute
        ));

    shot.setBackClino(clino(
        [&]() { return protoShot.has_legacy_backclinoreading(); },
        [&]() { return protoShot.legacy_backclinoreading(); },
        [&]() { return protoShot.legacy_backclinostate(); }, //Handle older version 5 attributes
        [&]() { return protoShot.legacy_backclino(); } //Handle older version 5 attribute
        ));

    shot.setDistanceIncluded(protoShot.includedistance());

    //Old code from version 5
    // shot.setCompass(protoShot.compass());
    // shot.setBackCompass(protoShot.backcompass());
    // shot.setClino(protoShot.clino());
    // shot.setBackClino(protoShot.backclino());
    // shot.setCompassState((cwCompassStates::State)protoShot.compassstate());
    // shot.setBackCompassState((cwCompassStates::State)protoShot.backcompassstate());
    // shot.setClinoState((cwClinoStates::State)protoShot.clinostate());
    // shot.setBackClinoState((cwClinoStates::State)protoShot.backclinostate());

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
        QString name = LOAD_STRING(namePosition, stationname);
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
    lead.setDescription(loadLegacyString(protoLead.legacy_description()));
    lead.setSize(loadSizeF(protoLead.size()));
    lead.setCompleted(protoLead.completed());
    return lead;
}

cwDistanceReading cwRegionLoadTask::distanceReading(const CavewhereProto::DistanceReading &protoDistanceReading)
{
    return cwDistanceReading(loadLegacyString(protoDistanceReading.legacy_value()));
}

cwCompassReading cwRegionLoadTask::compassReading(const CavewhereProto::CompassReading &protoCompassReading)
{
    return cwCompassReading(loadLegacyString(protoCompassReading.legacy_value()));
}

cwClinoReading cwRegionLoadTask::clinoReading(const CavewhereProto::ClinoReading &protoClinoReading)
{
    return cwClinoReading(loadLegacyString(protoClinoReading.legacy_value()));
}

int cwRegionLoadTask::loadFileVersion(const CavewhereProto::CavingRegion &protoRegion)
{
    return protoRegion.has_version() ? protoRegion.version() : 0;
}


/**
 * @brief cwRegionLoadTask::loadString
 * @param protoString
 * @return
 */
QString cwRegionLoadTask::loadString(const std::string& protoString)
{
    return QString::fromUtf8(QByteArrayView(protoString.c_str(), protoString.length())); //QByteArray::fromStdString(protoString.c_str()));
}

QString cwRegionLoadTask::loadLegacyString(const QtProto::QString &protoString)
{
    const std::string& string = protoString.stringdata();
    return QString::fromUtf8(string.c_str(), string.length());
}

QString cwRegionLoadTask::doesNotExistErrorMessage(const QString &filename)
{
    return QString("Couldn't open '%1' because it doesn't exist").arg(filename);
}

QString cwRegionLoadTask::fileNotReadableErrorMessage(const QString &filename)
{
    return QString("Couldn't open '%1' because you don't have permission to read it").arg(filename);
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
QStringList cwRegionLoadTask::loadLegacyStringList(const QtProto::QStringList &protoStringList)
{
    QStringList list;

    if(protoStringList.legacy_strings_size() > 0) {
        list.reserve(protoStringList.legacy_strings_size());
        for(int i = 0; i < protoStringList.legacy_strings_size(); ++i) {
            list.append(loadLegacyString(protoStringList.legacy_strings(i)));
        }
    }

    return list;
}

/**
 * @brief cwRegionLoadTask::insureVacuuming
 *
 * This will make sure that the SQL database is using vacuuming
 *
 * This make sure sqlite is cleaning up after itself
 */
void cwRegionLoadTask::insureVacuuming()
{
    int vacuum = -1;

    {
        QString SQL = "PRAGMA auto_vacuum";
        QSqlQuery isVaccumingQuery(SQL, database());

        if(isVaccumingQuery.next()) {
            vacuum = isVaccumingQuery.value(0).toInt();
        }
    }

    switch(vacuum) {
    case 0: {
        //Vacuum is off
        //Turn on full Vacuum
        QSqlQuery turnOnFullVacuum(database());

        bool success = turnOnFullVacuum.exec("PRAGMA auto_vacuum = 1");
        if(!success) {
            qDebug() << "Turn on vacuum error:" << turnOnFullVacuum.lastError().text() << LOCATION;
        }

        success = turnOnFullVacuum.exec("VACUUM");
        if(!success) {
            qDebug() << "Vacuum error:" << turnOnFullVacuum.lastError().text();
        }

        break;
    }
    case 1:
        //Full Vacuum
        break; //Do nothing
    case 2: {
        //Incremental Vacuum
        QString SQL = "PRAGMA auto_vacuum 1";
        QSqlQuery turnOnFullVacuum(database());
        turnOnFullVacuum.exec(SQL);
    }
    }
}


