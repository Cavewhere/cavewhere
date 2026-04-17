#ifndef CWSAVELOADPRIVATE_H
#define CWSAVELOADPRIVATE_H

//Our includes
#include "cwSaveLoad.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwNoteLiDAR.h"
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
#include <google/protobuf/util/json_util.h>

//Monad
#include "Monad/Monad.h"

//Qt includes
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
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
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

//std includes
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace QQuickGit {
class GitRepository;
}

namespace {

QString defaultDataRoot(const QString& projectName)
{
    return cwSaveLoad::sanitizeFileName(projectName);
}

namespace LongPathFs {

#ifdef Q_OS_WIN
static LPCWSTR toWide(const QString& s) { return reinterpret_cast<LPCWSTR>(s.utf16()); }

static QString prefixed(const QString& path)
{
    QString native = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (!native.startsWith(QStringLiteral("\\\\?\\"))) {
        native.prepend(QStringLiteral("\\\\?\\"));
    }
    return native;
}
#endif

static bool renameDir(const QString& src, const QString& dst)
{
    if (QDir().rename(src, dst)) {
        return true;
    }
#ifdef Q_OS_WIN
    return MoveFileW(toWide(prefixed(src)), toWide(prefixed(dst))) != 0;
#else
    return false;
#endif
}

static bool renameFile(const QString& src, const QString& dst)
{
    if (QFile::rename(src, dst)) {
        return true;
    }
#ifdef Q_OS_WIN
    return MoveFileW(toWide(prefixed(src)), toWide(prefixed(dst))) != 0;
#else
    return false;
#endif
}

static bool removeFile(const QString& path)
{
    if (QFile::remove(path)) {
        return true;
    }
#ifdef Q_OS_WIN
    return DeleteFileW(toWide(prefixed(path))) != 0;
#else
    return false;
#endif
}

static bool removeDirRecursive(const QString& path)
{
    if (QDir(path).removeRecursively()) {
        return true;
    }
#ifdef Q_OS_WIN
    // QDir failed — iterate with long-path aware removal.
    // Also clears read-only attributes that git/LFS may set.
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString p = prefixed(it.filePath());
        SetFileAttributesW(toWide(p), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(toWide(p));
    }
    // Remove empty directories bottom-up
    QStringList dirs;
    QDirIterator dirIt(path, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                       QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        dirs.append(dirIt.filePath());
    }
    std::reverse(dirs.begin(), dirs.end());
    for (const QString& d : dirs) {
        RemoveDirectoryW(toWide(prefixed(d)));
    }
    RemoveDirectoryW(toWide(prefixed(path)));
    return !QFileInfo::exists(path);
#else
    return false;
#endif
}

} // namespace LongPathFs

} // anonymous namespace

struct cwSaveLoadPrivate {

    struct Job {
        enum class Kind { File, Directory, };
        enum class Action { Move, Remove, EnsureDir, WriteFile, CopyFile, Custom };

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
            case Action::CopyFile:
                return "CopyFile";
            case Action::Custom:
                return "Custom";
            }
            return "Unknown";
        }

        QString toString() const {
            const QString objectHex = QString::number(reinterpret_cast<quintptr>(objectId), 16);
            const auto* writePayload = std::get_if<WriteFilePayload>(&payload);
            const auto* copyPayload = std::get_if<CopyFilePayload>(&payload);
            const auto* customPayload = std::get_if<CustomPayload>(&payload);
            const QString sourcePath = copyPayload ? copyPayload->sourcePath : QString();
            return QStringLiteral("Job{action=%1 kind=%2 objectId=0x%3 oldPath='%4' newPath='%5' sourcePath='%6' dataRoot='%7' hasMessage=%8 hasCustom=%9}")
                    .arg(QString::fromLatin1(actionName(action)),
                         QString::fromLatin1(kindName(kind)),
                         objectHex,
                         oldPath,
                         path,
                         sourcePath,
                         dataRoot)
                    .arg(writePayload && writePayload->message != nullptr)
                    .arg(customPayload && customPayload->action != nullptr);
        }

        static Monad::ResultBase ensureDirectoryExists(const QString& directoryPath)
        {
            QDir directory;
            if (!directory.mkpath(directoryPath)) {
                return Monad::ResultBase(QStringLiteral("Failed to create directory: %1").arg(directoryPath));
            }
            return Monad::ResultBase();
        }

        static Monad::ResultBase moveOrReplaceFile(const QString& sourcePath, const QString& destinationPath)
        {
            if (QFileInfo::exists(destinationPath)) {
                if (QFileInfo(destinationPath).isDir()) {
                    return Monad::ResultBase(QStringLiteral("Failed to move %1 -> %2 (destination is a directory)")
                                             .arg(sourcePath, destinationPath));
                }
                if (!LongPathFs::removeFile(destinationPath)) {
                    return Monad::ResultBase(QStringLiteral("Failed to replace file: %1").arg(destinationPath));
                }
            } else {
                const QString parentDirectoryPath = QFileInfo(destinationPath).absolutePath();
                const auto ensureParentResult = ensureDirectoryExists(parentDirectoryPath);
                if (ensureParentResult.hasError()) {
                    return ensureParentResult;
                }
            }

            if (!LongPathFs::renameFile(sourcePath, destinationPath)) {
                return Monad::ResultBase(QStringLiteral("Failed to move file %1 -> %2").arg(sourcePath, destinationPath));
            }

            return Monad::ResultBase();
        }

        static Monad::ResultBase mergeDirectoryContents(const QString& sourceDirectoryPath,
                                                        const QString& destinationDirectoryPath)
        {
            const auto ensureDestinationResult = ensureDirectoryExists(destinationDirectoryPath);
            if (ensureDestinationResult.hasError()) {
                return ensureDestinationResult;
            }

            QDir sourceDirectory(sourceDirectoryPath);
            const QFileInfoList entries = sourceDirectory.entryInfoList(QDir::NoDotAndDotDot
                                                                        | QDir::AllEntries
                                                                        | QDir::Hidden
                                                                        | QDir::System);
            for (const QFileInfo& entryInfo : entries) {
                const QString sourceEntryPath = entryInfo.absoluteFilePath();
                const QString destinationEntryPath = QDir(destinationDirectoryPath).filePath(entryInfo.fileName());

                if (entryInfo.isDir()) {
                    if (QFileInfo::exists(destinationEntryPath)) {
                        if (!QFileInfo(destinationEntryPath).isDir()) {
                            return Monad::ResultBase(
                                        QStringLiteral("Failed to merge directory %1 into %2 (destination entry exists and is not a directory)")
                                        .arg(sourceEntryPath, destinationEntryPath));
                        }

                        const auto nestedMergeResult = mergeDirectoryContents(sourceEntryPath, destinationEntryPath);
                        if (nestedMergeResult.hasError()) {
                            return nestedMergeResult;
                        }

                        if (!LongPathFs::removeDirRecursive(sourceEntryPath)) {
                            return Monad::ResultBase(QStringLiteral("Failed to remove directory after merge: %1").arg(sourceEntryPath));
                        }
                    } else if (!LongPathFs::renameDir(sourceEntryPath, destinationEntryPath)) {
                        return Monad::ResultBase(QStringLiteral("Failed to move directory %1 -> %2")
                                                 .arg(sourceEntryPath, destinationEntryPath));
                    }
                    continue;
                }

                const auto moveFileResult = moveOrReplaceFile(sourceEntryPath, destinationEntryPath);
                if (moveFileResult.hasError()) {
                    return moveFileResult;
                }
            }

            return Monad::ResultBase();
        }

        static Monad::ResultBase moveOrMergeDirectory(const QString& sourcePath, const QString& destinationPath)
        {
            if (QFileInfo::exists(destinationPath) && !QFileInfo(destinationPath).isDir()) {
                return Monad::ResultBase(QStringLiteral("Failed to move %1 -> %2 (destination is not a directory)")
                                         .arg(sourcePath, destinationPath));
            }

            if (!QFileInfo::exists(destinationPath)) {
                if (!LongPathFs::renameDir(sourcePath, destinationPath)) {
                    return Monad::ResultBase(QStringLiteral("Failed to move %1 -> %2").arg(sourcePath, destinationPath));
                }
                return Monad::ResultBase();
            }

            const auto mergeResult = mergeDirectoryContents(sourcePath, destinationPath);
            if (mergeResult.hasError()) {
                return mergeResult;
            }

            if (!LongPathFs::removeDirRecursive(sourcePath)) {
                return Monad::ResultBase(QStringLiteral("Failed to remove directory after merge: %1").arg(sourcePath));
            }

            return Monad::ResultBase();
        }

        static Monad::ResultBase executeMoveJob(Kind kind, const QString& oldPath, const QString& newPath)
        {
            if (kind == Kind::Directory) {
                return moveOrMergeDirectory(oldPath, newPath);
            }

            return moveOrReplaceFile(oldPath, newPath);
        }

        Monad::ResultBase execute() const {
            auto isInsideRoot = [](const QString& rootPath, const QString& targetPath) {
                if (rootPath.isEmpty() || targetPath.isEmpty()) {
                    return false;
                }
                const QString root = QDir::cleanPath(QDir(rootPath).absolutePath());
                if (root.isEmpty()) {
                    return false;
                }
                const QString target = QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath());
                if (target.isEmpty()) {
                    return false;
                }
                return target == root || target.startsWith(root + QStringLiteral("/"));
            };

            auto isProjectFile = [&](const QString& targetPath) {
                const QString root = QDir::cleanPath(QDir(dataRoot).absolutePath());
                const QString target = QDir::cleanPath(QFileInfo(targetPath).absolutePath());
                const QString parentOfRoot = QDir(root).absolutePath() + QStringLiteral("/") + QStringLiteral("..");
                const QString normalizedParent = QDir::cleanPath(QDir(parentOfRoot).absolutePath());
                const bool isProjectFileWrite = (action == Action::WriteFile && kind == Kind::File);
                const bool isInProjectRoot = (target == normalizedParent)
                        || target.startsWith(normalizedParent + QStringLiteral("/"));

                return isProjectFileWrite && isInProjectRoot;
            };

            auto ensureInsideRoot = [&](const QString& targetPath) -> Monad::ResultBase {
                if (!isInsideRoot(dataRoot, targetPath) && !isProjectFile(targetPath)) [[unlikely]] {
                    return Monad::ResultBase(QStringLiteral("Path outside dataRoot: %1").arg(targetPath));
                }
                    return Monad::ResultBase();
            };

            switch(action) {
            case Action::EnsureDir: {
                auto rootCheck = ensureInsideRoot(path);
                if (rootCheck.hasError()) {
                    return rootCheck;
                }
                QDir dir;
                if (!dir.mkpath(path)) {
                    return Monad::ResultBase(QStringLiteral("Failed to create directory: %1").arg(path));
                }
                return Monad::ResultBase();
            }
            case Action::Move: {
                auto oldCheck = ensureInsideRoot(oldPath);
                if (oldCheck.hasError()) {
                    return oldCheck;
                }
                auto newCheck = ensureInsideRoot(path);
                if (newCheck.hasError()) {
                    return newCheck;
                }

                if (!QFileInfo::exists(oldPath)) {
                    return Monad::ResultBase();
                }

                if (oldPath == path) {
                    return Monad::ResultBase();
                }

                const auto moveResult = executeMoveJob(kind, oldPath, path);
                if (moveResult.hasError()) {
                    return moveResult;
                }

                Q_ASSERT(QFileInfo::exists(path));
                return Monad::ResultBase();
            }
            case Action::Remove:
            {
                auto rootCheck = ensureInsideRoot(oldPath);
                if (rootCheck.hasError()) {
                    return rootCheck;
                }
            }
                switch(kind) {
                case Kind::File:
                    if (!QFileInfo::exists(oldPath)) {
                        return Monad::ResultBase();
                    }
                    Q_ASSERT(QFileInfo(oldPath).isFile());
                    if (!QFile::remove(oldPath)) {
                        return Monad::ResultBase(QStringLiteral("Failed to remove file: %1").arg(oldPath));
                    }
                    return Monad::ResultBase();
                case Kind::Directory:
                    if (!QFileInfo::exists(oldPath)) {
                        return Monad::ResultBase();
                    }
                    Q_ASSERT(QFileInfo(oldPath).isDir());
                    if (!QDir(oldPath).removeRecursively()) {
                        return Monad::ResultBase(QStringLiteral("Failed to remove directory: %1").arg(oldPath));
                    }
                    return Monad::ResultBase();
                }
            case Action::WriteFile: {
                auto* writePayload = std::get_if<WriteFilePayload>(&payload);
                if (!writePayload || !writePayload->message) {
                    return Monad::ResultBase(QStringLiteral("Missing message for WriteFile job: %1").arg(path));
                }
                {
                    auto rootCheck = ensureInsideRoot(path);
                    if (rootCheck.hasError()) {
                        return rootCheck;
                    }
                }
                return Monad::mbind(ensurePathForFile(path), [&](Monad::ResultBase /*result*/) {
                    QSaveFile file(path);
                    if (!file.open(QFile::WriteOnly)) {
                        return Monad::ResultBase(QStringLiteral("Failed to open file for writing: %1").arg(path));
                    }

                    std::string json_output;
                    google::protobuf::util::JsonPrintOptions options;
                    options.add_whitespace = true;

                    auto status = google::protobuf::util::MessageToJsonString(*writePayload->message, &json_output, options);
                    if (!status.ok()) {
                        return Monad::ResultBase(QStringLiteral("Failed to convert proto message to JSON: %1").arg(status.ToString().c_str()));
                    }

                    file.write(json_output.c_str(), json_output.size());
                    file.commit();
                    return Monad::ResultBase();
                });
            }
            case Action::CopyFile: {
                auto* copyPayload = std::get_if<CopyFilePayload>(&payload);
                if (!copyPayload || copyPayload->sourcePath.isEmpty()) {
                    return Monad::ResultBase(QStringLiteral("Missing source path for CopyFile job: %1").arg(path));
                }
                {
                    auto rootCheck = ensureInsideRoot(path);
                    if (rootCheck.hasError()) {
                        return rootCheck;
                    }
                }
                return Monad::mbind(ensurePathForFile(path), [&](Monad::ResultBase /*result*/) {
                    if (!QFile::copy(copyPayload->sourcePath, path)) {
                        return Monad::ResultBase(QStringLiteral("Failed to copy %1 -> %2").arg(copyPayload->sourcePath, path));
                    }
                    return Monad::ResultBase();
                });
            }
            case Action::Custom:
                if (auto* customPayload = std::get_if<CustomPayload>(&payload)) {
                    if (!customPayload->action) {
                        return Monad::ResultBase(QStringLiteral("Missing custom action for job"));
                    }
                    return customPayload->action();
                }
                return Monad::ResultBase(QStringLiteral("Missing custom action for job"));
            }
            return Monad::ResultBase();
        }

        static bool lessThan(const Job& a, const Job& b) {
            auto pathDepth = [](const QString& path) -> int {
                const QString cleaned = QDir::cleanPath(path);

                int slashCount = 0;
                for (const QChar ch : QStringView{cleaned}) {
                    if (ch == u'/') {
                        ++slashCount;
                    }
                }
                return slashCount;
            };

            auto kindRank = [](Job::Kind kind) -> int {
                // Files first (0), directories second (1).
                return (kind == Job::Kind::File) ? 0 : 1;
            };

            auto actionRank = [](Job::Action action) -> int {
                return action == Job::Action::Remove ? 0 : 1;
            };

            {
                const int aRank = actionRank(a.action);
                const int bRank = actionRank(b.action);
                if (aRank != bRank) {
                    return aRank < bRank;
                }
            }

            {
                const int aRank = kindRank(a.kind);
                const int bRank = kindRank(b.kind);
                if (aRank != bRank) {
                    return aRank < bRank;
                }
            }

            const QString aPath = a.action == Action::EnsureDir ? a.path : a.oldPath;
            const QString bPath = b.action == Action::EnsureDir ? b.path : b.oldPath;
            const int aDepth = pathDepth(aPath);
            const int bDepth = pathDepth(bPath);
            if (aDepth != bDepth) {
                // Deeper first.
                return aDepth > bDepth;
            }

            // Stable lexical tie-breaker.
            return aPath < bPath;
        }
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

    bool trackConnected(const QObject* object)
    {
        if (object == nullptr) {
            return false;
        }
        if (connectedObjects.contains(object)) {
            return false;
        }
        connectedObjects.insert(object);
        return true;
    }

    bool isTrackedConnected(const QObject* object) const
    {
        return object != nullptr && connectedObjects.contains(object);
    }

    void trackDisconnected(const QObject* object)
    {
        if (object == nullptr) {
            return;
        }
        connectedObjects.remove(object);
    }

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

    static Monad::ResultBase ensurePathForFile(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            bool success = dir.mkpath(".");
            if(success) {
                return Monad::ResultBase();
            } else {
                return Monad::ResultBase(QStringLiteral("Couldn't create directory:") + dir.absolutePath());
            }
        }
        return Monad::ResultBase();
    }

    cwSaveLoadPrivate() {
        m_pendingJobsDeferred.complete();
        m_saveThreadPool.setMaxThreadCount(1);
    }

    bool hasQueuedOperationType(Operation::Type type) const
    {
        if (activeOperation
                && activeOperation->generation == operationGeneration
                && activeOperation->type == type) {
            return true;
        }

        for (const auto& op : operationQueue) {
            if (op
                    && op->generation == operationGeneration
                    && op->type == type) {
                return true;
            }
        }

        return false;
    }

    void maybeStartPendingFileJobs(cwSaveLoad* context)
    {
        if (activeOperation == nullptr) {
            return;
        }

        if (m_pendingJobs.isEmpty()) {
            return;
        }

        if (m_pendingJobsDeferred.future().isFinished()) {
            execFileSystemJobs(context);
        }
    }

    void ensureSaveFlushScheduled(cwSaveLoad* context)
    {
        if (retiring) {
            return;
        }

        if (hasQueuedOperationType(Operation::Type::SaveFlush)) {
            return;
        }

        enqueueOperation(context, Operation::Type::SaveFlush, [context]() {
            return context->saveFlushImpl();
        });
    }

    QString takePendingSaveJobErrors()
    {
        if (m_pendingSaveJobErrors.isEmpty()) {
            return QString();
        }

        const QString joined = m_pendingSaveJobErrors.join(QLatin1Char('\n'));
        m_pendingSaveJobErrors.clear();
        return joined;
    }

    QFuture<Monad::ResultBase> enqueueOperation(cwSaveLoad* context,
                                         Operation::Type type,
                                         std::function<QFuture<Monad::ResultBase>()> run)
    {
        auto op = std::make_shared<Operation>();
        op->type = type;
        op->generation = operationGeneration;
        op->run = std::move(run);

        operationQueue.enqueue(op);
        runNextOperation(context);
        return op->deferred.future();
    }

    void runNextOperation(cwSaveLoad* context)
    {
        if (activeOperation != nullptr) {
            return;
        }

        if (operationQueue.isEmpty()) {
            return;
        }

        activeOperation = operationQueue.dequeue();
        auto op = activeOperation;

        if (op->generation != operationGeneration) {
            op->deferred.complete(Monad::ResultBase(QStringLiteral("Operation canceled.")));
            activeOperation.reset();
            runNextOperation(context);
            return;
        }

        maybeStartPendingFileJobs(context);

        QFuture<Monad::ResultBase> opFuture;
        if (op->run) {
            opFuture = op->run();
        } else {
            opFuture = AsyncFuture::completed(Monad::ResultBase(QStringLiteral("Invalid operation request.")));
        }

        AsyncFuture::observe(opFuture)
                .context(context, [this, context, opFuture, op]() {
            Monad::ResultBase result = opFuture.result();
            if (op->generation != operationGeneration && !result.hasError()) {
                result = Monad::ResultBase(QStringLiteral("Operation canceled."));
            }

            if (!op->deferred.future().isFinished()) {
                op->deferred.complete(result);
            }

            if (activeOperation == op) {
                activeOperation.reset();
            }

            if (op->type == Operation::Type::SaveFlush && result.hasError()) {
                const QString reason = result.errorMessage().isEmpty()
                        ? QStringLiteral("Save flush failed.")
                        : result.errorMessage();
                cancelPendingOperations(reason);
            }

            runNextOperation(context);
        });
    }

    void cancelPendingOperations(const QString& reason)
    {
        ++operationGeneration;

        if (activeOperation && !activeOperation->deferred.future().isFinished()) {
            activeOperation->deferred.complete(Monad::ResultBase(reason));
        }

        while (!operationQueue.isEmpty()) {
            auto op = operationQueue.dequeue();
            op->deferred.complete(Monad::ResultBase(reason));
        }
    }

    void addFileSystemJob(Job job, cwSaveLoad* context) {
        if (retiring) {
            return;
        }

        Q_ASSERT(job.path.isEmpty());
        Q_ASSERT(job.oldPath.isEmpty());

        const QString dataRootName = context->dataRoot();
        const QString dataRootPath = dataRootName.isEmpty()
                ? context->projectRootDir().absolutePath()
                : context->projectRootDir().absoluteFilePath(dataRootName);
        job.dataRoot = normalizeQueuedPath(QDir(dataRootPath).absolutePath());

        auto toDirOrFilePath = [](const Job& job, const QString& path) {
            return job.kind == Job::Kind::Directory ? QFileInfo(path).absoluteDir().absolutePath() : path;
        };

        auto oldPath = [this, toDirOrFilePath](const Job& job) {
            return toDirOrFilePath(job, m_objectStates[job.objectId].currentPath);
        };

        auto path = [this, context, toDirOrFilePath](const Job& job) {
            return toDirOrFilePath(job, absolutePathFor(context, static_cast<const QObject*>(job.objectId)));
        };

        job.path = path(job);
        job.oldPath = oldPath(job);
        if (!job.path.isEmpty()) {
            job.path = normalizeQueuedPath(job.path);
        }
        if (!job.oldPath.isEmpty()) {
            job.oldPath = normalizeQueuedPath(job.oldPath);
        }


        auto emitObjectPathHelper = [](const Monad::ResultBase& result, auto doneFunc, auto emitFunc) {
            if (doneFunc) {
                doneFunc(result);
            }

            if (!result.hasError()) {
                emitFunc();
            }
        };

        if (job.action == Job::Action::WriteFile && job.kind == Job::Kind::File) {
            auto& state = m_objectStates[job.objectId];
            if (state.currentPath.isEmpty()) {
                state.currentPath = job.path;
            } else {
                // During reconcile-driven renames, write jobs can be queued while directory/file
                // move jobs are still normalizing object-state paths. Keep state aligned with the
                // intended write destination instead of aborting on transient ordering.
                state.currentPath = job.path;
            }
        } else if (job.kind == Job::Kind::File && job.action == Job::Action::Move) {
            auto& state = m_objectStates[job.objectId];
            if (state.currentPath == job.path) {
                return;
            }
            state.currentPath = job.path;

            const auto originalOnDone = job.onDone;
            job.onDone =
                    [this,
                    emitObjectPathHelper,
                    context,
                    objectId = job.objectId,
                    originalOnDone]
                    (const Monad::ResultBase& result)
            {
                emitObjectPathHelper(result, originalOnDone, [objectId, context]() {
                    if (auto* object = static_cast<QObject*>(const_cast<void*>(objectId))) {
                        emit context->objectPathReady(object);
                    }
                });
            };
        } else if (job.kind == Job::Kind::Directory && job.action == Job::Action::Move) {
            const QString oldDir = job.oldPath;
            const QString newDir = job.path;

            if (oldDir.isEmpty() || newDir.isEmpty() || oldDir == newDir) {
                return;
            }

            const QString prefix = oldDir + QStringLiteral("/");
            for (auto it = m_objectStates.begin(); it != m_objectStates.end(); ++it) {
                const QString currentPath = it.value().currentPath;
                if (currentPath == oldDir) {
                    it.value().currentPath = newDir;
                } else if (currentPath.startsWith(prefix)) {
                    it.value().currentPath = newDir + QStringLiteral("/") + currentPath.mid(prefix.size());
                }
            }

            const auto originalOnDone = job.onDone;
            job.onDone =
                    [this,
                    emitObjectPathHelper,
                    context,
                    objectId = job.objectId,
                    originalOnDone]
                    (const Monad::ResultBase& result)
            {
                emitObjectPathHelper(result, originalOnDone, [objectId, context]() {
                    // Emit only for the directory object; listeners cascade so we avoid N per-file signals.
                    if (auto* object = static_cast<QObject*>(const_cast<void*>(objectId))) {
                        emit context->objectPathReady(object);
                    }
                });
            };
        } else if (job.kind == Job::Kind::Directory && job.action == Job::Action::Remove) {
            const QString oldDir = job.oldPath;
            if (!oldDir.isEmpty()) {
                const QString prefix = oldDir + QStringLiteral("/");
                for (auto it = m_objectStates.begin(); it != m_objectStates.end();) {
                    const QString currentPath = it.value().currentPath;
                    if (currentPath == oldDir || currentPath.startsWith(prefix)) {
                        it = m_objectStates.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        m_pendingJobs.append(job);

        if (activeOperation != nullptr) {
            maybeStartPendingFileJobs(context);
        } else {
            ensureSaveFlushScheduled(context);
        }
    }

    void addExplicitFileSystemJob(Job job, cwSaveLoad* context) {
        if (retiring) {
            return;
        }

        // Explicit jobs are already path-resolved and don't track object state; move jobs here
        // can race with path updates and break path-ready notifications. Use addFileSystemJob for moves.
        Q_ASSERT(job.action != Job::Action::Move);

        const QString dataRootName = context->dataRoot();
        const QString dataRootPath = dataRootName.isEmpty()
                ? context->projectRootDir().absolutePath()
                : context->projectRootDir().absoluteFilePath(dataRootName);
        job.dataRoot = normalizeQueuedPath(QDir(dataRootPath).absolutePath());
        if (!job.path.isEmpty()) {
            job.path = normalizeQueuedPath(job.path);
        }
        if (!job.oldPath.isEmpty()) {
            job.oldPath = normalizeQueuedPath(job.oldPath);
        }

        m_pendingJobs.append(job);

        if (activeOperation != nullptr) {
            maybeStartPendingFileJobs(context);
        } else {
            ensureSaveFlushScheduled(context);
        }
    }

    QString absolutePathFor(const cwSaveLoad* context, const QObject* object) const {
        if (auto note = qobject_cast<const cwNote*>(object)) {
            return context->absolutePathPrivate(note);
        }
        if (auto lidar = qobject_cast<const cwNoteLiDAR*>(object)) {
            return context->absolutePathPrivate(lidar);
        }
        if (auto trip = qobject_cast<const cwTrip*>(object)) {
            return context->absolutePathPrivate(trip);
        }
        if (auto cave = qobject_cast<const cwCave*>(object)) {
            return context->absolutePathPrivate(cave);
        }
        if(auto region = qobject_cast<const cwCavingRegion*>(object)) {
            return projectFileName;
        }
        return QString();
    }

    static QString normalizedAbsolutePath(const QString& path)
    {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    ObjectState& stateFor(const void* object) {
        return m_objectStates[object];
    }

    static LoadedPathIndex buildLoadedPathIndex(const cwCavingRegionData& loadedRegion)
    {
        LoadedPathIndex index;
        for (const cwCaveData& caveData : loadedRegion.caves) {
            if (caveData.id.isNull()) {
                continue;
            }
            index.caveNameById.insert(caveData.id, caveData.name);

            for (const cwTripData& tripData : caveData.trips) {
                if (tripData.id.isNull()) {
                    continue;
                }
                index.tripPartsById.insert(tripData.id, LoadedTripPathParts {caveData.name, tripData.name});

                for (const cwNoteData& noteData : tripData.noteModel.notes) {
                    if (noteData.id.isNull()) {
                        continue;
                    }
                    index.notePartsById.insert(noteData.id,
                                               LoadedNotePathParts {caveData.name, tripData.name, noteData.name});
                }

                for (const cwNoteLiDARData& noteData : tripData.noteLiDARModel.notes) {
                    if (noteData.id.isNull()) {
                        continue;
                    }
                    index.lidarPartsById.insert(noteData.id,
                                                LoadedLiDARPathParts {caveData.name, tripData.name, noteData.name});
                }
            }
        }

        return index;
    }

    void seedStatePathFromLoaded(const void* objectId, const QString& absolutePath)
    {
        if (objectId == nullptr || absolutePath.isEmpty()) {
            return;
        }

        auto& state = m_objectStates[objectId];
        const QString normalizedLoadedPath = normalizeQueuedPath(absolutePath);
        state.loadedPath = normalizedLoadedPath;
        if (state.currentPath.isEmpty()) {
            state.currentPath = normalizedLoadedPath;
            return;
        }

        const QString normalizedCurrentPath = normalizeQueuedPath(state.currentPath);
        const bool currentExists = QFileInfo::exists(normalizedCurrentPath);
        const bool loadedExists = QFileInfo::exists(normalizedLoadedPath);

        // Keep the existing path when it still exists on disk; this preserves local-branch
        // source paths needed for reconcile move jobs. Seed from loaded only when the current
        // source is missing.
        if (!currentExists && loadedExists) {
            state.currentPath = normalizedLoadedPath;
            return;
        }

        state.currentPath = normalizedCurrentPath;
    }

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

    void resetObjectStates(cwSaveLoad* context) {
        m_objectStates.clear();

        auto addObjects = [this, context](auto objects) {
            for(const auto object : objects) {
                auto& state = stateFor(object);
                state.currentPath = normalizeQueuedPath(absolutePathFor(context, object));
                state.loadedPath.clear();
            }
        };

        addObjects(m_regionTreeModel->all<cwCave*>(QModelIndex(), &cwRegionTreeModel::cave));
        addObjects(m_regionTreeModel->all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip));
        addObjects(m_regionTreeModel->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note));
        addObjects(m_regionTreeModel->all<cwNoteLiDAR*>(QModelIndex(), &cwRegionTreeModel::noteLiDAR));
    }

    void seedObjectStatesFromLoadedData(cwSaveLoad* context,
                                        const cwCavingRegionData& loadedRegion,
                                        const QString& loadedDataRootName)
    {
        if (context == nullptr || m_regionTreeModel->cavingRegion() == nullptr) {
            return;
        }

        QString dataRootName = loadedDataRootName;
        if (dataRootName.isEmpty()) {
            dataRootName = defaultDataRoot(loadedRegion.name);
        }

        const QDir baseDataRootDir = dataRootName.isEmpty()
                ? context->projectRootDir()
                : QDir(context->projectRootDir().absoluteFilePath(dataRootName));
        const LoadedPathIndex loadedPathIndex = buildLoadedPathIndex(loadedRegion);

        for (cwCave* cave : m_regionTreeModel->all<cwCave*>(QModelIndex(), &cwRegionTreeModel::cave)) {
            if (cave == nullptr || cave->id().isNull()) {
                continue;
            }
            const auto caveNameIt = loadedPathIndex.caveNameById.constFind(cave->id());
            if (caveNameIt == loadedPathIndex.caveNameById.constEnd()) {
                continue;
            }

            const QString caveDirName = cwSaveLoad::sanitizeFileName(caveNameIt.value());
            const QString caveFileName = cwSaveLoad::sanitizeFileName(caveNameIt.value() + QStringLiteral(".cwcave"));
            seedStatePathFromLoaded(cave, baseDataRootDir.filePath(QDir(caveDirName).filePath(caveFileName)));
        }

        for (cwTrip* trip : m_regionTreeModel->all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip)) {
            if (trip == nullptr || trip->id().isNull()) {
                continue;
            }
            const auto partsIt = loadedPathIndex.tripPartsById.constFind(trip->id());
            if (partsIt == loadedPathIndex.tripPartsById.constEnd()) {
                continue;
            }

            const QString caveDirName = cwSaveLoad::sanitizeFileName(partsIt->caveName);
            const QString tripDirName = cwSaveLoad::sanitizeFileName(partsIt->tripName);
            const QString tripFileName = cwSaveLoad::sanitizeFileName(partsIt->tripName + QStringLiteral(".cwtrip"));
            seedStatePathFromLoaded(trip, baseDataRootDir.filePath(QDir(caveDirName).filePath(
                                                                       QDir(QStringLiteral("trips")).filePath(
                                                                           QDir(tripDirName).filePath(tripFileName)))));
        }

        for (cwNote* note : m_regionTreeModel->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note)) {
            if (note == nullptr || note->id().isNull()) {
                continue;
            }
            const auto partsIt = loadedPathIndex.notePartsById.constFind(note->id());
            if (partsIt == loadedPathIndex.notePartsById.constEnd()) {
                continue;
            }

            const QString caveDirName = cwSaveLoad::sanitizeFileName(partsIt->caveName);
            const QString tripDirName = cwSaveLoad::sanitizeFileName(partsIt->tripName);
            const QString noteFileName = cwSaveLoad::sanitizeFileName(partsIt->noteName + QStringLiteral(".cwnote"));
            seedStatePathFromLoaded(note, baseDataRootDir.filePath(QDir(caveDirName).filePath(
                                                                       QDir(QStringLiteral("trips")).filePath(
                                                                           QDir(tripDirName).filePath(
                                                                               QDir(QStringLiteral("notes")).filePath(noteFileName))))));
        }

        for (cwNoteLiDAR* note : m_regionTreeModel->all<cwNoteLiDAR*>(QModelIndex(), &cwRegionTreeModel::noteLiDAR)) {
            if (note == nullptr || note->id().isNull()) {
                continue;
            }
            const auto partsIt = loadedPathIndex.lidarPartsById.constFind(note->id());
            if (partsIt == loadedPathIndex.lidarPartsById.constEnd()) {
                continue;
            }

            const QString caveDirName = cwSaveLoad::sanitizeFileName(partsIt->caveName);
            const QString tripDirName = cwSaveLoad::sanitizeFileName(partsIt->tripName);
            const QString noteFileName = cwSaveLoad::sanitizeFileName(partsIt->noteName + QStringLiteral(".cwnote3d"));
            seedStatePathFromLoaded(note, baseDataRootDir.filePath(QDir(caveDirName).filePath(
                                                                       QDir(QStringLiteral("trips")).filePath(
                                                                           QDir(tripDirName).filePath(
                                                                               QDir(QStringLiteral("notes")).filePath(noteFileName))))));
        }
    }

    static QString cleanupPathForDescriptor(const QString& absoluteDescriptorPath)
    {
        if (absoluteDescriptorPath.isEmpty()) {
            return QString();
        }

        const QString normalizedPath = QDir::cleanPath(QFileInfo(absoluteDescriptorPath).absoluteFilePath());
        if (normalizedPath.endsWith(QStringLiteral(".cwcave"), Qt::CaseInsensitive)
                || normalizedPath.endsWith(QStringLiteral(".cwtrip"), Qt::CaseInsensitive)) {
            return QFileInfo(absoluteDescriptorPath).absoluteDir().absolutePath();
        }

        return normalizedPath;
    }

    Monad::ResultBase cleanupStaleLoadedPaths(cwSaveLoad* context)
    {
        if (context == nullptr) {
            return Monad::ResultBase();
        }

        struct RemovalTarget {
            QString path;
            bool isDirectory = false;
        };

        QList<RemovalTarget> targets;
        for (auto it = m_objectStates.cbegin(); it != m_objectStates.cend(); ++it) {
            const ObjectState& state = it.value();
            if (state.loadedPath.isEmpty() || state.currentPath.isEmpty()) {
                continue;
            }

            const QString cleanupLoadedPath = cleanupPathForDescriptor(state.loadedPath);
            const QString cleanupDesiredPath = cleanupPathForDescriptor(state.currentPath);
            if (cleanupLoadedPath.isEmpty() || cleanupDesiredPath.isEmpty()) {
                continue;
            }

            if (QDir::cleanPath(cleanupLoadedPath) == QDir::cleanPath(cleanupDesiredPath)) {
                continue;
            }

            const QFileInfo loadedInfo(cleanupLoadedPath);
            if (!loadedInfo.exists()) {
                continue;
            }

            targets.append(RemovalTarget { QDir::cleanPath(loadedInfo.absoluteFilePath()),
                                           loadedInfo.isDir() });
        }

        if (targets.isEmpty()) {
            return Monad::ResultBase();
        }

        std::sort(targets.begin(), targets.end(), [](const RemovalTarget& lhs, const RemovalTarget& rhs) {
            return lhs.path.size() > rhs.path.size();
        });

        QSet<QString> removedTargets;
        for (const RemovalTarget& target : std::as_const(targets)) {
            if (target.path.isEmpty() || removedTargets.contains(target.path)) {
                continue;
            }

            QFileInfo targetInfo(target.path);
            if (!targetInfo.exists()) {
                continue;
            }

            if (target.isDirectory || targetInfo.isDir()) {
                if (!QDir(target.path).removeRecursively()) {
                    return Monad::ResultBase(QStringLiteral("Failed to remove stale loaded directory: %1")
                                             .arg(target.path));
                }
            } else {
                if (!QFile::remove(target.path)) {
                    return Monad::ResultBase(QStringLiteral("Failed to remove stale loaded file: %1")
                                             .arg(target.path));
                }
            }
            removedTargets.insert(target.path);
        }

        return Monad::ResultBase();
    }

    // Normalize paths for queued jobs in a way that is stable across aliases/symlinks.
    // We cannot canonicalize the full path directly because many queued destinations
    // do not exist yet, and QFileInfo::canonicalFilePath() then returns an empty string.
    // Instead, canonicalize the deepest existing ancestor and append unresolved segments.
    static QString normalizeQueuedPath(const QString& inputPath)
    {
        if (inputPath.isEmpty()) {
            return inputPath;
        }

        QString unresolved = QDir::cleanPath(QFileInfo(inputPath).absoluteFilePath());
        QStringList unresolvedSegments;
        QFileInfo unresolvedInfo(unresolved);

        while (!unresolvedInfo.exists()) {
            const QString segment = unresolvedInfo.fileName();
            const QString parentPath = unresolvedInfo.path();
            if (segment.isEmpty() || parentPath == unresolved) {
                break;
            }

            unresolvedSegments.prepend(segment);
            unresolved = parentPath;
            unresolvedInfo.setFile(unresolved);
        }

        QString normalizedBase = QDir::cleanPath(QFileInfo(unresolved).absoluteFilePath());
        if (unresolvedInfo.exists()) {
            const QString canonicalBase = unresolvedInfo.canonicalFilePath();
            if (!canonicalBase.isEmpty()) {
                normalizedBase = QDir::cleanPath(canonicalBase);
            }
        }

        QString normalized = normalizedBase;
        for (const QString& segment : unresolvedSegments) {
            normalized = QDir(normalized).filePath(segment);
            normalized = QDir::cleanPath(normalized);
        }

        return normalized;
    }


    void saveProtoMessage(
            cwSaveLoad* context,
            std::unique_ptr<const google::protobuf::Message> message,
            const void* objectId
            );

    // Returns a map of objectId -> ordered job indices for all non-null objectIds.
    QHash<const void*, QList<int>> jobIndicesByObjectId() const
    {
        QHash<const void*, QList<int>> result;
        for (int i = 0; i < m_pendingJobs.size(); ++i) {
            if (m_pendingJobs[i].objectId != nullptr) {
                result[m_pendingJobs[i].objectId].append(i);
            }
        }
        return result;
    }

    // Rule 1: For each object with multiple WriteFile jobs, drop all but the last.
    void dropRedundantWrites(const QList<int>& indices, QSet<int>& indicesToDrop) const
    {
        int lastWriteIdx = -1;
        for (int idx : indices) {
            if (m_pendingJobs[idx].action == Job::Action::WriteFile
                    && !indicesToDrop.contains(idx)) {
                if (lastWriteIdx != -1) {
                    indicesToDrop.insert(lastWriteIdx);
                }
                lastWriteIdx = idx;
            }
        }
    }

    // Rule 2: If a Remove exists for an object, all its pending WriteFile jobs are moot.
    void dropWritesSupersededByRemove(const QList<int>& indices, QSet<int>& indicesToDrop) const
    {
        bool hasRemove = false;
        for (int idx : indices) {
            if (m_pendingJobs[idx].action == Job::Action::Remove) {
                hasRemove = true;
                break;
            }
        }
        if (!hasRemove) {
            return;
        }
        for (int idx : indices) {
            if (m_pendingJobs[idx].action == Job::Action::WriteFile) {
                indicesToDrop.insert(idx);
            }
        }
    }

    // Rule 3: Collapse sequential Moves for the same object and kind (A→B + B→C becomes A→C).
    void collapseSequentialMoves(const QList<int>& indices, QSet<int>& indicesToDrop)
    {
        for (Job::Kind kind : {Job::Kind::File, Job::Kind::Directory}) {
            QList<int> moveIndices;
            for (int idx : indices) {
                if (m_pendingJobs[idx].action == Job::Action::Move
                        && m_pendingJobs[idx].kind == kind) {
                    moveIndices.append(idx);
                }
            }
            if (moveIndices.size() > 1) {
                m_pendingJobs[moveIndices.last()].oldPath =
                        m_pendingJobs[moveIndices.first()].oldPath;
                for (int i = 0; i < moveIndices.size() - 1; ++i) {
                    indicesToDrop.insert(moveIndices[i]);
                }
            }
        }
    }

    void compressPendingJobs()
    {
        if (m_pendingJobs.size() <= 1) {
            return;
        }

        QSet<int> indicesToDrop;

        const auto indexMap = jobIndicesByObjectId();
        for (auto it = indexMap.cbegin(); it != indexMap.cend(); ++it) {
            const QList<int>& indices = it.value();
            if (indices.size() <= 1) {
                continue;
            }
            dropWritesSupersededByRemove(indices, indicesToDrop);
            dropRedundantWrites(indices, indicesToDrop);
            collapseSequentialMoves(indices, indicesToDrop);
        }

        if (indicesToDrop.isEmpty()) {
            return;
        }

        QList<Job> compressed;
        compressed.reserve(m_pendingJobs.size() - indicesToDrop.size());
        for (int i = 0; i < m_pendingJobs.size(); ++i) {
            if (!indicesToDrop.contains(i)) {
                compressed.append(m_pendingJobs[i]);
            }
        }
        m_pendingJobs = std::move(compressed);
    }

    void execFileSystemJobs(cwSaveLoad* context) {
        if (m_pendingJobs.isEmpty()) {
            return;
        }

        if (m_pendingJobsDeferred.future().isFinished()) {
            compressPendingJobs();
            m_pendingJobsDeferred = {};
        }

        if (m_pendingJobs.isEmpty()) {
            m_pendingJobsDeferred.complete();
            return;
        }

        Q_ASSERT(m_pendingJobsDeferred.future().isRunning());

        Job job = m_pendingJobs.takeFirst();

        auto future = QtConcurrent::run(&m_saveThreadPool, [job]() {
            return job.execute();
        });

        AsyncFuture::observe(future).context(context, [this, context, job, future]() {
            const auto data = future.result();
            if(data.hasError()) {
                m_pendingSaveJobErrors.append(
                            QStringLiteral("%1 (%2)")
                            .arg(data.errorMessage(), job.toString()));
            }
            if (job.onDone) {
                job.onDone(data);
            }
            if (m_pendingJobs.isEmpty()) {
                m_pendingJobsDeferred.complete();
            } else {
                //Start another job
                execFileSystemJobs(context);
            }
        });
    }

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
