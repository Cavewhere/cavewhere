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

// How far above the entry file's own directory the mirror base may sit.
// The measured Survex corpus climbs exactly one level (a shared error
// model beside the cave master); a deeper climb means the entry's
// includes are scattered across the project, and mirroring from their
// ancestor would drag half the disk into the attachment.
constexpr int kMaxRebaseClimb = 2;

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

// Longest common directory prefix of the files in `canonicalPaths`,
// compared component by component.
//
// Precondition: every input is a QFileInfo::canonicalFilePath() result
// (which is what the scanner records). Canonical paths carry the case
// the filesystem itself stores, so the same directory always spells
// itself the same way, and a case-sensitive comparison implements each
// platform's case rules by construction — while a case-insensitive one
// would merge two genuinely distinct directories on a case-sensitive
// volume.
//
// Returns "/" when the paths share only the root, and an empty string
// when even their first components differ (different Windows volumes).
QString commonAncestorDir(const QStringList& canonicalPaths)
{
    if (canonicalPaths.isEmpty()) {
        return QString();
    }

    QStringList prefix = QFileInfo(canonicalPaths.first()).path().split(QLatin1Char('/'));
    for (const QString& path : canonicalPaths) {
        const QStringList components = QFileInfo(path).path().split(QLatin1Char('/'));
        int shared = 0;
        while (shared < prefix.size()
               && shared < components.size()
               && prefix.at(shared) == components.at(shared)) {
            ++shared;
        }
        prefix = prefix.mid(0, shared);
        if (prefix.isEmpty()) {
            return QString();
        }
    }

    if (prefix.size() == 1) {
        // A single component is a root: the empty component of a Unix
        // path, or a Windows drive stub. Both need the trailing slash -
        // QDir reads a bare "C:" as relative to that drive's current
        // directory rather than its root.
        return prefix.first() + QLatin1Char('/');
    }
    return prefix.join(QLatin1Char('/'));
}

// Number of directory levels `dir` sits below `base`, which must be an
// ancestor of (or equal to) `dir`.
int climbFrom(const QString& base, const QString& dir)
{
    const QString relative = QDir(base).relativeFilePath(dir);
    if (relative == QLatin1String(".")) {
        return 0;
    }
    return static_cast<int>(relative.split(QLatin1Char('/'), Qt::SkipEmptyParts).size());
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

    // Mirror from the dependencies' common ancestor rather than the
    // entry's own directory, so an ordinary *include "../shared.svx"
    // still lands inside the attachment.
    const QString basePath = commonAncestorDir(scan.dependencies);
    if (basePath.isEmpty()) {
        plan.warnings.append(QStringLiteral("dependencies share no common folder"));
        return plan;
    }

    const QString entryDirPath = QFileInfo(scan.dependencies.first()).absolutePath();
    if (climbFrom(basePath, entryDirPath) > kMaxRebaseClimb) {
        plan.warnings.append(
            QStringLiteral("the files this entry includes are spread more than %1 folders "
                           "above it — attach the file that owns them instead")
                .arg(kMaxRebaseClimb));
        return plan;
    }

    // commonAncestorDir joins canonical components, so the base is
    // already absolute and clean.
    const QDir base(basePath);
    plan.baseDir = basePath;

    const QDir attachmentDirObj(attachmentDir);
    const QString attachmentAbs = attachmentDirObj.absolutePath();

    QSet<QString> expectedSet;
    expectedSet.reserve(scan.dependencies.size());

    for (const QString& source : scan.dependencies) {
        const QString rel = base.relativeFilePath(source);
        // Defensive only: the base is an ancestor of every dependency by
        // construction, so no relative path can climb out of it.
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
