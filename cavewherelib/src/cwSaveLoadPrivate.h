#ifndef CWSAVELOADPRIVATE_H
#define CWSAVELOADPRIVATE_H

//Our includes
#include "cwSaveLoad.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwLazLayer.h"
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
#include <QDeadlineTimer>
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

    //! Hidden sibling of the data root holding the directories a delete took
    //! away, so an undo can move them back. Named to match .cw_cache, and
    //! excluded from git and from bundles the same way.
    static constexpr QLatin1StringView kTrashDirName = QLatin1StringView(".cw_trash");

    struct Job {
        enum class Kind { File, Directory, };
        enum class Action { Move, Remove, EnsureDir, WriteFile, Copy, Custom };

        struct EmptyPayload { };
        struct WriteFilePayload { std::shared_ptr<const google::protobuf::Message> message; };
        struct CopyFilePayload { QString sourcePath; };
        struct CustomPayload { std::function<Monad::ResultBase()> action; };
        using Payload = std::variant<EmptyPayload, WriteFilePayload, CopyFilePayload, CustomPayload>;

        // The object this job acts for. Held as a plain pointer because it is
        // an identity: jobs are grouped and compressed by it, and a job
        // outlives the object often enough that a clearing QPointer could not
        // serve as the key. Never dereference it after the job is queued —
        // objectGuard is what says whether there is still an object there.
        const QObject* objectId = nullptr;

        // Says whether objectId still points at a live object. A move job
        // reports completion by handing that object back through
        // cwSaveLoad::objectPathReady, and a queued move can outlive the
        // object that asked for it — deleting a trip while its rename is
        // still pending does exactly that. A move whose guard has cleared
        // finishes silently rather than handing listeners a freed pointer to
        // qobject_cast.
        QPointer<QObject> objectGuard;

        Kind kind = Kind::File;
        Action action = Action::Move;
        std::function<void(const Monad::ResultBase&)> onDone;
        Payload payload = EmptyPayload{};

        QString oldPath;
        QString path;
        QString dataRoot;
        // Project root (parent of dataRoot when dataRoot is set, equal to
        // dataRoot otherwise). Used by ensureInsideRoot so jobs may target
        // siblings of dataRoot inside the project (e.g. "GIS Layers/").
        QString projectRoot;
        // Sub-identity within (objectId, kind) for entities that own more than
        // one artifact (e.g. cwLazLayer owns both a .laz and a .cwlaz). Jobs
        // with different tags are never merged or cancelled against each other
        // by the compression rules, so two artifacts of the same logical
        // object can be queued and executed independently. Empty string is the
        // default and preserves single-artifact behavior bit-for-bit.
        QString tag;

        Job() = default;
        Job(const QObject* objectId, Kind kind, Action action)
            : objectId(objectId),
              objectGuard(const_cast<QObject*>(objectId)),
              kind(kind),
              action(action),
              payload(EmptyPayload{})
        {
        }

        Job(const QObject* objectId,
            Kind kind,
            Action action,
            std::shared_ptr<const google::protobuf::Message> message)
            : objectId(objectId),
              objectGuard(const_cast<QObject*>(objectId)),
              kind(kind),
              action(action),
              payload(WriteFilePayload{std::move(message)})
        {
        }

        Job(const QObject* objectId, Kind kind, Action action, QString sourcePath)
            : objectId(objectId),
              objectGuard(const_cast<QObject*>(objectId)),
              kind(kind),
              action(action),
              payload(CopyFilePayload{std::move(sourcePath)})
        {
        }

        Job(const QObject* objectId,
            Kind kind,
            Action action,
            std::function<Monad::ResultBase()> customAction)
            : objectId(objectId),
              objectGuard(const_cast<QObject*>(objectId)),
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

    //! A loaded object's path, as the names it was saved under: nodePath is the
    //! node names from the root down (the root's own excluded), the same list
    //! cwSurveyNode::path() yields, which cwSaveLoad::relativeNodeDir turns into
    //! a directory.
    struct LoadedTripPathParts {
        QStringList nodePath;
        QString tripName;
    };

    //! A 2D note, a LiDAR note and a sketch all live in their trip's notes/
    //! directory and differ only in file suffix, so one shape names all three.
    struct LoadedNotePathParts {
        QStringList nodePath;
        QString tripName;
        QString noteName;
    };

    struct LoadedPathIndex {
        QHash<QUuid, QStringList> nodePathById;
        QHash<QUuid, LoadedTripPathParts> tripPartsById;
        QHash<QUuid, LoadedNotePathParts> notePartsById;
        QHash<QUuid, LoadedNotePathParts> lidarPartsById;
        QHash<QUuid, LoadedNotePathParts> sketchPartsById;
    };

    //Where the objects are currently being saved
    //This the absolute directory to the m_rootDir
    QHash<const QObject*, ObjectState> m_objectStates;

    // Objects whose destruction is already wired to erase their
    // m_objectStates entry. Tracked separately from the hash itself so an
    // entry erased for another reason (a directory Remove job,
    // resetObjectStates) keeps its existing connection instead of growing a
    // duplicate on the next insert.
    QSet<const QObject*> m_lifetimeWatched;

    // The trash entry each removed object owns, .cw_trash/<id>, while its
    // delete is still on the undo stack. The owner is what says whose entry
    // it is: a note destroyed while it sits inside a trashed node's entry
    // must leave that entry — the node's undo still needs it — alone.
    QHash<const QObject*, QString> m_trashEntries;

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

    //! The FileVersion the project's files were last written with, -1 before the
    //! first look. Every file of a project carries the same stamp, so when
    //! cwSaveLoad::stampVersion moves — the first hierarchy appears, or the last
    //! of it goes away — the whole project is rewritten.
    int stampedVersion = -1;
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
            ImportFiles,
            DiscardChanges
        };

        Type type = Type::SaveFlush;
        quint64 generation = 0;
        std::function<QFuture<Monad::ResultBase>()> run;
        AsyncFuture::Deferred<Monad::ResultBase> deferred;
    };

    // FIFO of pending operations plus the one currently running. It owns an
    // "idle" future (finished()) that resolves whenever the queue is empty and
    // nothing is active. enqueue()/activateNext()/clearActive()/cancelQueued()
    // are the only mutators, so the idle future is maintained as a function of
    // queue state and can never be stranded by a drain path that forgets to
    // update it.
    //
    // The idle future is the ordering waitForFinished() relies on: a SaveFlush
    // operation only settles after saveFlushImpl() has emitted
    // saveFlushCompleted, so a finished idle future guarantees that emit
    // already happened. Blocking on the raw m_pendingJobsDeferred (which is
    // upstream of the flush emit) does not give that guarantee.
    class OperationQueue
    {
    public:
        OperationQueue() { m_idle.complete(); }

        bool isEmpty() const { return m_queue.isEmpty(); }
        bool hasActive() const { return m_active != nullptr; }
        const std::shared_ptr<Operation>& active() const { return m_active; }
        QFuture<void> finished() const { return m_idle.future(); }

        bool hasOperation(Operation::Type type, quint64 generation) const
        {
            const auto matches = [&](const std::shared_ptr<Operation>& op) {
                return op && op->generation == generation && op->type == type;
            };
            if (matches(m_active)) {
                return true;
            }
            for (const auto& op : m_queue) {
                if (matches(op)) {
                    return true;
                }
            }
            return false;
        }

        void enqueue(std::shared_ptr<Operation> op)
        {
            m_queue.enqueue(std::move(op));
            if (m_idle.future().isFinished()) {
                m_idle = {};
            }
        }

        std::shared_ptr<Operation> activateNext()
        {
            m_active = m_queue.dequeue();
            return m_active;
        }

        void clearActive()
        {
            m_active.reset();
            settleIfDrained();
        }

        // Drop every queued operation, completing each one's deferred with
        // reason. An active operation is left untouched; it settles the idle
        // future from clearActive().
        void cancelQueued(const QString& reason)
        {
            while (!m_queue.isEmpty()) {
                m_queue.dequeue()->deferred.complete(Monad::ResultBase(reason));
            }
            settleIfDrained();
        }

    private:
        void settleIfDrained()
        {
            if (m_active == nullptr
                    && m_queue.isEmpty()
                    && !m_idle.future().isFinished()) {
                m_idle.complete();
            }
        }

        QQueue<std::shared_ptr<Operation>> m_queue;
        std::shared_ptr<Operation> m_active;
        AsyncFuture::Deferred<void> m_idle;
    };

    OperationQueue m_operations;
    quint64 operationGeneration = 0;
    QString pendingCancellationReason;
    quint64 modelMutationEpoch = 0;
    quint64 localMutationEpoch = 0;
    bool suppressLocalMutationTracking = false;

    // Open while cwSaveLoad is rewriting the project's own files in place —
    // a discard's reset, a sync's pull, a checkout or restore. A filesystem
    // watcher sees those writes exactly as it sees a user's edit in a text
    // editor, so reportProjectFileChangedOnDisk() consults this to keep
    // CaveWhere's own work from marking the project modified.
    struct SelfWriteWindow {
        // How long the window stays open past the write that opened it,
        // covering the lag before the watcher event arrives (FSEvents on
        // macOS coalesces, so this is not instant). A heuristic, and the
        // trade-off runs both ways: too short re-dirties the project right
        // after a discard, too long drops a real edit made in the same
        // breath as one. Sized for the first, since the second takes a user
        // typing into an editor within a second and a half of a git
        // checkout. A deterministic discriminator — asking git whether the
        // path differs from HEAD — would beat the clock, but QQuickGit
        // reports no per-path status, so it needs a submodule API first.
        static constexpr int kSettleMs = 1500;

        void begin() { ++m_depth; }

        // For a write that is already done by the time we hear about it, so
        // there is no scope left to bracket — a queued file job landing on
        // disk. Opens the settle tail alone, which is the only part the
        // echo of a finished write can still fall into.
        void noteWriteCompleted() { m_settle = QDeadlineTimer(kSettleMs); }

        void end()
        {
            Q_ASSERT(m_depth > 0);
            --m_depth;
            if (m_depth == 0) {
                m_settle = QDeadlineTimer(kSettleMs);
            }
        }

        bool isOpen() const { return m_depth > 0 || !m_settle.hasExpired(); }

    private:
        // Counted rather than a flag, so two overlapping operations cannot
        // close each other's window — a discard can start mid-sync, since
        // cwProject::discardChanges() has no syncInProgress() guard.
        int m_depth = 0;
        QDeadlineTimer m_settle; //!< default-constructed is expired, i.e. closed
    };

    SelfWriteWindow selfWrite;

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

    // Recursive directory copy. Fails if destination already exists.
    static Monad::ResultBase copyDirectoryRecursively(const QDir& sourceDir,
                                                      const QDir& destinationDir);

    // Move a directory across filesystems robustly: tries QDir::rename first
    // (fast, same-filesystem case), falls back to copy + recursive delete when
    // rename fails (cross-filesystem case, e.g. cloud-mounted volumes such as
    // pcloud). On copy-fallback success but source-cleanup failure, returns a
    // ResultBase with errorCode == Warning carrying the cleanup message —
    // hasError() is false so callers don't treat it as a failed operation.
    static CAVEWHERE_LIB_EXPORT Monad::ResultBase moveDirectoryRobust(const QString& sourcePath,
                                                                       const QString& destinationPath);

    // Sanity-check that a path is safe to pass to fsRemoveDirRecursive.
    // Rejects empty, relative, root ("/", "C:\\"), or non-existent paths,
    // as well as paths containing '..'. Returns a ResultBase with an error
    // describing the rejection; hasError() is false on success.
    // Exposed for unit testing.
    static CAVEWHERE_LIB_EXPORT Monad::ResultBase validatePathSafeForRecursiveRemoval(const QString& path);

    // Test seam for moveDirectoryRobust. Defaults to a thin wrapper around
    // QDir::rename. Tests swap this to deterministically exercise the
    // copy-fallback branch without needing an actual cross-filesystem mount.
    using RenameDirectoryFn = bool (*)(const QString& src, const QString& dst);
    static CAVEWHERE_LIB_EXPORT RenameDirectoryFn renameDirectoryFn;

    cwSaveLoadPrivate();

    bool hasQueuedOperationType(Operation::Type type) const;

    void maybeStartPendingFileJobs(cwSaveLoad* context);

    void ensureSaveFlushScheduled(cwSaveLoad* context);

    QString takePendingSaveJobErrors();

    QFuture<Monad::ResultBase> enqueueOperation(cwSaveLoad* context,
                                         Operation::Type type,
                                         std::function<QFuture<Monad::ResultBase>()> run);

    void runNextOperation(cwSaveLoad* context);

    QFuture<void> operationsFinished() const { return m_operations.finished(); }

    void cancelPendingOperations(const QString& reason);

    void addFileSystemJob(Job job, cwSaveLoad* context);

    void addExplicitFileSystemJob(Job job, cwSaveLoad* context);

    QString absolutePathFor(const cwSaveLoad* context, const QObject* object) const;

    static QString normalizedAbsolutePath(const QString& path);

    ObjectState& stateFor(const QObject* object, cwSaveLoad* context) {
        auto it = m_objectStates.find(object);
        if (it == m_objectStates.end()) {
            it = m_objectStates.insert(object, ObjectState());
            watchObjectLifetime(object, context);
        }
        return it.value();
    }

    void watchObjectLifetime(const QObject* object, cwSaveLoad* context);

    //! Re-arms the destruction watch after rowsAboutToBeRemoved's
    //! disconnect(object, nullptr, this, nullptr) has taken it down: that
    //! disconnect names the cwSaveLoad context as the receiver, which is the
    //! context watchObjectLifetime connects to, so a removed object would
    //! otherwise die unheard — and its trashed directory would stay on disk.
    void rewatchObjectLifetime(const QObject* object, cwSaveLoad* context);

    //! \a context's trash directory, a hidden sibling of the data root.
    static QString trashDir(const cwSaveLoad* context);

    //! True when \a path names the trash directory or something inside it.
    static bool isInsideTrash(const cwSaveLoad* context, const QString& path);

    //! True when \a path carries the trash directory's name as a segment: the
    //! cheap necessary condition isInsideTrash() asks before canonicalizing.
    static bool namesTrashDir(const QString& path);

    //! Queues the one Directory Move a delete costs, never a Remove: from
    //! where \a object was saved to .cw_trash/<id>/<basename>, so an undo can
    //! move the whole subtree — trips, note images and attachments included —
    //! back out. \a object owns the entry, which is removed for good when it is
    //! destroyed — when the undo command holding it leaves the stack.
    //!
    //! A file removal already queued for something inside that directory is
    //! dropped when the delete itself is what takes it away, and re-aimed at
    //! the trash when it belongs to an earlier, unrelated delete.
    void moveDirectoryToTrash(cwSaveLoad* context, const QObject* object, const QUuid& id);

    //! Carries \a object's directory out of the trash and back to where the
    //! tree now puts it, through the same moveDirectory() a node that changed
    //! parent goes through. Does nothing for an object outside the trash.
    void restoreDirectoryFromTrash(cwSaveLoad* context, const QObject* object);

    //! Queues the Directory Remove of \a absolutePath, a path inside the trash.
    void queueTrashRemove(cwSaveLoad* context, const QString& absolutePath);

    //! Queues the Directory Remove of the whole trash, for a project that is
    //! being opened: a crash between a delete and a close leaves entries no
    //! undo stack can reach any more.
    void sweepTrash(cwSaveLoad* context);

    //! Removes the trash right now, and forgets every object state that named
    //! something inside it. For a project about to change root — Save As moves
    //! or copies the root, and neither a copy nor the folder the user is handed
    //! may carry what a delete took away. The undos still on the stack restore
    //! their descriptors, and their payload stays behind.
    void discardTrash(cwSaveLoad* context);

    //! \a object, every object below it, and nothing else: what a delete of
    //! \a object carries into the trash, and what an undo brings back.
    static QSet<const QObject*> subtreeObjects(const QObject* object);

    //! Re-points every object state at or under \a oldDir to sit under
    //! \a newDir, keeping each basename — what a directory move does on disk.
    void rebaseObjectStatesUnder(const QString& oldDir, const QString& newDir);

    // The nodes currently between cwSurveyNode::beginMoveNode and nodeMoved.
    // A move reaches the tree model as a row removal followed by a row
    // insertion, and a row removal is what makes cwSaveLoad delete a node's
    // directory; this is how the two are told apart, so a move costs one
    // Directory Move and the subtree's trips, note images and attachments are
    // never deleted and rewritten.
    QSet<const QObject*> m_movingNodes;

    //! True when \a object is a node currently moving, or sits under one. The
    //! whole subtree's rows are announced as removed and re-inserted by a move,
    //! and a removed row is otherwise a deletion: the trip's directory and the
    //! note's image file would go with it.
    bool isInsideMovingNode(const QObject* object) const;

    //! Queues the one Directory Move \a object's change of place costs, from
    //! where it was saved to where the tree now puts it, plus the file rename
    //! a name its new siblings already held forces. The loaded paths below
    //! \a object are dropped, whether it is a node or a trip.
    //!
    //! Call it while the object already hangs in its new place — during
    //! cwSurveyNode::parentNodeChanged, or during the row insertion an undone
    //! delete makes — so dirPrivate() answers the new directory while the
    //! object state still holds the old one.
    void moveDirectory(cwSaveLoad* context, const QObject* object);

    //! Drops loadedPath for \a node and every node, trip, note, LiDAR note and
    //! sketch below it, because the Move carried that whole directory away and
    //! cleanupStaleLoadedPaths() has nothing left there to remove.
    //!
    //! currentPath is deliberately left to the Move job, which re-points every
    //! state under the old directory by string prefix — keeping each file's
    //! basename, which is what a filesystem move does. Recomputing it from the
    //! live objects instead would claim a descriptor is already named after a
    //! node the move just renamed, and the rename that follows would then find
    //! nothing to move.
    void dropLoadedPathsUnder(const cwSurveyNode* node);

    //! Drops loadedPath for \a trip and the notes, LiDAR notes and sketches it
    //! owns, for the same reason the node form does.
    void dropLoadedPathsUnder(const cwTrip* trip);

    //! Drops loadedPath for every object state naming \a directory or
    //! something inside it, for a directory whose live objects can no longer
    //! be walked — the subtree a delete carried into the trash. A loaded path
    //! left there tells cleanupStaleLoadedPaths() to remove a directory the
    //! move already emptied, and whatever the user has put back in its place.
    void dropLoadedPathsUnderDir(const QString& directory);

    static LoadedPathIndex buildLoadedPathIndex(const cwCavingRegionData& loadedRegion);

    void seedStatePathFromLoaded(cwSaveLoad* context, const QObject* objectId, const QString& absolutePath);

    template<typename TObject>
    void enqueueRenameIfNeeded(cwSaveLoad* context, const TObject* object)
    {
        if (context == nullptr || object == nullptr) {
            return;
        }

        auto& state = stateFor(object, context);
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
            const QObject* objectId
            );

    // Group key for compression: jobs that share (objectId, tag) are
    // considered the same artifact-stream and may be merged or cancelled
    // against each other. Jobs that differ in either field are independent.
    struct GroupKey {
        const QObject* objectId = nullptr;
        QString tag;

        bool operator==(const GroupKey& other) const noexcept {
            return objectId == other.objectId && tag == other.tag;
        }

        friend size_t qHash(const GroupKey& key, size_t seed = 0) noexcept {
            return qHashMulti(seed, key.objectId, key.tag);
        }
    };

    // Returns a map of (objectId, tag) -> ordered job indices for all non-null objectIds.
    QHash<GroupKey, QList<int>> jobIndicesByGroup() const;

    // Rule 1: For each object with multiple WriteFile jobs, drop all but the last.
    void dropRedundantWrites(const QList<int>& indices, QSet<int>& indicesToDrop) const;

    // Rule 2: If a Remove exists for an object, all its pending WriteFile jobs are moot.
    void dropWritesSupersededByRemove(const QList<int>& indices, QSet<int>& indicesToDrop) const;

    // Rule 3: Collapse sequential Moves for the same object and kind (A→B + B→C becomes A→C).
    // A run ends where the chain does, and also where a Move of the other kind
    // sits between two of them — that job reads a path this run wrote, and the
    // survivor keeps the last index, so collapsing across it would run ahead of
    // the move it depends on.
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
            } else if constexpr (std::is_same_v<T, cwLazLayer>) {
                saveProtoMessage(context, cwSaveLoad::toProtoLazLayer(object), object);
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
        auto& state = stateFor(object, context);
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
