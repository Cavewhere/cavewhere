#ifndef CWSAVELOADPRIVATE_H
#define CWSAVELOADPRIVATE_H

//Our includes
#include "cwSaveLoad.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwNoteLiDAR.h"
#include "cwSketch.h"
#include "cwCavingRegion.h"
#include "cwRegionTreeModel.h"
#include "cwRegionIOTask.h"
#include "cwUniqueConnectionChecker.h"
#include "cwFutureManagerToken.h"
#include "cwError.h"

//Async future
#include <asyncfuture.h>

//Google protobuffer
#include <google/protobuf/message.h>

//Monad
#include "Monad/Monad.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUuid>
#include <QFuture>
#include <QPointer>
#include <QThreadPool>

//std includes
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace QQuickGit {
class GitRepository;
}

struct cwSaveLoadPrivate {

    struct Job {
        enum class Kind { File, Directory, };
        enum class Action { Move, Remove, EnsureDir, WriteFile, Copy, Custom };

        struct EmptyPayload { };
        struct WriteFilePayload { std::shared_ptr<const google::protobuf::Message> message; };
        struct CopyFilePayload { QString sourcePath; };
        struct CustomPayload { std::function<Monad::ResultBase()> action; };
        using Payload = std::variant<EmptyPayload, WriteFilePayload, CopyFilePayload, CustomPayload>;

        const void* objectId = nullptr;
        Kind kind = Kind::File;
        Action action = Action::Move;
        std::function<void(const Monad::ResultBase&)> onDone;
        Payload payload = EmptyPayload{};

        QString oldPath;
        QString path;
        QString dataRoot;

        Job() = default;
        Job(const void* objectId, Kind kind, Action action)
            : objectId(objectId),
              kind(kind),
              action(action),
              payload(EmptyPayload{})
        {
        }

        Job(const void* objectId,
            Kind kind,
            Action action,
            std::shared_ptr<const google::protobuf::Message> message)
            : objectId(objectId),
              kind(kind),
              action(action),
              payload(WriteFilePayload{std::move(message)})
        {
        }

        Job(const void* objectId, Kind kind, Action action, QString sourcePath)
            : objectId(objectId),
              kind(kind),
              action(action),
              payload(CopyFilePayload{std::move(sourcePath)})
        {
        }

        Job(const void* objectId,
            Kind kind,
            Action action,
            std::function<Monad::ResultBase()> customAction)
            : objectId(objectId),
              kind(kind),
              action(action),
              payload(CustomPayload{std::move(customAction)})
        {
        }

        static const char* kindName(Kind kind) {
            switch (kind) {
            case Kind::File:
                return "File";
            case Kind::Directory:
                return "Directory";
            }
            return "Unknown";
        }

        static const char* actionName(Action action) {
            switch (action) {
            case Action::Move:
                return "Move";
            case Action::Remove:
                return "Remove";
            case Action::EnsureDir:
                return "EnsureDir";
            case Action::WriteFile:
                return "WriteFile";
            case Action::Copy:
                return "Copy";
            case Action::Custom:
                return "Custom";
            }
            return "Unknown";
        }

        QString toString() const;

        static Monad::ResultBase ensureDirectoryExists(const QString& directoryPath);
        static Monad::ResultBase moveOrReplaceFile(const QString& sourcePath, const QString& destinationPath);
        static Monad::ResultBase mergeDirectoryContents(const QString& sourceDirectoryPath,
                                                        const QString& destinationDirectoryPath);
        static Monad::ResultBase moveOrMergeDirectory(const QString& sourcePath, const QString& destinationPath);
        static Monad::ResultBase executeMoveJob(Kind kind, const QString& oldPath, const QString& newPath);

        Monad::ResultBase execute() const;

        static bool lessThan(const Job& a, const Job& b);
    };

    QQuickGit::GitRepository* repository = nullptr;

    QString projectFileName;
    cwSaveLoad::ProjectMetadataData projectMetadata;

    struct ObjectState {
        QString currentPath;
        QString loadedPath;
    };

    struct LoadedTripPathParts {
        QString caveName;
        QString tripName;
    };

    struct LoadedNotePathParts {
        QString caveName;
        QString tripName;
        QString noteName;
    };

    struct LoadedLiDARPathParts {
        QString caveName;
        QString tripName;
        QString noteName;
    };

    struct LoadedPathIndex {
        QHash<QUuid, QString> caveNameById;
        QHash<QUuid, LoadedTripPathParts> tripPartsById;
        QHash<QUuid, LoadedNotePathParts> notePartsById;
        QHash<QUuid, LoadedLiDARPathParts> lidarPartsById;
    };

    //Where the objects are currently being saved
    //This the absolute directory to the m_rootDir
    QHash<const void*, ObjectState> m_objectStates;

    //For watching when object data has changed
    cwRegionTreeModel* m_regionTreeModel;

    //Saving jobs
    QList<Job> m_pendingJobs;
    AsyncFuture::Deferred<void> m_pendingJobsDeferred;
    QStringList m_pendingSaveJobErrors;

    // Dedicated single-thread pool for save/bundle I/O. Saves must not compete
    // with compute-heavy tasks (scrap triangulation, line-plot geometry, etc.)
    // on cwTask::threadPool(). When the user quits a large project whose
    // threadpool is saturated, save jobs would be starved and the application
    // would block on shutdown waiting for a thread to become available.
    QThreadPool m_saveThreadPool;

    bool isTemporary = true;
    bool saveEnabled = true;
    QStringList m_ownedTempDirs; // QTemporaryDir paths owned by this project, cleaned up on retire/destroy
    bool newProjectCalled = false;

    cwFutureManagerToken futureToken;

    //Helps watch if we already has objects connected, this is for debugging only
    cwUniqueConnectionChecker connectionChecker;
    QSet<const QObject*> connectedObjects;

    bool trackConnected(const QObject* object);
    bool isTrackedConnected(const QObject* object) const;
    void trackDisconnected(const QObject* object);

    bool retiring = false;
    QFuture<void> retireFuture;

    struct Operation {
        enum class Type {
            SaveFlush,
            BundlePackage,
            LoadProject,
            SyncProject,
            ReconcileExternal,
            RepairSave,
            ImportFiles
        };

        Type type = Type::SaveFlush;
        quint64 generation = 0;
        std::function<QFuture<Monad::ResultBase>()> run;
        AsyncFuture::Deferred<Monad::ResultBase> deferred;
    };

    QQueue<std::shared_ptr<Operation>> operationQueue;
    std::shared_ptr<Operation> activeOperation;
    quint64 operationGeneration = 0;
    QString pendingCancellationReason;
    quint64 modelMutationEpoch = 0;
    quint64 localMutationEpoch = 0;
    bool suppressLocalMutationTracking = false;

    struct RemoteApplyGuardState {
        bool active = false;
        bool mutationObserved = false;
        quint64 baselineLocalMutationEpoch = 0;

        void begin(quint64 currentEpoch)
        {
            active = true;
            mutationObserved = false;
            baselineLocalMutationEpoch = currentEpoch;
        }

        void end()
        {
            *this = {};
        }

        void noteMutation()
        {
            if (active) {
                mutationObserved = true;
            }
        }

        bool hasLocalMutation(quint64 currentEpoch) const
        {
            if (!active) {
                return false;
            }

            const bool changed = mutationObserved || currentEpoch != baselineLocalMutationEpoch;
            return changed;
        }
    };

    RemoteApplyGuardState remoteApplyGuard;
    bool pendingIdentityRepairSave = false;
    std::optional<cwSaveLoad::SyncReport> lastSyncReport;
    QList<cwError> lastLoadErrors;
    int lastLoadMaxFileVersion = 0;
    bool saveBlockedWarningEmitted = false;

    bool saveWillCauseDataLoss() const {
        return lastLoadMaxFileVersion > cwRegionIOTask::protoVersion();
    }

    static QList<cwSurveyChunkData> fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> & protoList);

    static Monad::ResultBase ensurePathForFile(const QString& filePath);

    // Filesystem helpers that handle Windows long-path (\\?\) prefixing when QFile/QDir fail.
    static QString defaultDataRoot(const QString& projectName);
    static bool fsRenameDir(const QString& src, const QString& dst);
    static bool fsRenameFile(const QString& src, const QString& dst);
    static bool fsRemoveFile(const QString& path);
    static bool fsRemoveDirRecursive(const QString& path);

    cwSaveLoadPrivate();

    bool hasQueuedOperationType(Operation::Type type) const;

    void maybeStartPendingFileJobs(cwSaveLoad* context);

    void ensureSaveFlushScheduled(cwSaveLoad* context);

    QString takePendingSaveJobErrors();

    QFuture<Monad::ResultBase> enqueueOperation(cwSaveLoad* context,
                                         Operation::Type type,
                                         std::function<QFuture<Monad::ResultBase>()> run);

    void runNextOperation(cwSaveLoad* context);

    void cancelPendingOperations(const QString& reason);

    void addFileSystemJob(Job job, cwSaveLoad* context);

    void addExplicitFileSystemJob(Job job, cwSaveLoad* context);

    QString absolutePathFor(const cwSaveLoad* context, const QObject* object) const;

    static QString normalizedAbsolutePath(const QString& path);

    ObjectState& stateFor(const void* object) {
        return m_objectStates[object];
    }

    static LoadedPathIndex buildLoadedPathIndex(const cwCavingRegionData& loadedRegion);

    void seedStatePathFromLoaded(const void* objectId, const QString& absolutePath);

    template<typename TObject>
    void enqueueRenameIfNeeded(cwSaveLoad* context, const TObject* object)
    {
        if (context == nullptr || object == nullptr) {
            return;
        }

        auto& state = stateFor(object);
        const QString desiredPath = normalizedAbsolutePath(absolutePathFor(context, object));
        if (desiredPath.isEmpty()) {
            return;
        }

        if (state.currentPath.isEmpty()) {
            state.currentPath = desiredPath;
            return;
        }

        const QString currentPath = normalizedAbsolutePath(state.currentPath);
        if (currentPath == desiredPath) {
            state.currentPath = currentPath;
            return;
        }

        const QString currentDir = QFileInfo(currentPath).absoluteDir().absolutePath();
        const QString desiredDir = QFileInfo(desiredPath).absoluteDir().absolutePath();
        if (QDir::cleanPath(currentDir) != QDir::cleanPath(desiredDir)) {
            addFileSystemJob(Job {object, Job::Kind::Directory, Job::Action::Move}, context);
        }
        addFileSystemJob(Job {object, Job::Kind::File, Job::Action::Move}, context);
    }

    void resetObjectStates(cwSaveLoad* context);

    void seedObjectStatesFromLoadedData(cwSaveLoad* context,
                                        const cwCavingRegionData& loadedRegion,
                                        const QString& loadedDataRootName);

    static QString cleanupPathForDescriptor(const QString& absoluteDescriptorPath);

    Monad::ResultBase cleanupStaleLoadedPaths(cwSaveLoad* context);

    // Normalize paths for queued jobs in a way that is stable across aliases/symlinks.
    // We cannot canonicalize the full path directly because many queued destinations
    // do not exist yet, and QFileInfo::canonicalFilePath() then returns an empty string.
    // Instead, canonicalize the deepest existing ancestor and append unresolved segments.
    static QString normalizeQueuedPath(const QString& inputPath);


    void saveProtoMessage(
            cwSaveLoad* context,
            std::unique_ptr<const google::protobuf::Message> message,
            const void* objectId
            );

    // Returns a map of objectId -> ordered job indices for all non-null objectIds.
    QHash<const void*, QList<int>> jobIndicesByObjectId() const;

    // Rule 1: For each object with multiple WriteFile jobs, drop all but the last.
    void dropRedundantWrites(const QList<int>& indices, QSet<int>& indicesToDrop) const;

    // Rule 2: If a Remove exists for an object, all its pending WriteFile jobs are moot.
    void dropWritesSupersededByRemove(const QList<int>& indices, QSet<int>& indicesToDrop) const;

    // Rule 3: Collapse sequential Moves for the same object and kind (A→B + B→C becomes A→C).
    void collapseSequentialMoves(const QList<int>& indices, QSet<int>& indicesToDrop);

    void compressPendingJobs();

    void execFileSystemJobs(cwSaveLoad* context);

    template<typename T>
    void saveObject(cwSaveLoad* context, const T* object) {
        if(saveEnabled) {
            const QString objectType = object ? object->metaObject()->className() : QStringLiteral("<null>");
            if (saveWillCauseDataLoss()) {
                if (!saveBlockedWarningEmitted) {
                    saveBlockedWarningEmitted = true;
                    emit context->saveBlockedByVersion(objectType);
                }
                return;
            }
            if (!suppressLocalMutationTracking) {
                ++localMutationEpoch;
                remoteApplyGuard.noteMutation();
                emit context->localMutationOccurred();
            }

            if constexpr (std::is_same_v<T, cwCave>) {
                saveProtoMessage(context, cwSaveLoad::toProtoCave(object), object);
            } else if constexpr (std::is_same_v<T, cwTrip>) {
                saveProtoMessage(context, cwSaveLoad::toProtoTrip(object), object);
            } else if constexpr (std::is_same_v<T, cwNote>) {
                saveProtoMessage(context, cwSaveLoad::toProtoNote(object), object);
            } else if constexpr (std::is_same_v<T, cwNoteLiDAR>) {
                saveProtoMessage(context, cwSaveLoad::toProtoNoteLiDAR(object), object);
            } else if constexpr (std::is_same_v<T, cwSketch>) {
                saveProtoMessage(context, cwSaveLoad::toProtoSketch(object), object);
            } else {
                static_assert(std::is_same_v<T, void>, "Unsupported saveObject type");
            }
        } else {
            const QString objectType = object ? object->metaObject()->className() : QStringLiteral("<null>");
        }
    }

    template<typename T>
    void renameDirectoryAndFile(cwSaveLoad* context, const T* object) {
        // Capture paths now to avoid partial state during rename.
        auto& state = stateFor(object);
        const QString oldFilePath = state.currentPath;
        QString newDirPath;
        if constexpr (std::is_same_v<T, cwCave>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else if constexpr (std::is_same_v<T, cwTrip>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else if constexpr (std::is_same_v<T, cwNote>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else if constexpr (std::is_same_v<T, cwNoteLiDAR>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else if constexpr (std::is_same_v<T, cwSketch>) {
            newDirPath = context->dirPrivate(object).absolutePath();
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported renameDirectoryAndFile type");
        }

        Q_UNUSED(oldFilePath);
        Q_UNUSED(newDirPath);

        addFileSystemJob(Job {object, Job::Kind::Directory, Job::Action::Move}, context);
        addFileSystemJob(Job {object, Job::Kind::File, Job::Action::Move}, context);
        context->save(object);
    }
};

#endif // CWSAVELOADPRIVATE_H
