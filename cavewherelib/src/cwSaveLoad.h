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
class cwAbstractNoteTransformation;
class cwNoteTranformation;
class cwNoteLiDARTransformation;
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
#include "cwRemoteAuthProvider.h"
#include "cwCavingRegionData.h"
#include "cwError.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwFutureManagerToken.h"
#include "cwResultDir.h"
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
class NoteTransformation;
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
template <typename T> class RepeatedPtrField;
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
    struct ProjectMetadataData {
        QString dataRoot;
        bool syncEnabled = true;
        QString projectId; // UUID stable across renames; empty on legacy projects
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
        QList<cwError> errors;
        int maxFileVersion = 0; //!< Highest fileVersion seen across all entities during load
    };

    struct SyncReport {
        enum class PullState {
            Unknown,
            AlreadyUpToDate,
            FastForward,
            Rebased,
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
        // Offset by 100 to avoid collisions with GitErrorCode which also
        // starts at CustomError + 1 and shares the same ResultBase transport.
        RetryEpochChanged = Monad::ResultBase::CustomError + 100,
        IncompatibleProjectVersion = Monad::ResultBase::CustomError + 101,
        HttpAuthFailed = Monad::ResultBase::CustomError + 102
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

    QList<cwError> lastLoadErrors() const;
    int lastLoadMaxFileVersion() const;

    static Monad::Result<ProjectLoadData> loadProject(const QString& filename);
    static Monad::Result<cwTripData> loadTrip(const QString& filename);
    static Monad::Result<cwTripData> loadTrip(const QByteArray& content);
    static Monad::Result<cwNoteData> loadNote(const QString& filename, const QDir& projectDir);
    static Monad::Result<cwNoteData> loadNote(const QByteArray& content, const QString& filename, const QDir& projectDir);

    static QString sanitizeFileName(QString input);

    // Load-time repair helpers (exposed for testing)
    static void repairTopLevelIds(ProjectLoadData& loadData);
    static void repairNestedScrapIds(ProjectLoadData& loadData);
    static void repairNameCollisions(ProjectLoadData& loadData);

    // Returns the folder that should be stored as "last directory" for a given
    // project file path.  For .cwproj files the .cwproj lives one level inside
    // the project folder, so two components are stripped (filename + project
    // folder).  For all other paths (including .cw bundles) one component is
    // stripped (the filename), matching the behaviour callers have always
    // expected from cwRootData::setLastDirectory.
    static QString lastDirectoryForProjectFile(const QString& filePath);
    static void initializeGitRepository(const QDir& repoDir);

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

    // Renames the on-disk data-root directory to match `bundleBaseName` and updates
    // in-memory metadata. Drains in-flight jobs and temporarily disables the save
    // pipeline, but does not change the project's filename or temporary state.
    // Used by cwProject::saveAs before packaging a bundle so the zipped tree has
    // a data-root directory whose name matches the bundle.
    Monad::ResultBase prepareBundleStage(const QString& bundleBaseName);

    QFuture<Monad::ResultBase> saveBundledArchive(const QString& targetArchivePath);
    QFuture<Monad::ResultBase> enqueueFlushAndCommit();

    // Queues the dataRoot directory rename and .cwproj descriptor file rename jobs, and
    // rebuilds internal object-state paths to reflect the new dataRoot. Called by the sync
    // handler after a remote project rename when the nameChanged early-out has already fired
    // (because d->projectMetadata.dataRoot was pre-set from loaded data before handlers run).
    void enqueueProjectRenameJobs(const QString& oldDescriptorPath,
                                  const QString& newDescriptorPath);
    // Enqueues filesystem deletion jobs for a conflicting project descriptor file and its
    // accompanying data directory. Used to clean up the "loser" project directory left on
    // disk after a rename/rename merge conflict where ours-wins resolution was applied.
    void enqueueConflictingProjectCleanup(const QString& conflictingDescriptorRelPath);
    // Enqueues recursive deletion of an orphaned cave or trip directory left on disk after
    // a rename/rename merge conflict. The path is relative to the project root directory.
    void enqueueOrphanDirectoryCleanup(const QString& orphanDirRelPath);
    void setOwnedTempDir(const QString& path);

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

    void discardChanges();

    //For testing
    void waitForFinished();

    bool isTemporaryProject() const;

    QString dataRoot() const;
    void setDataRoot(const QString& dataRoot);

    QString projectId() const;

    bool syncEnabled() const;
    void setSyncEnabled(bool enabled);

    void setAuthProvider(cwRemoteAuthProvider* provider);
    cwRemoteAuthProvider* authProvider() const { return m_authProvider; }
    bool requiresProviderCredentials() const;

    QFuture<Monad::ResultBase> sync();
    QFuture<Monad::ResultBase> resetBranchAndReconcile(const QString& refSpec,
                                                       BranchResetMode resetMode = BranchResetMode::Hard);
    // Compatibility overload; prefer typed BranchResetMode.
    QFuture<Monad::ResultBase> resetBranchAndReconcile(const QString& refSpec,
                                                       int resetMode);
    // Compatibility wrapper; use resetBranchAndReconcile for branch-moving semantics.
    QFuture<Monad::ResultBase> checkoutAndReconcile(const QString& refSpec,
                                                    int checkoutMode = 1);
    QFuture<Monad::ResultBase> restoreToCommitAndReconcile(const QString& targetSha);
    std::optional<SyncReport> lastSyncReport() const;

    QFuture<void> retire();

    static void ensureGitExcludeHasLocalEntries(const QDir& repoDir);
    static cwResultDir repositoryDir(const QUrl& localDir, const QString& name);
    static Monad::Result<cwCaveData> loadCave(const QString& filename);
    static Monad::Result<cwNoteLiDARData> loadNoteLiDAR(const QString& filename, const QDir &projectDir);
    static Monad::Result<cwNoteLiDARData> loadNoteLiDAR(const QByteArray& content, const QString& filename, const QDir &projectDir);
    static cwTripData tripDataFromProtoTrip(const CavewhereProto::Trip& proto);
    static cwNoteData noteDataFromProtoNote(const CavewhereProto::Note& protoNote, const QString& filename);
    static cwNoteLiDARData noteLiDARDataFromProtoNoteLiDAR(const CavewhereProto::NoteLiDAR& protoNote, const QString& filename);

    // Proto primitive type helpers (shared by cwSaveLoad and cwRegionLoadTask)
    static QDate loadDate(const QtProto::QDate& protoDate);
    static QSize loadSize(const QtProto::QSize& protoSize);
    static QSizeF loadSizeF(const QtProto::QSizeF& protoSize);
    static QPointF loadPointF(const QtProto::QPointF& protoPointF);
    static QVector3D loadVector3D(const QtProto::QVector3D& protoVector3D);
    static QVector2D loadVector2D(const QtProto::QVector2D& protoVector2D);
    static void saveString(std::string *protoString, const QString &string);
    static void saveDate(QtProto::QDate* protoDate, QDate date);
    static void saveSize(QtProto::QSize* protoSize, QSize size);
    static void saveSizeF(QtProto::QSizeF* protoSize, QSizeF size);
    static void savePointF(QtProto::QPointF* protoPointF, QPointF point);
    static void saveVector3D(QtProto::QVector3D* protoVector3D, QVector3D vector3D);
    static void saveQUuid(std::string *protoString, const QUuid& id);
    static void saveStringList(google::protobuf::RepeatedPtrField<std::string>* protoStringList, const QStringList& stringList);

signals:
    void fileNameChanged();
    void dataRootChanged();
    void isTemporaryProjectChanged();
    void objectPathReady(QObject* object);
    void localMutationOccurred(); //!< Emitted when user-visible data is mutated (save queued, tracking not suppressed)
    void saveFlushCompleted(); //!< Emitted after pending file writes are flushed to disk
    void discardCompleted();
    void discardFailed(const QString& errorMessage);
    void saveBlockedByVersion(const QString& entityDescription); //!< Emitted when a save is skipped because the project has newer-version entities

private:
    void initializeRepositoryForCurrentFile();
    // If the current projectFileName no longer exists on disk (e.g. after a git rename),
    // scan repoPath for a single .cwproj and update the in-memory filename to match.
    void updateFileNameFromSingleCwproj(const QString& repoPath);

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
    QPointer<cwRemoteAuthProvider> m_authProvider;

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
    static void removeTemporaryProjectDir(const QString& ownedTempDirPath);

    enum class ProjectTransferMode { Move, Copy };
    Monad::ResultBase transferProjectTo(const QString& destinationFileUrl, ProjectTransferMode mode);

    // Renames the data-root directory under `targetRootDir` from the current metadata
    // name to `newDataRootName` and updates in-memory metadata / object states. No-op
    // when the name is unchanged. Shared between transferProjectTo and
    // prepareBundleStage so .cwproj and .cw save-as paths stay in sync.
    Monad::ResultBase renameDataRootOnDisk(const QDir& targetRootDir,
                                           const QString& newDataRootName);

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
    static QString friendlyProjectName();
    QDir createTemporaryDirectory(const QString& subDirName);

    QFuture<void> completeSaveJobs();

    static QDir caveDirHelper(const QDir& projectDir, const cwCave *cave);
    static QDir tripDirHelper(const QDir& caveDir, const cwTrip* trip);
    static QDir noteDirHelper(const QDir& tripDir);

    static QUuid toUuid(const std::string& uuidStr);

    Monad::ResultBase commitProjectChanges(const QString& subject = QString(),
                                           const QString& description = QString());
    QFuture<Monad::ResultBase> loadImpl(const QString& filename);
    using GitOperationFn = std::function<QFuture<Monad::ResultBase>(QQuickGit::GitRepository* repo)>;
    QFuture<Monad::ResultBase> gitOperationAndReconcile(const QString& operationLabel,
                                                        const GitOperationFn& gitOp);
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
    static cwNoteTransformationData fromProtoNoteTransformation(const CavewhereProto::NoteTransformation& protoNoteTransform);
    static cwNoteLiDARTransformationData fromProtoLiDARNoteTransformation(const CavewhereProto::NoteLiDARTransformation& protoNoteTransform);
    static cwLength::Data fromProtoLength(const CavewhereProto::Length& protoLength);
    static std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data> fromProtoProjectedScraptViewMatrix(const CavewhereProto::ProjectedProfileScrapViewMatrix protoViewMatrix);
    static cwImageResolution::Data fromProtoImageResolution(const CavewhereProto::ImageResolution& protoImageResolution);
    static QQuaternion fromProtoQuaternion(const QtProto::QQuaternion& protoQuaternion);

    static void saveNoteLiDARTranformation(CavewhereProto::NoteLiDARTransformation *protoNoteTransformation,
                                           cwNoteLiDARTransformation *noteTransformation);
    static void saveQQuaternion(QtProto::QQuaternion* protoQuaternion,
                                const QQuaternion& quaternion);

    // Proto serialization helpers (moved from cwRegionSaveTask)
    static void saveLength(CavewhereProto::Length* protoLength, cwLength* length);
    static void saveImageResolution(CavewhereProto::ImageResolution* protoImageRes, cwImageResolution* imageResolution);
    static void saveImage(CavewhereProto::Image* protoImage, const cwImage& image);
    static void saveNoteStation(CavewhereProto::NoteStation* protoNoteStation, const cwNoteStation& noteStation);
    static void saveTeamMember(CavewhereProto::TeamMember* protoTeamMember, const cwTeamMember& teamMember);
    static void saveLead(CavewhereProto::Lead* protoLead, const cwLead& lead);
    static void saveProjectedScrapViewMatrix(CavewhereProto::ProjectedProfileScrapViewMatrix* protoViewMatrix,
                                             cwProjectedProfileScrapViewMatrix* viewMatrix);
    static void saveNoteTranformation(CavewhereProto::NoteTransformation* protoNoteTransformation,
                                      cwAbstractNoteTransformation *noteTransformation);
    static void saveStationShot(CavewhereProto::StationShot* protoStation, const cwStation& station);
    static void saveStationShot(CavewhereProto::StationShot* protoShot, const cwShot& shot);
    static void saveTripCalibration(CavewhereProto::TripCalibration* protoTripCalibration, cwTripCalibration* tripCalibration);
    static void saveSurveyChunk(CavewhereProto::SurveyChunk* protoChunk, cwSurveyChunk* chunk);
    static void saveTeam(CavewhereProto::Team* protoTeam, cwTeam* team);
    static void saveScrap(CavewhereProto::Scrap* protoScrap, cwScrap* scrap);



    template<typename ResultType, typename MakeResultFunc>
    QFuture<Monad::ResultBase> copyFilesAndEmitResults(
        const QList<QString>& sourceFilePaths,
        const QDir& destinationDirectory,
        MakeResultFunc makeResult,
        std::function<void (QList<ResultType>)> outputCallBackFunc);
    

};


#endif // CWSAVELOAD_H
