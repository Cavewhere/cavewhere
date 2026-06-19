#include "cwSaveLoadPrivate.h"

//Our includes
#include "cwProtoUtils.h"
#include "cwSurveyChunk.h"

//Google protobuffer
#include "cavewhere.pb.h"
#include <google/protobuf/util/json_util.h>

//Qt includes
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {

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

} // anonymous namespace

QString cwSaveLoadPrivate::defaultDataRoot(const QString& projectName)
{
    return cwSaveLoad::sanitizeFileName(projectName);
}

bool cwSaveLoadPrivate::fsRenameDir(const QString& src, const QString& dst)
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

bool cwSaveLoadPrivate::fsRenameFile(const QString& src, const QString& dst)
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

bool cwSaveLoadPrivate::fsRemoveFile(const QString& path)
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

bool cwSaveLoadPrivate::fsRemoveDirRecursive(const QString& path)
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

namespace {
bool defaultRenameDirectory(const QString& src, const QString& dst)
{
    return QDir().rename(src, dst);
}
}

cwSaveLoadPrivate::RenameDirectoryFn cwSaveLoadPrivate::renameDirectoryFn = &defaultRenameDirectory;

Monad::ResultBase cwSaveLoadPrivate::copyDirectoryRecursively(const QDir& sourceDir,
                                                              const QDir& destinationDir)
{
    if (!sourceDir.exists()) {
        return Monad::ResultBase(QStringLiteral("Source directory '%1' does not exist.").arg(sourceDir.absolutePath()));
    }

    if (QFileInfo::exists(destinationDir.absolutePath())) {
        return Monad::ResultBase(QStringLiteral("Destination '%1' already exists.").arg(destinationDir.absolutePath()));
    }

    if (!QDir().mkpath(destinationDir.absolutePath())) {
        return Monad::ResultBase(QStringLiteral("Cannot create directory '%1'.").arg(destinationDir.absolutePath()));
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot
                                                          | QDir::AllEntries
                                                          | QDir::Hidden
                                                          | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString targetPath = destinationDir.absoluteFilePath(entry.fileName());
        // Refuse symlinks: entry.isDir() returns true for symlinks-to-dirs,
        // so naive recursion would escape the source tree (e.g. a symlink
        // pointing to '/' would copy the entire filesystem). Project
        // directories aren't expected to contain symlinks.
        if (entry.isSymLink()) {
            return Monad::ResultBase(
                QStringLiteral("Refusing to copy symlink '%1'.").arg(entry.absoluteFilePath()));
        }
        if (entry.isDir()) {
            auto result = copyDirectoryRecursively(QDir(entry.absoluteFilePath()), QDir(targetPath));
            if (result.hasError()) {
                return result;
            }
        } else {
            if (QFileInfo::exists(targetPath) && !QFile::remove(targetPath)) {
                return Monad::ResultBase(QStringLiteral("Can't overwrite '%1'.").arg(targetPath));
            }
            if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
                return Monad::ResultBase(QStringLiteral("Can't copy '%1' to '%2'.").arg(entry.absoluteFilePath(), targetPath));
            }
        }
    }

    return Monad::ResultBase();
}

Monad::ResultBase cwSaveLoadPrivate::validatePathSafeForRecursiveRemoval(const QString& path)
{
    if (path.isEmpty()) {
        return Monad::ResultBase(QStringLiteral("path is empty"));
    }

    const QFileInfo info(path);
    if (!info.isAbsolute()) {
        return Monad::ResultBase(QStringLiteral("path is not absolute: '%1'").arg(path));
    }

    if (path.contains(QStringLiteral("/.."))
        || path.contains(QStringLiteral("\\.."))) {
        return Monad::ResultBase(QStringLiteral("path contains '..': '%1'").arg(path));
    }

    const QString cleaned = QDir::cleanPath(info.absoluteFilePath());
    if (QDir(cleaned).isRoot()) {
        return Monad::ResultBase(QStringLiteral("refusing to remove filesystem root: '%1'").arg(cleaned));
    }

    if (!info.exists()) {
        return Monad::ResultBase(QStringLiteral("path does not exist: '%1'").arg(path));
    }

    if (!info.isDir()) {
        return Monad::ResultBase(QStringLiteral("path is not a directory: '%1'").arg(path));
    }

    return Monad::ResultBase();
}

Monad::ResultBase cwSaveLoadPrivate::moveDirectoryRobust(const QString& sourcePath,
                                                         const QString& destinationPath)
{
    // Reject obviously bad inputs up-front. We will eventually delete sourcePath
    // (in the copy-fallback branch), so refuse anything that's empty, relative,
    // or a filesystem root. This applies to both the rename and copy branches.
    if (sourcePath.isEmpty() || destinationPath.isEmpty()) {
        return Monad::ResultBase(QStringLiteral("Source or destination path is empty."));
    }

    const QString cleanSrc = QDir::cleanPath(QFileInfo(sourcePath).absoluteFilePath());
    const QString cleanDst = QDir::cleanPath(QFileInfo(destinationPath).absoluteFilePath());

    // Reject overlapping source and destination. If destination equals source
    // or sits inside source, deleting the source after copy would also wipe
    // the freshly-copied destination.
    if (cleanDst == cleanSrc
        || cleanDst.startsWith(cleanSrc + QStringLiteral("/"))) {
        return Monad::ResultBase(
            QStringLiteral("Destination '%1' is inside source '%2'; refusing to move.")
                .arg(destinationPath, sourcePath));
    }

    // Fast path: same-filesystem rename.
    if (renameDirectoryFn(sourcePath, destinationPath)) {
        return Monad::ResultBase();
    }

    // Cross-filesystem (e.g. pcloud) — fall back to copy + recursive delete.
    auto copyResult = copyDirectoryRecursively(QDir(sourcePath), QDir(destinationPath));
    if (copyResult.hasError()) {
        return copyResult;
    }

    // Pre-deletion safety checks. We do not want a misuse of this helper
    // (empty path, root, non-existent dir, ...) to wipe a user's filesystem.
    auto safety = validatePathSafeForRecursiveRemoval(sourcePath);
    if (safety.hasError()) {
        return Monad::ResultBase(
            QStringLiteral("Saved to '%1' but refused to remove source: %2")
                .arg(destinationPath, safety.errorMessage()),
            Monad::ResultBase::Warning);
    }

    // Final sanity check before deleting: the copy reported success, so the
    // destination must exist as a directory. If it doesn't, something is
    // very wrong — bail without touching the source.
    if (!QFileInfo(destinationPath).isDir()) {
        return Monad::ResultBase(
            QStringLiteral("Copy reported success but destination '%1' is not a directory; "
                           "leaving source '%2' intact.")
                .arg(destinationPath, sourcePath));
    }

    if (!fsRemoveDirRecursive(sourcePath)) {
        // Destination is intact; only the source cleanup failed. Return a Warning
        // so the message rides up through the result chain — hasError() is false
        // for ResultBase::Warning, so callers don't treat it as a failure.
        return Monad::ResultBase(
            QStringLiteral("Saved to '%1' but couldn't remove temp source '%2'.")
                .arg(destinationPath, sourcePath),
            Monad::ResultBase::Warning);
    }
    return Monad::ResultBase();
}

// ============================================================================
// Job static helpers and methods
// ============================================================================

QString cwSaveLoadPrivate::Job::toString() const {
    const QString objectHex = QString::number(reinterpret_cast<quintptr>(objectId), 16);
    const auto* writePayload = std::get_if<WriteFilePayload>(&payload);
    const auto* bytesPayload = std::get_if<WriteBytesPayload>(&payload);
    const auto* copyPayload = std::get_if<CopyFilePayload>(&payload);
    const auto* customPayload = std::get_if<CustomPayload>(&payload);
    const QString sourcePath = copyPayload ? copyPayload->sourcePath : QString();
    return QStringLiteral("Job{action=%1 kind=%2 tag='%3' objectId=0x%4 oldPath='%5' newPath='%6' sourcePath='%7' dataRoot='%8' hasMessage=%9 bytes=%10 hasCustom=%11}")
            .arg(QString::fromLatin1(actionName(action)),
                 QString::fromLatin1(kindName(kind)),
                 tag,
                 objectHex,
                 oldPath,
                 path,
                 sourcePath,
                 dataRoot)
            .arg(writePayload && writePayload->message != nullptr)
            .arg(bytesPayload ? bytesPayload->bytes.size() : -1)
            .arg(customPayload && customPayload->action != nullptr);
}

Monad::ResultBase cwSaveLoadPrivate::Job::ensureDirectoryExists(const QString& directoryPath)
{
    QDir directory;
    if (!directory.mkpath(directoryPath)) {
        return Monad::ResultBase(QStringLiteral("Failed to create directory: %1").arg(directoryPath));
    }
    return Monad::ResultBase();
}

Monad::ResultBase cwSaveLoadPrivate::Job::moveOrReplaceFile(const QString& sourcePath, const QString& destinationPath)
{
    if (QFileInfo::exists(destinationPath)) {
        if (QFileInfo(destinationPath).isDir()) {
            return Monad::ResultBase(QStringLiteral("Failed to move %1 -> %2 (destination is a directory)")
                                     .arg(sourcePath, destinationPath));
        }
        if (!cwSaveLoadPrivate::fsRemoveFile(destinationPath)) {
            return Monad::ResultBase(QStringLiteral("Failed to replace file: %1").arg(destinationPath));
        }
    } else {
        const QString parentDirectoryPath = QFileInfo(destinationPath).absolutePath();
        const auto ensureParentResult = ensureDirectoryExists(parentDirectoryPath);
        if (ensureParentResult.hasError()) {
            return ensureParentResult;
        }
    }

    if (!cwSaveLoadPrivate::fsRenameFile(sourcePath, destinationPath)) {
        return Monad::ResultBase(QStringLiteral("Failed to move file %1 -> %2").arg(sourcePath, destinationPath));
    }

    return Monad::ResultBase();
}

Monad::ResultBase cwSaveLoadPrivate::Job::mergeDirectoryContents(const QString& sourceDirectoryPath,
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

                if (!cwSaveLoadPrivate::fsRemoveDirRecursive(sourceEntryPath)) {
                    return Monad::ResultBase(QStringLiteral("Failed to remove directory after merge: %1").arg(sourceEntryPath));
                }
            } else if (!cwSaveLoadPrivate::fsRenameDir(sourceEntryPath, destinationEntryPath)) {
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

Monad::ResultBase cwSaveLoadPrivate::Job::moveOrMergeDirectory(const QString& sourcePath, const QString& destinationPath)
{
    if (QFileInfo::exists(destinationPath) && !QFileInfo(destinationPath).isDir()) {
        return Monad::ResultBase(QStringLiteral("Failed to move %1 -> %2 (destination is not a directory)")
                                 .arg(sourcePath, destinationPath));
    }

    if (!QFileInfo::exists(destinationPath)) {
        if (!cwSaveLoadPrivate::fsRenameDir(sourcePath, destinationPath)) {
            return Monad::ResultBase(QStringLiteral("Failed to move %1 -> %2").arg(sourcePath, destinationPath));
        }
        return Monad::ResultBase();
    }

    const auto mergeResult = mergeDirectoryContents(sourcePath, destinationPath);
    if (mergeResult.hasError()) {
        return mergeResult;
    }

    if (!cwSaveLoadPrivate::fsRemoveDirRecursive(sourcePath)) {
        return Monad::ResultBase(QStringLiteral("Failed to remove directory after merge: %1").arg(sourcePath));
    }

    return Monad::ResultBase();
}

Monad::ResultBase cwSaveLoadPrivate::Job::executeMoveJob(Kind kind, const QString& oldPath, const QString& newPath)
{
    if (kind == Kind::Directory) {
        return moveOrMergeDirectory(oldPath, newPath);
    }

    return moveOrReplaceFile(oldPath, newPath);
}

Monad::ResultBase cwSaveLoadPrivate::Job::execute() const {
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

    // Allow jobs anywhere inside the project root — this subsumes dataRoot
    // (always a subdir) and covers project-level siblings like the GIS Layers
    // folder, plus the project file itself.
    auto ensureInsideRoot = [&](const QString& targetPath) -> Monad::ResultBase {
        if (!isInsideRoot(projectRoot, targetPath)) [[unlikely]] {
            return Monad::ResultBase(QStringLiteral("Path outside project root: %1").arg(targetPath));
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
        {
            auto rootCheck = ensureInsideRoot(path);
            if (rootCheck.hasError()) {
                return rootCheck;
            }
        }
        // Pre-serialized bytes (e.g. a palette glyph/brush JSON) are written
        // verbatim, so the on-disk form matches its single-sourced serializer.
        if (auto* bytesPayload = std::get_if<WriteBytesPayload>(&payload)) {
            return Monad::mbind(ensurePathForFile(path), [&](Monad::ResultBase /*result*/) {
                QSaveFile file(path);
                if (!file.open(QFile::WriteOnly)) {
                    return Monad::ResultBase(QStringLiteral("Failed to open file for writing: %1").arg(path));
                }
                file.write(bytesPayload->bytes);
                if (!file.commit()) {
                    return Monad::ResultBase(QStringLiteral("Failed to write file: %1").arg(path));
                }
                return Monad::ResultBase();
            });
        }
        auto* writePayload = std::get_if<WriteFilePayload>(&payload);
        if (!writePayload || !writePayload->message) {
            return Monad::ResultBase(QStringLiteral("Missing message for WriteFile job: %1").arg(path));
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
            if (!file.commit()) {
                return Monad::ResultBase(QStringLiteral("Failed to write file: %1").arg(path));
            }
            return Monad::ResultBase();
        });
    }
    case Action::Copy: {
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

bool cwSaveLoadPrivate::Job::lessThan(const Job& a, const Job& b) {
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

// ============================================================================
// cwSaveLoadPrivate methods
// ============================================================================

cwSaveLoadPrivate::cwSaveLoadPrivate() {
    m_pendingJobsDeferred.complete();
    m_saveThreadPool.setMaxThreadCount(1);
}

bool cwSaveLoadPrivate::trackConnected(const QObject* object)
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

bool cwSaveLoadPrivate::isTrackedConnected(const QObject* object) const
{
    return object != nullptr && connectedObjects.contains(object);
}

void cwSaveLoadPrivate::trackDisconnected(const QObject* object)
{
    if (object == nullptr) {
        return;
    }
    connectedObjects.remove(object);
}

bool cwSaveLoadPrivate::hasQueuedOperationType(Operation::Type type) const
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

void cwSaveLoadPrivate::maybeStartPendingFileJobs(cwSaveLoad* context)
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

void cwSaveLoadPrivate::ensureSaveFlushScheduled(cwSaveLoad* context)
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

QString cwSaveLoadPrivate::takePendingSaveJobErrors()
{
    if (m_pendingSaveJobErrors.isEmpty()) {
        return QString();
    }

    const QString joined = m_pendingSaveJobErrors.join(QLatin1Char('\n'));
    m_pendingSaveJobErrors.clear();
    return joined;
}

QFuture<Monad::ResultBase> cwSaveLoadPrivate::enqueueOperation(cwSaveLoad* context,
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

void cwSaveLoadPrivate::runNextOperation(cwSaveLoad* context)
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
        const QString cancelReason = pendingCancellationReason.isEmpty()
                ? QStringLiteral("Operation canceled.")
                : pendingCancellationReason;
        op->deferred.complete(Monad::ResultBase(cancelReason));
        activeOperation.reset();
        runNextOperation(context);
        return;
    }

    pendingCancellationReason.clear();
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

void cwSaveLoadPrivate::cancelPendingOperations(const QString& reason)
{
    ++operationGeneration;
    pendingCancellationReason = reason;

    if (activeOperation && !activeOperation->deferred.future().isFinished()) {
        activeOperation->deferred.complete(Monad::ResultBase(reason));
    }

    while (!operationQueue.isEmpty()) {
        auto op = operationQueue.dequeue();
        op->deferred.complete(Monad::ResultBase(reason));
    }
}

void cwSaveLoadPrivate::addFileSystemJob(Job job, cwSaveLoad* context) {
    if (retiring) {
        return;
    }

    Q_ASSERT(job.path.isEmpty());
    Q_ASSERT(job.oldPath.isEmpty());

    const QString dataRootName = context->dataRoot();
    const QString projectRootPath = context->projectRootDir().absolutePath();
    const QString dataRootPath = dataRootName.isEmpty()
            ? projectRootPath
            : context->projectRootDir().absoluteFilePath(dataRootName);
    job.dataRoot = normalizeQueuedPath(QDir(dataRootPath).absolutePath());
    job.projectRoot = normalizeQueuedPath(QDir(projectRootPath).absolutePath());

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

void cwSaveLoadPrivate::addExplicitFileSystemJob(Job job, cwSaveLoad* context) {
    if (retiring) {
        return;
    }

    // Explicit jobs are already path-resolved and don't track m_objectStates.
    // Move jobs here must therefore carry both oldPath and path set by the
    // caller (no per-object path lookup will fill them in), and they will
    // not emit objectPathReady — the destination must be a sibling artifact
    // for which no path-ready signal is meaningful (e.g. cwLazLayer's .laz
    // source file, which is not the canonical metadata file cwSaveLoad
    // tracks). For metadata-file moves, use addFileSystemJob instead so
    // path-ready emission is wired correctly.
    Q_ASSERT(job.action != Job::Action::Move
             || (!job.oldPath.isEmpty() && !job.path.isEmpty()));

    const QString dataRootName = context->dataRoot();
    const QString projectRootPath = context->projectRootDir().absolutePath();
    const QString dataRootPath = dataRootName.isEmpty()
            ? projectRootPath
            : context->projectRootDir().absoluteFilePath(dataRootName);
    job.dataRoot = normalizeQueuedPath(QDir(dataRootPath).absolutePath());
    job.projectRoot = normalizeQueuedPath(QDir(projectRootPath).absolutePath());
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

QString cwSaveLoadPrivate::absolutePathFor(const cwSaveLoad* context, const QObject* object) const {
    if (auto note = qobject_cast<const cwNote*>(object)) {
        return context->absolutePathPrivate(note);
    }
    if (auto lidar = qobject_cast<const cwNoteLiDAR*>(object)) {
        return context->absolutePathPrivate(lidar);
    }
    if (auto sketch = qobject_cast<const cwSketch*>(object)) {
        return context->absolutePathPrivate(sketch);
    }
    if (auto laz = qobject_cast<const cwLazLayer*>(object)) {
        return context->absolutePathPrivate(laz);
    }
    if (auto trip = qobject_cast<const cwTrip*>(object)) {
        return context->absolutePathPrivate(trip);
    }
    if (auto cave = qobject_cast<const cwCave*>(object)) {
        return context->absolutePathPrivate(cave);
    }
    if(auto region = qobject_cast<const cwCavingRegion*>(object)) {
        Q_UNUSED(region);
        return projectFileName;
    }
    return QString();
}

QString cwSaveLoadPrivate::normalizedAbsolutePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

cwSaveLoadPrivate::LoadedPathIndex cwSaveLoadPrivate::buildLoadedPathIndex(const cwCavingRegionData& loadedRegion)
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

            for (const cwSketchData& sketchData : tripData.sketchModel.notes) {
                if (sketchData.id.isNull()) {
                    continue;
                }
                index.sketchPartsById.insert(sketchData.id,
                                             LoadedSketchPathParts {caveData.name, tripData.name, sketchData.name});
            }
        }
    }

    return index;
}

void cwSaveLoadPrivate::seedStatePathFromLoaded(const void* objectId, const QString& absolutePath)
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

void cwSaveLoadPrivate::resetObjectStates(cwSaveLoad* context) {
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
    addObjects(m_regionTreeModel->all<cwSketch*>(QModelIndex(), &cwRegionTreeModel::sketch));
}

void cwSaveLoadPrivate::seedObjectStatesFromLoadedData(cwSaveLoad* context,
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

    for (cwSketch* sketch : m_regionTreeModel->all<cwSketch*>(QModelIndex(), &cwRegionTreeModel::sketch)) {
        if (sketch == nullptr || sketch->id().isNull()) {
            continue;
        }
        const auto partsIt = loadedPathIndex.sketchPartsById.constFind(sketch->id());
        if (partsIt == loadedPathIndex.sketchPartsById.constEnd()) {
            continue;
        }

        const QString caveDirName = cwSaveLoad::sanitizeFileName(partsIt->caveName);
        const QString tripDirName = cwSaveLoad::sanitizeFileName(partsIt->tripName);
        const QString sketchFileName = cwSaveLoad::sanitizeFileName(partsIt->sketchName + QStringLiteral(".cwsketch"));
        seedStatePathFromLoaded(sketch, baseDataRootDir.filePath(QDir(caveDirName).filePath(
                                                                     QDir(QStringLiteral("trips")).filePath(
                                                                         QDir(tripDirName).filePath(
                                                                             QDir(QStringLiteral("notes")).filePath(sketchFileName))))));
    }
}

QString cwSaveLoadPrivate::cleanupPathForDescriptor(const QString& absoluteDescriptorPath)
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

Monad::ResultBase cwSaveLoadPrivate::cleanupStaleLoadedPaths(cwSaveLoad* context)
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

QString cwSaveLoadPrivate::normalizeQueuedPath(const QString& inputPath)
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

Monad::ResultBase cwSaveLoadPrivate::ensurePathForFile(const QString& filePath) {
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

QHash<cwSaveLoadPrivate::GroupKey, QList<int>> cwSaveLoadPrivate::jobIndicesByGroup() const
{
    QHash<GroupKey, QList<int>> result;
    for (int i = 0; i < m_pendingJobs.size(); ++i) {
        const Job& job = m_pendingJobs.at(i);
        if (job.objectId != nullptr) {
            result[GroupKey{job.objectId, job.tag}].append(i);
        }
    }
    return result;
}

void cwSaveLoadPrivate::dropRedundantWrites(const QList<int>& indices, QSet<int>& indicesToDrop) const
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

void cwSaveLoadPrivate::dropWritesSupersededByRemove(const QList<int>& indices, QSet<int>& indicesToDrop) const
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

void cwSaveLoadPrivate::collapseSequentialMoves(const QList<int>& indices, QSet<int>& indicesToDrop)
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

void cwSaveLoadPrivate::compressPendingJobs()
{
    if (m_pendingJobs.size() <= 1) {
        return;
    }

    QSet<int> indicesToDrop;

    const auto indexMap = jobIndicesByGroup();
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

void cwSaveLoadPrivate::execFileSystemJobs(cwSaveLoad* context) {
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

// ============================================================================
// Existing out-of-line definitions (preserved from prior extraction)
// ============================================================================

QList<cwSurveyChunkData> cwSaveLoadPrivate::fromProtoSurveyChunks(const google::protobuf::RepeatedPtrField<CavewhereProto::SurveyChunk> &protoList)
{
    QList<cwSurveyChunkData> chunks;

    if(!protoList.empty()) {
        chunks.reserve(protoList.size());

        for (const auto& protoChunk : protoList) {
            chunks.append(cwProtoUtils::fromProtoSurveyChunk(protoChunk));
        }
    }

    return chunks;
}

void cwSaveLoadPrivate::saveProtoMessage(
        cwSaveLoad* context,
        std::unique_ptr<const google::protobuf::Message> message,
        const void* objectId)
{
    Job job(
                objectId,
                Job::Kind::File,
                Job::Action::WriteFile,
                std::shared_ptr<const google::protobuf::Message>(std::move(message))
                );

    addFileSystemJob(job, context);
}
