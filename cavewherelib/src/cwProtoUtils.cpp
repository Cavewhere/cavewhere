#include "cwProtoUtils.h"

//Our includes
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwNoteLiDARTransformation.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwLength.h"
#include "cwLead.h"
#include "cwTeam.h"
#include "cwScrap.h"
#include "cwImage.h"
#include "cwImageResolution.h"
#include "cwUnits.h"
#include "cwReading.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwProjectedProfileScrapViewMatrix.h"

//Google protobuffer
#include "cavewhere.pb.h"
#include "qt.pb.h"

//Qt includes
#include <QFileInfo>

namespace {

QString fromLegacyQtStringImpl(const QtProto::QString& protoString)
{
    const std::string& stringData = protoString.stringdata();
    return QString::fromUtf8(stringData.c_str(), static_cast<int>(stringData.length()));
}

template<typename StringFunc, typename State>
void saveReading(StringFunc getProtoString, const cwReading& reading, State emptyState) {
    if(reading.state() != static_cast<int>(emptyState)) {
        *getProtoString() = reading.value().toUtf8().toStdString();
    }
}

} // anonymous namespace

namespace cwProtoUtils {

QUuid toUuid(const std::string& uuidStr)
{
    return QUuid::fromString(QString::fromStdString(uuidStr));
}

QString fromLegacyQtString(const QtProto::QString& protoString)
{
    return fromLegacyQtStringImpl(protoString);
}

QStringList fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string>& protoStringList)
{
    QStringList stringList;

    if(!protoStringList.empty()) {
        stringList.reserve(protoStringList.size());

        for (const auto& str : protoStringList) {
            stringList.append(QString::fromStdString(str));
        }
    }

    return stringList;
}

QDate loadDate(const QtProto::QDate& protoDate)
{
    return QDate(protoDate.year(), protoDate.month(), protoDate.day());
}

QSize loadSize(const QtProto::QSize& protoSize)
{
    QSize size;
    size.setWidth(protoSize.width());
    size.setHeight(protoSize.height());
    return size;
}

QSizeF loadSizeF(const QtProto::QSizeF& protoSize)
{
    QSizeF size;
    size.setWidth(protoSize.width());
    size.setHeight(protoSize.height());
    return size;
}

QPointF loadPointF(const QtProto::QPointF& protoPointF)
{
    return QPointF(protoPointF.x(), protoPointF.y());
}

QVector3D loadVector3D(const QtProto::QVector3D& protoVector3D)
{
    return QVector3D(protoVector3D.x(), protoVector3D.y(), protoVector3D.z());
}

QVector2D loadVector2D(const QtProto::QVector2D& protoVector2D)
{
    return QVector2D(protoVector2D.x(), protoVector2D.y());
}

void saveString(std::string* protoString, const QString& string)
{
    *protoString = string.toUtf8().toStdString();
}

void saveDate(QtProto::QDate* protoDate, QDate date)
{
    protoDate->set_day(date.day());
    protoDate->set_month(date.month());
    protoDate->set_year(date.year());
}

void saveSize(QtProto::QSize* protoSize, QSize size)
{
    protoSize->set_width(size.width());
    protoSize->set_height(size.height());
}

void saveSizeF(QtProto::QSizeF* protoSize, QSizeF size)
{
    protoSize->set_width(size.width());
    protoSize->set_height(size.height());
}

void savePointF(QtProto::QPointF* protoPointF, QPointF point)
{
    protoPointF->set_x(point.x());
    protoPointF->set_y(point.y());
}

void saveVector3D(QtProto::QVector3D* protoVector3D, QVector3D vector3D)
{
    protoVector3D->set_x(vector3D.x());
    protoVector3D->set_y(vector3D.y());
    protoVector3D->set_z(vector3D.z());
}

void saveQUuid(std::string* protoString, const QUuid& id)
{
    saveString(protoString, id.toString(QUuid::WithoutBraces));
}

void saveStringList(google::protobuf::RepeatedPtrField<std::string>* protoStringList, const QStringList& stringList)
{
    for(const auto& string : stringList) {
        protoStringList->Add(string.toUtf8().toStdString());
    }
}

void saveLength(CavewhereProto::Length* protoLength, cwLength* length)
{
    protoLength->set_value(length->value());
    protoLength->set_unit((CavewhereProto::Units_LengthUnit)length->unit());
}

void saveLength(CavewhereProto::Length* protoLength, const cwLength::Data& data)
{
    protoLength->set_value(data.value);
    protoLength->set_unit(static_cast<CavewhereProto::Units_LengthUnit>(data.unit));
}

void saveImageResolution(CavewhereProto::ImageResolution* protoImageRes, cwImageResolution* imageResolution)
{
    protoImageRes->set_value(imageResolution->value());
    protoImageRes->set_unit((CavewhereProto::Units_ImageResolutionUnit)imageResolution->unit());
}

void saveImage(CavewhereProto::Image* protoImage, const cwImage& image)
{
    Q_ASSERT(image.mode() == cwImage::Mode::Path);

    saveString(protoImage->mutable_path(), image.path());

    protoImage->set_dotpermeter(image.originalDotsPerMeter());
    saveSize(protoImage->mutable_size(), image.originalSize());
    if (image.page() >= 0) {
        protoImage->set_page(image.page());
    }
    if (image.unit() != cwImage::Unit::Pixels) {
        protoImage->set_imageunit(
                    static_cast<CavewhereProto::Image_Unit>(static_cast<int>(image.unit())));
    }
}

void saveNoteStation(CavewhereProto::NoteStation* protoNoteStation, const cwNoteStation& noteStation)
{
    saveString(protoNoteStation->mutable_name(), noteStation.name());
    savePointF(protoNoteStation->mutable_positiononnote(), noteStation.positionOnNote());
    if (!noteStation.id().isNull()) {
        saveQUuid(protoNoteStation->mutable_id(), noteStation.id());
    }
}

void saveTeamMember(CavewhereProto::TeamMember* protoTeamMember, const cwTeamMember& teamMember)
{
    saveQUuid(protoTeamMember->mutable_id(), teamMember.id());
    saveString(protoTeamMember->mutable_name(), teamMember.name());
    saveStringList(protoTeamMember->mutable_jobs(), teamMember.jobs());
}

void saveLead(CavewhereProto::Lead* protoLead, const cwLead& lead)
{
    savePointF(protoLead->mutable_positiononnote(), lead.positionOnNote());

    if(!lead.desciption().isEmpty()) {
        saveString(protoLead->mutable_description(), lead.desciption());
    }

    if(lead.size().isValid()) {
        saveSizeF(protoLead->mutable_size(), lead.size());
    }

    protoLead->set_completed(lead.completed());
    if (!lead.id().isNull()) {
        saveQUuid(protoLead->mutable_id(), lead.id());
    }
}

void saveProjectedScrapViewMatrix(CavewhereProto::ProjectedProfileScrapViewMatrix* protoViewMatrix, cwProjectedProfileScrapViewMatrix* viewMatrix)
{
    protoViewMatrix->set_azimuth(viewMatrix->azimuth());
    protoViewMatrix->set_direction(static_cast<CavewhereProto::ProjectedProfileScrapViewMatrix::Direction>(viewMatrix->direction()));
}

void saveNoteTranformation(CavewhereProto::NoteTransformation* protoNoteTransformation,
                           cwAbstractNoteTransformation* noteTransformation)
{
    protoNoteTransformation->set_northup(noteTransformation->northUp());
    saveLength(protoNoteTransformation->mutable_scalenumerator(),
               noteTransformation->scaleNumerator());
    saveLength(protoNoteTransformation->mutable_scaledenominator(),
               noteTransformation->scaleDenominator());
}

void saveStationShot(CavewhereProto::StationShot* protoStation, const cwStation& station)
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

void saveStationShot(CavewhereProto::StationShot* protoShot, const cwShot& shot)
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

void saveTripCalibration(CavewhereProto::TripCalibration* proto, cwTripCalibration* tripCalibration)
{
    proto->set_correctedcompassbacksight(tripCalibration->hasCorrectedCompassBacksight());
    proto->set_correctedclinobacksight(tripCalibration->hasCorrectedClinoBacksight());
    proto->set_tapecalibration(tripCalibration->tapeCalibration());
    proto->set_frontcompasscalibration(tripCalibration->frontCompassCalibration());
    proto->set_frontclinocalibration(tripCalibration->frontClinoCalibration());
    proto->set_backcompasscalibration(tripCalibration->backCompassCalibration());
    proto->set_backclinocalibration(tripCalibration->backClinoCalibration());
    proto->set_declination(tripCalibration->declination());
    proto->set_distanceunit((CavewhereProto::Units_LengthUnit)tripCalibration->distanceUnit());
    proto->set_frontsights(tripCalibration->hasFrontSights());
    proto->set_backsights(tripCalibration->hasBackSights());
    proto->set_correctedcompassfrontsight(tripCalibration->hasCorrectedCompassFrontsight());
    proto->set_correctedclinofrontsight(tripCalibration->hasCorrectedClinoFrontsight());
}

void saveSurveyChunk(CavewhereProto::SurveyChunk* protoChunk, cwSurveyChunk* chunk)
{
    Q_ASSERT(chunk->stationCount() - 1 == chunk->shotCount() || chunk->isStationAndShotsEmpty());

    saveQUuid(protoChunk->mutable_id(), chunk->id());

    for(int i = 0; i < chunk->stationCount(); ++i) {
        auto station = protoChunk->add_leg();
        saveStationShot(station, chunk->station(i));

        if(i < chunk->shotCount()) {
            auto shot = protoChunk->add_leg();
            saveStationShot(shot, chunk->shot(i));
        }
    }
}

void saveTeam(CavewhereProto::Team* protoTeam, cwTeam* team)
{
    foreach(const cwTeamMember& teamMember, team->teamMembers()) {
        CavewhereProto::TeamMember* protoTeamMember = protoTeam->add_teammembers();
        saveTeamMember(protoTeamMember, teamMember);
    }
}

void saveScrap(CavewhereProto::Scrap* protoScrap, cwScrap* scrap)
{
    saveQUuid(protoScrap->mutable_id(), scrap->id());

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

    protoScrap->set_calculatenotetransform(scrap->calculateNoteTransform());
    if(!scrap->calculateNoteTransform()) {
        saveNoteTranformation(protoScrap->mutable_notetransformation(), scrap->noteTransformation());
    }
    protoScrap->set_type(static_cast<CavewhereProto::Scrap_ScrapType>(scrap->type()));

    if(scrap->type() == cwScrap::ProjectedProfile && !scrap->calculateNoteTransform()) {
        saveProjectedScrapViewMatrix(protoScrap->mutable_profileviewmatrix(), static_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()));
    }
}

void saveNoteLiDARTranformation(CavewhereProto::NoteLiDARTransformation* protoNoteTransformation, cwNoteLiDARTransformation* noteTransformation)
{
    if (!protoNoteTransformation || !noteTransformation) { return; }

    //Save the base class
    saveNoteTranformation(protoNoteTransformation->mutable_plantransform(), noteTransformation);

    // upMode (enum)
    // The enum values appear to align (Custom=0, XisUp=1, YisUp=2, ZisUp=3). If they differ,
    // add a mapping switch here instead of static_cast.
    protoNoteTransformation->set_upmode(
                static_cast<CavewhereProto::NoteLiDARTransformation_UpMode>(noteTransformation->upMode())
                );

    // upSign (float)
    protoNoteTransformation->set_upsign(noteTransformation->upSign());

    // upCustom (QtProto::QQuaternion) — only meaningful when mode is Custom; safe to always write.
    if(noteTransformation->upMode() == cwNoteLiDARTransformation::UpMode::Custom) {
        saveQQuaternion(protoNoteTransformation->mutable_upcustom(), noteTransformation->upCustom());
    }
}

void saveQQuaternion(QtProto::QQuaternion* protoQuaternion, const QQuaternion& quaternion)
{
    if (!protoQuaternion) { return; }

    protoQuaternion->set_x(quaternion.x());
    protoQuaternion->set_y(quaternion.y());
    protoQuaternion->set_z(quaternion.z());
    protoQuaternion->set_scalar(quaternion.scalar());
}

cwTripCalibrationData fromProtoTripCalibration(const CavewhereProto::TripCalibration& proto)
{
    cwTripCalibrationData tripCalibration;
    tripCalibration.setCorrectedCompassBacksight(proto.correctedcompassbacksight());
    tripCalibration.setCorrectedClinoBacksight(proto.correctedclinobacksight());
    tripCalibration.setCorrectedCompassFrontsight(proto.correctedcompassfrontsight());
    tripCalibration.setCorrectedClinoFrontsight(proto.correctedclinofrontsight());
    tripCalibration.setTapeCalibration(proto.tapecalibration());
    tripCalibration.setFrontCompassCalibration(proto.frontcompasscalibration());
    tripCalibration.setFrontClinoCalibration(proto.frontclinocalibration());
    tripCalibration.setBackCompassCalibration(
                proto.has_backcompasscalibration()
                ? proto.backcompasscalibration()
                : proto.legacy_backcompassscalibration());
    tripCalibration.setBackClinoCalibration(proto.backclinocalibration());
    tripCalibration.setDeclination(proto.declination());
    tripCalibration.setDistanceUnit((cwUnits::LengthUnit)proto.distanceunit());
    tripCalibration.setFrontSights(proto.has_frontsights() ? proto.frontsights() : true);
    tripCalibration.setBackSights(proto.has_backsights() ? proto.backsights() : false);
    return tripCalibration;
}

cwTeamData fromProtoTeam(const CavewhereProto::Team& proto)
{
    QList<cwTeamMember> members;
    members.reserve(proto.teammembers_size());
    for(int i = 0; i < proto.teammembers_size(); i++) {
        cwTeamMember member = fromProtoTeamMember(proto.teammembers(i));
        members.append(member);
    }
    return {
        members
    };
}

cwTeamMember fromProtoTeamMember(const CavewhereProto::TeamMember& proto)
{
    cwTeamMember member;
    auto id = toUuid(proto.id());
    if(!id.isNull()) {
        member.setId(id);
    }
    member.setJobs(fromProtoStringList(proto.jobs()));
    member.setName(QString::fromStdString(proto.name()));
    return member;
}

cwSurveyChunkData fromProtoSurveyChunk(const CavewhereProto::SurveyChunk& protoChunk)
{
    cwSurveyChunkData chunkData;
    chunkData.id = toUuid(protoChunk.id());

    const int legCount = protoChunk.leg_size();
    if (legCount > 0 && legCount % 2 == 0) {
        qWarning() << "Malformed SurveyChunk: even leg count" << legCount
                   << "(expected odd: station, shot, station, shot, ..., station)";
        return chunkData;
    }

    for (int i = 0; i < legCount; i += 2) {
        const auto& protoStation = protoChunk.leg(i);
        chunkData.stations.append(fromProtoStation(protoStation));

        if(i + 1 < legCount) {
            const auto& protoShot = protoChunk.leg(i + 1);
            chunkData.shots.append(fromProtoShot(protoShot));
        }
    }

    return chunkData;
}

cwStation fromProtoStation(const CavewhereProto::StationShot& protoStation)
{
    cwStation station;

    station.setId(toUuid(protoStation.id()));

    if (protoStation.has_name()) {
        station.setName(QString::fromStdString(protoStation.name()));
    }

    if (protoStation.has_left()) {
        station.setLeft(QString::fromStdString(protoStation.left()));
    }

    if (protoStation.has_right()) {
        station.setRight(QString::fromStdString(protoStation.right()));
    }

    if (protoStation.has_up()) {
        station.setUp(QString::fromStdString(protoStation.up()));
    }

    if (protoStation.has_down()) {
        station.setDown(QString::fromStdString(protoStation.down()));
    }

    return station;
}

cwShot fromProtoShot(const CavewhereProto::StationShot& protoShot)
{
    cwShot shot;

    shot.setId(toUuid(protoShot.id()));

    if (protoShot.has_includedistance()) {
        shot.setDistanceIncluded(protoShot.includedistance());
    }

    if (protoShot.has_distance()) {
        shot.setDistance(QString::fromStdString(protoShot.distance()));
    }

    if (protoShot.has_compass()) {
        shot.setCompass(QString::fromStdString(protoShot.compass()));
    }

    if (protoShot.has_backcompass()) {
        shot.setBackCompass(QString::fromStdString(protoShot.backcompass()));
    }

    if (protoShot.has_clino()) {
        shot.setClino(QString::fromStdString(protoShot.clino()));
    }

    if (protoShot.has_backclino()) {
        shot.setBackClino(QString::fromStdString(protoShot.backclino()));
    }

    return shot;
}

cwScrapData fromProtoScrap(const CavewhereProto::Scrap& protoScrap)
{
    cwScrapData scrapData;

    if(protoScrap.has_id()) {
        scrapData.id = toUuid(protoScrap.id());
    }

    // Load outline points
    for (const QtProto::QPointF& protoPoint : protoScrap.outlinepoints()) {
        scrapData.outlinePoints.append(loadPointF(protoPoint));
    }

    // Load stations
    for (const CavewhereProto::NoteStation& protoStation : protoScrap.notestations()) {
        scrapData.stations.append(fromProtoNoteStation(protoStation));
    }

    // Load leads
    for (const CavewhereProto::Lead& protoLead : protoScrap.leads()) {
        scrapData.leads.append(fromProtoLead(protoLead));
    }

    // Load calculate note transform flag
    scrapData.calculateNoteTransform = protoScrap.calculatenotetransform();

    // Load note transformation only for manually-controlled scraps
    if (!scrapData.calculateNoteTransform && protoScrap.has_notetransformation()) {
        scrapData.noteTransformation = fromProtoNoteTransformation(protoScrap.notetransformation());
    }

    //Generate the correct scrap type
    if(protoScrap.has_type()) {
        switch(protoScrap.type()) {
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_Plan:
            scrapData.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();
            break;
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_RunningProfile:
            scrapData.viewMatrix = std::make_unique<cwRunningProfileScrapViewMatrix::Data>();
            break;
        case CavewhereProto::Scrap::ScrapType::Scrap_ScrapType_ProjectedProfile:
            // Load view matrix
            if (!scrapData.calculateNoteTransform && protoScrap.has_profileviewmatrix()) {
                scrapData.viewMatrix = fromProtoProjectedScraptViewMatrix(protoScrap.profileviewmatrix());
            } else {
                scrapData.viewMatrix = std::make_unique<cwProjectedProfileScrapViewMatrix::Data>();
            }
            break;
        default:
            scrapData.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();
            break;
        }
    } else {
        scrapData.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();
    }

    return scrapData;
}

cwNoteStation fromProtoNoteStation(const CavewhereProto::NoteStation& protoNoteStation)
{
    cwNoteStation noteStation;
    if (protoNoteStation.has_name()) {
        noteStation.setName(QString::fromStdString(protoNoteStation.name()));
    } else if (protoNoteStation.has_legacy_name()) {
        noteStation.setName(fromLegacyQtStringImpl(protoNoteStation.legacy_name()));
    }
    noteStation.setPositionOnNote(loadPointF(protoNoteStation.positiononnote()));
    if (protoNoteStation.has_id()) {
        noteStation.setId(toUuid(protoNoteStation.id()));
    } else {
        noteStation.setId(QUuid());
    }
    return noteStation;
}

cwLead fromProtoLead(const CavewhereProto::Lead& protoLead)
{
    cwLead lead;

    // Load position on note
    lead.setPositionOnNote(loadPointF(protoLead.positiononnote()));

    // Load description if present
    if (protoLead.has_description()) {
        lead.setDescription(QString::fromStdString(protoLead.description()));
    } else if (protoLead.has_legacy_description()) {
        lead.setDescription(fromLegacyQtStringImpl(protoLead.legacy_description()));
    }

    // Load size if present and valid
    if (protoLead.has_size()) {
        QSizeF size = loadSizeF(protoLead.size());
        if (size.isValid()) {
            lead.setSize(size);
        }
    }

    // Load completed flag
    lead.setCompleted(protoLead.completed());
    if (protoLead.has_id()) {
        lead.setId(toUuid(protoLead.id()));
    } else {
        lead.setId(QUuid());
    }

    return lead;
}

cwNoteTransformationData fromProtoNoteTransformation(const CavewhereProto::NoteTransformation& protoNoteTransform)
{
    cwNoteTransformationData data;

    data.north = protoNoteTransform.northup();

    if (protoNoteTransform.has_scalenumerator()) {
        data.scale.scaleNumerator = fromProtoLength(protoNoteTransform.scalenumerator());
    }

    if (protoNoteTransform.has_scaledenominator()) {
        data.scale.scaleDenominator = fromProtoLength(protoNoteTransform.scaledenominator());
    }

    return data;
}

cwNoteLiDARTransformationData fromProtoLiDARNoteTransformation(const CavewhereProto::NoteLiDARTransformation& protoNoteTransform)
{
    cwNoteLiDARTransformationData data;

    if(protoNoteTransform.has_plantransform()) {
        //Do a slice
        cwNoteTransformationData& base = data;
        base = fromProtoNoteTransformation(protoNoteTransform.plantransform());
    }

    if(protoNoteTransform.has_upsign()) {
        data.upSign = protoNoteTransform.upsign();
    }

    if(protoNoteTransform.has_upmode()) {
        data.upMode = static_cast<cwNoteLiDARTransformationData::UpMode>(protoNoteTransform.upmode());

        if(data.upMode == cwNoteLiDARTransformationData::UpMode::Custom
                && protoNoteTransform.has_upcustom()) {
            data.upRotation = fromProtoQuaternion(protoNoteTransform.upcustom());
        }
    }

    return data;
}

std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data> fromProtoProjectedScraptViewMatrix(const CavewhereProto::ProjectedProfileScrapViewMatrix protoViewMatrix)
{
    auto matrix = std::make_unique<cwProjectedProfileScrapViewMatrix::Data>();
    matrix->setAzimuth(protoViewMatrix.azimuth());
    matrix->setDirection(static_cast<cwProjectedProfileScrapViewMatrix::AzimuthDirection>(protoViewMatrix.direction()));
    return matrix;
}

cwImageResolution::Data fromProtoImageResolution(const CavewhereProto::ImageResolution& protoImageResolution)
{
    cwImageResolution::Data resolution;
    resolution.value = protoImageResolution.value();
    resolution.unit = static_cast<cwUnits::ImageResolutionUnit>(protoImageResolution.unit());
    return resolution;
}

QQuaternion fromProtoQuaternion(const QtProto::QQuaternion& protoQuaternion)
{
    return QQuaternion(protoQuaternion.scalar(), protoQuaternion.x(), protoQuaternion.y(), protoQuaternion.z());
}

cwLength::Data fromProtoLength(const CavewhereProto::Length& protoLength)
{
    return {
        protoLength.unit(),
                protoLength.value()
    };
}

void savePenPoint(CavewhereProto::PenPoint* protoPoint, const cwPenPoint& point)
{
    savePointF(protoPoint->mutable_position(), point.position);
    protoPoint->set_pressure(point.pressure);
    protoPoint->set_timestampms(point.timestampMs);
}

void savePenStroke(CavewhereProto::PenStroke* protoStroke, const cwPenStroke& stroke)
{
    protoStroke->set_kind(static_cast<CavewhereProto::PenStroke_Kind>(stroke.kind));
    protoStroke->set_width(stroke.width);
    if (stroke.color.isValid()) {
        saveString(protoStroke->mutable_colorhex(), stroke.color.name(QColor::HexArgb));
    }
    for (const auto& point : stroke.points) {
        savePenPoint(protoStroke->add_points(), point);
    }
    if (!stroke.id.isNull()) {
        saveQUuid(protoStroke->mutable_id(), stroke.id);
    }
}

void saveScale(CavewhereProto::Scale* protoScale, const cwScale::Data& scale)
{
    saveLength(protoScale->mutable_scalenumerator(), scale.scaleNumerator);
    saveLength(protoScale->mutable_scaledenominator(), scale.scaleDenominator);
}

void saveSketch(CavewhereProto::Sketch* protoSketch, const cwSketchData& data)
{
    if (!data.id.isNull()) {
        saveQUuid(protoSketch->mutable_id(), data.id);
    }
    saveString(protoSketch->mutable_name(), data.name);
    protoSketch->set_viewtype(static_cast<CavewhereProto::Sketch_ViewType>(data.viewType));
    saveScale(protoSketch->mutable_mapscale(), data.mapScale);
    for (const auto& stroke : data.strokes) {
        savePenStroke(protoSketch->add_strokes(), stroke);
    }
}

cwPenPoint fromProtoPenPoint(const CavewhereProto::PenPoint& protoPoint)
{
    cwPenPoint point;
    point.position    = loadPointF(protoPoint.position());
    point.pressure    = protoPoint.has_pressure() ? protoPoint.pressure() : -1.0;
    point.timestampMs = protoPoint.timestampms();
    return point;
}

cwPenStroke fromProtoPenStroke(const CavewhereProto::PenStroke& protoStroke)
{
    cwPenStroke stroke;
    stroke.kind  = static_cast<cwPenStroke::Kind>(protoStroke.kind());
    stroke.width = protoStroke.width();
    if (protoStroke.has_colorhex()) {
        stroke.color = QColor(QString::fromStdString(protoStroke.colorhex()));
    }
    stroke.points.reserve(protoStroke.points_size());
    for (const auto& protoPoint : protoStroke.points()) {
        stroke.points.append(fromProtoPenPoint(protoPoint));
    }
    if (protoStroke.has_id()) {
        stroke.id = toUuid(protoStroke.id());
    }
    return stroke;
}

cwScale::Data fromProtoScale(const CavewhereProto::Scale& protoScale)
{
    cwScale::Data data;
    data.scaleNumerator   = fromProtoLength(protoScale.scalenumerator());
    data.scaleDenominator = fromProtoLength(protoScale.scaledenominator());
    return data;
}

cwSketchData fromProtoSketch(const CavewhereProto::Sketch& protoSketch)
{
    cwSketchData data;
    if (protoSketch.has_id()) {
        data.id = toUuid(protoSketch.id());
    }
    data.name = QString::fromStdString(protoSketch.name());

    const auto vt = protoSketch.viewtype();
    if (vt == CavewhereProto::Sketch_ViewType_Plan) {
        data.viewType = cwSketchData::Plan;
    } else {
        qWarning("cwSketch: view type %d not supported yet; falling back to Plan", int(vt));
        data.viewType = cwSketchData::Plan;
    }

    if (protoSketch.has_mapscale()) {
        data.mapScale = fromProtoScale(protoSketch.mapscale());
    }

    data.strokes.reserve(protoSketch.strokes_size());
    for (const auto& protoStroke : protoSketch.strokes()) {
        data.strokes.append(fromProtoPenStroke(protoStroke));
    }

    return data;
}

} // namespace cwProtoUtils
