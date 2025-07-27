#ifndef CWSAVELOAD_H
#define CWSAVELOAD_H

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

//Qt includes
#include <QDir>
#include <QFuture>
#include <QHash>


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

    void saveCave(const QDir& dir, const cwCave* cave);
    static std::unique_ptr<CavewhereProto::Cave> toProtoCave(const cwCave* cave);

    void saveTrip(const QDir& dir, const cwTrip* trip);
    static std::unique_ptr<CavewhereProto::Trip> toProtoTrip(const cwTrip* trip);

    void saveNote(const QDir& dir, const cwNote* note);
    static std::unique_ptr<CavewhereProto::Note> toProtoNote(const cwNote* note);

    void saveAllFromV6(const QDir& dir, const cwProject* region);

    void loadCavingRegion(const QString& filename);


    //For testing
    void waitForFinished();

    static QString sanitizeFileName(QString input);

    void saveProtoMessage(
        const QDir& dir,
        const QString& filename,
        std::unique_ptr<const google::protobuf::Message> message);


private:
    struct Data;
    std::unique_ptr<Data> d;



};


#endif // CWSAVELOAD_H
