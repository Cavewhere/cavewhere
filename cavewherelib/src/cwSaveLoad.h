#ifndef CWSAVELOAD_H
#define CWSAVELOAD_H

//Our includes
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwSurveyNoteLiDARModel;
class cwTripCalibration;
class cwSurveyChunk;
class cwTeam;
class cwNote;
class cwImage;
class cwScrap;
class cwImageResolution;
class cwNoteStation;
class cwNoteTranformation;
class cwNoteLiDARTransformation;
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
class cwNoteLiDAR;
class cwNoteLiDARData;
#include "cwCavingRegionData.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwFutureManagerToken.h"
#include "CaveWhereLibExport.h"

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
class StationShot;
class NoteLiDAR;
class NoteLiDARTransformation;
class Project;
class ProjectMetadata;
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
class QQuaternion;
};

namespace google::protobuf {
class Message;
}

namespace QQuickGit {
class GitRepository;
}

//Monad includes
#include <Monad/Result.h>

//Qt includes
#include <QDir>
#include <QFuture>
#include <QHash>
#include <QPointer>
#include <QUndoStack>
#include <functional>

class CAVEWHERE_LIB_EXPORT cwSaveLoad : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged);
    Q_PROPERTY(bool isTemporaryProject READ isTemporaryProject NOTIFY isTemporaryProjectChanged)

public:
    enum class GitMode {
        ManagedNew,
        ExistingRepo,
        NoGit
    };
    Q_ENUM(GitMode);

    struct ProjectMetadataData {
        QString dataRoot;
        GitMode gitMode = GitMode::ManagedNew;
        bool syncEnabled = true;
    };

    struct ProjectLoadData {
        cwCavingRegionData region;
        ProjectMetadataData metadata;
    };

    cwSaveLoad(QObject* parent = nullptr);
    ~cwSaveLoad();

    void newProject();

    QString fileName() const;
    void setFileName(const QString& filename, bool initRepository = true);
    QFuture<Monad::ResultBase> load(const QString& filename);

    // void setRootDir(const QDir& rootDir);
    // QDir rootDir() const;

    //Save when ever data has changed (remove save / load?)
    void setCavingRegion(cwCavingRegion *region);
    const cwCavingRegion* cavingRegion() const;


    QQuickGit::GitRepository* repository() const;

    cwFutureManagerToken futureManagerToken() const;
    void setFutureManagerToken(const cwFutureManagerToken& futureManagerToken);
    void setUndoStack(QUndoStack* undoStack);

    QFuture<Monad::ResultString> saveAllFromV6(const QDir& dir, const cwProject* region, const QString& projectFileName);

    static QFuture<Monad::Result<ProjectLoadData>> loadAll(const QString& filename);

    static Monad::Result<cwCavingRegionData> loadCavingRegion(const QString& filename);
    static Monad::Result<ProjectLoadData> loadProject(const QString& filename);

    static QString sanitizeFileName(QString input);

    void saveProtoMessage(
        std::unique_ptr<const google::protobuf::Message> message,
        const void* pointerId);

    static std::unique_ptr<CavewhereProto::Cave> toProtoCave(const cwCave* cave);
    static std::unique_ptr<CavewhereProto::Trip> toProtoTrip(const cwTrip* trip);

    void addImages(QList<QUrl> noteImagePaths,
                   const QDir& dir,
                   std::function<void (QList<cwImage>)> outputCallBackFunc);

    Monad::ResultBase moveProjectTo(const QString& destinationFileUrl);
    Monad::ResultBase copyProjectTo(const QString& destinationFileUrl);
    Monad::ResultBase deleteTemporaryProject();

    void addFiles(QList<QUrl> files,
                  const QDir& dir,
                  std::function<void (QList<QString>)> fileCallBackFunc);


    //Returns the relative path to the project
    static QDir projectDir(const cwProject* project);
    QDir projectDir() const;

    static QString fileName(const cwCavingRegion* region);
    static QString absolutePath(const cwCavingRegion* region);
    static QDir dir(const cwCavingRegion* region);

    static QString fileName(const cwCave* cave);
    static QString absolutePath(const cwCave* cave);
    static QDir dir(const cwCave* cave);

    static QString fileName(const cwTrip* trip);
    static QString absolutePath(const cwTrip* trip);
    static QDir dir(const cwTrip* trip);

    static QDir dir(cwSurveyNoteModel* notes);
    static QDir dir(cwSurveyNoteLiDARModel* notes);

    static QString fileName(const cwNote* note);
    static QString absolutePath(const cwNote* note);
    static QString absolutePath(const cwNote* note, const QString& imageFilename);
    static QDir dir(const cwNote* note);

    static QString fileName(const cwNoteLiDAR* note);
    static QString absolutePath(const cwNoteLiDAR* note);
    static QString absolutePath(const cwNoteLiDAR* note, const QString& lidarFilename);
    static QDir dir(const cwNoteLiDAR* note);

    static cwImage absolutePathNoteImage(const cwNote* note);

    //For testing
    void waitForFinished();

    bool isTemporaryProject() const;

    QString dataRoot() const;
    void setDataRoot(const QString& dataRoot);

    GitMode gitMode() const;
    void setGitMode(GitMode mode);

    bool syncEnabled() const;
    void setSyncEnabled(bool enabled);

    QFuture<void> retire();

signals:
    void fileNameChanged();
    void isTemporaryProjectChanged();

private:
    struct Data;
    friend struct Data;
    std::unique_ptr<Data> d;
    QPointer<QUndoStack> m_undoStack;

    void saveProject(const QDir& dir, const cwCavingRegion* region);
    std::unique_ptr<CavewhereProto::Project> toProtoProject(const cwCavingRegion* region);
    QDir projectRootDir() const;

    QString fileNamePrivate(const cwCave* cave) const;
    QString absolutePathPrivate(const cwCave* cave) const;
    QDir dirPrivate(const cwCave* cave) const;

    QString fileNamePrivate(const cwTrip* trip) const;
    QString absolutePathPrivate(const cwTrip* trip) const;
    QDir dirPrivate(const cwTrip* trip) const;

    QDir dirPrivate(cwSurveyNoteModel* notes) const;
    QDir dirPrivate(cwSurveyNoteLiDARModel* notes) const;

    QString fileNamePrivate(const cwNote* note) const;
    QString absolutePathPrivate(const cwNote* note) const;
    QString absolutePathPrivate(const cwNote* note, const QString& imageFilename) const;
    QDir dirPrivate(const cwNote* note) const;

    QString fileNamePrivate(const cwNoteLiDAR* note) const;
    QString absolutePathPrivate(const cwNoteLiDAR* note) const;
    QString absolutePathPrivate(const cwNoteLiDAR* note, const QString& lidarFilename) const;
    QDir dirPrivate(const cwNoteLiDAR* note) const;

    void setSaveEnabled(bool enabled);

    enum class ProjectTransferMode { Move, Copy };
    Monad::ResultBase transferProjectTo(const QString& destinationFileUrl, ProjectTransferMode mode);

    void disconnectTreeModel();
    void connectTreeModel();

    void disconnectObjects();
    void connectObjects();

    //Save
    void connectCave(cwCave* cave);
    void connectTrip(cwTrip* trip);
    void connectNote(cwNote* note);
    void connectScrap(cwScrap* scrap);
    void connectNoteLiDAR(cwNoteLiDAR * lidarNote);

    void setTemporary(bool isTemp);

    // QString
    QString randomName() const;
    QDir createTemporaryDirectory(const QString& subDirName);

    QFuture<void> completeSaveJobs();

    static QDir caveDirHelper(const QDir& projectDir, const cwCave *cave);
    static QDir tripDirHelper(const QDir& caveDir, const cwTrip* trip);
    static QDir noteDirHelper(const QDir& tripDir);

    static QUuid toUuid(const std::string& uuidStr);



    void saveCavingRegion(const cwCavingRegion* region);
    static std::unique_ptr<CavewhereProto::CavingRegion> toProtoCavingRegion(const cwCavingRegion* region);
    static QString regionFileName(const QDir& dir, const cwCavingRegion* region);

    void save(const cwCave* cave);

    void save(const cwTrip* trip);

    void save(const cwNote* note);
    static std::unique_ptr<CavewhereProto::Note> toProtoNote(const cwNote* note);

    void save(const cwNoteLiDAR* note);
    static std::unique_ptr<CavewhereProto::NoteLiDAR> toProtoNoteLiDAR(const cwNoteLiDAR* note);

    static Monad::Result<cwCaveData> loadCave(const QString& filename);
    static Monad::Result<cwTripData> loadTrip(const QString& filename);
    static Monad::Result<cwNoteData> loadNote(const QString& filename, const QDir &projectDir);
    static Monad::Result<cwNoteLiDARData> loadNoteLiDAR(const QString& filename, const QDir &projectDir);

    static cwTripCalibrationData fromProtoTripCalibration(const CavewhereProto::TripCalibration& proto);
    static cwTeamData fromProtoTeam(const CavewhereProto::Team& proto);
    static cwTeamMember fromProtoTeamMember(const CavewhereProto::TeamMember& proto);
    static cwSurveyChunkData fromProtoSurveyChunk(const CavewhereProto::SurveyChunk& protoChunk);
    static cwStation fromProtoStation(const CavewhereProto::StationShot& protoStation);
    static cwShot fromProtoShot(const CavewhereProto::StationShot& protoShot);
    static cwScrapData fromProtoScrap(const CavewhereProto::Scrap& protoScrap);
    static cwNoteStation fromProtoNoteStation(const CavewhereProto::NoteStation& protoNoteStation);
    static cwLead fromProtoLead(const CavewhereProto::Lead& protoLead);
    static cwNoteTransformationData fromProtoNoteTransformation(const CavewhereProto::NoteTranformation& protoNoteTransform);
    static cwNoteLiDARTransformationData fromProtoLiDARNoteTransformation(const CavewhereProto::NoteLiDARTransformation& protoNoteTransform);
    static cwLength::Data fromProtoLength(const CavewhereProto::Length& protoLength);
    static std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data> fromProtoProjectedScraptViewMatrix(const CavewhereProto::ProjectedProfileScrapViewMatrix protoViewMatrix);
    static cwImageResolution::Data fromProtoImageResolution(const CavewhereProto::ImageResolution& protoImageResolution);
    static QQuaternion fromProtoQuaternion(const QtProto::QQuaternion& protoQuaternion);

    static void saveNoteLiDARTranformation(CavewhereProto::NoteLiDARTransformation *protoNoteTransformation,
                                           cwNoteLiDARTransformation *noteTransformation);
    static void saveQQuaternion(QtProto::QQuaternion* protoQuaternion,
                                const QQuaternion& quaternion);


    template<typename ResultType, typename MakeResultFunc>
    void copyFilesAndEmitResults(const QList<QString>& sourceFilePaths,
                                 const QDir& destinationDirectory,
                                 MakeResultFunc makeResult,
                                 std::function<void (QList<ResultType>)> outputCallBackFunc);
    

};


#endif // CWSAVELOAD_H
