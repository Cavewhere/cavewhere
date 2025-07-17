/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRegionSaveTask.h"
#include "cwRegionIOTask.h"
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
#include "cavewhereVersion.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwLead.h"

//Google protobuffer
#include "cavewhere.pb.h"
#include "qt.pb.h"

//Qt includes
#include <QSqlQuery>
#include <QSqlError>

//Std includes
#include <sstream>

cwRegionSaveTask::cwRegionSaveTask(QObject *parent) :
    cwRegionIOTask(parent)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

QByteArray cwRegionSaveTask::serializedData(cwCavingRegion* region)
{
    CavewhereProto::CavingRegion protoRegion;
    saveCavingRegion(protoRegion, region);

    std::string regionString = protoRegion.SerializeAsString();

    QByteArray regionByteArray;
    if(!regionString.empty()) {
        regionByteArray = QByteArray(&regionString[0], regionString.size());
    }
    return regionByteArray;
}

QList<cwError> cwRegionSaveTask::save(cwCavingRegion* region)
{
    //Open a datebase connection
    bool connected = connectToDatabase("saveRegionTask");
    if(connected) {

        cwProject::createDefaultSchema(database());

        saveToProtoBuffer(region);

        disconnectToDatabase();
    }

    return errors();
}

void cwRegionSaveTask::runTask() {
    //Finished
    done();
}

/**
 * @brief cwRegionSaveTask::saveToProtoBuffer
 *
 * Save cavewhere object data usingo google protobuffer
 */
void cwRegionSaveTask::saveToProtoBuffer(cwCavingRegion* region)
{
    cwSQLManager::Transaction transaction(database());

    QByteArray regionByteArray = serializedData(region);

    QSqlQuery insertCavingRegion(database());
    QString queryStr =
            QString("INSERT OR REPLACE INTO ObjectData ") +
            QString("(id, protoBuffer) ") +
            QString("VALUES (1, ?)");

    bool successful = insertCavingRegion.prepare(queryStr);
    if(!successful) {
        addError(cwError(QString("Couldn't create query to insert region proto buffer data:") + insertCavingRegion.lastError().text(), cwError::Fatal));
        stop();
    }

    insertCavingRegion.bindValue(0, regionByteArray);
    bool success = insertCavingRegion.exec();

    if(!success) {
        addError(cwError(QString("Couldn't execute query:") + insertCavingRegion.lastError().databaseText() + " " + queryStr + " " + LOCATION_STR, cwError::Fatal));
    }
}

/**
 * @brief cwRegionSaveTask::saveCave
 * @param protoCave
 * @param cave
 */
void cwRegionSaveTask::saveCave(CavewhereProto::Cave *protoCave, cwCave *cave)
{
    saveString(protoCave->mutable_name(), cave->name());
    protoCave->set_lengthunit((CavewhereProto::Units_LengthUnit)cave->length()->unit());
    protoCave->set_depthunit((CavewhereProto::Units_LengthUnit)cave->depth()->unit());

    foreach(cwTrip* trip, cave->trips()) {
        CavewhereProto::Trip* protoTrip = protoCave->add_trips();
        saveTrip(protoTrip, trip);
    }

    saveStationLookup(protoCave->mutable_stationpositionlookup(), cave->stationPositionLookup());
    protoCave->set_stationpositionlookupstale(cave->isStationPositionLookupStale());
    saveSurveyNetwork(protoCave->mutable_network(), cave->network());
}

/**
 * @brief cwRegionSaveTask::saveTrip
 * @param protoTrip
 * @param trip
 */
void cwRegionSaveTask::saveTrip(CavewhereProto::Trip *protoTrip, cwTrip *trip)
{
    saveString(protoTrip->mutable_name(), trip->name());
    saveDate(protoTrip->mutable_date(), trip->date().date());
    saveSurveyNoteModel(protoTrip->mutable_notemodel(), trip->notes());
    saveTripCalibration(protoTrip->mutable_tripcalibration(), trip->calibrations());
    saveTeam(protoTrip->mutable_team(), trip->team());

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        CavewhereProto::SurveyChunk* protoChunk = protoTrip->add_chunks();
        saveSurveyChunk(protoChunk, chunk);
    }
}

/**
 * @brief cwRegionSaveTask::saveSurveyNoteModel
 * @param protoNoteModel
 * @param noteModel
 */
void cwRegionSaveTask::saveSurveyNoteModel(CavewhereProto::SurveyNoteModel *protoNoteModel, cwSurveyNoteModel *noteModel)
{
    foreach(cwNote* note, noteModel->notes()) {
        CavewhereProto::Note* protoNote = protoNoteModel->add_notes();
        saveNote(protoNote, note);
    }
}

/**
 * @brief cwRegionSaveTask::saveTripCalibration
 * @param protoTripCalibration
 * @param tripCalibration
 */
void cwRegionSaveTask::saveTripCalibration(CavewhereProto::TripCalibration *proto, cwTripCalibration *tripCalibration)
{
    proto->set_correctedcompassbacksight(tripCalibration->hasCorrectedCompassBacksight());
    proto->set_correctedclinobacksight(tripCalibration->hasCorrectedClinoBacksight());
    proto->set_tapecalibration(tripCalibration->tapeCalibration());
    proto->set_frontcompasscalibration(tripCalibration->frontCompassCalibration());
    proto->set_frontclinocalibration(tripCalibration->frontClinoCalibration());
    proto->set_backcompassscalibration(tripCalibration->backCompassCalibration());
    proto->set_backclinocalibration(tripCalibration->backClinoCalibration());
    proto->set_declination(tripCalibration->declination());
    proto->set_distanceunit((CavewhereProto::Units_LengthUnit)tripCalibration->distanceUnit());
    proto->set_frontsights(tripCalibration->hasFrontSights());
    proto->set_backsights(tripCalibration->hasBackSights());
    proto->set_correctedcompassfrontsight(tripCalibration->hasCorrectedCompassFrontsight());
    proto->set_correctedclinofrontsight(tripCalibration->hasCorrectedClinoFrontsight());
}

/**
 * @brief cwRegionSaveTask::saveSurveyChunk
 * @param protoChunk
 * @param chunk
 */
void cwRegionSaveTask::saveSurveyChunk(CavewhereProto::SurveyChunk *protoChunk, cwSurveyChunk *chunk)
{
    // foreach(cwStation station, chunk->stations()) {
    //     CavewhereProto::Station* protoStation = protoChunk->add_stations();
    //     saveStation(protoStation, station);
    // }

    // foreach(cwShot shot, chunk->shots()) {
    //     CavewhereProto::Shot* protoShot = protoChunk->add_shots();
    //     saveShot(protoShot, shot);
    // }

    Q_ASSERT(chunk->stationCount() - 1 == chunk->shotCount());

    saveQUuid(protoChunk->mutable_id(), chunk->id());

    for(int i = 0; i < chunk->stationCount(); ++i) {
        auto station = protoChunk->add_leg();
        saveStationShot(station, chunk->station(i));

        if(i < chunk->shotCount()) {
            auto shot = protoChunk->add_leg();
            saveStationShot(shot, chunk->shot(i));
        }
    }

    auto calibrations = chunk->calibrations();
    for(auto iter = calibrations.begin(); iter != calibrations.end(); ++iter) {
        CavewhereProto::ChunkCalibration* protoCalibration = protoChunk->add_calibrations();
        protoCalibration->set_shotindex(iter.key());
        saveTripCalibration(protoCalibration->mutable_calibration(), iter.value());
    }
}

/**
 * @brief cwRegionSaveTask::saveTeam
 * @param protoTeam
 * @param team
 */
void cwRegionSaveTask::saveTeam(CavewhereProto::Team *protoTeam, cwTeam *team)
{
    foreach(cwTeamMember teamMember, team->teamMembers()) {
        CavewhereProto::TeamMember* protoTeamMember = protoTeam->add_teammembers();
        saveTeamMember(protoTeamMember, teamMember);
    }

}

/**
 * @brief cwRegionSaveTask::saveNote
 * @param protoNote
 * @param note
 */
void cwRegionSaveTask::saveNote(CavewhereProto::Note *protoNote, cwNote *note)
{
    saveImage(protoNote->mutable_image(), note->image());
    protoNote->set_rotation(note->rotate());
    saveImageResolution(protoNote->mutable_imageresolution(), note->imageResolution());

    foreach(cwScrap* scrap, note->scraps()) {
        CavewhereProto::Scrap* protoScrap = protoNote->add_scraps();
        saveScrap(protoScrap, scrap);
    }
}

/**
 * @brief cwRegionSaveTask::saveImage
 * @param protoImage
 * @param image
 */
void cwRegionSaveTask::saveImage(CavewhereProto::Image *protoImage, const cwImage &image)
{
    protoImage->set_originalid(image.original());
    protoImage->set_iconid(image.icon());
    protoImage->set_dotpermeter(image.originalDotsPerMeter());
    saveSize(protoImage->mutable_size(), image.originalSize());
    foreach(int mipmapId, image.mipmaps()) {
        protoImage->add_mipmapids(mipmapId);
    }

}

/**
 * @brief cwRegionSaveTask::saveScrap
 * @param protoScrap
 * @param scrap
 */
void cwRegionSaveTask::saveScrap(CavewhereProto::Scrap *protoScrap, cwScrap *scrap)
{
    foreach(QPointF outlinePoint, scrap->points()) {
        QtProto::QPointF* protoPoint = protoScrap->add_outlinepoints();
        savePointF(protoPoint, outlinePoint);
    }

    foreach(cwNoteStation station, scrap->stations()) {
        CavewhereProto::NoteStation* protoNoteStation = protoScrap->add_notestations();
        saveNoteStation(protoNoteStation, station);
    }

    foreach(const cwLead& lead, scrap->leads()) {
        CavewhereProto::Lead* protoLead = protoScrap->add_leads();
        saveLead(protoLead, lead);
    }

    saveNoteTranformation(protoScrap->mutable_notetransformation(), scrap->noteTransformation());
    protoScrap->set_calculatenotetransform(scrap->calculateNoteTransform());
    saveTriangulatedData(protoScrap->mutable_triangledata(), scrap->triangulationData());
    protoScrap->set_type(static_cast<CavewhereProto::Scrap_ScrapType>(scrap->type()));

    if(scrap->type() == cwScrap::ProjectedProfile) {
        saveProjectedScrapViewMatrix(protoScrap->mutable_profileviewmatrix(), static_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()));
    }
}

/**
 * @brief cwRegionSaveTask::saveImageResolution
 * @param protoImageRes
 * @param imageResolution
 */
void cwRegionSaveTask::saveImageResolution(CavewhereProto::ImageResolution *protoImageRes, cwImageResolution *imageResolution)
{
    protoImageRes->set_value(imageResolution->value());
    protoImageRes->set_unit((CavewhereProto::Units_ImageResolutionUnit)imageResolution->unit());
}

/**
 * @brief cwRegionSaveTask::saveNoteStation
 * @param protoNoteStation
 * @param noteStation
 */
void cwRegionSaveTask::saveNoteStation(CavewhereProto::NoteStation* protoNoteStation, const cwNoteStation &noteStation)
{
    saveString(protoNoteStation->mutable_name(), noteStation.name());
    savePointF(protoNoteStation->mutable_positiononnote(), noteStation.positionOnNote());
}

/**
 * @brief cwRegionSaveTask::saveNoteTranformation
 * @param protoNoteTransformation
 * @param noteTransformation
 */
void cwRegionSaveTask::saveNoteTranformation(CavewhereProto::NoteTranformation *protoNoteTransformation,
                                             cwNoteTranformation *noteTransformation)
{
    protoNoteTransformation->set_northup(noteTransformation->northUp());
    saveLength(protoNoteTransformation->mutable_scalenumerator(),
               noteTransformation->scaleNumerator());
    saveLength(protoNoteTransformation->mutable_scaledenominator(),
               noteTransformation->scaleDenominator());
}

/**
 * @brief cwRegionSaveTask::saveTriangulatedData
 * @param protoTriangulatedData
 * @param triangluatedData
 */
void cwRegionSaveTask::saveTriangulatedData(CavewhereProto::TriangulatedData *protoTriangulatedData,
                                            const cwTriangulatedData &triangluatedData)
{
    saveImage(protoTriangulatedData->mutable_croppedimage(),
              triangluatedData.croppedImage());

    foreach(QVector3D point, triangluatedData.points()) {
        QtProto::QVector3D* protoVector3D = protoTriangulatedData->add_points();
        saveVector3D(protoVector3D, point);
    }

    foreach(QVector2D texCoord, triangluatedData.texCoords()) {
        QtProto::QVector2D* protoVector2D = protoTriangulatedData->add_texcoords();
        saveVector2D(protoVector2D, texCoord);
    }

    foreach(uint index, triangluatedData.indices()) {
        protoTriangulatedData->add_indices(index);
    }

    foreach(QVector3D leadPoint, triangluatedData.leadPoints()) {
        QtProto::QVector3D* protoVector3D = protoTriangulatedData->add_leadpositions();
        saveVector3D(protoVector3D, leadPoint);
    }

    protoTriangulatedData->set_stale(triangluatedData.isStale());
}

/**
 * @brief cwRegionSaveTask::saveString
 * @param protoString
 * @param string
 */
void cwRegionSaveTask::saveString(std::string *protoString, const QString& string)
{
    *protoString = string.toUtf8().toStdString();
}

/**
 * @brief cwRegionSaveTask::saveDate
 * @param protoDate
 * @param date
 */
void cwRegionSaveTask::saveDate(QtProto::QDate *protoDate, QDate date)
{
   protoDate->set_day(date.day());
   protoDate->set_month(date.month());
   protoDate->set_year(date.year());
}

/**
 * @brief cwRegionSaveTask::saveSize
 * @param protoSize
 * @param size
 */
void cwRegionSaveTask::saveSize(QtProto::QSize *protoSize, QSize size)
{
   protoSize->set_width(size.width());
   protoSize->set_height(size.height());
}

/**
 * @brief cwRegionSaveTask::saveSizeF
 * @param protoSize
 * @param size
 */
void cwRegionSaveTask::saveSizeF(QtProto::QSizeF *protoSize, QSizeF size)
{
    protoSize->set_width(size.width());
    protoSize->set_height(size.height());
}

/**
 * @brief cwRegionSaveTask::savePointF
 * @param protoPointF
 * @param point
 */
void cwRegionSaveTask::savePointF(QtProto::QPointF *protoPointF, QPointF point)
{
    protoPointF->set_x(point.x());
    protoPointF->set_y(point.y());
}

void cwRegionSaveTask::saveVector3D(QtProto::QVector3D *protoVector3D, QVector3D vector3D)
{
    protoVector3D->set_x(vector3D.x());
    protoVector3D->set_y(vector3D.y());
    protoVector3D->set_z(vector3D.z());
}

void cwRegionSaveTask::saveVector2D(QtProto::QVector2D *protoVector2D, QVector2D vector2D)
{
    protoVector2D->set_x(vector2D.x());
    protoVector2D->set_y(vector2D.y());
}

/**
 * @brief cwRegionSaveTask::saveStringList
 * @param protoStringList
 * @param stringlist
 */
void cwRegionSaveTask::saveStringList(QtProto::QStringList *protoStringList, QStringList stringlist)
{
    foreach(QString string, stringlist) {
        auto protoString = protoStringList->add_strings();
        saveString(protoString, string);
    }
}

void cwRegionSaveTask::saveQUuid(std::string *protoString, const QUuid &id)
{
    saveString(protoString, id.toString(QUuid::WithoutBraces));
}

/**
 * @brief cwRegionSaveTask::saveLength
 * @param protoLength
 * @param length
 */
void cwRegionSaveTask::saveLength(CavewhereProto::Length *protoLength, cwLength *length)
{
    protoLength->set_value(length->value());
    protoLength->set_unit((CavewhereProto::Units_LengthUnit)length->unit());
}

/**
 * @brief cwRegionSaveTask::saveTeamMember
 * @param protoTeamMember
 * @param teamMember
 */
void cwRegionSaveTask::saveTeamMember(CavewhereProto::TeamMember *protoTeamMember, const cwTeamMember& teamMember)
{
    saveString(protoTeamMember->mutable_name(), teamMember.name());
    saveStringList(protoTeamMember->mutable_jobs(), teamMember.jobs());
}

/**
 * @brief cwRegionSaveTask::saveStation
 * @param protoStation
 * @param station
 */
void cwRegionSaveTask::saveStation(CavewhereProto::Station *protoStation, const cwStation &station)
{
    // saveString(protoStation->mutable_name(), station.name());

    // saveReading([&](){return protoStation->mutable_left();}, station.left(), cwDistanceReading::State::Empty);
    // saveReading([&](){return protoStation->mutable_right();}, station.right(), cwDistanceReading::State::Empty);
    // saveReading([&](){return protoStation->mutable_up();}, station.up(), cwDistanceReading::State::Empty);
    // saveReading([&](){return protoStation->mutable_down();}, station.down(), cwDistanceReading::State::Empty);

    // Version 6
    // saveDistanceReading(protoStation->mutable_leftreading(), station.left());
    // saveDistanceReading(protoStation->mutable_rightreading(), station.right());
    // saveDistanceReading(protoStation->mutable_upreading(), station.up());
    // saveDistanceReading(protoStation->mutable_downreading(), station.down());

    // Version 5
    // protoStation->set_left(station.left());
    // protoStation->set_right(station.right());
    // protoStation->set_up(station.up());
    // protoStation->set_down(station.down());
    // protoStation->set_leftstate((CavewhereProto::DistanceStates_State)station.leftInputState());
    // protoStation->set_rightstate((CavewhereProto::DistanceStates_State)station.rightInputState());
    // protoStation->set_upstate((CavewhereProto::DistanceStates_State)station.upInputState());
    // protoStation->set_downstate((CavewhereProto::DistanceStates_State)station.downInputState());
}

/**
 * @brief cwRegionSaveTask::saveShot
 * @param shot
 * @param shot
 */
void cwRegionSaveTask::saveShot(CavewhereProto::Shot *protoShot, const cwShot &shot)
{
    // if(!shot.isDistanceIncluded()) {
    //     //By default distance is included, only write it if it's not include
    //     protoShot->set_includedistance(shot.isDistanceIncluded());
    // }

    // saveReading([&](){ return protoShot->mutable_distance(); }, shot.distance(), cwDistanceReading::State::Empty);
    // saveReading([&](){ return protoShot->mutable_compass(); }, shot.backCompass(), cwCompassReading::State::Empty);
    // saveReading([&](){ return protoShot->mutable_backcompass(); }, shot.backCompass(), cwCompassReading::State::Empty);
    // saveReading([&](){ return protoShot->mutable_clino(); }, shot.clino(), cwClinoReading::State::Empty);
    // saveReading([&](){ return protoShot->mutable_backclino(); }, shot.backClino(), cwClinoReading::State::Empty);

    //Version 6
    // saveDistanceReading(protoShot->mutable_distancereading(), shot.distance());
    // saveCompassReading(protoShot->mutable_compassreading(), shot.compass());
    // saveCompassReading(protoShot->mutable_backcompassreading(), shot.backCompass());
    // saveClinoReading(protoShot->mutable_clinoreading(), shot.clino());
    // saveClinoReading(protoShot->mutable_backclinoreading(), shot.backClino());

    //Version 5
    // protoShot->set_distance(shot.distance());
    // protoShot->set_compass(shot.compass());
    // protoShot->set_backcompass(shot.backCompass());
    // protoShot->set_clino(shot.clino());
    // protoShot->set_backclino(shot.backClino());
    // protoShot->set_distancestate((CavewhereProto::DistanceStates_State)shot.distanceState());
    // protoShot->set_compassstate((CavewhereProto::CompassStates_State)shot.compassState());
    // protoShot->set_backcompassstate((CavewhereProto::CompassStates_State)shot.backCompassState());
    // protoShot->set_clinostate((CavewhereProto::ClinoStates_State)shot.clinoState());
    // protoShot->set_backclinostate((CavewhereProto::ClinoStates_State)shot.backClinoState());
}

void cwRegionSaveTask::saveStationShot(CavewhereProto::StationShot *protoStation, const cwStation &station)
{
    saveQUuid(protoStation->mutable_id(), station.id());

    if(!station.name().isEmpty()) {
        saveString(protoStation->mutable_name(), station.name());
    }

    saveReading([&](){return protoStation->mutable_left();}, station.left(), cwDistanceReading::State::Empty);
    saveReading([&](){return protoStation->mutable_right();}, station.right(), cwDistanceReading::State::Empty);
    saveReading([&](){return protoStation->mutable_up();}, station.up(), cwDistanceReading::State::Empty);
    saveReading([&](){return protoStation->mutable_down();}, station.down(), cwDistanceReading::State::Empty);
}

void cwRegionSaveTask::saveStationShot(CavewhereProto::StationShot *protoShot, const cwShot &shot)
{
    saveQUuid(protoShot->mutable_id(), shot.id());

    if(!shot.isDistanceIncluded()) {
        //By default distance is included, only write it if it's not include
        protoShot->set_includedistance(shot.isDistanceIncluded());
    }

    saveReading([&](){ return protoShot->mutable_distance(); }, shot.distance(), cwDistanceReading::State::Empty);
    saveReading([&](){ return protoShot->mutable_compass(); }, shot.compass(), cwCompassReading::State::Empty);
    saveReading([&](){ return protoShot->mutable_backcompass(); }, shot.backCompass(), cwCompassReading::State::Empty);
    saveReading([&](){ return protoShot->mutable_clino(); }, shot.clino(), cwClinoReading::State::Empty);
    saveReading([&](){ return protoShot->mutable_backclino(); }, shot.backClino(), cwClinoReading::State::Empty);
}

/**
 * @brief cwRegionSaveTask::saveCavingRegion
 * @param region
 */
void cwRegionSaveTask::saveCavingRegion(CavewhereProto::CavingRegion &protoRegion, cwCavingRegion* region)
{
    foreach(cwCave* cave, region->caves()) {
        CavewhereProto::Cave* protoCave = protoRegion.add_caves();
        saveCave(protoCave, cave);
    }

    protoRegion.set_version(protoVersion());
    saveString(protoRegion.mutable_cavewhereversion(), CavewhereVersion);
}

/**
 * @brief cwRegionSaveTask::saveStationLookup
 * @param positionLookup
 * @param stationLookup
 *
 * Saves the station position lookup
 */
void cwRegionSaveTask::saveStationLookup(CavewhereProto::StationPositionLookup *positionLookup,
                                         const cwStationPositionLookup &stationLookup)
{
    QMap<QString, QVector3D> positions = stationLookup.positions();
    QMapIterator<QString, QVector3D> iter(positions);
    while(iter.hasNext()) {
        iter.next();

        CavewhereProto::StationPositionLookup_NamePosition* namePosition = positionLookup->add_stationpositions();
        saveString(namePosition->mutable_stationname(), iter.key());
        saveVector3D(namePosition->mutable_position(), iter.value());
    }
}

/**
 * @brief cwRegionSaveTask::saveLead
 * @param protoLead
 * @param lead
 *
 * Saves the lead to disk
 */
void cwRegionSaveTask::saveLead(CavewhereProto::Lead *protoLead, const cwLead &lead)
{
    savePointF(protoLead->mutable_positiononnote(), lead.positionOnNote());
    saveString(protoLead->mutable_description(), lead.desciption());
    saveSizeF(protoLead->mutable_size(), lead.size());
    protoLead->set_completed(lead.completed());
}

void cwRegionSaveTask::saveSurveyNetwork(CavewhereProto::SurveyNetwork *protoSurveyNetwork, const cwSurveyNetwork &network)
{
    auto stations = network.stations();
    std::sort(stations.begin(), stations.end());
    for(auto station : stations) {
        auto neighbors = network.neighbors(station);
        std::sort(neighbors.begin(), neighbors.end());

        auto newStationItem = protoSurveyNetwork->add_stations();
        saveString(newStationItem->mutable_stationname(), station);
        saveStringList(newStationItem->mutable_neighbors(), neighbors);
    }
}

void cwRegionSaveTask::saveProjectedScrapViewMatrix(CavewhereProto::ProjectedProfileScrapViewMatrix *protoViewMatrix, cwProjectedProfileScrapViewMatrix *viewMatrix)
{
    protoViewMatrix->set_azimuth(viewMatrix->azimuth());
    protoViewMatrix->set_direction(static_cast<CavewhereProto::ProjectedProfileScrapViewMatrix::Direction >(viewMatrix->direction()));
}

// void cwRegionSaveTask::saveDistanceReading(CavewhereProto::DistanceReading *protoDistanceReading, const cwDistanceReading &reading)
// {
//     saveString(protoDistanceReading->mutable_legacy_value(), reading.value());
// }

// void cwRegionSaveTask::saveCompassReading(CavewhereProto::CompassReading *protoCompassReading, const cwCompassReading &reading)
// {
//     saveString(protoCompassReading->mutable_legacy_value(), reading.value());
// }

// void cwRegionSaveTask::saveClinoReading(CavewhereProto::ClinoReading *protoClinoReading, const cwClinoReading &reading)
// {
//     saveString(protoClinoReading->mutable_legacy_value(), reading.value());
// }

