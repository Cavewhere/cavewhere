/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterlineSync.h"

//Our includes
#include "cwSaveLoad.h"

//AsyncFuture
#include <asyncfuture.h>

//Qt includes
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

namespace {

constexpr const char* kParentDir = "..";
constexpr const char* kParentDirPrefix = "../";

bool relativeEscapesAttachmentDir(const QString& rel)
{
    // QDir::cleanPath collapses redundant separators and "./" segments
    // but preserves leading "../" - any survivor means the dep would
    // land outside the attachment dir.
    const QString cleaned = QDir::cleanPath(rel);
    if (cleaned == QLatin1String(kParentDir)) {
        return true;
    }
    if (cleaned.startsWith(QLatin1String(kParentDirPrefix))) {
        return true;
    }
    return false;
}

bool destinationMatchesSource(const QFileInfo& srcInfo, const QFileInfo& dstInfo)
{
    // Same predicate as enqueueExternalCenterlineCopyIfNewer's
    // in-job check, lifted to plan time so reconcile can skip the
    // enqueue entirely when nothing needs to happen. The duplication
    // is intentional defence: a TOCTOU race between plan and execute
    // is still caught in-job.
    if (!dstInfo.exists()) {
        return false;
    }
    if (srcInfo.size() != dstInfo.size()) {
        return false;
    }
    return srcInfo.lastModified() <= dstInfo.lastModified();
}

QString canonicalPathOrCleaned(const QString& path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return canonical;
    }

    // Nothing on disk to canonicalize. Walk up to the nearest existing
    // ancestor, canonicalize that, and re-append the tail, so a symlinked
    // directory above a file that is not there still resolves to where the
    // file would be.
    //
    // The walk leaves any ".." in place for the filesystem to resolve.
    // Collapsing it textually first would step over a symlink the OS
    // would have followed: with "<dir>/link" pointing elsewhere,
    // "<dir>/link/../x" lexically reads as "<dir>/x" but really lives
    // beside the link's target. That is also why the walk starts from the
    // raw string rather than absoluteFilePath(), which does the collapsing
    // itself. Walking with QFileInfo::path() keeps the loop off bare drive
    // stubs like "C:", which name the current directory on that drive
    // rather than its root.
    const QString absolute = QDir::isAbsolutePath(path)
        ? path
        : QDir::current().filePath(path);

    QFileInfo walk(absolute);
    QString tail;
    while (true) {
        const QString parent = walk.path();
        if (parent.isEmpty() || parent == walk.filePath()) {
            // Reached the root without finding anything that exists.
            return QDir::cleanPath(info.absoluteFilePath());
        }
        const QString name = walk.fileName();
        tail = tail.isEmpty() ? name : name + QLatin1Char('/') + tail;

        const QString ancestor = QFileInfo(parent).canonicalFilePath();
        if (!ancestor.isEmpty()) {
            return ancestor.endsWith(QLatin1Char('/'))
                ? ancestor + tail
                : ancestor + QLatin1Char('/') + tail;
        }
        walk = QFileInfo(parent);
    }
}

QStringList enumerateFilesRecursively(const QString& dirPath)
{
    QStringList result;
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return result;
    }
    QDirIterator it(dirPath,
                    QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(it.next());
    }
    // Deterministic order matters for tests; QDirIterator does not
    // guarantee it across platforms.
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

namespace cwExternalCenterlineSync {

ReconcilePlan computePlan(
    const cwExternalCenterlineScanner::ScanResult& scan,
    const QString& attachmentDir,
    CopyPolicy copyPolicy)
{
    ReconcilePlan plan;

    if (scan.dependencies.isEmpty() || attachmentDir.isEmpty()) {
        return plan;
    }

    const QString entryFile = scan.dependencies.first();
    const QFileInfo entryInfo(entryFile);
    const QDir entryDir = entryInfo.absoluteDir();
    const QDir attachmentDirObj(attachmentDir);
    const QString attachmentAbs = attachmentDirObj.absolutePath();

    QSet<QString> expectedSet;
    expectedSet.reserve(scan.dependencies.size());

    for (const QString& source : scan.dependencies) {
        const QString rel = entryDir.relativeFilePath(source);
        if (rel.isEmpty() || relativeEscapesAttachmentDir(rel)) {
            plan.warnings.append(
                QStringLiteral("dependency %1 is not reachable under attachment dir %2 - omitted")
                    .arg(source, attachmentAbs));
            continue;
        }
        const QString dest = QDir::cleanPath(attachmentDirObj.absoluteFilePath(rel));

        plan.expectedFiles.append(dest);
        expectedSet.insert(dest);

        const QFileInfo srcInfo(source);
        const QFileInfo dstInfo(dest);
        if (copyPolicy == CopyPolicy::SkipUpToDate
            && destinationMatchesSource(srcInfo, dstInfo)) {
            continue;
        }
        plan.copies.emplace_back(source, dest);
    }

    // GC: files in attachmentDir not in expected closure.
    const QStringList existing = enumerateFilesRecursively(attachmentAbs);
    for (const QString& path : existing) {
        const QString normalized = QDir::cleanPath(path);
        if (!expectedSet.contains(normalized)) {
            plan.removes.append(normalized);
        }
    }

    return plan;
}

QFuture<Monad::ResultBase> reconcile(
    cwSaveLoad* saveLoad,
    const cwExternalCenterlineScanner::ScanResult& scan,
    const QString& attachmentDir,
    CopyPolicy copyPolicy)
{
    if (!saveLoad) {
        return AsyncFuture::completed(
            Monad::ResultBase(QStringLiteral("reconcile: cwSaveLoad is null")));
    }

    const ReconcilePlan plan = computePlan(scan, attachmentDir, copyPolicy);

    for (const auto& [source, destination] : plan.copies) {
        if (copyPolicy == CopyPolicy::Overwrite) {
            saveLoad->enqueueExternalCenterlineCopyOverwriting(source, destination);
        } else {
            saveLoad->enqueueExternalCenterlineCopyIfNewer(source, destination);
        }
    }
    for (const QString& dead : plan.removes) {
        saveLoad->enqueueExternalCenterlineRemoveFile(dead);
    }

    // Chain on the project-wide "all queued jobs finished" future.
    // Individual job failures surface through the existing cwSaveLoad
    // error channel; the returned future itself reports completion
    // status. Callers that care about per-job errors should observe
    // saveFlushCompleted alongside this future.
    return AsyncFuture::observe(saveLoad->pendingJobsFinished())
        .context(saveLoad, []() {
            return Monad::ResultBase();
        })
        .future();
}

bool isContainedIn(const QString& path, const QString& boundaryDir)
{
    if (path.isEmpty() || boundaryDir.isEmpty()) {
        return false;
    }

    const QString resolved = canonicalPathOrCleaned(path);
    const QString boundary = canonicalPathOrCleaned(boundaryDir);
    if (resolved == boundary) {
        return true;
    }

    const QString prefix = boundary.endsWith(QLatin1Char('/'))
        ? boundary
        : boundary + QLatin1Char('/');
    return resolved.startsWith(prefix);
}

} // namespace cwExternalCenterlineSync
