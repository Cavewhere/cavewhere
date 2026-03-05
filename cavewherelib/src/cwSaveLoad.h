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
#include "cwSyncTypes.h"
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
class Account;
}

//Monad includes
#include <Monad/Result.h>

//Qt includes
#include <QByteArray>
#include <QDir>
#include <QFuture>
#include <QHash>
#include <QPointer>
#include <QUndoStack>
#include <functional>
#include <optional>

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

    struct IdentityRepairData {
        bool required = false;
        int generatedIds = 0;
        int duplicateIds = 0;
    };

    struct ProjectLoadData {
        cwCavingRegionData region;
        ProjectMetadataData metadata;
        IdentityRepairData identityRepair;
    };

    struct SyncReport {
        enum class PullState {
            Unknown,
            AlreadyUpToDate,
            FastForward,
            MergeCommitCreated,
            MergeConflicts
        };

        QString beforeHead;
        QString afterHead;
        QString mergeBaseHead;
        PullState pullState = PullState::Unknown;
        QStringList backendPaths;
        QStringList commitDiffPaths;
        QStringList hydrationDeltaPaths;
        QStringList changedPaths;
        QStringList diagnostics;
        bool hasBackendPaths = false;
        bool hasCommitDiffPaths = false;
        bool hasHydrationDeltaPaths = false;
    };

    enum class SyncErrorCode : int {
        RetryEpochChanged = Monad::ResultBase::CustomError + 1,
        IncompatibleProjectVersion = Monad::ResultBase::CustomError + 2
    };

    using BranchResetMode = cwSyncTypes::BranchResetMode;

    cwSaveLoad(QObject* parent = nullptr);
    ~cwSaveLoad();

    void newProject();

    QString fileName() const;
    void setFileName(const QString& filename);
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
    static Monad::Result<cwTripData> loadTrip(const QString& filename);
    static Monad::Result<cwTripData> loadTrip(const QByteArray& content);
    static Monad::Result<cwNoteData> loadNote(const QString& filename, const QDir& projectDir);
    static Monad::Result<cwNoteData> loadNote(const QByteArray& content, const QString& filename, const QDir& projectDir);

    static QString sanitizeFileName(QString input);

    void saveProtoMessage(
        std::unique_ptr<const google::protobuf::Message> message,
        const void* pointerId);

    static std::unique_ptr<CavewhereProto::Cave> toProtoCave(const cwCave* cave);
    static std::unique_ptr<CavewhereProto::Trip> toProtoTrip(const cwTrip* trip);

    void addImages(QList<QUrl> noteImagePaths,
                   const QDir& dir,
                   std::function<void (QList<cwImage>)> outputCallBackFunc);
    void addImages(QList<QUrl> noteImagePaths,
                   std::function<QDir()> destinationDirResolver,
                   std::function<void (QList<cwImage>)> outputCallBackFunc);

    Monad::ResultBase moveProjectTo(const QString& destinationFileUrl);
    Monad::ResultBase copyProjectTo(const QString& destinationFileUrl);
    Monad::ResultBase deleteTemporaryProject();

    void addFiles(QList<QUrl> files,
                  const QDir& dir,
                  std::function<void (QList<QString>)> fileCallBackFunc);
    void addFiles(QList<QUrl> files,
                  std::function<QDir()> destinationDirResolver,
                  std::function<void (QList<QString>)> fileCallBackFunc);


    QDir dataRootDir() const;

    QDir dir(cwSurveyNoteModel* notes) const;
    QDir dir(cwSurveyNoteLiDARModel* notes) const;

    QString absolutePath(const cwNote* note, const QString& imageFilename) const;
    QString absolutePath(const cwNoteLiDAR* note, const QString& lidarFilename) const;
    cwImage absolutePathNoteImage(const cwNote* note) const;

    //For testing
    void waitForFinished();

    bool isTemporaryProject() const;

    QString dataRoot() const;
    void setDataRoot(const QString& dataRoot);

    GitMode gitMode() const;
    void setGitMode(GitMode mode);

    bool syncEnabled() const;
    void setSyncEnabled(bool enabled);

    QFuture<Monad::ResultBase> sync();
    QFuture<Monad::ResultBase> resetBranchAndReconcile(const QString& refSpec,
                                                       BranchResetMode resetMode = BranchResetMode::Hard);
    // Compatibility overload; prefer typed BranchResetMode.
    QFuture<Monad::ResultBase> resetBranchAndReconcile(const QString& refSpec,
                                                       int resetMode);
    // Compatibility wrapper; use resetBranchAndReconcile for branch-moving semantics.
    QFuture<Monad::ResultBase> checkoutAndReconcile(const QString& refSpec,
                                                    int checkoutMode = 1);
    std::optional<SyncReport> lastSyncReport() const;
    Monad::ResultBase commitProjectChanges(const QString& subject = QString(),
                                           const QString& description = QString());

    QFuture<void> retire();

    static void ensureGitExcludeHasLocalEntries(const QDir& repoDir);
    static Monad::Result<cwCaveData> loadCave(const QString& filename);
    static Monad::Result<cwNoteLiDARData> loadNoteLiDAR(const QString& filename, const QDir &projectDir);
    static Monad::Result<cwNoteLiDARData> loadNoteLiDAR(const QByteArray& content, const QString& filename, const QDir &projectDir);
    static cwTripData tripDataFromProtoTrip(const CavewhereProto::Trip& proto);
    static cwNoteData noteDataFromProtoNote(const CavewhereProto::Note& protoNote, const QString& filename);
    static cwNoteLiDARData noteLiDARDataFromProtoNoteLiDAR(const CavewhereProto::NoteLiDAR& protoNote, const QString& filename);

signals:
    void fileNameChanged();
    void dataRootChanged();
    void isTemporaryProjectChanged();
    void objectPathReady(QObject* object);

private:
    void initializeRepositoryForCurrentFile();

    enum class ReconcileApplyMode {
        Merge,
        TargetCommitWins
    };

    struct ReconcileExternalResult {
        enum class Outcome {
            NoOp,
            MutatedRequiresPrePushPersistence
        };

        Outcome outcome = Outcome::NoOp;
        bool persistNoteDescriptors = false;
        bool persistLiDARNoteDescriptors = false;
    };

    struct ReconcileAttemptState {
        quint64 planEpoch = 0;
        quint64 localMutationPlanEpoch = 0;
        std::optional<SyncReport> report;
        std::optional<ReconcileExternalResult> reconcileOutcome;
    };

    enum class FinalizeMode {
        SyncPush,
        CheckoutLocal
    };

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

    QFuture<Monad::ResultBase> loadImpl(const QString& filename);
    QFuture<Monad::ResultBase> saveFlushImpl();
    QFuture<Monad::ResultBase> enqueueReconcilePhase(const QFuture<Monad::ResultBase>& prepareFuture,
                                                     quint64 syncGeneration,
                                                     const std::shared_ptr<ReconcileAttemptState>& attemptState,
                                                     ReconcileApplyMode applyMode);
    QFuture<Monad::ResultBase> enqueueFinalizePhase(const QFuture<Monad::ResultBase>& reconcileFuture,
                                                    quint64 syncGeneration,
                                                    QQuickGit::GitRepository* repo,
                                                    const std::shared_ptr<ReconcileAttemptState>& attemptState,
                                                    FinalizeMode mode);
    QFuture<Monad::Result<ReconcileExternalResult>> reconcileExternalImpl(const SyncReport& report,
                                                                           quint64 syncGeneration,
                                                                           quint64 planEpoch,
                                                                           ReconcileApplyMode applyMode);
    QFuture<Monad::ResultBase> persistIdentityRepairSave(bool persistNoteDescriptors = true,
                                                         bool persistLiDARNoteDescriptors = true);



    void saveCavingRegion(const cwCavingRegion* region);
    static std::unique_ptr<CavewhereProto::CavingRegion> toProtoCavingRegion(const cwCavingRegion* region);
    static QString regionFileName(const QDir& dir, const cwCavingRegion* region);

    void save(const cwCave* cave);

    void save(const cwTrip* trip);

    void save(const cwNote* note);
    static std::unique_ptr<CavewhereProto::Note> toProtoNote(const cwNote* note);

    void save(const cwNoteLiDAR* note);
    static std::unique_ptr<CavewhereProto::NoteLiDAR> toProtoNoteLiDAR(const cwNoteLiDAR* note);

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
    QFuture<Monad::ResultBase> copyFilesAndEmitResults(
        const QList<QString>& sourceFilePaths,
        const QDir& destinationDirectory,
        MakeResultFunc makeResult,
        std::function<void (QList<ResultType>)> outputCallBackFunc);
    

};


#endif // CWSAVELOAD_H
