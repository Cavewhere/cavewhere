#ifndef CWSAVELOAD_H
#define CWSAVELOAD_H

//Our includes
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwTripCalibration;
class cwSurveyChunk;
class cwTeam;
class cwNote;
class cwImage;
class cwScrap;
class cwImageResolution;
class cwNoteStation;
class cwNoteTranformation;
class cwTriangulatedData;
class cwLength;
class cwTeamMember;
class cwStation;
class cwShot;
class cwStationPositionLookup;
class cwLead;
class cwSurveyNetwork;
class cwProjectedProfileScrapViewMatrix;
class cwDistanceReading;
class cwClinoReading;
class cwCompassReading;
class cwCavingRegion;
class cwProject;
#include "cwCavingRegionData.h"
#include "cwProjectedProfileScrapViewMatrix.h"

//Google protobuffer
namespace CavewhereProto {
class CavingRegion;
class Cave;
class Trip;
class SurveyNoteModel;
class TripCalibration;
class SurveyChunk;
class Team;
class Note;
class Image;
class Scrap;
class ImageResolution;
class NoteStation;
class NoteTranformation;
class TriangulatedData;
class Length;
class TeamMember;
class Station;
class Shot;
class StationPositionLookup;
class Lead;
class SurveyNetwork;
class ProjectedProfileScrapViewMatrix;
class DistanceReading;
class CompassReading;
class ClinoReading;
};

namespace QtProto {
class QString;
class QDate;
class QSizeF;
class QSize;
class QPointF;
class QVector3D;
class QVector2D;
class QStringList;
};

namespace google::protobuf {
class Message;
}

//Monad includes
#include <Monad/Result.h>

//Qt includes
#include <QDir>
#include <QFuture>
#include <QHash>

//Proto includes
#include "cavewhere.pb.h"

class cwSaveLoad : public QObject
{
    Q_OBJECT

public:
    cwSaveLoad(QObject* parent = nullptr);
    ~cwSaveLoad();

    void setRootDir(const QDir& rootDir);
    QDir rootDir() const;

    //Save when ever data has changed (remove save / load?)

    //Base folder
    void save(const cwCavingRegion* region);

    void saveCavingRegion(const QDir& dir, const cwCavingRegion* region);
    static std::unique_ptr<CavewhereProto::CavingRegion> toProtoCavingRegion(const cwCavingRegion* region);
    static QString regionFileName(const QDir& dir, const cwCavingRegion* region);

    void saveCave(const QDir& dir, const cwCave* cave);
    static std::unique_ptr<CavewhereProto::Cave> toProtoCave(const cwCave* cave);

    void saveTrip(const QDir& dir, const cwTrip* trip);
    static std::unique_ptr<CavewhereProto::Trip> toProtoTrip(const cwTrip* trip);

    void saveNote(const QDir& dir, const cwNote* note);
    static std::unique_ptr<CavewhereProto::Note> toProtoNote(const cwNote* note);

    QString saveAllFromV6(const QDir& dir, const cwProject* region);

    static Monad::Result<cwCavingRegionData> loadAll(const QString& filename);

    static Monad::Result<cwCavingRegionData> loadCavingRegion(const QString& filename);
    static Monad::Result<cwCaveData> loadCave(const QString& filename);
    static Monad::Result<cwTripData> loadTrip(const QString& filename);
    static Monad::Result<cwNoteData> loadNote(const QString& filename);

    static cwTripCalibrationData fromProtoTripCalibration(const CavewhereProto::TripCalibration& proto);
    static cwTeamData fromProtoTeam(const CavewhereProto::Team& proto);
    static cwTeamMember fromProtoTeamMember(const CavewhereProto::TeamMember& proto);
    static QList<cwSurveyChunkData> fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> & protoList);
    static cwSurveyChunkData fromProtoSurveyChunk(const CavewhereProto::SurveyChunk& protoChunk);
    static cwStation fromProtoStation(const CavewhereProto::StationShot& protoStation);
    static cwShot fromProtoShot(const CavewhereProto::StationShot& protoShot);
    static cwScrapData fromProtoScrap(const CavewhereProto::Scrap& protoScrap);
    static cwNoteStation fromProtoNoteStation(const CavewhereProto::NoteStation& protoNoteStation);
    static cwLead fromProtoLead(const CavewhereProto::Lead& protoLead);
    static cwNoteTransformationData fromProtoNoteTransformation(const CavewhereProto::NoteTranformation& protoNoteTransform);
    static cwLength::Data fromProtoLength(const CavewhereProto::Length& protoLength);
    static std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data> fromProtoProjectedScraptViewMatrix(const CavewhereProto::ProjectedProfileScrapViewMatrix protoViewMatrix);
    static cwImageResolution::Data fromProtoImageResolution(const CavewhereProto::ImageResolution& protoImageResolution);

    //For testing
    void waitForFinished();

    static QString sanitizeFileName(QString input);
    static QUuid toUuid(const std::string& uuidStr);

    static QStringList fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string> &protoStringList);



    void saveProtoMessage(
        const QDir& dir,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message);


private:
    struct Data;
    std::unique_ptr<Data> d;



};


#endif // CWSAVELOAD_H
