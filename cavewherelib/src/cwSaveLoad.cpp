#include "cwSaveLoad.h"
#include "cwSaveLoadPrivate.h"
#include "cwProtoUtils.h"
#include "cwNameUtils.h"
#include "cwSanitizedNameSet.h"
#include "cwRemoteAuthProvider.h"
#include "cwDebug.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwLength.h"
#include "cwLead.h"
#include "cwRegionIOTask.h"
#include "cwConcurrent.h"
#include "cwFutureManagerModel.h"
#include "cwTeam.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwEquateModel.h"
#include "cwGeoReference.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNote.h"
#include "cwImageProvider.h"
#include "cwImageUtils.h"
#include "cavewhereVersion.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwRegionTreeModel.h"
#include "cwScrap.h"
#include "cwPDFConverter.h"
#include "cwUnits.h"
#include "cwImageUtils.h"
#include "cwSvgReader.h"
#include "cwZip.h"
#include "cwNoteLiDAR.h"
#include "cwSketch.h"
#include "cwSketchData.h"
#include "cwImageResolution.h"
#include "cwUniqueConnectionChecker.h"
#include "cwGlobals.h"
#include "cwSyncMergeRegistry.h"
#include "cwError.h"

//Async future
#include <asyncfuture.h>
#include <memory>

//Google protobuffer
#include <google/protobuf/util/json_util.h>
#include "google/protobuf/message.h"
#include "cavewhere.pb.h"
#include "qt.pb.h"

//Qt includes
#include <QtConcurrent>
#include <QSaveFile>
#include <QSet>
#include <QFile>
#include <QDateTime>
#include <QQueue>
#include <QDirIterator>

#ifdef CW_WITH_PDF_SUPPORT
#include <QPdfDocument>
#endif
#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QUuid>
#include <QtCore/qscopeguard.h>
#include <QScopedValueRollback>
#include <QFileInfo>
#include <QThread>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <QDebug>

//QQuickGit
#include "GitRepository.h"
#include "Account.h"
#include "LfsStore.h"
#include "LfsPolicy.h"

//#include <git2.h>

//Monad
#include "Monad/Monad.h"

//std includes
#include <algorithm>
#include <atomic>
#include <functional>
#include <optional>
#include <variant>
#include <type_traits>

using namespace Monad;

namespace {

//! The FileVersion a project with no hierarchy is stamped with: the newest
//! format a build that predates the survey tree can read and save.
constexpr int kFlatProjectVersion = 9;

//! The directory a trip's notes live in.
const QLatin1String kNotesDirName("notes");

//! The directory an attached survey file is copied into.
const QLatin1String kExternalCenterlineDirName("external-centerline");

//! The directory a node's child nodes live in, a sibling of trips/.
const QLatin1String kSubNodeDirName("sub");

//! The directory a node's trips live in.
const QLatin1String kTripsDirName("trips");

//! Where a directory sits in a .cwproj's layout. Position is the only thing
//! that says whether a directory called "trips" holds a node's surveys or is a
//! node the user named that, so a descriptor scan decides what to descend into
//! by the role it reached a directory through, never by the bare name.
enum class ScanRole {
    DataRoot,     //!< The project's data root; its children are survey nodes.
    Node,         //!< A survey node's own directory.
    SubNodeList,  //!< A node's sub/ directory; its children are survey nodes.
    TripsList,    //!< A node's trips/ directory; its children are trips.
    Trip,         //!< A trip's own directory.
    Unknown       //!< A directory the layout does not name; descended, never pruned.
};

//! The role a child directory of \a parentRole named \a name is reached
//! through, or nothing when the scan prunes it. Content directories (notes/,
//! external-centerline/ and, for the node scan, trips/) hold no descriptor the
//! scan wants, and a copied Compass project alone puts hundreds of files under
//! external-centerline/. \a scanTrips says whether the caller is looking for
//! trip descriptors and so has to walk into trips/.
std::optional<ScanRole> childScanRole(ScanRole parentRole, const QString& name, bool scanTrips)
{
    const auto named = [&name](QLatin1String dirName) {
        return name.compare(dirName, Qt::CaseInsensitive) == 0;
    };

    switch (parentRole) {
    case ScanRole::DataRoot:
    case ScanRole::SubNodeList:
        return ScanRole::Node;
    case ScanRole::Node:
        if (named(kSubNodeDirName)) {
            return ScanRole::SubNodeList;
        }
        if (named(kTripsDirName)) {
            return scanTrips ? std::optional(ScanRole::TripsList) : std::nullopt;
        }
        if (named(kExternalCenterlineDirName)) {
            return std::nullopt;
        }
        return ScanRole::Unknown;
    case ScanRole::TripsList:
        return ScanRole::Trip;
    case ScanRole::Trip:
        if (named(kNotesDirName) || named(kExternalCenterlineDirName)) {
            return std::nullopt;
        }
        return ScanRole::Unknown;
    case ScanRole::Unknown:
        return ScanRole::Unknown;
    }
    return ScanRole::Unknown;
}

//! The version to stamp into one object's own file. Every file of a project
//! carries the same stamp, so each of these walks up to the region and asks
//! cwSaveLoad::stampVersion. An object no region holds — a trip serialized for a
//! git merge, say — carries no hierarchy of its own and takes the flat stamp.
int stampVersionFor(const cwCave* cave)
{
    return cwSaveLoad::stampVersion(cave != nullptr ? cave->parentRegion() : nullptr);
}

int stampVersionFor(const cwTrip* trip)
{
    return trip != nullptr ? stampVersionFor(trip->parentCave()) : kFlatProjectVersion;
}

int stampVersionFor(const cwNote* note)
{
    return note != nullptr ? stampVersionFor(note->parentTrip()) : kFlatProjectVersion;
}

int stampVersionFor(const cwNoteLiDAR* note)
{
    return note != nullptr ? stampVersionFor(note->parentTrip()) : kFlatProjectVersion;
}

int stampVersionFor(const cwSketch* sketch)
{
    return sketch != nullptr ? stampVersionFor(sketch->parentTrip()) : kFlatProjectVersion;
}

// The geo-reference enums cross into the proto by static_cast, so their values
// are file format. Reordering either C++ enum — inserting a state, adding an
// anchor kind that isn't last — would silently re-read every existing project's
// frame as something else. Pin them here so that becomes a compile error.
static_assert(static_cast<int>(cwGeoReference::Ungeoreferenced)
              == CavewhereProto::GeoReference_State_UNGEOREFERENCED);
static_assert(static_cast<int>(cwGeoReference::Anchored)
              == CavewhereProto::GeoReference_State_ANCHORED);
static_assert(static_cast<int>(cwGeoReference::Frozen)
              == CavewhereProto::GeoReference_State_FROZEN);
static_assert(static_cast<int>(cwGeoReference::Anchor::None)
              == CavewhereProto::GeoReference_AnchorKind_NO_ANCHOR);
static_assert(static_cast<int>(cwGeoReference::Anchor::FixStation)
              == CavewhereProto::GeoReference_AnchorKind_FIX_STATION);
static_assert(static_cast<int>(cwGeoReference::Anchor::LazLayer)
              == CavewhereProto::GeoReference_AnchorKind_LAZ_LAYER);

QDir projectRootDirForFile(const QString& projectFileName)
{
    QFileInfo info(projectFileName);
    return info.absoluteDir();
}

QDir gitDirForRepository(const QDir& repoDir)
{
    const QString dotGitPath = repoDir.filePath(QStringLiteral(".git"));
    const QFileInfo gitInfo(dotGitPath);
    if (gitInfo.isDir()) {
        return QDir(gitInfo.absoluteFilePath());
    }

    if (!gitInfo.isFile()) {
        return QDir();
    }

    QFile gitFile(gitInfo.absoluteFilePath());
    if (!gitFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QDir();
    }

    const QByteArray line = gitFile.readLine();
    const QByteArray gitDirPrefix = "gitdir:";
    if (!line.startsWith(gitDirPrefix)) {
        return QDir();
    }

    QString gitDirPath = QString::fromUtf8(line.mid(gitDirPrefix.size())).trimmed();
    if (gitDirPath.isEmpty()) {
        return QDir();
    }

    QDir gitDir(gitDirPath);
    if (gitDir.isRelative()) {
        gitDir = QDir(repoDir.filePath(gitDirPath));
    }

    return gitDir.exists() ? gitDir : QDir();
}

ResultBase saveBundledArchiveAtomic(const QString& projectRootPath,
                                    const QString& targetArchivePath,
                                    const QStringList& excludePatterns)
{
    if (projectRootPath.isEmpty()) {
        return ResultBase(QStringLiteral("Project root path is empty."));
    }
    if (targetArchivePath.isEmpty()) {
        return ResultBase(QStringLiteral("Bundle target path is empty."));
    }

    const QFileInfo targetInfo(targetArchivePath);
    const QString targetDirPath = targetInfo.absolutePath();
    const QString targetFileName = targetInfo.fileName();
    if (targetDirPath.isEmpty() || targetFileName.isEmpty()) {
        return ResultBase(QStringLiteral("Invalid bundle target path '%1'.").arg(targetArchivePath));
    }

    const QString tempArchivePath = QDir(targetDirPath).absoluteFilePath(targetFileName + QStringLiteral(".tmp"));
    const QString backupArchivePath = QDir(targetDirPath).absoluteFilePath(targetFileName + QStringLiteral(".bak"));

    QFile::remove(tempArchivePath);

    const auto zipResult = cwZip::zipDirectory(projectRootPath, tempArchivePath, excludePatterns);
    if (zipResult.hasError()) {
        QFile::remove(tempArchivePath);
        return ResultBase(QStringLiteral("Failed to package bundled project: %1").arg(zipResult.errorMessage()));
    }

    const bool targetExists = QFileInfo::exists(targetArchivePath);
    QFile::remove(backupArchivePath);

    if (targetExists) {
        if (!QFile::rename(targetArchivePath, backupArchivePath)) {
            QFile::remove(tempArchivePath);
            return ResultBase(QStringLiteral("Failed to stage backup '%1'.").arg(backupArchivePath));
        }
    }

    if (!QFile::rename(tempArchivePath, targetArchivePath)) {
        if (targetExists) {
            QFile::rename(backupArchivePath, targetArchivePath);
        }
        QFile::remove(tempArchivePath);
        return ResultBase(QStringLiteral("Failed to finalize bundled save to '%1'.").arg(targetArchivePath));
    }

    QFile::remove(backupArchivePath);
    return ResultBase();
}

cwImage::OriginalImageInfo imageInfo(const QString& path) {
    cwImage::OriginalImageInfo info;
    const QFileInfo fileInfo(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return info;
    }

    QByteArray imageData = file.readAll();
    if (imageData.isEmpty()) {
        return info;
    }

    const QByteArray format = fileInfo.suffix().toLatin1();
    if (fileInfo.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        return cwSvgReader::imageInfo(imageData, format);
    } else {
        QImage image = cwImageUtils::imageWithAutoTransform(imageData, format);
        if (image.isNull()) {
            return info;
        }

        info.originalSize = image.size();
        info.originalDotsPerMeter = image.dotsPerMeterX();
        info.unit = cwImage::Unit::Pixels;
    }

    return info;
}

#ifdef CW_WITH_PDF_SUPPORT
cwImage::OriginalImageInfo imageInfo(const QPdfDocument& document, int pageIndex) {
    cwImage::OriginalImageInfo info;
    if (pageIndex < 0 || pageIndex >= document.pageCount()) {
        return info;
    }

    const QSizeF pagePoints = document.pagePointSize(pageIndex);
    const QSize pointSize(qRound(pagePoints.width()),
                          qRound(pagePoints.height()));
    constexpr double inchesToMeters = cwUnits::convert(1.0, cwUnits::Inches, cwUnits::Meters);
    constexpr double pointsToInches = 1.0 / cwUnits::PointsPerInch;
    constexpr int pointsPerMeter = qRound(1.0 / (pointsToInches * inchesToMeters));

    info.originalSize = pointSize;
    info.originalDotsPerMeter = pointsPerMeter;
    info.unit = cwImage::Unit::Points;

    return info;
}
#endif

cwImage loadImage(const CavewhereProto::Image& protoImage, const QString& noteFilename) {
    cwImage image;

    const QString rawImagePath = QString::fromStdString(protoImage.path());
    const QString imageFileName = QFileInfo(rawImagePath).fileName();
    image.setPath(imageFileName.isEmpty() ? rawImagePath : imageFileName);

    if (protoImage.has_page()) {
        image.setPage(protoImage.page());
    }

    const bool hasImageUnit = protoImage.has_imageunit();
    if (hasImageUnit) {
        image.setUnit(static_cast<cwImage::Unit>(protoImage.imageunit()));
    }

    const bool hasSize = protoImage.has_size();
    if (hasSize) {
        image.setOriginalSize(cwProtoUtils::loadSize(protoImage.size()));
    }

    const bool hasDots = protoImage.has_dotpermeter();
    if (hasDots) {
        image.setOriginalDotsPerMeter(protoImage.dotpermeter());
    }

    const QString suffix = QFileInfo(imageFileName).suffix().toLower();
    const bool isPdf = suffix == QStringLiteral("pdf");
    const bool isSvg = suffix == QStringLiteral("svg");

    if (!hasImageUnit) {
        if (isPdf) {
            image.setUnit(cwImage::Unit::Points);
        } else if (isSvg) {
            image.setUnit(cwImage::Unit::SvgUnits);
        } else {
            image.setUnit(cwImage::Unit::Pixels);
        }
    }

    const bool sizeMissing = !hasSize || !image.originalSize().isValid();
    const bool dotsMissing = !hasDots || image.originalDotsPerMeter() <= 0;
    const bool unitMismatch = (isPdf && image.unit() != cwImage::Unit::Points)
            || (isSvg && image.unit() != cwImage::Unit::SvgUnits)
            || (!isPdf && !isSvg && image.unit() != cwImage::Unit::Pixels);
    const bool needsReload = sizeMissing
            || dotsMissing
            || (!hasImageUnit && image.unit() != cwImage::Unit::Pixels)
            || unitMismatch;

    if (needsReload) {
        const QDir noteDir = QFileInfo(noteFilename).dir();
        const QString imagePath = noteDir.absoluteFilePath(image.path());
        const QFileInfo imageFileInfo(imagePath);
        if (imageFileInfo.exists()) {
            if (isPdf) {
#ifdef CW_WITH_PDF_SUPPORT
                QPdfDocument document;
                if (document.load(imagePath) == QPdfDocument::Error::None) {
                    const int pageIndex = image.page() >= 0 ? image.page() : 0;
                    const cwImage::OriginalImageInfo info = imageInfo(document, pageIndex);
                    if (info.originalSize.isValid()) {
                        image.setOriginalImageInfo(info);
                    }
                }
#endif
            } else {
                const cwImage::OriginalImageInfo info = imageInfo(imagePath);
                if (info.originalSize.isValid()) {
                    image.setOriginalImageInfo(info);
                }
            }
        }
    }

    return image;
}

QSet<QString> cavewhereTrackedExtensions()
{
    static const QSet<QString> trackedExtensions = {
        // Raster image formats
        QStringLiteral("bmp"),
        QStringLiteral("gif"),
        QStringLiteral("ico"),
        QStringLiteral("cur"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("jp2"),
        QStringLiteral("png"),
        QStringLiteral("pbm"),
        QStringLiteral("pgm"),
        QStringLiteral("ppm"),
        QStringLiteral("tif"),
        QStringLiteral("tiff"),
        QStringLiteral("webp"),
        QStringLiteral("xbm"),
        QStringLiteral("xpm"),
        QStringLiteral("heic"),
        QStringLiteral("heif"),
        QStringLiteral("avif"),
        // Vector / document formats
        QStringLiteral("svg"),
        QStringLiteral("pdf"),
        // 3D assets
        QStringLiteral("glb"),
        // Point clouds (geospatial layers)
        QStringLiteral("laz"),
        QStringLiteral("las"),
    };
    return trackedExtensions;
}

bool hasRemoteConfigured(const QQuickGit::GitRepository* repository)
{
    return repository != nullptr && !repository->remotes().isEmpty();
}

QStringList uniqueSortedPaths(const QStringList& paths)
{
    QSet<QString> seen;
    QStringList result;
    result.reserve(paths.size());
    for (const QString& path : paths) {
        const QString normalized = QDir::fromNativeSeparators(path);
        if (normalized.isEmpty() || seen.contains(normalized)) {
            continue;
        }
        seen.insert(normalized);
        result.append(normalized);
    }
    std::sort(result.begin(), result.end());
    return result;
}

QString normalizeSyncPath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

struct LfsCandidateState {
    bool isPointer = false;
    QString pointerOid;
    qint64 size = -1;
    qint64 lastModifiedUtcMsecs = 0;
};

using LfsSnapshot = QHash<QString, LfsCandidateState>;

LfsSnapshot captureLfsSnapshot(const QString& repoPath)
{
    LfsSnapshot snapshot;
    const QDir repoDir(repoPath);
    const QSet<QString> trackedExtensions = cavewhereTrackedExtensions();

    QDirIterator it(repoPath,
                    QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString relativePath = QDir::fromNativeSeparators(repoDir.relativeFilePath(info.absoluteFilePath()));
        if (relativePath.isEmpty()
                || relativePath == QStringLiteral(".git")
                || relativePath.startsWith(QStringLiteral(".git/"))
                //What a delete carried into the trash is out of git's sight,
                //so an LFS snapshot that counted it would report a hydration
                //delta for files no commit can ever hold.
                || relativePath.startsWith(QStringLiteral(".cw_trash/"))) {
            continue;
        }

        const QString extension = info.suffix().trimmed().toLower();
        if (!trackedExtensions.contains(extension)) {
            continue;
        }

        LfsCandidateState state;
        state.size = info.size();
        state.lastModifiedUtcMsecs = info.lastModified().toUTC().toMSecsSinceEpoch();
        if (state.size >= 0 && state.size <= 8192) {
            QFile file(info.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly)) {
                const QByteArray data = file.readAll();
                QQuickGit::LfsPointer pointer;
                if (QQuickGit::LfsPointer::parse(data, &pointer)) {
                    state.isPointer = true;
                    state.pointerOid = pointer.oid;
                }
            }
        }

        snapshot.insert(relativePath, state);
    }

    return snapshot;
}

//! What a restore of a merge handler's files wrote, and what it could not write.
struct RestoreFromCommitOutcome {
    int writtenFileCount = 0;
    QStringList skippedPaths;
};

//! Writes each of \a relativePaths back from \a commit. An LFS-tracked file — a note
//! image is one — is a pointer in the commit, so its bytes come from \a lfsStore; with
//! the object missing locally the pointer text is written as it stands, which is how an
//! unhydrated clone already holds its own images, and a later hydration fills it in.
RestoreFromCommitOutcome restoreFilesFromCommit(const QDir& repoRoot,
                                                const QString& commit,
                                                const QStringList& relativePaths,
                                                const std::shared_ptr<QQuickGit::LfsStore>& lfsStore)
{
    RestoreFromCommitOutcome outcome;
    for (const QString& relativePath : relativePaths) {
        const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(repoRoot.absolutePath(),
                                                                                 commit,
                                                                                 relativePath);
        if (contentResult.hasError() || contentResult.value().isEmpty()) {
            outcome.skippedPaths.append(relativePath);
            continue;
        }

        QByteArray content = contentResult.value();
        QQuickGit::LfsPointer pointer;
        if (lfsStore != nullptr
                && QQuickGit::LfsPointer::parse(content, &pointer)
                && pointer.isValid()) {
            const auto objectResult = lfsStore->readObject(pointer.oid);
            if (!objectResult.hasError() && !objectResult.value().isEmpty()) {
                content = objectResult.value();
            }
        }

        const QString absolutePath = repoRoot.absoluteFilePath(relativePath);
        QFile file(absolutePath);
        if (!QDir().mkpath(QFileInfo(absolutePath).absolutePath())
                || !file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                || file.write(content) != content.size()) {
            outcome.skippedPaths.append(relativePath);
            continue;
        }
        ++outcome.writtenFileCount;
    }

    return outcome;
}

QStringList hydrationDeltaPaths(const LfsSnapshot& beforeSnapshot,
                                const LfsSnapshot& afterSnapshot)
{
    QSet<QString> allPaths = QSet<QString>(beforeSnapshot.keyBegin(), beforeSnapshot.keyEnd());
    allPaths.unite(QSet<QString>(afterSnapshot.keyBegin(), afterSnapshot.keyEnd()));

    QStringList changedPaths;
    changedPaths.reserve(allPaths.size());
    for (const QString& path : allPaths) {
        const bool hadBefore = beforeSnapshot.contains(path);
        const bool hasAfter = afterSnapshot.contains(path);
        if (hadBefore != hasAfter) {
            changedPaths.append(path);
            continue;
        }

        const LfsCandidateState& before = beforeSnapshot[path];
        const LfsCandidateState& after = afterSnapshot[path];
        if (before.isPointer != after.isPointer
                || before.pointerOid != after.pointerOid
                || before.size != after.size
                || before.lastModifiedUtcMsecs != after.lastModifiedUtcMsecs) {
            changedPaths.append(path);
        }
    }

    return uniqueSortedPaths(changedPaths);
}

cwSaveLoad::SyncReport::PullState toPullState(QQuickGit::GitRepository::MergeResult::State mergeState)
{
    switch (mergeState) {
    case QQuickGit::GitRepository::MergeResult::AlreadyUpToDate:
        return cwSaveLoad::SyncReport::PullState::AlreadyUpToDate;
    case QQuickGit::GitRepository::MergeResult::FastForward:
        return cwSaveLoad::SyncReport::PullState::FastForward;
    case QQuickGit::GitRepository::MergeResult::Rebased:
        return cwSaveLoad::SyncReport::PullState::Rebased;
    case QQuickGit::GitRepository::MergeResult::MergeCommitCreated:
        return cwSaveLoad::SyncReport::PullState::MergeCommitCreated;
    case QQuickGit::GitRepository::MergeResult::MergeConflicts:
        return cwSaveLoad::SyncReport::PullState::MergeConflicts;
    case QQuickGit::GitRepository::MergeResult::UnknownState:
    default:
        return cwSaveLoad::SyncReport::PullState::Unknown;
    }
}

cwSaveLoad::SyncReport buildSyncReport(const QString& repoPath,
                                       const QString& beforeHead,
                                       const QString& afterHead,
                                       cwSaveLoad::SyncReport::PullState pullState,
                                       const LfsSnapshot& beforeSnapshot)
{
    cwSaveLoad::SyncReport report;
    report.beforeHead = beforeHead;
    report.afterHead = afterHead;
    report.pullState = pullState;
    QString mergeBaseSecondCommit = afterHead;
    if (pullState == cwSaveLoad::SyncReport::PullState::MergeCommitCreated) {
        const auto parentOidsResult =
                QQuickGit::GitRepository::commitParentOids(repoPath, afterHead);
        if (parentOidsResult.hasError()) {
            report.diagnostics.append(parentOidsResult.errorMessage());
        } else {
            const QStringList parentOids = parentOidsResult.value();
            if (parentOids.size() >= 2) {
                mergeBaseSecondCommit = parentOids[1];
            }
        }
    }

    const auto mergeBaseResult =
            QQuickGit::GitRepository::mergeBaseCommitOid(repoPath, beforeHead, mergeBaseSecondCommit);
    if (mergeBaseResult.hasError()) {
        report.diagnostics.append(mergeBaseResult.errorMessage());
    } else {
        report.mergeBaseHead = mergeBaseResult.value();
    }

    const auto commitPathsResult =
            QQuickGit::GitRepository::diffPathsBetweenCommits(repoPath, beforeHead, afterHead);
    if (commitPathsResult.hasError()) {
        report.diagnostics.append(commitPathsResult.errorMessage());
    } else {
        report.commitDiffPaths = commitPathsResult.value();
    }

    const LfsSnapshot afterSnapshot = captureLfsSnapshot(repoPath);
    report.hydrationDeltaPaths = hydrationDeltaPaths(beforeSnapshot, afterSnapshot);
    report.backendPaths = uniqueSortedPaths(report.backendPaths);

    QStringList changedPaths = report.backendPaths;
    changedPaths.append(report.commitDiffPaths);
    changedPaths.append(report.hydrationDeltaPaths);
    report.changedPaths = uniqueSortedPaths(changedPaths);
    report.hasBackendPaths = !report.backendPaths.isEmpty();
    report.hasCommitDiffPaths = !report.commitDiffPaths.isEmpty();
    report.hasHydrationDeltaPaths = !report.hydrationDeltaPaths.isEmpty();
    return report;
}

constexpr int SyncMaxRetriesPerSession = 3;

ResultBase syncRetryLimitResult(const QString& reason)
{
    return ResultBase(QStringLiteral("Sync did not complete after %1 retries: %2")
                      .arg(SyncMaxRetriesPerSession)
                      .arg(reason));
}

// Compile-time guard: SyncErrorCode and GitErrorCode share the same
// ResultBase int transport, so their numeric values must never overlap.
using GitErr = QQuickGit::GitRepository::GitErrorCode;
using SyncErr = cwSaveLoad::SyncErrorCode;
static_assert(static_cast<int>(SyncErr::RetryEpochChanged) != static_cast<int>(GitErr::PushRejectedByRemoteAdvance));
static_assert(static_cast<int>(SyncErr::RetryEpochChanged) != static_cast<int>(GitErr::PushWildcardRefSpecUnsupported));
static_assert(static_cast<int>(SyncErr::RetryEpochChanged) != static_cast<int>(GitErr::PushFailed));
static_assert(static_cast<int>(SyncErr::RetryEpochChanged) != static_cast<int>(GitErr::HttpAuthFailed));
static_assert(static_cast<int>(SyncErr::IncompatibleProjectVersion) != static_cast<int>(GitErr::PushRejectedByRemoteAdvance));
static_assert(static_cast<int>(SyncErr::HttpAuthFailed) != static_cast<int>(GitErr::HttpAuthFailed));

bool isPushRejectedByRemoteAdvance(const ResultBase& pushResult)
{
    if (!pushResult.hasError()) {
        return false;
    }

    return QQuickGit::GitRepository::isPushRejectedByRemoteAdvanceError(pushResult.errorCode());
}

QString defaultCommitSubject(const QString& action)
{
    const QString normalizedAction = action.trimmed().isEmpty() ? QStringLiteral("Save") : action.trimmed();
    return QStringLiteral("%1 from CaveWhere").arg(normalizedAction);
}

QString defaultCommitDescription()
{
    return QStringLiteral("Automatic commit at %1")
            .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
}

QString uuidToProtoString(const QUuid& uuid)
{
    return uuid.toString(QUuid::WithoutBraces);
}

QUuid repairedTopLevelId(const QUuid& candidateId,
                         QSet<QUuid>& seenIds,
                         cwSaveLoad::IdentityRepairData& repair)
{
    if (!candidateId.isNull() && !seenIds.contains(candidateId)) {
        seenIds.insert(candidateId);
        return candidateId;
    }

    repair.required = true;
    if (candidateId.isNull()) {
        ++repair.generatedIds;
    } else {
        ++repair.duplicateIds;
    }

    QUuid repairedId;
    do {
        repairedId = QUuid::createUuid();
    } while (repairedId.isNull() || seenIds.contains(repairedId));

    seenIds.insert(repairedId);
    return repairedId;
}

} // namespace (anonymous helpers above — repairedTopLevelId, etc.)

QQuickGit::LfsPolicy cavewhereLfsPolicy()
{
    QQuickGit::LfsPolicy policy;
    const auto alwaysEligible = [](const QString&, const QByteArray*) {
        return true;
    };

    for (const QString& extension : cavewhereTrackedExtensions()) {
        policy.setRule(extension, alwaysEligible);
    }

    return policy;
}

void regenerateNoteSubtreeIds(cwNoteData& note)
{
    note.id = QUuid::createUuid();
    for (cwScrapData& scrap : note.scraps) {
        scrap.id = QUuid::createUuid();
        for (cwNoteStation& station : scrap.stations) {
            station.setId(QUuid::createUuid());
        }
        for (cwLead& lead : scrap.leads) {
            lead.setId(QUuid::createUuid());
        }
    }
}

void regenerateNoteLiDARSubtreeIds(cwNoteLiDARData& noteLiDAR)
{
    noteLiDAR.id = QUuid::createUuid();
    for (cwNoteLiDARStation& station : noteLiDAR.stations) {
        station.setId(QUuid::createUuid());
    }
}

void regenerateSketchSubtreeIds(cwSketchData& sketch)
{
    sketch.id = QUuid::createUuid();
    for (cwPenStroke& stroke : sketch.strokes) {
        stroke.id = QUuid::createUuid();
    }
}

void regenerateTripSubtreeIds(cwTripData& trip)
{
    trip.id = QUuid::createUuid();
    for (cwNoteData& note : trip.noteModel.notes) {
        regenerateNoteSubtreeIds(note);
    }
    for (cwNoteLiDARData& noteLiDAR : trip.noteLiDARModel.notes) {
        regenerateNoteLiDARSubtreeIds(noteLiDAR);
    }
    for (cwSketchData& sketch : trip.sketchModel.notes) {
        regenerateSketchSubtreeIds(sketch);
    }
}

void regenerateCaveSubtreeIds(cwCaveData& cave)
{
    cave.id = QUuid::createUuid();
    for (cwTripData& trip : cave.trips) {
        regenerateTripSubtreeIds(trip);
    }
    for (cwCaveData& child : cave.nodes) {
        regenerateCaveSubtreeIds(child);
    }
}

void cwSaveLoad::repairTopLevelIds(ProjectLoadData& loadData)
{
    QSet<QUuid> seenCaveIds;
    QSet<QUuid> seenTripIds;
    QSet<QUuid> seenNoteIds;
    QSet<QUuid> seenNoteLiDARIds;
    QSet<QUuid> seenSketchIds;

    // Returns true (and regenerates the subtree) if id is a duplicate.
    auto detectDuplicate = [&](QUuid& id, QSet<QUuid>& seenIds, auto regenerateFn) {
        if (!id.isNull() && seenIds.contains(id)) {
            loadData.identityRepair.required = true;
            ++loadData.identityRepair.duplicateIds;
            regenerateFn();
            seenIds.insert(id);
            return true;
        }
        return false;
    };

    //Ids are unique across the whole project, so the walk covers every node of
    //the tree: a hand-copied directory under a parent's sub/ duplicates ids
    //just as a copied top-level cave does, and an unrepaired duplicate lets one
    //node's loaded path stand in for the other's.
    const auto repairNode = [&](cwCaveData& cave, auto&& self) -> void {
        if (detectDuplicate(cave.id, seenCaveIds, [&]{ regenerateCaveSubtreeIds(cave); })) {
            return;
        }

        cave.id = repairedTopLevelId(cave.id, seenCaveIds, loadData.identityRepair);
        for (cwTripData& trip : cave.trips) {
            if (detectDuplicate(trip.id, seenTripIds, [&]{ regenerateTripSubtreeIds(trip); })) {
                continue;
            }

            trip.id = repairedTopLevelId(trip.id, seenTripIds, loadData.identityRepair);
            for (cwNoteData& note : trip.noteModel.notes) {
                if (detectDuplicate(note.id, seenNoteIds, [&]{ regenerateNoteSubtreeIds(note); })) {
                    continue;
                }
                note.id = repairedTopLevelId(note.id, seenNoteIds, loadData.identityRepair);
            }
            for (cwNoteLiDARData& note : trip.noteLiDARModel.notes) {
                if (detectDuplicate(note.id, seenNoteLiDARIds, [&]{ regenerateNoteLiDARSubtreeIds(note); })) {
                    continue;
                }
                note.id = repairedTopLevelId(note.id, seenNoteLiDARIds, loadData.identityRepair);
            }
            for (cwSketchData& sketch : trip.sketchModel.notes) {
                if (detectDuplicate(sketch.id, seenSketchIds, [&]{ regenerateSketchSubtreeIds(sketch); })) {
                    continue;
                }
                sketch.id = repairedTopLevelId(sketch.id, seenSketchIds, loadData.identityRepair);
            }
        }

        for (cwCaveData& child : cave.nodes) {
            self(child, self);
        }
    };

    for (cwCaveData& cave : loadData.region.caves) {
        repairNode(cave, repairNode);
    }
}

void cwSaveLoad::repairNestedScrapIds(ProjectLoadData& loadData)
{
    const auto repairNode = [](cwCaveData& cave, ProjectLoadData& loadData, auto&& self) -> void {
        for (cwTripData& trip : cave.trips) {
            for (cwNoteData& note : trip.noteModel.notes) {
                for (cwScrapData& scrap : note.scraps) {
                    QSet<QUuid> seenStationIds;
                    for (cwNoteStation& station : scrap.stations) {
                        station.setId(repairedTopLevelId(station.id(), seenStationIds, loadData.identityRepair));
                    }

                    QSet<QUuid> seenLeadIds;
                    for (cwLead& lead : scrap.leads) {
                        lead.setId(repairedTopLevelId(lead.id(), seenLeadIds, loadData.identityRepair));
                    }
                }
            }

            for (cwNoteLiDARData& noteLiDAR : trip.noteLiDARModel.notes) {
                QSet<QUuid> seenLiDARStationIds;
                for (cwNoteLiDARStation& station : noteLiDAR.stations) {
                    station.setId(repairedTopLevelId(station.id(), seenLiDARStationIds, loadData.identityRepair));
                }
            }
        }

        for (cwCaveData& child : cave.nodes) {
            self(child, loadData, self);
        }
    };

    for (cwCaveData& cave : loadData.region.caves) {
        repairNode(cave, loadData, repairNode);
    }
}

void cwSaveLoad::repairNameCollisions(ProjectLoadData& loadData)
{
    // Dedup name in nameSet; if renamed, append a warning error.
    auto dedup = [&](cwSanitizedNameSet& names, QString& name, auto makeWarning) {
        const QString deduped = names.deduplicateName(name);
        if (deduped != name) {
            loadData.errors.append(cwError(makeWarning(name, deduped), cwError::Warning));
            name = deduped;
        }
        names.insert(name);
    };

    //Sibling nodes share a directory, so their names have to differ after
    //sanitizing at every depth: a node's children collide with each other under
    //its sub/, not with its trips, which live in a separate trips/.
    const auto dedupSiblingNodes = [&](QList<cwCaveData>& siblings, const QString& parentName) {
        cwSanitizedNameSet nodeNames;
        for (cwCaveData& node : siblings) {
            dedup(nodeNames, node.name, [&](const QString& old, const QString& fixed) {
                if (parentName.isEmpty()) {
                    return QStringLiteral("Cave \"%1\" renamed to \"%2\" to avoid a name collision on disk.").arg(old, fixed);
                }
                return QStringLiteral("Survey node \"%1\" in \"%2\" renamed to \"%3\" to avoid a name collision on disk.").arg(old, parentName, fixed);
            });
        }
    };

    dedupSiblingNodes(loadData.region.caves, QString());

    const auto repairNode = [&](cwCaveData& cave, auto&& self) -> void {
        dedupSiblingNodes(cave.nodes, cave.name);

        cwSanitizedNameSet tripNames;
        for (cwTripData& trip : cave.trips) {
            dedup(tripNames, trip.name, [&](const QString& old, const QString& fixed) {
                return QStringLiteral("Trip \"%1\" in cave \"%2\" renamed to \"%3\" to avoid a name collision on disk.").arg(old, cave.name, fixed);
            });
        }

        for (cwTripData& trip : cave.trips) {
            cwSanitizedNameSet noteNames;
            for (cwNoteData& note : trip.noteModel.notes) {
                dedup(noteNames, note.name, [&](const QString& old, const QString& fixed) {
                    return QStringLiteral("Note \"%1\" in trip \"%2\" renamed to \"%3\" to avoid a name collision on disk.").arg(old, trip.name, fixed);
                });
            }

            cwSanitizedNameSet lidarNames;
            for (cwNoteLiDARData& note : trip.noteLiDARModel.notes) {
                dedup(lidarNames, note.name, [&](const QString& old, const QString& fixed) {
                    return QStringLiteral("LiDAR note \"%1\" in trip \"%2\" renamed to \"%3\" to avoid a name collision on disk.").arg(old, trip.name, fixed);
                });
            }

            cwSanitizedNameSet sketchNames;
            for (cwSketchData& sketch : trip.sketchModel.notes) {
                dedup(sketchNames, sketch.name, [&](const QString& old, const QString& fixed) {
                    return QStringLiteral("Sketch \"%1\" in trip \"%2\" renamed to \"%3\" to avoid a name collision on disk.").arg(old, trip.name, fixed);
                });
            }
        }

        for (cwCaveData& child : cave.nodes) {
            self(child, self);
        }
    };

    for (cwCaveData& cave : loadData.region.caves) {
        repairNode(cave, repairNode);
    }
}

// Reopen for remaining file-local helpers (git exclude patterns, etc.)
namespace {

//! Excludes the trash from git and from bundles, the way ".cw_cache/" is
//! excluded: a directory a delete filled is never committed, synced or copied.
const QString kTrashDirPattern = QStringLiteral(".cw_trash/");
const QString kTrashDirEntry = QStringLiteral(".cw_trash");
const QString kCacheDirPattern = QStringLiteral(".cw_cache/");
const QString kCacheDirEntry = QStringLiteral(".cw_cache");
const QString kDsStoreEntry = QStringLiteral(".DS_Store");

QStringList readGitExcludePatterns(const QDir& repoDir)
{
    const QDir gitDir = gitDirForRepository(repoDir);
    if (!gitDir.exists()) {
        return {};
    }

    QFile excludeFile(gitDir.filePath(QStringLiteral("info/exclude")));
    if (!excludeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList patterns;
    while (!excludeFile.atEnd()) {
        const QString line = QString::fromUtf8(excludeFile.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('!'))) {
            continue;
        }
        patterns.append(line);
    }

    return patterns;
}

} // namespace

void cwSaveLoad::ensureGitExcludeHasLocalEntries(const QDir& repoDir)
{
    const QDir gitDir = gitDirForRepository(repoDir);
    if (!gitDir.exists()) {
        return;
    }

    const QDir infoDir(gitDir.filePath(QStringLiteral("info")));
    if (!infoDir.exists()) {
        if (!QDir().mkpath(infoDir.absolutePath())) {
            return;
        }
    }

    const QString excludePath = infoDir.filePath(QStringLiteral("exclude"));
    QByteArray existingContents;
    if (QFile::exists(excludePath)) {
        QFile readFile(excludePath);
        if (readFile.open(QIODevice::ReadOnly)) {
            existingContents = readFile.readAll();
        }
    }

    const QStringList lines = QString::fromUtf8(existingContents).split('\n');
    const QList<QPair<QString, QStringList>> entries = {
        {kCacheDirPattern, {kCacheDirPattern, kCacheDirEntry}},
        {kTrashDirPattern, {kTrashDirPattern, kTrashDirEntry}},
        {kDsStoreEntry, {kDsStoreEntry}}
    };

    QStringList missingEntries;
    for (const auto& entry : entries) {
        bool found = false;
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (entry.second.contains(trimmed)) {
                found = true;
                break;
            }
        }

        if (!found) {
            missingEntries.append(entry.first);
        }
    }

    if (missingEntries.isEmpty()) {
        return;
    }

    QFile writeFile(excludePath);
    if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }

    if (!existingContents.isEmpty() && !existingContents.endsWith('\n')) {
        writeFile.write("\n");
    }
    for (const QString& entry : missingEntries) {
        writeFile.write(entry.toUtf8());
        writeFile.write("\n");
    }
}

cwResultDir cwSaveLoad::repositoryDir(const QUrl& localDir, const QString& name)
{
    if (name.isEmpty()) {
        return cwResultDir(QStringLiteral("Caving area name is empty"));
    }

    auto quotedFilename = [](const QString& fullName, const QString& shortName) {
        auto quote = [](const QString& str) {
            return QStringLiteral("\"") + str + QStringLiteral("\" ");
        };
#ifdef CW_MOBILE_BUILD
        return quote(shortName);
#else
        Q_UNUSED(shortName)
        return quote(fullName);
#endif
    };

    if (localDir.isLocalFile()) {
        QFileInfo info(localDir.toLocalFile() + QStringLiteral("/") + name);
        if (info.exists()) {
            return cwResultDir(quotedFilename(info.absoluteFilePath(), name) + QStringLiteral("exists, use a different name"));
        }
        return QDir(info.absoluteFilePath());
    }

    return cwResultDir(quotedFilename(localDir.toString(), name) + QStringLiteral("is not a non-local directory"));
}

template<typename ProtoType>
static Result<ProtoType> loadMessage(const QByteArray& content, const QString& sourceLabel)
{
    if (content.isEmpty()) {
        return Result<ProtoType>(QStringLiteral("No data in %1.").arg(sourceLabel));
    }

    ProtoType proto;
    const std::string jsonPayload(content.constData(),
                                  static_cast<size_t>(content.size()));
    static const auto parseOptions = [] {
        google::protobuf::util::JsonParseOptions opts;
        opts.ignore_unknown_fields = true;
        return opts;
    }();
    const auto status = google::protobuf::util::JsonStringToMessage(jsonPayload, &proto, parseOptions);
    if (!status.ok()) {
        return Result<ProtoType>(
                    QStringLiteral("Failed to parse %1: %2")
                    .arg(sourceLabel, QString::fromStdString(std::string(status.message()))));
    }

    return Result<ProtoType>(proto);
}

template<typename ProtoType>
static Result<ProtoType> loadMessage(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        return Result<ProtoType>(file.errorString());
    }

    return loadMessage<ProtoType>(file.readAll(), filename);
}

// template<typename NoteT>
// static QString noteRelativePathHelper(const NoteT* note, const QString& storedPath)
// {
//     const QString fileName = QFileInfo(storedPath).fileName();
//     const QString nameToUse = fileName.isEmpty() ? storedPath : fileName;

//     if (!note) {
//         return nameToUse;
//     }

//     const cwTrip* trip = note->parentTrip();
//     const cwCave* cave = trip ? trip->parentCave() : nullptr;
//     const cwCavingRegion* region = cave ? cave->parentRegion() : nullptr;
//     const cwProject* project = region ? region->parentProject() : nullptr;

//     if (project == nullptr || project->filename().isEmpty()) {
//         return QString();
//     }

//     const QDir projectDir = QFileInfo(project->filename()).absoluteDir();
//     const QString absolutePath = cwSaveLoad::dir(note).absoluteFilePath(nameToUse);
//     return projectDir.relativeFilePath(absolutePath);
// }


// cwSaveLoad::~cwSaveLoad() = default;
cwSaveLoad::~cwSaveLoad() {
    for (const QString& dir : std::as_const(d->m_ownedTempDirs)) {
        removeTemporaryProjectDir(dir);
    }

    // Discard any queued-but-not-yet-dispatched filesystem jobs. This handles
    // abnormal destruction (e.g. the owning async chain was cancelled) where
    // retire() was never called. Complete the deferred so observers don't hang.
    d->m_pendingJobs.clear();
    if (!d->m_pendingJobsDeferred.future().isFinished()) {
        d->m_pendingJobsDeferred.complete();
    }
}

namespace {

struct ProjectDestination {
    QString sanitizedBaseName;
    QString rootDirPath;
    QString projectFilePath;
};

Result<ProjectDestination> projectDestination(const QString& destinationUrl)
{
    QString localPath = cwGlobals::convertFromURL(destinationUrl);
    if (localPath.isEmpty()) {
        return Result<ProjectDestination>(QStringLiteral("Save location can't be empty."));
    }

    localPath = cwGlobals::addExtension(localPath, QStringLiteral("cwproj"));
    if (QFileInfo(localPath).suffix().compare(QStringLiteral("cw"), Qt::CaseInsensitive) == 0) {
        localPath = QFileInfo(localPath).path() + QDir::separator()
                + QFileInfo(localPath).completeBaseName() + QStringLiteral(".cwproj");
    }
    QFileInfo destinationInfo(localPath);
    if (!destinationInfo.isAbsolute()) {
        destinationInfo = QFileInfo(QDir::current(), localPath);
    }

    const QString baseName = destinationInfo.completeBaseName();
    if (baseName.isEmpty()) {
        return Result<ProjectDestination>(QStringLiteral("Save name can't be empty."));
    }

    const QString sanitizedBaseName = cwSaveLoad::sanitizeFileName(baseName);
    if (sanitizedBaseName.isEmpty()) {
        return Result<ProjectDestination>(QStringLiteral("Save name can't be empty."));
    }

    QDir destinationDirectory = destinationInfo.absoluteDir();

    QString rootDirPath;
    if (destinationDirectory.dirName() == sanitizedBaseName) {
        rootDirPath = destinationDirectory.absolutePath();
    } else {
        rootDirPath = destinationDirectory.absoluteFilePath(sanitizedBaseName);
    }

    QFileInfo rootInfo(rootDirPath);
    QDir parentDir = rootInfo.dir();
    if (!parentDir.exists()) {
        if (!QDir().mkpath(parentDir.absolutePath())) {
            return Result<ProjectDestination>(QStringLiteral("Cannot create directory '%1'.").arg(parentDir.absolutePath()));
        }
    }

    const QDir rootDir(rootDirPath);
    const QString projectFilePath = rootDir.absoluteFilePath(sanitizedBaseName + QStringLiteral(".cwproj"));

    return Result<ProjectDestination>({sanitizedBaseName, rootDirPath, projectFilePath});
}

}

ResultBase cwSaveLoad::transferProjectTo(const QString& destinationFileUrl, ProjectTransferMode mode)
{
    const auto destinationResult = projectDestination(destinationFileUrl);
    if (destinationResult.hasError()) {
        return ResultBase(destinationResult.errorMessage());
    }

    const auto destination = destinationResult.value();

    const QString currentProjectFile = fileName();
    if (currentProjectFile.isEmpty()) {
        return ResultBase(QStringLiteral("Project does not have an associated file."));
    }

    const QString currentRootPath = projectRootDir().absolutePath();
    const QString targetRootPath = destination.rootDirPath;
    const QString normalizedCurrentRoot = QDir(currentRootPath).absolutePath();
    const QString normalizedTargetRoot = QDir(targetRootPath).absolutePath();
    const bool sameRoot = normalizedCurrentRoot == normalizedTargetRoot;

    if (!sameRoot && QFileInfo::exists(targetRootPath)) {
        return ResultBase(QStringLiteral("Destination folder '%1' already exists.").arg(targetRootPath));
    }

    setSaveEnabled(false);
    auto enableGuard = qScopeGuard([this]() { setSaveEnabled(true); });

    waitForFinished();

    // A Move renames the whole root, hidden directories included, and a Copy
    // is a copy: neither may carry what a delete took away into the folder the
    // user is about to hand to someone. The undos still on the stack keep
    // their descriptors and lose their payload, which is what a delete cost
    // before the trash existed.
    d->discardTrash(this);

    // Captured non-fatal warning from the transfer step (e.g. copy succeeded
    // but temp source cleanup failed on a cloud volume). Surfaced as the
    // function's return value if the rest of transferProjectTo succeeds.
    ResultBase transferWarning;

    if (!sameRoot) {
        if (mode == ProjectTransferMode::Move) {
            auto moveResult = cwSaveLoadPrivate::moveDirectoryRobust(currentRootPath, targetRootPath);
            if (moveResult.hasError()) {
                return moveResult;
            }
            if (!moveResult.errorMessage().isEmpty()) {
                transferWarning = moveResult;
            }
            // Project root was moved out (or copied + source removed) — remove the
            // now-empty QTemporaryDir parent(s). waitForFinished() was called above,
            // so no in-flight jobs. fsRemoveDirRecursive handles both cases.
            for (const QString& dir : std::as_const(d->m_ownedTempDirs)) {
                removeTemporaryProjectDir(dir);
            }
            d->m_ownedTempDirs.clear();
        } else if (mode == ProjectTransferMode::Copy) {
            auto copyResult = cwSaveLoadPrivate::copyDirectoryRecursively(QDir(currentRootPath), QDir(targetRootPath));
            if (copyResult.hasError()) {
                return copyResult;
            }
        } else {
            return ResultBase(QStringLiteral("Unknown ProjectTransferMode, this is a bug"));
        }
    }

    QDir targetRootDir(sameRoot ? normalizedCurrentRoot : normalizedTargetRoot);
    const QString desiredFilePath = destination.projectFilePath;
    const QString sourceFileName = QFileInfo(currentProjectFile).fileName();
    QString sourceFilePath = targetRootDir.absoluteFilePath(sourceFileName);

    if (!QFileInfo::exists(sourceFilePath)) {
        const QString label = mode == ProjectTransferMode::Move ? QStringLiteral("Moved") : QStringLiteral("Copied");
        return ResultBase(QStringLiteral("%1 project file '%2' is missing.").arg(label, sourceFilePath));
    }

    const QString desiredFileName = QFileInfo(desiredFilePath).fileName();
    const QString desiredPath = targetRootDir.absoluteFilePath(desiredFileName);

    if (sourceFilePath != desiredPath) {
        if (QFileInfo::exists(desiredPath) && !QFile::remove(desiredPath)) {
            return ResultBase(QStringLiteral("Couldn't overwrite '%1'.").arg(desiredPath));
        }
        if (!QFile::rename(sourceFilePath, desiredPath)) {
            return ResultBase(QStringLiteral("Couldn't rename project file to '%1'.").arg(desiredPath));
        }
        sourceFilePath = desiredPath;
    }

    const auto region = d->m_regionTreeModel->cavingRegion();

    // For Copy mode on already-saved projects, preserve the existing dataRoot rather than
    // deriving a new one from the destination basename. This keeps the copy internally
    // consistent without mutating the loaded project's identity.
    const bool isAlreadySavedCopy = (mode == ProjectTransferMode::Copy && d->hasDurableHome);
    const QString newDataRootName = isAlreadySavedCopy ? d->projectMetadata.dataRoot
                                                       : destination.sanitizedBaseName;

    if (!isAlreadySavedCopy) {
        auto renameResult = renameDataRootOnDisk(targetRootDir, newDataRootName);
        if (renameResult.hasError()) {
            return renameResult;
        }
        if (transferWarning.errorMessage().isEmpty() && !renameResult.errorMessage().isEmpty()) {
            transferWarning = renameResult;
        }
    }

    auto* lazLayers = region != nullptr ? region->lazLayers() : nullptr;

    // Retarget before setFileName's setGisLayersDir runs, so the layer objects
    // and their loaded point data survive the root change. Left to that setter,
    // the rescan would re-create every row: a full point re-stream that also
    // spuriously dirties the just-saved project via localMutationOccurred.
    if (!sameRoot && lazLayers != nullptr) {
        lazLayers->retargetGisLayersDir(
                    QDir(targetRootDir.absoluteFilePath(cwLazLayerModel::folderName())));
    }

    setFileName(desiredFilePath);
    initializeRepositoryForCurrentFile();
    markDurableHome();
    // Do NOT call region->setName() here: the sanitizedBaseName is a filesystem-safe
    // name and is not appropriate as the user-visible display name. The caller
    // (cwProject::saveAs) is responsible for setting the region name to the raw
    // user-chosen basename once the transfer completes.
    d->resetObjectStates(this);

    // resetObjectStates covers caves/trips/notes only, so re-register each
    // layer's .cwlaz path here. The rowsInserted hookup that used to do it
    // fires only for re-created rows, and the retarget above keeps the rows.
    if (lazLayers != nullptr) {
        for (cwLazLayer* layer : lazLayers->layers()) {
            d->seedStatePathFromLoaded(this, layer, absolutePathPrivate(layer));
        }
    }
    saveProject(targetRootDir, region);

    setSaveEnabled(true);
    enableGuard.dismiss();

    return transferWarning;
}

Monad::ResultBase cwSaveLoad::renameDataRootOnDisk(const QDir& targetRootDir,
                                                   const QString& newDataRootName)
{
    const auto region = d->m_regionTreeModel->cavingRegion();
    QString oldDataRootName = d->projectMetadata.dataRoot;
    if (oldDataRootName.isEmpty()) {
        oldDataRootName = cwSaveLoadPrivate::defaultDataRoot(region ? region->name() : QString());
    }

    if (newDataRootName.isEmpty() || oldDataRootName == newDataRootName) {
        return ResultBase();
    }

    const QString oldDataRootPath = targetRootDir.absoluteFilePath(oldDataRootName);
    const QString newDataRootPath = targetRootDir.absoluteFilePath(newDataRootName);
    if (QFileInfo::exists(oldDataRootPath)) {
        if (QFileInfo::exists(newDataRootPath)) {
            return ResultBase(QStringLiteral("Destination data root '%1' already exists.").arg(newDataRootPath));
        }
        auto moveResult = cwSaveLoadPrivate::moveDirectoryRobust(oldDataRootPath, newDataRootPath);
        if (moveResult.hasError()) {
            return moveResult;
        }
        d->projectMetadata.dataRoot = newDataRootName;
        emit dataRootChanged();
        return moveResult;
    }

    QDir(targetRootDir).mkpath(newDataRootName);
    d->projectMetadata.dataRoot = newDataRootName;
    emit dataRootChanged();

    return ResultBase();
}

Monad::ResultBase cwSaveLoad::prepareBundleStage(const QString& bundleBaseName)
{
    const QString sanitized = sanitizeFileName(bundleBaseName);
    Q_ASSERT(bundleBaseName.isEmpty() || !sanitized.isEmpty());
    if (sanitized.isEmpty()) {
        return ResultBase();
    }
    if (d->projectMetadata.dataRoot == sanitized) {
        return ResultBase();
    }
    if (d->projectFileName.isEmpty()) {
        return ResultBase(QStringLiteral("Project does not have an associated file."));
    }

    setSaveEnabled(false);
    auto enableGuard = qScopeGuard([this]() { setSaveEnabled(true); });

    waitForFinished();

    const auto region = d->m_regionTreeModel->cavingRegion();

    // The region-rename handler (connected to cavingRegion::nameChanged) early-outs
    // for temporary projects, so setting the display name here does not trigger a
    // second on-disk rename. renameDataRootOnDisk below does the real work.
    if (region) {
        region->setName(bundleBaseName);
    }

    const QDir rootDir = projectRootDir();
    auto renameResult = renameDataRootOnDisk(rootDir, sanitized);
    if (renameResult.hasError()) {
        return renameResult;
    }

    d->resetObjectStates(this);
    saveProject(rootDir, region);

    return ResultBase();
}

cwSaveLoad::cwSaveLoad(QObject *parent) :
    QObject(parent),
    d(std::make_unique<cwSaveLoadPrivate>())
{
    d->m_regionTreeModel = new cwRegionTreeModel(this);
    d->repository = new QQuickGit::GitRepository(this);
    d->repository->setLfsPolicy(cavewhereLfsPolicy());

    //Connect all for watching for saves
    connectTreeModel();
}

void cwSaveLoad::newProject()
{
    Q_ASSERT(!d->newProjectCalled);
    d->newProjectCalled = true;

    Q_ASSERT(d->m_pendingJobs.isEmpty());

    //Disconnect all connections
    disconnectTreeModel();

    //Clear the current data
    auto region = d->m_regionTreeModel->cavingRegion();

    if(region) [[likely]] {
        //Generate a temporary directory name (not shown to user)
        const auto tempName = randomName();

        //Pre-fill the region name with a friendly human-readable name
        region->setName(friendlyProjectName());

        //Create the temp directory
        auto tempDir = createTemporaryDirectory(tempName);

        //Assign a stable UUID for this new project.
        d->projectMetadata.projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);

        //Save the project file
        d->projectMetadata.dataRoot = cwSaveLoadPrivate::defaultDataRoot(region->name());
        d->projectMetadata.syncEnabled = true;
        tempDir.mkpath(d->projectMetadata.dataRoot);

        markTemporaryHome();
        setFileName(regionFileName(tempDir, region));
        initializeRepositoryForCurrentFile();

        saveProject(tempDir, region);
        seedStampedVersion();

        //Connect all for watching for saves
        connectTreeModel();

        emit dataRootChanged();
    }

}

QString cwSaveLoad::fileName() const
{
    return d->projectFileName;
}

QFuture<ResultBase> cwSaveLoad::load(const QString &filename)
{
    auto saveFlushFuture = d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::SaveFlush, [this]() {
        return saveFlushImpl();
    });

    auto loadFuture = d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::LoadProject, [this, filename, saveFlushFuture]() {
        const auto saveFlushResult = saveFlushFuture.result();
        if (saveFlushResult.hasError()) {
            d->pendingIdentityRepairSave = false;
            return AsyncFuture::completed(saveFlushResult);
        }

        d->pendingIdentityRepairSave = false;
        return loadImpl(filename);
    });

    return d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::RepairSave, [this, loadFuture]() {
        const auto loadResult = loadFuture.result();
        if (loadResult.hasError()) {
            d->pendingIdentityRepairSave = false;
            return AsyncFuture::completed(loadResult);
        }

        if (!d->pendingIdentityRepairSave) {
            return AsyncFuture::completed(loadResult);
        }

        auto repairFuture = persistIdentityRepairSave();
        return AsyncFuture::observe(repairFuture)
                .context(this, [this, repairFuture]() {
            const auto result = repairFuture.result();
            if (!result.hasError()) {
                d->pendingIdentityRepairSave = false;
                ++d->modelMutationEpoch;
            }
            return result;
        }).future();
    });
}

QFuture<ResultBase> cwSaveLoad::loadImpl(const QString &filename)
{
    // qDebug() << "---- Loading: " << filename;
    const quint64 loadGeneration = d->operationGeneration;
    const auto canceledResult = []() {
        return ResultBase(QStringLiteral("Operation canceled."));
    };

    //Disconnect all connections
    disconnectTreeModel();
    d->pendingIdentityRepairSave = false;

    auto oldJobs = saveFlushImpl();

    QFuture<ResultBase> future = oldJobs.then(this, [this, filename, loadGeneration, canceledResult](ResultBase flushResult) {
        if (flushResult.hasError()) {
            return AsyncFuture::completed(flushResult);
        }

        if (d->operationGeneration != loadGeneration || d->retiring) {
            return AsyncFuture::completed(canceledResult());
        }

        const auto continueLoad = [this, filename, loadGeneration, canceledResult]() {
            if (d->operationGeneration != loadGeneration || d->retiring) {
                return AsyncFuture::completed(canceledResult());
            }

            auto projectDataFuture = cwSaveLoad::loadAll(filename);

            d->futureToken.addJob(projectDataFuture, QStringLiteral("Loading"));

            return AsyncFuture::observe(projectDataFuture)
                    .context(this, [this, projectDataFuture, filename, loadGeneration, canceledResult]() {
                return mbind(projectDataFuture, [this, projectDataFuture, filename, loadGeneration, canceledResult](const ResultBase&) {
                    if (d->operationGeneration != loadGeneration
                            || d->retiring
                            || d->m_regionTreeModel->cavingRegion() == nullptr) {
                        return canceledResult();
                    }

                    //The filename needs to be set first because, image providers should
                    //have the filename before the region model is set
                    setFileName(filename);
                    initializeRepositoryForCurrentFile();
                    //A crash between a delete and a close leaves trash entries
                    //no undo stack can reach any more, so opening a project
                    //empties the trash.
                    d->sweepTrash(this);
                    markDurableHome();

                    setSaveEnabled(false);

                    const auto& loadData = projectDataFuture.result().value();
                    d->projectMetadata = loadData.metadata;
                    d->pendingIdentityRepairSave = loadData.identityRepair.required;
                    d->lastLoadErrors = loadData.errors;
                    d->lastLoadMaxFileVersion = loadData.maxFileVersion;
                    d->saveBlockedWarningEmitted = false;
                    emit dataRootChanged();

                    d->m_regionTreeModel->cavingRegion()->setData(loadData.region);

                    //What is on disk is what the project was just loaded from,
                    //so the rewrite pass starts from the file version the load
                    //saw rather than from a previously opened project's.
                    seedStampedVersion();

                    // Clear the undo stack so old objects from
                    // clearCaves()/addCaves() commands are freed.
                    if (auto* undoStack = d->m_regionTreeModel->cavingRegion()->undoStack()) {
                        undoStack->clear();
                    }

                    // d->projectFileName = filename;

                    d->resetObjectStates(this);

                    setSaveEnabled(true);
                    connectTreeModel();
                    ++d->modelMutationEpoch;

                    return ResultBase();
                });
            }).future();
        };

        // LFS hydration is not attempted during load — it would
        // trigger a keychain prompt before the user has interacted.
        // Missing files are detected post-load and surfaced via
        // cwProject::lfsFilesNeedSync; the user syncs explicitly.
        return continueLoad();
    }).unwrap();
    return future;
}

QFuture<ResultBase> cwSaveLoad::saveFlushImpl()
{
    return AsyncFuture::observe(completeSaveJobs())
            .context(this, [this]() {
        const QString pendingErrors = d->takePendingSaveJobErrors();
        emit saveFlushCompleted();
        if (!pendingErrors.isEmpty()) {
            return ResultBase(QStringLiteral("Save flush failed:\n%1").arg(pendingErrors));
        }
        return ResultBase();
    })
            .future();
}

QFuture<ResultBase> cwSaveLoad::persistIdentityRepairSave(bool persistNoteDescriptors,
                                                          bool persistLiDARNoteDescriptors)
{
    auto* region = d->m_regionTreeModel->cavingRegion();
    if (region == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("No caving region is available for repair save.")));
    }

    const bool previousSuppressTracking = d->suppressLocalMutationTracking;
    d->suppressLocalMutationTracking = true;
    auto suppressTrackingGuard = qScopeGuard([this, previousSuppressTracking]() {
        d->suppressLocalMutationTracking = previousSuppressTracking;
    });

    // Avoid rewriting project metadata for every reconcile apply. This can create
    // unnecessary sync churn (for example, cavewhereVersion-only diffs) when the
    // reconcile only touches cave/trip/note payload paths.
    if (d->pendingIdentityRepairSave) {
        saveProject(projectRootDir(), region);
    }
    const bool persistAllDescriptors = d->pendingIdentityRepairSave;
    const bool persistNotes = persistAllDescriptors || persistNoteDescriptors;
    const bool persistLiDARNotes = persistAllDescriptors || persistLiDARNoteDescriptors;

    //Every node of the tree, not only the root's own children: a nested node
    //whose on-disk name has drifted needs its rename enqueued before
    //cleanupStaleLoadedPaths runs at the end of this same function, or that
    //cleanup removes its directory with nothing having moved the content out.
    const QList<cwSurveyNode*> nodes = region->rootNode()->allNodes();
    for (cwSurveyNode* node : nodes) {
        auto* cave = qobject_cast<cwCave*>(node);
        if (cave == nullptr) {
            continue;
        }

        d->enqueueRenameIfNeeded(this, cave);
        if (persistAllDescriptors) {
            save(cave);
        }
        for (cwTrip* trip : cave->trips()) {
            d->enqueueRenameIfNeeded(this, trip);
            if (persistAllDescriptors) {
                save(trip);
            }

            for (cwNote* note : trip->notes()->notes()) {
                d->enqueueRenameIfNeeded(this, note);
                if (persistNotes) {
                    save(note);
                }
            }

            for (QObject* noteObject : trip->notesLiDAR()->notes()) {
                if (auto* lidarNote = qobject_cast<cwNoteLiDAR*>(noteObject)) {
                    d->enqueueRenameIfNeeded(this, lidarNote);
                    if (persistLiDARNotes) {
                        save(lidarNote);
                    }
                }
            }

            for (QObject* sketchObject : trip->notesSketch()->notes()) {
                if (auto* sketch = qobject_cast<cwSketch*>(sketchObject)) {
                    d->enqueueRenameIfNeeded(this, sketch);
                    if (persistAllDescriptors) {
                        save(sketch);
                    }
                }
            }
        }
    }

    auto flushFuture = saveFlushImpl();
    return AsyncFuture::observe(flushFuture)
            .context(this, [this, flushFuture]() -> ResultBase {
        const auto flushResult = flushFuture.result();
        if (flushResult.hasError()) {
            return flushResult;
        }

        return d->cleanupStaleLoadedPaths(this);
    })
            .future();
}

QFuture<ResultBase> cwSaveLoad::enqueueReconcilePhase(const QFuture<ResultBase>& prepareFuture,
                                                      quint64 syncGeneration,
                                                      const std::shared_ptr<ReconcileAttemptState>& attemptState,
                                                      ReconcileApplyMode applyMode)
{
    return d->enqueueOperation(
                this,
                cwSaveLoadPrivate::Operation::Type::ReconcileExternal,
                [this, prepareFuture, syncGeneration, attemptState, applyMode]() -> QFuture<ResultBase> {
        Q_ASSERT(prepareFuture.isFinished());
        const auto prepareResult = prepareFuture.result();
        if (prepareResult.hasError()) {
            return AsyncFuture::completed(prepareResult);
        }

        if (d->operationGeneration != syncGeneration || d->retiring) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Operation canceled.")));
        }

        if (!attemptState->report.has_value()) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Sync report is unavailable.")));
        }

        if (d->localMutationEpoch != attemptState->localMutationPlanEpoch) {
            return AsyncFuture::completed(ResultBase(
                                              QStringLiteral("Sync plan is stale because the model changed before apply."),
                                              static_cast<int>(SyncErrorCode::RetryEpochChanged)));
        }
        auto reconcileImplFuture = reconcileExternalImpl(*attemptState->report,
                                                         syncGeneration,
                                                         attemptState->planEpoch,
                                                         applyMode);
        return AsyncFuture::observe(reconcileImplFuture)
                .context(this, [reconcileImplFuture, attemptState]() -> ResultBase {
            const auto reconcileResult = reconcileImplFuture.result();
            if (reconcileResult.hasError()) {
                return ResultBase(reconcileResult.errorMessage(),
                                  reconcileResult.errorCode());
            }

            attemptState->reconcileOutcome = reconcileResult.value();
            return ResultBase();
        })
                .future();
    });
}

QFuture<ResultBase> cwSaveLoad::enqueueFinalizePhase(const QFuture<ResultBase>& reconcileFuture,
                                                     quint64 syncGeneration,
                                                     QQuickGit::GitRepository* repo,
                                                     const std::shared_ptr<ReconcileAttemptState>& attemptState,
                                                     FinalizeMode mode)
{
    return d->enqueueOperation(
                this,
                cwSaveLoadPrivate::Operation::Type::SyncProject,
                [this, reconcileFuture, syncGeneration, repo, attemptState, mode]() -> QFuture<ResultBase> {
        Q_ASSERT(reconcileFuture.isFinished());
        const auto reconcileResult = reconcileFuture.result();
        if (reconcileResult.hasError()) {
            return AsyncFuture::completed(reconcileResult);
        }

        if (d->operationGeneration != syncGeneration || d->retiring) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Operation canceled.")));
        }

        if (!attemptState->reconcileOutcome.has_value()) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Reconcile outcome is unavailable.")));
        }

        const auto staleFromRemoteApplyGuard = [this]() -> std::optional<ResultBase> {
                if (!d->remoteApplyGuard.hasLocalMutation(d->localMutationEpoch)) {
                return std::nullopt;
                }
                return ResultBase(
                    QStringLiteral("Sync plan is stale because the model changed during remote apply."),
                    static_cast<int>(SyncErrorCode::RetryEpochChanged));
                };

        const auto pushWithoutRetry = [this, repo, syncGeneration, staleFromRemoteApplyGuard]() -> QFuture<ResultBase> {
            if (const auto staleResult = staleFromRemoteApplyGuard()) {
                return AsyncFuture::completed(*staleResult);
            }

            auto pushFuture = repo->push();
            return AsyncFuture::observe(pushFuture)
                    .context(this, [this, repo, pushFuture, syncGeneration]() -> ResultBase {
                if (d->operationGeneration != syncGeneration || d->retiring) {
                    return ResultBase(QStringLiteral("Operation canceled."));
                }
                const auto pushResult = pushFuture.result();
                if (repo) {
                    repo->checkStatus();
                }
                return pushResult;
            })
                    .future();
        };

        if (mode == FinalizeMode::SyncPush
                && attemptState->reconcileOutcome->outcome == ReconcileExternalResult::Outcome::NoOp) {
            return pushWithoutRetry();
        }

        if (const auto staleResult = staleFromRemoteApplyGuard()) {
            return AsyncFuture::completed(*staleResult);
        }

        if (attemptState->reconcileOutcome->outcome
                != ReconcileExternalResult::Outcome::MutatedRequiresPrePushPersistence) {
            return mode == FinalizeMode::SyncPush
                    ? pushWithoutRetry()
                    : AsyncFuture::completed(ResultBase());
        }

        auto repairFuture = persistIdentityRepairSave(
                    attemptState->reconcileOutcome->persistNoteDescriptors,
                    attemptState->reconcileOutcome->persistLiDARNoteDescriptors);
        return AsyncFuture::observe(repairFuture)
                .context(this, [this, repairFuture, pushWithoutRetry, staleFromRemoteApplyGuard, syncGeneration, mode]() -> QFuture<ResultBase> {
            if (d->operationGeneration != syncGeneration || d->retiring) {
                return AsyncFuture::completed(ResultBase(QStringLiteral("Operation canceled.")));
            }

            const auto repairResult = repairFuture.result();
            if (repairResult.hasError()) {
                return AsyncFuture::completed(repairResult);
            }

            if (const auto staleResult = staleFromRemoteApplyGuard()) {
                return AsyncFuture::completed(*staleResult);
            }

            d->pendingIdentityRepairSave = false;
            ++d->modelMutationEpoch;

            if (mode == FinalizeMode::CheckoutLocal) {
                return AsyncFuture::completed(ResultBase());
            }

            auto commitResult = commitProjectChanges(defaultCommitSubject(QStringLiteral("Sync Reconcile")),
                                                     defaultCommitDescription());
            if (commitResult.hasError()) {
                return AsyncFuture::completed(commitResult);
            }

            return pushWithoutRetry();
        })
                .future();
    });
}

void cwSaveLoad::initializeRepositoryForCurrentFile()
{
    if (d->projectFileName.isEmpty()) {
        return;
    }

    d->repository->setDirectory(QFileInfo(d->projectFileName).absoluteDir());
    d->repository->initRepository();
    ensureGitExcludeHasLocalEntries(d->repository->directory());
}

void cwSaveLoad::updateFileNameFromSingleCwproj(const QString& repoPath)
{
    if (QFileInfo::exists(d->projectFileName)) {
        return;
    }
    const QDir repoRootDir(repoPath);
    const QStringList cwprojFiles =
            repoRootDir.entryList(QStringList() << QStringLiteral("*.cwproj"), QDir::Files);
    if (cwprojFiles.size() == 1) {
        setFileName(repoRootDir.absoluteFilePath(cwprojFiles.first()));
    }
}

void cwSaveLoad::setFileName(const QString &filename)
{
    //This should load the filename
    if(d->projectFileName != filename) {
        d->projectFileName = filename;
        d->repository->setDirectory(QFileInfo(filename).absoluteDir());

        // Repoint the GIS Layers folder so the laz model scans the new root.
        // Applies to newProject (temp dir), load, save-as, and rename paths
        // — all of which funnel through setFileName.
        if (!d->projectFileName.isEmpty()) {
            if (auto* region = d->m_regionTreeModel
                    ? d->m_regionTreeModel->cavingRegion()
                    : nullptr) {
                if (auto* lazLayers = region->lazLayers()) {
                    lazLayers->setGisLayersDir(
                                QDir(projectRootDir().absoluteFilePath(
                                         cwLazLayerModel::folderName())));
                }
            }
        }

        emit fileNameChanged();
    }
}


void cwSaveLoad::setCavingRegion(cwCavingRegion *region)
{
    d->m_regionTreeModel->setCavingRegion(region);
    if (region != nullptr && !d->projectFileName.isEmpty()) {
        if (auto* lazLayers = region->lazLayers()) {
            lazLayers->setGisLayersDir(
                        QDir(projectRootDir().absoluteFilePath(
                                 cwLazLayerModel::folderName())));
        }
    }
}

const cwCavingRegion *cwSaveLoad::cavingRegion() const
{
    return d->m_regionTreeModel->cavingRegion();
}

void cwSaveLoad::setSaveEnabled(bool enabled)
{
    if (d->saveEnabled != enabled) {
    }
    d->saveEnabled = enabled;
}

QQuickGit::GitRepository *cwSaveLoad::repository() const
{
    return d->repository;
}

cwFutureManagerToken cwSaveLoad::futureManagerToken() const
{
    return d->futureToken;
}

void cwSaveLoad::setFutureManagerToken(const cwFutureManagerToken &futureManagerToken)
{
    d->futureToken = futureManagerToken;
}

void cwSaveLoad::setUndoStack(QUndoStack *undoStack)
{
    if(m_undoStack != undoStack) {
        m_undoStack = undoStack;
    }
}

void cwSaveLoad::saveProject(const QDir &dir, const cwCavingRegion *region)
{
    Q_UNUSED(dir);
    saveProtoMessage(toProtoProject(region), region);
}

void cwSaveLoad::seedStampedVersion()
{
    auto region = d->m_regionTreeModel->cavingRegion();
    d->stampedVersion = region == nullptr ? -1 : stampVersion(region);
}

void cwSaveLoad::restampProjectIfVersionChanged()
{
    auto region = d->m_regionTreeModel->cavingRegion();
    if (region == nullptr) {
        return;
    }

    if (!d->m_movingNodes.isEmpty()) {
        //Halfway through a move the node is off the tree, so stampVersion would
        //answer for a shape the project never has — and the rewrite it triggers
        //cannot reach the subtree traveling with the node. The nodeMoved
        //handler asks again once the node is back on the tree.
        return;
    }

    const int version = stampVersion(region);
    if (version == d->stampedVersion) {
        return;
    }

    if (!d->saveEnabled || d->projectFileName.isEmpty()) {
        //The version moved but nothing can be written yet, so it stays
        //unrecorded and the next call that can write does the rewrite.
        return;
    }

    d->stampedVersion = version;

    //Every file of a project carries the same FileVersion, so the whole project
    //is rewritten the moment the format it needs changes — the first hierarchy
    //appearing, or the last of it going away. That keeps the stamp an honest
    //answer to "can the previous build open this?" instead of a record of what
    //each file happened to be written beside.
    saveProject(projectRootDir(), region);

    const QList<cwSurveyNode*> nodes = region->rootNode()->allNodes();
    for (cwSurveyNode* node : nodes) {
        if (auto cave = qobject_cast<cwCave*>(node)) {
            save(cave);
        }
    }

    const QList<cwTrip*> trips = region->rootNode()->allTrips();
    for (cwTrip* trip : trips) {
        save(trip);

        const QList<cwNote*> notes = trip->notes()->notes();
        for (cwNote* note : notes) {
            save(note);
        }

        const QList<QObject*> lidarNotes = trip->notesLiDAR()->notes();
        for (QObject* note : lidarNotes) {
            if (auto lidar = qobject_cast<cwNoteLiDAR*>(note)) {
                save(lidar);
            }
        }

        const QList<QObject*> sketches = trip->notesSketch()->notes();
        for (QObject* sketch : sketches) {
            if (auto sketchObject = qobject_cast<cwSketch*>(sketch)) {
                save(sketchObject);
            }
        }
    }
}

std::unique_ptr<CavewhereProto::Project> cwSaveLoad::toProtoProject(const cwCavingRegion *region)
{
    auto protoProject = std::make_unique<CavewhereProto::Project>();
    auto fileVersion = protoProject->mutable_fileversion();
    fileVersion->set_version(stampVersion(region));
    cwProtoUtils::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    if (region != nullptr) {
        cwProtoUtils::saveString(protoProject->mutable_name(), region->name());
    }

    // Write the UUID only if one has been assigned (new projects get one in newProject();
    // legacy projects loaded without a UUID field are left as-is to avoid spurious diffs).
    if (!d->projectMetadata.projectId.isEmpty()) {
        *(protoProject->mutable_id()) = d->projectMetadata.projectId.toStdString();
    }

    auto metadata = d->projectMetadata;
    if (metadata.dataRoot.isEmpty()) {
        metadata.dataRoot = cwSaveLoadPrivate::defaultDataRoot(region ? region->name() : QString());
    }
    d->projectMetadata = metadata;

    auto protoMetadata = protoProject->mutable_metadata();
    cwProtoUtils::saveString(protoMetadata->mutable_dataroot(), metadata.dataRoot);
    protoMetadata->set_syncenabled(metadata.syncEnabled);

    if (region != nullptr) {
        protoMetadata->set_unitsystem(
            static_cast<CavewhereProto::Units_UnitSystem>(region->unitSystem()));

        // The local projection: written whole, and only once there is something
        // to say, so a project that has never been georeferenced keeps a
        // metadata file with nothing to say about its frame. The vertical datum
        // is not part of the frame — elevations, and whatever they are heights
        // above, exist before any anchor does — so it keeps the message alive on
        // its own.
        const auto* geoReference = region->geoReference();
        if (!geoReference->localCoordinateSystem().isEmpty()
            || !geoReference->verticalDatum().isEmpty()) {
            auto protoGeoReference = protoMetadata->mutable_georeference();
            protoGeoReference->set_state(
                static_cast<CavewhereProto::GeoReference_State>(geoReference->state()));
            cwProtoUtils::saveString(protoGeoReference->mutable_localcoordinatesystem(),
                                     geoReference->localCoordinateSystem());

            const auto anchor = geoReference->anchor();
            protoGeoReference->set_anchorkind(
                static_cast<CavewhereProto::GeoReference_AnchorKind>(anchor.kind));
            if (anchor.isValid()) {
                cwProtoUtils::saveString(protoGeoReference->mutable_anchorid(),
                                         anchor.id.toString(QUuid::WithoutBraces));
            }

            if (!geoReference->verticalDatum().isEmpty()) {
                cwProtoUtils::saveString(protoGeoReference->mutable_verticaldatum(),
                                         geoReference->verticalDatum());
            }
        }

        for (const cwEquate& equate : region->equates()->equates()) {
            cwProtoUtils::saveEquate(protoProject->add_equates(), equate);
        }
    }

    return protoProject;
}

void cwSaveLoad::saveCavingRegion(const cwCavingRegion *region)
{
    if (d->saveWillCauseDataLoss()) {
        if (!d->saveBlockedWarningEmitted) {
            d->saveBlockedWarningEmitted = true;
            emit saveBlockedByVersion(QStringLiteral("cwCavingRegion"));
        }
        return;
    }
    saveProtoMessage(toProtoCavingRegion(region), region);
}

std::unique_ptr<CavewhereProto::CavingRegion> cwSaveLoad::toProtoCavingRegion(const cwCavingRegion *region)
{
    auto protoRegion = std::make_unique<CavewhereProto::CavingRegion>();
    protoRegion->set_version(stampVersion(region));
    cwProtoUtils::saveString(protoRegion->mutable_cavewhereversion(), CavewhereVersion);
    cwProtoUtils::saveString(protoRegion->mutable_name(), region->name());
    return protoRegion;
}

QString cwSaveLoad::regionFileName(const QDir &dir, const cwCavingRegion *region)
{
    return dir.absoluteFilePath(sanitizeFileName(QStringLiteral("%1.cwproj").arg(region->name())));
}

void cwSaveLoad::save(const cwCave *cave)
{
    d->saveObject(this, cave);
}

void cwSaveLoad::save(const cwTrip *trip)
{
    d->saveObject(this, trip);
}

void cwSaveLoad::save(const cwNote* note)
{
    d->saveObject(this, note);
}

std::unique_ptr<CavewhereProto::Cave> cwSaveLoad::toProtoCave(const cwCave *cave)
{
    auto protoCave = std::make_unique<CavewhereProto::Cave>();
    auto fileVersion = protoCave->mutable_fileversion();
    fileVersion->set_version(stampVersionFor(cave));
    cwProtoUtils::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);
    *(protoCave->mutable_name()) = cave->name().toStdString();
    if (!cave->id().isNull()) {
        *(protoCave->mutable_id()) = uuidToProtoString(cave->id()).toStdString();
    }
    protoCave->set_lengthunit(static_cast<CavewhereProto::Units_LengthUnit>(cave->length()->unit()));
    protoCave->set_depthunit(static_cast<CavewhereProto::Units_LengthUnit>(cave->depth()->unit()));

    for (const cwFixStation& fix : cave->fixStations()->fixStations()) {
        cwProtoUtils::saveFixStation(protoCave->add_fixstations(), fix);
    }

    if (!cave->externalCenterline().isEmpty()) {
        auto* protoExternal = protoCave->mutable_external_centerline();
        *(protoExternal->mutable_entry_file()) =
                cave->externalCenterline().entryFile().toStdString();
    }

    //Survey-tree scalars. Each is written only when it differs from the default
    //a file without it means, so a flat project's .cwcave is byte-identical to
    //what the previous format wrote.
    if (cave->kind() != cwSurveyNode::Kind::Cave) {
        protoCave->set_kind(static_cast<CavewhereProto::SurveyNodeKind>(cave->kind()));
    }
    if (cave->isReadOnly()) {
        protoCave->set_read_only(true);
    }
    if (!cave->sourceId().isNull()) {
        *(protoCave->mutable_source_id()) = uuidToProtoString(cave->sourceId()).toStdString();
    }
    if (!cave->sourcePath().isEmpty()) {
        *(protoCave->mutable_source_path()) = cave->sourcePath().toStdString();
    }

    return protoCave;
}

std::unique_ptr<CavewhereProto::Trip> cwSaveLoad::toProtoTrip(const cwTrip *trip)
{
    //Copy trip data into proto, on the main thread
    auto protoTrip = std::make_unique<CavewhereProto::Trip>();
    auto fileVersion = protoTrip->mutable_fileversion();
    fileVersion->set_version(stampVersionFor(trip));
    cwProtoUtils::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    *(protoTrip->mutable_name()) = trip->name().toStdString();
    if (!trip->id().isNull()) {
        *(protoTrip->mutable_id()) = uuidToProtoString(trip->id()).toStdString();
    }

    // cwProtoUtils::saveString(protoTrip->mutable_name(), trip->name());
    cwProtoUtils::saveDate(protoTrip->mutable_date(), trip->date().date());
    cwProtoUtils::saveTripCalibration(protoTrip->mutable_tripcalibration(), trip->calibrations());

    if(trip->team()->rowCount() > 0) {
        cwProtoUtils::saveTeam(protoTrip->mutable_team(), trip->team());
    }

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        CavewhereProto::SurveyChunk* protoChunk = protoTrip->add_chunks();
        cwProtoUtils::saveSurveyChunk(protoChunk, chunk);
    }

    if (!trip->externalCenterline().isEmpty()) {
        auto* protoExternal = protoTrip->mutable_external_centerline();
        *(protoExternal->mutable_entry_file()) =
                trip->externalCenterline().entryFile().toStdString();
    }

    if (!trip->stationPrefix().isEmpty()) {
        *(protoTrip->mutable_station_prefix()) = trip->stationPrefix().toStdString();
    }

    if (!trip->sourcePath().isEmpty()) {
        *(protoTrip->mutable_source_path()) = trip->sourcePath().toStdString();
    }

    return protoTrip;
}

// Keep your original behavior: PDFs are detected but handled separately.
inline bool isPDF(const QString& path) {
    if (cwPDFConverter::isSupported()) {
        QFileInfo info(path);
        return info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0;
    }
    return false;
}

struct CopyCommand {
    int index = 0;
    QString sourceFilePath;
    QString destinationFilePath;
};

static QString uniqueDestinationPath(const QDir& destinationDirectory,
                                     const QString& sourceFilePath,
                                     const QSet<QString>& reservedPaths)
{
    const QFileInfo sourceInfo(sourceFilePath);
    const QString baseName = sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix();

    auto isTaken = [&reservedPaths](const QString& path) {
        return QFileInfo::exists(path) || reservedPaths.contains(path);
    };

    QString candidate = destinationDirectory.absoluteFilePath(sourceInfo.fileName());
    int counter = 1;
    while (isTaken(candidate)) {
        const QString numberedName = suffix.isEmpty()
                ? QStringLiteral("%1-%2").arg(baseName).arg(counter)
                : QStringLiteral("%1-%2.%3").arg(baseName).arg(counter).arg(suffix);
        candidate = destinationDirectory.absoluteFilePath(numberedName);
        ++counter;
    }

    return candidate;
}


// ------------------------
// Generic copy-and-emit pipe
// ------------------------
template<typename ResultType, typename MakeResultFunc>
QFuture<ResultBase> cwSaveLoad::copyFilesAndEmitResults(const QList<QString>& sourceFilePaths,
                                                        const QDir& destinationDirectory,
                                                        MakeResultFunc makeResult,
                                                        std::function<void (QList<ResultType>)> outputCallBackFunc)
{
    const QDir rootDirectory = dataRootDir();
    Q_ASSERT(rootDirectory.exists());
    // qDebug() << "RootDir:" << rootDirectory;

    QList<CopyCommand> commands;
    commands.reserve(sourceFilePaths.size());
    QSet<QString> reservedPaths;
    reservedPaths.reserve(sourceFilePaths.size());
    int index = 0;
    for (const QString& source : sourceFilePaths) {
        const QString destination = uniqueDestinationPath(destinationDirectory,
                                                          source,
                                                          reservedPaths);
        commands.append(CopyCommand{index, source, destination});
        reservedPaths.insert(destination);
        ++index;
    }

    if (commands.isEmpty()) {
        outputCallBackFunc({});
        return AsyncFuture::completed(ResultBase());
    }

    struct CopyBatchState {
        QVector<Monad::Result<QString>> results;
        std::atomic<int> remaining{0};
        AsyncFuture::Deferred<ResultBase> deferred;

        explicit CopyBatchState(int count)
            : results(count),
              remaining(count)
        {
        }
    };

    auto state = std::make_shared<CopyBatchState>(commands.size());

    auto finalizeResults = [state, makeResult, outputCallBackFunc]() {
        QList<ResultType> finalResults;
        finalResults.reserve(state->results.size());
        QStringList errorMessages;

        std::transform(state->results.begin(), state->results.end(),
                       std::back_inserter(finalResults),
                       [makeResult, &errorMessages](const Monad::Result<QString>& relativePathResult) {
            if (!relativePathResult.hasError()) {
                return makeResult(relativePathResult.value()); // ResultType
            } else {
                qWarning() << "Error:" << relativePathResult.errorMessage() << LOCATION;
                errorMessages.append(relativePathResult.errorMessage());
                return ResultType{};
            }
        });

        outputCallBackFunc(finalResults);
        if (!errorMessages.isEmpty()) {
            state->deferred.complete(ResultBase(QStringLiteral("Import copy failed:\n%1")
                                                .arg(errorMessages.join(QLatin1Char('\n')))));
        } else {
            state->deferred.complete(ResultBase());
        }
    };

    for (const CopyCommand& command : commands) {
        cwSaveLoadPrivate::Job job;
        job.action = cwSaveLoadPrivate::Job::Action::Copy;
        job.kind = cwSaveLoadPrivate::Job::Kind::File;
        job.payload = cwSaveLoadPrivate::Job::CopyFilePayload{command.sourceFilePath};
        job.path = command.destinationFilePath;
        job.onDone = [state, command, rootDirectory, finalizeResults](const Monad::ResultBase& result) {
            if (!result.hasError()) {
                state->results[command.index] = Monad::ResultString(rootDirectory.relativeFilePath(command.destinationFilePath));
            } else {
                state->results[command.index] = Monad::ResultString(result.errorMessage(), result.errorCode());
            }

            if (state->remaining.fetch_sub(1) == 1) {
                finalizeResults();
            }
        };

        d->addExplicitFileSystemJob(job, this);
    }
    return state->deferred.future();
}

// ------------------------
// Specializations for cwImage and QString
// ------------------------
static auto makeImageFromRelativePath(const QDir& rootDirectory) {
    return [rootDirectory](const QString& relativePath) -> cwImage {
        const QString absolutePath = rootDirectory.absoluteFilePath(relativePath);
        cwImage image;
        image.setOriginalImageInfo(imageInfo(absolutePath));
        const QFileInfo fileInfo(relativePath);
        image.setPath(fileInfo.fileName());
        // qDebug() << "Returning image:" << image;
        return image;
    };
}

static auto makeRelativePathEcho() {
    return [](const QString& relativePath) -> QString {
        return relativePath;
    };
}

// ------------------------
// addImages: now uses the generic pipeline
// ------------------------
void cwSaveLoad::addImages(QList<QUrl> noteImagePaths,
                           const QDir& dir,
                           std::function<void (QList<cwImage>)> outputCallBackFunc)
{
    addImages(noteImagePaths, [dir]() { return dir; }, outputCallBackFunc);
}

void cwSaveLoad::addImages(QList<QUrl> noteImagePaths,
                           std::function<QDir()> destinationDirResolver,
                           std::function<void (QList<cwImage>)> outputCallBackFunc)
{
    auto queuedFuture = d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::ImportFiles, [this, noteImagePaths, destinationDirResolver, outputCallBackFunc]() {
        const quint64 importGeneration = d->operationGeneration;
        auto guardedCallback = [this, importGeneration, outputCallBackFunc](QList<cwImage> images) {
            if (d->operationGeneration != importGeneration || d->retiring) {
                return;
            }
            outputCallBackFunc(images);
        };

        if (!destinationDirResolver) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Import destination resolver is missing.")));
        }
        Q_ASSERT(QThread::currentThread() == this->thread());
        const QDir destinationDir = destinationDirResolver();
        if (destinationDir.absolutePath().isEmpty()) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Import destination directory is invalid.")));
        }

        QVector<QString> imageFilePaths;
        QVector<QString> pdfFilePaths;
        imageFilePaths.reserve(noteImagePaths.size());
        pdfFilePaths.reserve(noteImagePaths.size());

        for (const QUrl& url : noteImagePaths) {
            const QString path = cwGlobals::importPathFromUrl(url);
            if (path.isEmpty()) continue;
            if (isPDF(path)) {
                pdfFilePaths.append(path);
            } else {
                imageFilePaths.append(path);
            }
        }

        const QDir rootDirectory = dataRootDir();
        auto imageFuture = copyFilesAndEmitResults<cwImage>(
                    imageFilePaths,
                    destinationDir,
                    makeImageFromRelativePath(rootDirectory),
                    guardedCallback);

        auto finishImport = [this](QFuture<ResultBase> copyFuture) {
            return copyFuture.then(this, [this](ResultBase copyResult) -> QFuture<ResultBase> {
                if (copyResult.hasError()) {
                    return AsyncFuture::completed(copyResult);
                }
                return saveFlushImpl();
            }).unwrap();
        };

        if (pdfFilePaths.isEmpty() || !cwPDFConverter::isSupported()) {
            return finishImport(imageFuture);
        }

#ifdef CW_WITH_PDF_SUPPORT
        auto makeImagesFromPdf = [rootDirectory](const QString& relativePath) {
            QList<cwImage> images;
            const QString absolutePath = rootDirectory.absoluteFilePath(relativePath);

            QPdfDocument document;
            if (document.load(absolutePath) != QPdfDocument::Error::None) {
                qWarning() << "Failed to load PDF:" << absolutePath;
                return images;
            }

            const int pageCount = document.pageCount();
            const QString fileName = QFileInfo(relativePath).fileName();

            images.reserve(pageCount);

            for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
                cwImage image;
                image.setPath(fileName);
                image.setOriginalImageInfo(imageInfo(document, pageIndex));
                image.setPage(pageIndex);
                images.append(image);
            }

            return images;
        };

        auto combinedCopyFuture = imageFuture.then(this, [this, pdfFilePaths, destinationDir, makeImagesFromPdf, guardedCallback](ResultBase imageResult) -> QFuture<ResultBase> {
            if (imageResult.hasError()) {
                return AsyncFuture::completed(imageResult);
            }

            return copyFilesAndEmitResults<QString>(
                        pdfFilePaths,
                        destinationDir,
                        makeRelativePathEcho(),
                        [makeImagesFromPdf, guardedCallback](QList<QString> relativePaths) {
                QList<cwImage> allImages;
                for (const QString& relativePath : relativePaths) {
                    allImages.append(makeImagesFromPdf(relativePath));
                }
                if (!allImages.isEmpty()) {
                    guardedCallback(allImages);
                }
            });
        }).unwrap();

        return finishImport(combinedCopyFuture);
#else
        qWarning() << "PDF support not enabled for cwSaveLoad::addImages";
        return finishImport(imageFuture);
#endif
    });

    if (d->futureToken.isValid()) {
        d->futureToken.addJob(queuedFuture, QStringLiteral("Adding images"));
    }
}

ResultBase cwSaveLoad::moveProjectTo(const QString& destinationFileUrl)
{
    return transferProjectTo(destinationFileUrl, ProjectTransferMode::Move);
}

ResultBase cwSaveLoad::copyProjectTo(const QString& destinationFileUrl)
{
    return transferProjectTo(destinationFileUrl, ProjectTransferMode::Copy);
}

QFuture<ResultBase> cwSaveLoad::saveBundledArchive(const QString& targetArchivePath)
{
    const QString normalizedTargetPath = cwGlobals::convertFromURL(targetArchivePath);
    if (normalizedTargetPath.isEmpty()) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Bundle target path is empty.")));
    }

    auto saveFlushFuture = d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::SaveFlush, [this]() {
        return saveFlushImpl();
    });

    return d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::BundlePackage, [this, saveFlushFuture, normalizedTargetPath]() {
        const auto flushResult = saveFlushFuture.result();
        if (flushResult.hasError()) {
            return AsyncFuture::completed(flushResult);
        }

        const auto commitResult = commitProjectChanges();
        if (commitResult.hasError()) {
            qWarning() << "Bundled save commit skipped:" << commitResult.errorMessage();
        }

        const QString workingProjectFile = fileName();
        const QString projectRootPath = QFileInfo(workingProjectFile).absolutePath();
        QStringList excludePatterns = readGitExcludePatterns(QDir(projectRootPath));
        if (!excludePatterns.contains(kTrashDirPattern)) {
            //A bundle is a copy, and a copy carries none of what a delete took
            //away — including for a project that has no git exclude file yet.
            excludePatterns.append(kTrashDirPattern);
        }
        auto packageFuture = QtConcurrent::run(&d->m_saveThreadPool, [projectRootPath, normalizedTargetPath, excludePatterns]() {
            return saveBundledArchiveAtomic(projectRootPath, normalizedTargetPath, excludePatterns);
        });

        if (d->futureToken.isValid()) {
            d->futureToken.addJob(packageFuture, QStringLiteral("Bundling project"));
        }

        // A written archive is a durable home, so mark it just like
        // load() and transferProjectTo() do. Leaving it
        // stale after Save As to a bundle (issue #597) suppressed the
        // region-rename handler, which silently dropped a post-Save-As
        // rename from the next re-zip.
        return AsyncFuture::observe(packageFuture)
                .context(this, [this, packageFuture]() -> ResultBase {
            const auto packageResult = packageFuture.result();
            if (!packageResult.hasError()) {
                markDurableHome();
            }
            return packageResult;
        }).future();
    });
}

QFuture<ResultBase> cwSaveLoad::enqueueFlushAndCommit()
{
    auto flushFuture = saveFlushImpl();
    return AsyncFuture::observe(flushFuture)
            .context(this, [this, flushFuture]() -> ResultBase {
        const auto flushResult = flushFuture.result();
        if (flushResult.hasError()) {
            return flushResult;
        }
        return commitProjectChanges();
    })
            .future();
}

void cwSaveLoad::setOwnedTempDir(const QString& path)
{
    if (!path.isEmpty() && !d->m_ownedTempDirs.contains(path)) {
        d->m_ownedTempDirs.append(path);
    }
}

void cwSaveLoad::removeTemporaryProjectDir(const QString& ownedTempDirPath)
{
    if (ownedTempDirPath.isEmpty()) {
        return;
    }

    const QString tempRoot = QDir::tempPath() + QStringLiteral("/");
    if (!ownedTempDirPath.startsWith(tempRoot)) {
        qWarning() << "cwSaveLoad::removeTemporaryProjectDir: refusing to delete path outside"
                    << "system temp directory:" << ownedTempDirPath;
        return;
    }

    cwSaveLoadPrivate::fsRemoveDirRecursive(ownedTempDirPath);
}

// ------------------------
// addFiles: generic file ingest that returns relative paths
// ------------------------
void cwSaveLoad::addFiles(QList<QUrl> files,
                          const QDir& dir,
                          std::function<void (QList<QString>)> fileCallBackFunc)
{
    addFiles(files, [dir]() { return dir; }, fileCallBackFunc);
}

void cwSaveLoad::addFiles(QList<QUrl> files,
                          std::function<QDir()> destinationDirResolver,
                          std::function<void (QList<QString>)> fileCallBackFunc)
{
    auto queuedFuture = d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::ImportFiles, [this, files, destinationDirResolver, fileCallBackFunc]() {
        const quint64 importGeneration = d->operationGeneration;
        auto guardedCallback = [this, importGeneration, fileCallBackFunc](QList<QString> paths) {
            if (d->operationGeneration != importGeneration || d->retiring) {
                return;
            }
            fileCallBackFunc(paths);
        };

        if (!destinationDirResolver) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Import destination resolver is missing.")));
        }
        Q_ASSERT(QThread::currentThread() == this->thread());
        const QDir destinationDir = destinationDirResolver();
        if (destinationDir.absolutePath().isEmpty()) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Import destination directory is invalid.")));
        }

        const QList<QString> sourceFilePaths = cwGlobals::importPathsFromUrls(files);

        auto copyFuture = copyFilesAndEmitResults<QString>(
                    sourceFilePaths,
                    destinationDir,
                    makeRelativePathEcho(),
                    guardedCallback);

        return copyFuture.then(this, [this](ResultBase copyResult) -> QFuture<ResultBase> {
            if (copyResult.hasError()) {
                return AsyncFuture::completed(copyResult);
            }
            return saveFlushImpl();
        }).unwrap();
    });

    if (d->futureToken.isValid()) {
        d->futureToken.addJob(queuedFuture, QStringLiteral("Adding files"));
    }
}


std::unique_ptr<CavewhereProto::Note> cwSaveLoad::toProtoNote(const cwNote *note)
{
    //Copy trip data into proto, on the main thread
    auto protoNote = std::make_unique<CavewhereProto::Note>();
    auto fileVersion = protoNote->mutable_fileversion();
    fileVersion->set_version(stampVersionFor(note));
    cwProtoUtils::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    cwProtoUtils::saveImage(protoNote->mutable_image(), note->image());

    protoNote->set_rotation(note->rotate());
    cwProtoUtils::saveImageResolution(protoNote->mutable_imageresolution(), note->imageResolution());

    foreach(cwScrap* scrap, note->scraps()) {
        CavewhereProto::Scrap* protoScrap = protoNote->add_scraps();
        cwProtoUtils::saveScrap(protoScrap, scrap);
    }

    *(protoNote->mutable_name()) = note->name().toStdString();
    if (!note->id().isNull()) {
        *(protoNote->mutable_id()) = uuidToProtoString(note->id()).toStdString();
    }

    return protoNote;
}

void cwSaveLoad::save(const cwNoteLiDAR *note)
{
    d->saveObject(this, note);
}

std::unique_ptr<CavewhereProto::NoteLiDAR> cwSaveLoad::toProtoNoteLiDAR(const cwNoteLiDAR *note)
{
    //Copy trip data into proto, on the main thread
    auto protoNote = std::make_unique<CavewhereProto::NoteLiDAR>();
    auto fileVersion = protoNote->mutable_fileversion();
    fileVersion->set_version(stampVersionFor(note));
    cwProtoUtils::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    *(protoNote->mutable_name()) = note->name().toStdString();
    *(protoNote->mutable_filename()) = note->filename().toStdString();
    if (!note->id().isNull()) {
        *(protoNote->mutable_id()) = uuidToProtoString(note->id()).toStdString();
    }

    protoNote->set_autocalculatenorth(note->autoCalculateNorth());

    for(const auto& noteStation : note->stations()) {
        //Save the note stations
        auto protoNoteStation = protoNote->add_notestations();
        *(protoNoteStation->mutable_name()) = noteStation.name().toStdString();
        cwProtoUtils::saveVector3D(protoNoteStation->mutable_positiononnote(), noteStation.positionOnNote());
        if (!noteStation.id().isNull()) {
            *(protoNoteStation->mutable_id()) = uuidToProtoString(noteStation.id()).toStdString();
        }
    }

    cwProtoUtils::saveNoteLiDARTranformation(protoNote->mutable_notetransformation(), note->noteTransformation());

    return protoNote;
}

void cwSaveLoad::save(const cwSketch *sketch)
{
    d->saveObject(this, sketch);
}

std::unique_ptr<CavewhereProto::Sketch> cwSaveLoad::toProtoSketch(const cwSketch *sketch)
{
    auto protoSketch = std::make_unique<CavewhereProto::Sketch>();
    auto fileVersion = protoSketch->mutable_fileversion();
    fileVersion->set_version(stampVersionFor(sketch));
    cwProtoUtils::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);

    cwProtoUtils::saveSketch(protoSketch.get(), sketch->data());
    return protoSketch;
}

void cwSaveLoad::save(const cwLazLayer* layer)
{
    // Without a project filename the job-queue path would resolve the .cwlaz
    // to <cwd>/GIS Layers/... — silently writing into whatever directory the
    // process happens to be running from. Reject early instead.
    if (d->projectFileName.isEmpty()) {
        qWarning() << "cwSaveLoad::save(cwLazLayer*) called before setFileName(); ignoring";
        return;
    }
    d->saveObject(this, layer);
}

std::unique_ptr<CavewhereProto::LazLayer> cwSaveLoad::toProtoLazLayer(const cwLazLayer* layer)
{
    auto proto = std::make_unique<CavewhereProto::LazLayer>();
    auto fileVersion = proto->mutable_fileversion();
    //A GIS layer belongs to the project rather than to a node, so it holds no
    //hierarchy and takes the flat stamp whatever the survey tree looks like.
    fileVersion->set_version(kFlatProjectVersion);
    cwProtoUtils::saveString(fileVersion->mutable_cavewhereversion(), CavewhereVersion);
    if (!layer->id().isNull()) {
        *(proto->mutable_id()) = uuidToProtoString(layer->id()).toStdString();
    }
    proto->set_enabled(layer->enabled());
    return proto;
}


/**
 * This saves all the data in region into directory
 *
 * This is useful for converting older CaveWhere files to the new file format
 */
QFuture<ResultString> cwSaveLoad::saveAllFromV6(
        const QDir &dir,
        const cwProject* project,
        const QString& projectFileName)
{
    auto makeDir = [](const QDir& dir) {
        dir.mkpath(QStringLiteral("."));
        return dir;
    };

    const QString projectName = QFileInfo(project->filename()).baseName();
    d->projectMetadata.dataRoot = cwSaveLoadPrivate::defaultDataRoot(projectName);
    d->projectMetadata.syncEnabled = true;
    emit dataRootChanged();

    const QDir dataRootDir = makeDir(QDir(dir.absoluteFilePath(d->projectMetadata.dataRoot)));

    const int protoVersion = project->fileVersion();

    auto saveNotes = [protoVersion, makeDir, this, projectFileName, dataRootDir](const QDir& tripDir, const cwSurveyNoteModel* notes) {
        const QDir noteDir = makeDir(noteDirHelper(tripDir));

        int imageIndex = 1;
        for(cwNote* note : notes->notes()) {

            auto noteData = note->data();
            QPointer<cwNote> notePtr(note);
            auto updatedNoteData = std::make_shared<cwNoteData>();

            // AsyncFuture::Deferred<ResultBase> noteDeferred;

            cwSaveLoadPrivate::Job saveImageJob(note,
                                                cwSaveLoadPrivate::Job::Kind::File,
                                                cwSaveLoadPrivate::Job::Action::Custom);
            saveImageJob.payload = cwSaveLoadPrivate::Job::CustomPayload{[protoVersion, projectFileName, dataRootDir, noteData, imageIndex, noteDir, updatedNoteData]() {
                cwImageProvider provider;
                provider.setProjectPath(projectFileName);

                cwNote noteCopy;
                noteCopy.setData(noteData);

                if(noteCopy.name().isEmpty()) {
                    noteCopy.setName(QStringLiteral("%1").arg(imageIndex, 3, 10, QLatin1Char('0')));
                }
                auto imageData = provider.data(noteCopy.image().original());

                auto filename = noteDir.absoluteFilePath(QStringLiteral("%1.%2")
                                                         .arg(imageIndex, 3, 10, QLatin1Char('0'))
                                                         .arg(imageData.format().toLower()));

                QSaveFile file(filename);
                file.open(QSaveFile::WriteOnly);
                file.write(imageData.data());
                file.commit();

                // Save the user's imageResolution before any setImage calls,
                // which trigger resetImageResolution() and would overwrite it
                // with the image's embedded DPI.
                const auto savedResolution = noteCopy.imageResolution()->data();

                // v6 did not apply EXIF auto-transform consistently.
                // Remap scrap coordinates and fix originalSize so they match
                // the post-rotation display used in v9.
                {
                    QByteArray rawBytes = imageData.data();
                    QBuffer buf(&rawBytes);
                    buf.open(QIODevice::ReadOnly);
                    QImageReader reader(&buf);
                    const auto transform = reader.transformation();

                    if (transform != QImageIOHandler::TransformationNone) {
                        const auto info = imageInfo(filename);
                        const QSize storedSize = noteCopy.image().originalSize();
                        const bool needsCoordFix = (storedSize != info.originalSize);

                        if (needsCoordFix) {
                            QTransform coordFix;
                            if (protoVersion < 6) {
                                // V1-V5: coords in landscape space, apply EXIF rotation
                                coordFix = cwImageUtils::transformForOrientation(transform);
                            } else {
                                // V6: display was auto-rotated but toNormalized used
                                // pre-rotation dims, re-normalize to post-rotation dims
                                coordFix = cwImageUtils::reNormalizationTransform(storedSize, info.originalSize);
                            }

                            for (cwScrap* scrap : noteCopy.scraps()) {
                                QVector<QPointF> outline = scrap->points();
                                for (auto& pt : outline) {
                                    pt = coordFix.map(pt);
                                }
                                scrap->setPoints(outline);

                                auto stations = scrap->stations();
                                for (auto& station : stations) {
                                    station.setPositionOnNote(coordFix.map(station.positionOnNote()));
                                }
                                scrap->setStations(stations);

                                auto leads = scrap->leads();
                                for (auto& lead : leads) {
                                    lead.setPositionOnNote(coordFix.map(lead.positionOnNote()));
                                }
                                scrap->setLeads(leads);
                            }
                        }

                        // Set correct post-rotation originalSize + DPI
                        cwImage img = noteCopy.image();
                        img.setOriginalImageInfo(info);
                        noteCopy.setImage(img);
                    }
                }

                cwImage noteImage = noteCopy.image();
                QString relativeFilename = dataRootDir.relativeFilePath(filename);
                const QString noteRelativeFilename = noteDir.relativeFilePath(filename);
                noteImage.setPath(noteRelativeFilename);

                noteCopy.setImage(noteImage);
                noteCopy.imageResolution()->setData(savedResolution);

                *updatedNoteData = noteCopy.data();
                return ResultBase();
            }};

            saveImageJob.onDone = [this, notePtr, updatedNoteData](const ResultBase& result) mutable {
                notePtr->setData(*updatedNoteData);
                save(notePtr);
            };

            d->addExplicitFileSystemJob(saveImageJob, this);

            ++imageIndex;
        }
    };

    auto saveTrips = [this, projectFileName, makeDir, saveNotes](const QDir& caveDir, const cwCave* cave) {
        for(const auto trip : cave->trips()) {
            const QDir tripDir = makeDir(tripDirHelper(caveDir, trip));
            int missingChunkIds = 0;
            QStringList missingChunkIndices;
            const QList<cwSurveyChunk*> chunks = trip->chunks();
            for (int i = 0; i < chunks.size(); ++i) {
                const cwSurveyChunk* chunk = chunks.at(i);
                if (chunk == nullptr || chunk->id().isNull()) {
                    ++missingChunkIds;
                    missingChunkIndices.append(QString::number(i));
                }
            }

            save(trip);
            saveNotes(tripDir, trip->notes());
        }
    };


    //Save the region's data
    cwCavingRegion region;
    region.setName(projectName);
    makeDir(dir);

    QString newProjectFilename = regionFileName(dir, &region);
    setFileName(newProjectFilename);
    initializeRepositoryForCurrentFile();

    saveProject(dir, &region);


    //Go through the whole node tree, pre-order: a node's own files, then its
    //trips, then the sub/ directory below it.
    const auto saveNode = [this, makeDir, saveTrips](const QDir& nodeDir, const cwCave* node, auto&& self) -> void {
        makeDir(nodeDir);
        save(node);
        saveTrips(nodeDir, node);
        for(const cwSurveyNode* child : node->childNodes()) {
            auto childCave = qobject_cast<const cwCave*>(child);
            if(childCave != nullptr) {
                self(nodeDirHelper(nodeDir, childCave), childCave, self);
            }
        }
    };

    for(const auto cave : project->cavingRegion()->caves()) {
        saveNode(caveDirHelper(dataRootDir, cave), cave, saveNode);
    }

    return AsyncFuture::observe(pendingJobsFinished())
            .context(this, [this, newProjectFilename]() {
        return ResultString(newProjectFilename);
    })
            .future();
}

template<typename ProtoType>
static std::optional<cwError> checkEntityVersion(const ProtoType& proto, const QString& filename, int& maxFileVersion)
{
    if (!proto.has_fileversion() || !proto.fileversion().has_version()) {
        return std::nullopt;
    }
    const int version = proto.fileversion().version();
    maxFileVersion = std::max(maxFileVersion, version);
    if (version > cwRegionIOTask::protoVersion()) {
        return cwError(
                    QStringLiteral("\"%1\" was created by a newer version of CaveWhere (v%2, file version %3). "
                                   "This copy only supports file version %4. "
                                   "Saving is disabled because it would lose data added by the newer version. "
                                   "Please upgrade CaveWhere to save or sync this project.")
                    .arg(QFileInfo(filename).fileName())
                    .arg(cwRegionIOTask::toVersion(version))
                    .arg(version)
                    .arg(cwRegionIOTask::protoVersion()),
                    cwError::Warning);
    }
    return std::nullopt;
}

QFuture<Monad::Result<cwSaveLoad::ProjectLoadData>> cwSaveLoad::loadAll(const QString &filename)
{
    return cwConcurrent::run([filename]() {
        auto projectResult = loadProject(filename);

        return projectResult.then([filename](Result<ProjectLoadData> result) {
            ProjectLoadData loadData = result.value();

            QDir projectRootDir = projectRootDirForFile(filename);
            QDir regionDir = QDir(projectRootDir.absoluteFilePath(loadData.metadata.dataRoot));

            auto filePathLess = [](const QFileInfo& a, const QFileInfo& b) {
                return a.absoluteFilePath() < b.absoluteFilePath();
            };

            // Every descriptor of one suffix under the data root, pruning the
            // content directories the layout puts a node's or a trip's payload
            // in. The prune is positional (childScanRole), so a node or trip the
            // user happened to name "trips" or "notes" is still walked into.
            // Symlinked directories stay out of the walk: a link to another
            // project's data root would hand this project that project's nodes
            // and trips, and a link that points at an ancestor would loop.
            // Symlinked descriptor files are still read, so a survey tracked as
            // a link keeps loading.
            const auto scanForFiles = [](const QDir& rootDir,
                                         const QString& suffix,
                                         bool scanTrips)
            {
                QFileInfoList found;
                const auto walk = [&](const QDir& dir, ScanRole role, auto&& self) -> void {
                    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs
                                                                   | QDir::Files
                                                                   | QDir::NoDotAndDotDot);
                    for (const QFileInfo& entry : entries) {
                        if (entry.isDir()) {
                            if (entry.isSymLink()) {
                                continue;
                            }
                            const auto childRole = childScanRole(role, entry.fileName(), scanTrips);
                            if (childRole.has_value()) {
                                self(QDir(entry.absoluteFilePath()), *childRole, self);
                            }
                            continue;
                        }
                        if (entry.suffix().compare(suffix, Qt::CaseInsensitive) == 0) {
                            found.append(entry);
                        }
                    }
                };
                walk(rootDir, ScanRole::DataRoot, walk);
                return found;
            };

            const auto parentDirPathOf = [](const QString& path) {
                return QDir::cleanPath(QFileInfo(path).absoluteDir().absolutePath());
            };

            // A node and its children are separate files, so the tree is grown
            // out of these entries and folded into cwCaveData::nodes at the end:
            // a cwCaveData* into a QList would dangle the moment the list grew.
            struct NodeEntry {
                cwCaveData data;
                QList<NodeEntry*> children;
            };

            std::vector<std::unique_ptr<NodeEntry>> nodeEntries;
            QHash<QString, NodeEntry*> nodeByDir;
            QList<NodeEntry*> rootEntries;

            const QString regionDirPath = QDir::cleanPath(regionDir.absolutePath());

            QFileInfoList caveFiles = scanForFiles(regionDir, QStringLiteral("cwcave"), false);

            // Sorted by directory path with a trailing separator, so a parent —
            // whose directory, with that separator, is a prefix of every child's
            // — is always placed before its children, whatever the descriptors
            // themselves are named.
            // Two descriptors in one directory fall back to the file path, so
            // which of them is kept and which is reported stays the same on
            // every open.
            const auto caveDirPathLess = [](const QFileInfo& a, const QFileInfo& b) {
                const auto dirKey = [](const QFileInfo& info) {
                    return QDir::cleanPath(info.absoluteDir().absolutePath()) + QLatin1Char('/');
                };
                const QString keyA = dirKey(a);
                const QString keyB = dirKey(b);
                if (keyA != keyB) {
                    return keyA < keyB;
                }
                return a.absoluteFilePath() < b.absoluteFilePath();
            };
            std::sort(caveFiles.begin(), caveFiles.end(), caveDirPathLess);

            for (const QFileInfo &caveFileInfo : caveFiles) {
                const QString cavePath = caveFileInfo.absoluteFilePath();
                const QString caveDirPath = QDir::cleanPath(caveFileInfo.absoluteDir().absolutePath());

                if (nodeByDir.contains(caveDirPath)) {
                    loadData.errors.append(cwError(
                                               QStringLiteral("Ignoring \"%1\": its directory already holds another survey node.")
                                               .arg(cavePath),
                                               cwError::Fatal));
                    continue;
                }

                // The loader never guesses a parent: a descriptor sits either in
                // the data root or in a node's sub/ directory, and anything
                // else is reported and skipped.
                NodeEntry* parentEntry = nullptr;
                const QString parentPath = parentDirPathOf(caveDirPath);
                if (parentPath != regionDirPath) {
                    const bool inSubNodeDir = QFileInfo(parentPath).fileName().compare(kSubNodeDirName, Qt::CaseInsensitive) == 0;
                    const auto parentIt = inSubNodeDir ? nodeByDir.constFind(parentDirPathOf(parentPath))
                                                       : nodeByDir.constEnd();
                    if (parentIt == nodeByDir.constEnd()) {
                        loadData.errors.append(cwError(
                                                   QStringLiteral("Ignoring \"%1\": it is not in the project's data root or in a survey node's \"%2\" directory.")
                                                   .arg(cavePath, kSubNodeDirName),
                                                   cwError::Fatal));
                        continue;
                    }
                    parentEntry = parentIt.value();
                }

                auto caveProtoResult = loadMessage<CavewhereProto::Cave>(cavePath);
                if (caveProtoResult.hasError()) {
                    loadData.errors.append(cwError(
                                               QStringLiteral("Could not load cave \"%1\": %2")
                                               .arg(caveFileInfo.fileName(), caveProtoResult.errorMessage()),
                                               cwError::Fatal));
                    continue;
                }

                const auto& caveProto = caveProtoResult.value();
                auto caveVersionWarning = checkEntityVersion(caveProto, cavePath, loadData.maxFileVersion);
                if (caveVersionWarning) {
                    loadData.errors.append(*caveVersionWarning);
                }

                nodeEntries.push_back(std::make_unique<NodeEntry>());
                NodeEntry* entry = nodeEntries.back().get();
                entry->data = caveDataFromProtoCave(caveProto);
                nodeByDir.insert(caveDirPath, entry);
                if (parentEntry != nullptr) {
                    parentEntry->children.append(entry);
                } else {
                    rootEntries.append(entry);
                }
            }

            const auto loadTripData = [&loadData, &filePathLess](const QFileInfo& tripFileInfo) -> std::optional<cwTripData> {
                const QString tripPath = tripFileInfo.absoluteFilePath();
                auto tripProtoResult = loadMessage<CavewhereProto::Trip>(tripPath);

                if (tripProtoResult.hasError()) {
                    loadData.errors.append(cwError(
                                               QStringLiteral("Could not load trip \"%1\": %2")
                                               .arg(tripFileInfo.fileName(), tripProtoResult.errorMessage()),
                                               cwError::Fatal));
                    return std::nullopt;
                }

                const auto& tripProto = tripProtoResult.value();
                auto tripVersionWarning = checkEntityVersion(tripProto, tripPath, loadData.maxFileVersion);
                if (tripVersionWarning) {
                    loadData.errors.append(*tripVersionWarning);
                }

                cwTripData trip = cwSaveLoad::tripDataFromProtoTrip(tripProto);

                QDir tripDir = tripFileInfo.absoluteDir();

                auto loadObjectsFromNotesDir = [tripDir, &filePathLess, &loadData](const QString& fileSuffix,
                        auto&& loadProtoFunc,
                        auto&& convertFunc,
                        auto& destinationList)
                {
                    QDir notesDir = tripDir.filePath("notes");
                    if (!notesDir.exists()) {
                        return;
                    }

                    QFileInfoList files = notesDir.entryInfoList(QStringList() << ("*" + fileSuffix),
                                                                 QDir::Files,
                                                                 QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);
                    std::sort(files.begin(), files.end(), filePathLess);
                    for (const QFileInfo& fileInfo : files) {
                        const QString notePath = fileInfo.absoluteFilePath();
                        auto protoResult = loadProtoFunc(notePath);
                        if (protoResult.hasError()) {
                            loadData.errors.append(cwError(
                                                       QStringLiteral("Could not load \"%1\": %2")
                                                       .arg(fileInfo.fileName(), protoResult.errorMessage()),
                                                       cwError::Fatal));
                            continue;
                        }

                        auto versionWarning = checkEntityVersion(protoResult.value(), notePath, loadData.maxFileVersion);
                        if (versionWarning) {
                            loadData.errors.append(*versionWarning);
                        }

                        destinationList.append(convertFunc(protoResult.value(), notePath));
                    }
                };

                //Load 2D notes
                loadObjectsFromNotesDir(QStringLiteral("cwnote"),
                                        [](const QString& path) {
                    return loadMessage<CavewhereProto::Note>(path);
                },
                [](const CavewhereProto::Note& proto, const QString& path) {
                    return cwSaveLoad::noteDataFromProtoNote(proto, path);
                },
                trip.noteModel.notes);

                //Load 3D lidar notes
                loadObjectsFromNotesDir(QStringLiteral("cwnote3d"),
                                        [](const QString& path) {
                    return loadMessage<CavewhereProto::NoteLiDAR>(path);
                },
                [](const CavewhereProto::NoteLiDAR& proto, const QString& path) {
                    return cwSaveLoad::noteLiDARDataFromProtoNoteLiDAR(proto, path);
                },
                trip.noteLiDARModel.notes);

                //Load sketches
                loadObjectsFromNotesDir(QStringLiteral("cwsketch"),
                                        [](const QString& path) {
                    return loadMessage<CavewhereProto::Sketch>(path);
                },
                [](const CavewhereProto::Sketch& proto, const QString& path) {
                    return cwSaveLoad::sketchDataFromProtoSketch(proto, path);
                },
                trip.sketchModel.notes);

                return trip;
            };

            // Every trip descriptor, matched to the node that holds it. A
            // .cwtrip whose nearest node ancestor is not its own trips/ parent
            // is an orphan (a peer moved the node while this side added the
            // trip): it attaches to that nearest node and is reported, so no
            // survey is ever dropped on load.
            QFileInfoList tripFiles = scanForFiles(regionDir,
                                                   QStringLiteral("cwtrip"),
                                                   true);
            std::sort(tripFiles.begin(), tripFiles.end(), filePathLess);

            for (const QFileInfo& tripFileInfo : tripFiles) {
                NodeEntry* owner = nullptr;
                bool inOwnTripsDir = false;
                QString candidate = QDir::cleanPath(tripFileInfo.absoluteDir().absolutePath());
                QString segmentBelow;
                while (candidate.startsWith(regionDirPath)) {
                    const auto ownerIt = nodeByDir.constFind(candidate);
                    if (ownerIt != nodeByDir.constEnd()) {
                        owner = ownerIt.value();
                        inOwnTripsDir = segmentBelow.compare(kTripsDirName, Qt::CaseInsensitive) == 0;
                        break;
                    }
                    if (candidate == regionDirPath) {
                        break;
                    }
                    segmentBelow = QFileInfo(candidate).fileName();
                    candidate = parentDirPathOf(candidate);
                }

                if (owner == nullptr) {
                    loadData.errors.append(cwError(
                                               QStringLiteral("Ignoring \"%1\": it belongs to no survey node.")
                                               .arg(tripFileInfo.absoluteFilePath()),
                                               cwError::Fatal));
                    continue;
                }

                auto tripData = loadTripData(tripFileInfo);
                if (!tripData.has_value()) {
                    continue;
                }

                if (!inOwnTripsDir) {
                    loadData.errors.append(cwError(
                                               QStringLiteral("Trip \"%1\" was found outside a \"trips\" directory and was loaded into \"%2\".")
                                               .arg(tripFileInfo.absoluteFilePath(), owner->data.name),
                                               cwError::Warning));
                }

                owner->data.trips.append(tripData.value());
            }

            const auto foldNodeTree = [](NodeEntry* entry, auto&& self) -> cwCaveData {
                cwCaveData data = std::move(entry->data);
                data.nodes.reserve(entry->children.size());
                for (NodeEntry* child : entry->children) {
                    data.nodes.append(self(child, self));
                }
                return data;
            };

            loadData.region.caves.reserve(rootEntries.size());
            for (NodeEntry* rootEntry : rootEntries) {
                loadData.region.caves.append(foldNodeTree(rootEntry, foldNodeTree));
            }
            repairTopLevelIds(loadData);
            repairNestedScrapIds(loadData);
            repairNameCollisions(loadData);
            return Result(loadData);
        });
    });
}

Monad::Result<cwSaveLoad::ProjectLoadData> cwSaveLoad::loadProject(const QString &filename)
{
    auto projectResult = loadMessage<CavewhereProto::Project>(filename);
    return Monad::mbind(projectResult, [](const Result<CavewhereProto::Project>& result)
    {
        auto projectProto = result.value();

        if (!projectProto.has_fileversion() || !projectProto.fileversion().has_version()) {
            return Result<ProjectLoadData>(QStringLiteral("Project file missing version information."));
        }

        const int fileVersion = projectProto.fileversion().version();
        if (fileVersion < 8) {
            return Result<ProjectLoadData>(QStringLiteral("Legacy CaveWhere v7 project files are not supported."));
        }

        ProjectLoadData loadData;
        loadData.maxFileVersion = fileVersion;

        if (fileVersion > cwRegionIOTask::protoVersion()) {
            loadData.errors.append(cwError(
                                       QStringLiteral("This project was created by a newer version of CaveWhere "
                                                      "(v%1, file version %2). This copy only supports file version %3. "
                                                      "Saving is disabled because it would lose data added by the newer version. "
                                                      "Please upgrade CaveWhere to save or sync this project.")
                                       .arg(cwRegionIOTask::toVersion(fileVersion))
                                       .arg(fileVersion)
                                       .arg(cwRegionIOTask::protoVersion()),
                                       cwError::Warning));
        }
        if (projectProto.has_name()) {
            loadData.region.name = QString::fromStdString(projectProto.name());
        }

        if (projectProto.has_id()) {
            loadData.metadata.projectId = QString::fromStdString(projectProto.id());
        }

        if (projectProto.has_metadata()) {
            const auto& metadataProto = projectProto.metadata();
            if (metadataProto.has_dataroot()) {
                loadData.metadata.dataRoot = QString::fromStdString(metadataProto.dataroot());
            }
            if (metadataProto.has_syncenabled()) {
                loadData.metadata.syncEnabled = metadataProto.syncenabled();
            }
            if (metadataProto.has_georeference()) {
                const auto& geoReferenceProto = metadataProto.georeference();
                auto& geoReference = loadData.region.geoReference;
                geoReference.state =
                    static_cast<cwGeoReference::State>(geoReferenceProto.state());
                geoReference.localCoordinateSystem =
                    QString::fromStdString(geoReferenceProto.localcoordinatesystem());
                geoReference.anchor.kind =
                    static_cast<cwGeoReference::Anchor::Kind>(geoReferenceProto.anchorkind());
                geoReference.anchor.id =
                    QUuid::fromString(QString::fromStdString(geoReferenceProto.anchorid()));
                geoReference.verticalDatum =
                    QString::fromStdString(geoReferenceProto.verticaldatum());
            }
            if (metadataProto.has_unitsystem()) {
                loadData.region.unitSystem =
                    static_cast<cwUnits::UnitSystem>(metadataProto.unitsystem());
            }
        }

        if (loadData.metadata.dataRoot.isEmpty()) {
            loadData.metadata.dataRoot = cwSaveLoadPrivate::defaultDataRoot(loadData.region.name);
        }

        loadData.region.equates.reserve(projectProto.equates_size());
        for (const auto& protoEquate : projectProto.equates()) {
            loadData.region.equates.append(cwProtoUtils::fromProtoEquate(protoEquate));
        }

        return Result(loadData);
    });
}

Monad::Result<cwCaveData> cwSaveLoad::loadCave(const QString &filename)
{
    auto caveResult = loadMessage<CavewhereProto::Cave>(filename);
    return Monad::mbind(caveResult, [](const Result<CavewhereProto::Cave>& result) {
        return Result(caveDataFromProtoCave(result.value()));
    });
}

cwCaveData cwSaveLoad::caveDataFromProtoCave(const CavewhereProto::Cave& caveProto)
{
    cwCaveData cave;
    if (caveProto.has_name()) {
        cave.name = QString::fromStdString(caveProto.name());
    }
    if (caveProto.has_id()) {
        cave.id = cwProtoUtils::toUuid(caveProto.id());
    }
    cave.lengthUnit = caveProto.has_lengthunit()
            ? static_cast<cwUnits::LengthUnit>(caveProto.lengthunit())
            : cwUnits::Meters;
    cave.depthUnit = caveProto.has_depthunit()
            ? static_cast<cwUnits::LengthUnit>(caveProto.depthunit())
            : cwUnits::Meters;

    if (caveProto.has_kind()) {
        cave.kind = cwSurveyNodeKind::fromSavedValue(caveProto.kind());
    }
    if (caveProto.has_read_only()) {
        cave.readOnly = caveProto.read_only();
    }
    if (caveProto.has_source_id()) {
        cave.sourceId = cwProtoUtils::toUuid(caveProto.source_id());
    }
    if (caveProto.has_source_path()) {
        cave.sourcePath = QString::fromStdString(caveProto.source_path());
    }
    if (caveProto.has_external_centerline()) {
        cave.externalCenterline = cwExternalCenterline(
                    QString::fromStdString(caveProto.external_centerline().entry_file()));
    }

    cave.fixStations.reserve(caveProto.fixstations_size());
    for (const auto& protoFix : caveProto.fixstations()) {
        cave.fixStations.append(cwProtoUtils::fromProtoFixStation(protoFix));
    }
    return cave;
}

Monad::Result<cwTripData> cwSaveLoad::loadTrip(const QString &filename)
{
    auto tripResult = loadMessage<CavewhereProto::Trip>(filename);
    return Monad::mbind(tripResult, [](const Result<CavewhereProto::Trip>& result) {
        return Result(cwSaveLoad::tripDataFromProtoTrip(result.value()));
    });
}

Monad::Result<cwTripData> cwSaveLoad::loadTrip(const QByteArray& content)
{
    auto tripResult = loadMessage<CavewhereProto::Trip>(content, QStringLiteral("trip payload"));
    return Monad::mbind(tripResult, [](const Result<CavewhereProto::Trip>& result) {
        return Result(cwSaveLoad::tripDataFromProtoTrip(result.value()));
    });
}

cwTripData cwSaveLoad::tripDataFromProtoTrip(const CavewhereProto::Trip& tripProto)
{
    cwTripData tripData;

    if (tripProto.has_name()) {
        tripData.name = QString::fromStdString(tripProto.name());
    }
    if (tripProto.has_id()) {
        tripData.id = cwProtoUtils::toUuid(tripProto.id());
    }

    if (tripProto.has_date()) {
        tripData.date = QDateTime(cwProtoUtils::loadDate(tripProto.date()), QTime());
    }

    if (tripProto.has_tripcalibration()) {
        tripData.calibrations = cwProtoUtils::fromProtoTripCalibration(tripProto.tripcalibration());
    }

    if (tripProto.has_team()) {
        tripData.team = cwProtoUtils::fromProtoTeam(tripProto.team());
    }

    tripData.chunks = cwSaveLoadPrivate::fromProtoSurveyChunks(tripProto.chunks());

    if (tripProto.has_external_centerline()) {
        tripData.externalCenterline = cwExternalCenterline(
                    QString::fromStdString(tripProto.external_centerline().entry_file()));
    }

    if (tripProto.has_station_prefix()) {
        tripData.stationPrefix = QString::fromStdString(tripProto.station_prefix());
    }

    if (tripProto.has_source_path()) {
        tripData.sourcePath = QString::fromStdString(tripProto.source_path());
    }

    return tripData;
}

cwNoteData cwSaveLoad::noteDataFromProtoNote(const CavewhereProto::Note& protoNote, const QString& filename)
{
    cwNoteData noteData;

    noteData.rotate = protoNote.rotation();
    noteData.imageResolution = cwProtoUtils::fromProtoImageResolution(protoNote.imageresolution());
    noteData.image = loadImage(protoNote.image(), filename);

    for (const auto& protoScrap : protoNote.scraps()) {
        noteData.scraps.append(cwProtoUtils::fromProtoScrap(protoScrap));
    }

    noteData.name = QString::fromStdString(protoNote.name());
    if (protoNote.has_id()) {
        noteData.id = cwProtoUtils::toUuid(protoNote.id());
    }

    return noteData;
}

Monad::Result<cwNoteData> cwSaveLoad::loadNote(const QString &filename, const QDir& projectDir)
{
    auto noteResult = loadMessage<CavewhereProto::Note>(filename);

    return Monad::mbind(noteResult, [filename, projectDir](const Result<CavewhereProto::Note>& result) -> Monad::Result<cwNoteData> {
        Q_UNUSED(projectDir)
        return cwSaveLoad::noteDataFromProtoNote(result.value(), filename);
    });
}

Monad::Result<cwNoteData> cwSaveLoad::loadNote(const QByteArray& content, const QString& filename, const QDir& projectDir)
{
    auto noteResult = loadMessage<CavewhereProto::Note>(content, filename);

    return Monad::mbind(noteResult, [filename, projectDir](const Result<CavewhereProto::Note>& result) -> Monad::Result<cwNoteData> {
        Q_UNUSED(projectDir)
        return cwSaveLoad::noteDataFromProtoNote(result.value(), filename);
    });
}

cwLazLayerData cwSaveLoad::lazLayerDataFromProtoLazLayer(const CavewhereProto::LazLayer& proto,
                                                         const QString& filename)
{
    cwLazLayerData data;

    // The .cwlaz filename's basename pairs back to the .laz/.las source. The
    // source extension isn't stored in the proto, so the discovery pass
    // matches by basename and supplies the right extension when populating
    // cwLazLayerData::fileName on the layer.
    const QFileInfo info(filename);
    if (!info.completeBaseName().isEmpty()) {
        data.fileName = info.completeBaseName();
    }

    if (proto.has_id()) {
        data.id = cwProtoUtils::toUuid(proto.id());
    }
    // Absence of `enabled` means default (true), matching the proto comment.
    if (proto.has_enabled()) {
        data.enabled = proto.enabled();
    }

    return data;
}

Monad::Result<cwLazLayerData> cwSaveLoad::loadLazLayer(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        return Result<cwLazLayerData>(file.errorString());
    }
    return loadLazLayer(file.readAll(), filename);
}

Monad::Result<cwLazLayerData> cwSaveLoad::loadLazLayer(const QByteArray& content, const QString& filename)
{
    // An empty .cwlaz file is treated as "default state" rather than an error
    // so partially-written or truncated metadata files load to safe defaults
    // (no garbage UUID, enabled=true) and the rest of the layer continues
    // working. The filename still drives basename pairing.
    if (content.isEmpty()) {
        CavewhereProto::LazLayer emptyProto;
        return Result<cwLazLayerData>(lazLayerDataFromProtoLazLayer(emptyProto, filename));
    }

    auto protoResult = loadMessage<CavewhereProto::LazLayer>(content, filename);

    return Monad::mbind(protoResult, [filename](const Result<CavewhereProto::LazLayer>& result) -> Monad::Result<cwLazLayerData> {
        const auto& proto = result.value();
        // Refuse to silently substitute a fresh UUID when the file claimed
        // one but it failed to parse — overwriting on the next save would
        // permanently lose the persisted identity.
        if (proto.has_id() && cwProtoUtils::toUuid(proto.id()).isNull()) {
            return Result<cwLazLayerData>(
                        QStringLiteral("Malformed id in %1: %2")
                        .arg(filename, QString::fromStdString(proto.id())));
        }
        return cwSaveLoad::lazLayerDataFromProtoLazLayer(proto, filename);
    });
}

Monad::Result<cwNoteLiDARData> cwSaveLoad::loadNoteLiDAR(const QString& filename, const QDir &projectDir) {
    auto noteResult = loadMessage<CavewhereProto::NoteLiDAR>(filename);

    return Monad::mbind(noteResult, [filename, projectDir](const Result<CavewhereProto::NoteLiDAR>& result) -> Monad::Result<cwNoteLiDARData> {
        Q_UNUSED(projectDir)
        return cwSaveLoad::noteLiDARDataFromProtoNoteLiDAR(result.value(), filename);
    });
}

Monad::Result<cwNoteLiDARData> cwSaveLoad::loadNoteLiDAR(const QByteArray& content,
                                                         const QString& filename,
                                                         const QDir& projectDir)
{
    auto noteResult = loadMessage<CavewhereProto::NoteLiDAR>(content, filename);

    return Monad::mbind(noteResult, [filename, projectDir](const Result<CavewhereProto::NoteLiDAR>& result) -> Monad::Result<cwNoteLiDARData> {
        Q_UNUSED(projectDir)
        return cwSaveLoad::noteLiDARDataFromProtoNoteLiDAR(result.value(), filename);
    });
}

cwNoteLiDARData cwSaveLoad::noteLiDARDataFromProtoNoteLiDAR(const CavewhereProto::NoteLiDAR& protoNote,
                                                            const QString& filename)
{
    Q_UNUSED(filename)

    cwNoteLiDARData noteData;

    const QString rawFilename = QString::fromStdString(protoNote.filename());
    // Older saves may contain project-relative paths; normalize to just the filename to stay resilient to renames.
    noteData.filename = QFileInfo(rawFilename).fileName().isEmpty() ? rawFilename : QFileInfo(rawFilename).fileName();
    noteData.name = QString::fromStdString(protoNote.name());
    if (protoNote.has_id()) {
        noteData.id = cwProtoUtils::toUuid(protoNote.id());
    }

    noteData.stations.reserve(protoNote.notestations_size());
    for (const auto& protoNoteStation : protoNote.notestations()) {
        cwNoteLiDARStation newStation;
        newStation.setPositionOnNote(cwProtoUtils::loadVector3D(protoNoteStation.positiononnote()));
        newStation.setName(QString::fromStdString(protoNoteStation.name()));
        if (protoNoteStation.has_id()) {
            newStation.setId(cwProtoUtils::toUuid(protoNoteStation.id()));
        } else {
            newStation.setId(QUuid());
        }
        noteData.stations.append(std::move(newStation));
    }

    if (protoNote.has_autocalculatenorth()) {
        noteData.autoCalculateNorth = protoNote.autocalculatenorth();
    }

    if (protoNote.has_notetransformation()) {
        noteData.transfrom = cwProtoUtils::fromProtoLiDARNoteTransformation(protoNote.notetransformation());
    }

    return noteData;
}

Monad::Result<cwSketchData> cwSaveLoad::loadSketch(const QString& filename, const QDir& projectDir)
{
    auto sketchResult = loadMessage<CavewhereProto::Sketch>(filename);
    return Monad::mbind(sketchResult, [filename, projectDir](const Result<CavewhereProto::Sketch>& result) -> Monad::Result<cwSketchData> {
        Q_UNUSED(projectDir)
        return cwSaveLoad::sketchDataFromProtoSketch(result.value(), filename);
    });
}

Monad::Result<cwSketchData> cwSaveLoad::loadSketch(const QByteArray& content,
                                                   const QString& filename,
                                                   const QDir& projectDir)
{
    auto sketchResult = loadMessage<CavewhereProto::Sketch>(content, filename);
    return Monad::mbind(sketchResult, [filename, projectDir](const Result<CavewhereProto::Sketch>& result) -> Monad::Result<cwSketchData> {
        Q_UNUSED(projectDir)
        return cwSaveLoad::sketchDataFromProtoSketch(result.value(), filename);
    });
}

cwSketchData cwSaveLoad::sketchDataFromProtoSketch(const CavewhereProto::Sketch& protoSketch,
                                                   const QString& filename)
{
    Q_UNUSED(filename)
    return cwProtoUtils::fromProtoSketch(protoSketch);
}


void cwSaveLoad::waitForFinished()
{
    cwFutureManagerModel model;
    auto saveJobs = completeSaveJobs();
    model.addJob(cwFuture(saveJobs, QString()));
    // Also wait for the operation queue to drain. A SaveFlush operation only
    // settles after saveFlushImpl() has emitted saveFlushCompleted, so this
    // gives callers a happens-before guarantee on that signal — completeSaveJobs()
    // alone resolves upstream of the flush emit and would race it.
    model.addJob(cwFuture(d->operationsFinished(), QString()));
    model.waitForFinished();
}

void cwSaveLoad::discardChanges()
{
    auto* repo = repository();
    if (!repo) {
        return;
    }

    setSaveEnabled(false);

    // `git reset --hard HEAD` + cleanUntracked rewrites project files the
    // external-centerline watcher is armed on. Their events land after
    // discardCompleted has already cleared cwProject::modified(), so
    // without this window a discard would immediately re-dirty the project
    // it just cleaned.
    d->selfWrite.begin();

    // Run the discard as a queued operation, the same shape sync() uses.
    // Operations are FIFO, so any in-flight or scheduled save flush drains
    // before the reset rewrites the tree, and waitForFinished() covers the
    // discard through the queue's idle future. Replacing m_pendingJobsDeferred
    // here instead would cancel a mid-flight drain future, and AsyncFuture
    // propagates that cancellation through this chain and the active flush
    // operation — silently dropping the reset and jamming the queue.
    d->ensureSaveFlushScheduled(this);

    auto discardFuture = d->enqueueOperation(
                this,
                cwSaveLoadPrivate::Operation::Type::DiscardChanges,
                [this, repo]() -> QFuture<ResultBase> {
        auto resetFuture = repo->reset(QStringLiteral("HEAD"),
                                       QQuickGit::GitRepository::ResetMode::Hard);
        return AsyncFuture::observe(resetFuture)
                .context(this, [repo, resetFuture]() {
                    const auto result = resetFuture.result();
                    if (!result.hasError()) {
                        repo->cleanUntracked();
                    }
                    return result;
                })
                .future();
    });

    d->futureToken.addJob(discardFuture,
                          QStringLiteral("Discarding changes"));

    // The operation deferred settles with an error Result on every
    // cancellation path, so this observer always runs and the self-write
    // window always closes.
    AsyncFuture::observe(discardFuture).context(this, [this, discardFuture]() {
        d->selfWrite.end();
        const auto result = discardFuture.result();
        if (result.hasError()) {
            qWarning() << "discardChanges: git reset --hard HEAD failed:"
                       << result.errorMessage();
            // The discard didn't take, so the project stays live: saves must
            // keep flowing to disk (the success path re-enables via the
            // reload discardCompleted triggers, which never happens here),
            // and the LAZ model resyncs with whatever a partial reset left
            // on disk — libgit2 checks files out one at a time, so an abort
            // can land after some of "GIS Layers/" was already rewritten.
            setSaveEnabled(true);
            emit discardFailed(result.errorMessage());
            rescanLazLayersAfterGitWrite();
            return;
        }
        emit discardCompleted();
    });
}

void cwSaveLoad::disconnectTreeModel()
{
    // Runs this→this, so the region disconnect below leaves it. Torn down here
    // rather than inside the region guard because it outlives the region, and
    // each reconnect cycle would otherwise add another.
    disconnect(this, &cwSaveLoad::discardCompleted,
               this, &cwSaveLoad::rescanLazLayersAfterGitWrite);

    // Disconnect from the region directly. cwCavingRegion is NOT included in the
    // cwRegionTreeModel::all<QObject*> iteration (the root item uses an invalid index
    // which returns QVariant() for ObjectRole), so disconnectObjects() below will not
    // touch it. This explicit call is the only disconnect for the region.
    if (auto* region = d->m_regionTreeModel->cavingRegion()) {
        disconnect(region, nullptr, this, nullptr);
        // lazLayers is a child of region and its connections survive
        // disconnectObjects(); each reconnect cycle would otherwise
        // accumulate duplicate rowsAboutToBeRemoved / rowsRemoved handlers.
        if (auto* lazLayers = region->lazLayers()) {
            disconnect(lazLayers, nullptr, this, nullptr);
        }
        // geoReference and the equate model are children of region too, so their
        // this-bound saveMetadata connections (wired in connectTreeModel) survive
        // the disconnect(region, ...) above. Each reconcile disconnects then
        // reconnects the tree, so without an explicit teardown they accumulate a
        // duplicate saveProject per cycle.
        if (auto* geoReference = region->geoReference()) {
            disconnect(geoReference, nullptr, this, nullptr);
        }
        if (auto* equateModel = region->equates()) {
            disconnect(equateModel, nullptr, this, nullptr);
        }
    }

    //Disconnect from the tree model
    disconnect(d->m_regionTreeModel, nullptr, this, nullptr);

    if (d->isTrackedConnected(d->m_regionTreeModel)) {
        d->trackDisconnected(d->m_regionTreeModel);
        d->connectionChecker.remove(d->m_regionTreeModel);
    }

    //Disconnect all the objects watch in the tree
    disconnectObjects();
}

void cwSaveLoad::connectTreeModel()
{

    if(!d->trackConnected(d->m_regionTreeModel)) {
        return;
    }
    d->connectionChecker.add(d->m_regionTreeModel);

    //Connect when region has changed
    connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsInserted,
            this, [this](const QModelIndex &parent, int first, int last) {
        bool nodeInserted = false;
        for(int i = first; i <= last; i++) {
            auto index = d->m_regionTreeModel->index(i, 0, parent);
            switch(index.data(cwRegionTreeModel::TypeRole).toInt()) {
            case cwRegionTreeModel::CaveType: {
                auto cave = d->m_regionTreeModel->cave(index);
                connectCave(cave);
                //An undone delete re-announces the row, and the subtree it
                //names is sitting in the trash: move it back before the
                //descriptor write, which by itself would leave the note
                //images and attachments behind.
                d->restoreDirectoryFromTrash(this, cave);
                save(cave);
                nodeInserted = true;
                break;
            }
            case cwRegionTreeModel::TripType: {
                auto trip = d->m_regionTreeModel->trip(index);
                connectTrip(trip);
                d->restoreDirectoryFromTrash(this, trip);
                save(trip);
                break;
            }
            case cwRegionTreeModel::NoteType: {
                auto note = d->m_regionTreeModel->note(index);
                connectNote(note);
                save(note);
                break;
            }
            case cwRegionTreeModel::ScrapType: {
                auto scrap = d->m_regionTreeModel->scrap(index);
                connectScrap(scrap);
                save(scrap->parentNote());
                break;
            }
            case cwRegionTreeModel::NoteLiDARType: {
                auto noteLiDAR = d->m_regionTreeModel->noteLiDAR(index);
                connectNoteLiDAR(noteLiDAR);
                save(noteLiDAR);
                break;
            }
            case cwRegionTreeModel::SketchType: {
                auto sketch = d->m_regionTreeModel->sketch(index);
                connectSketch(sketch);
                save(sketch);
                break;
            }
            default:
                break;
            }
        }

        //Only a node can move the format the project needs, so the walk
        //stampVersion does stays off the note and scrap rows a load pours in.
        if (nodeInserted) {
            restampProjectIfVersionChanged();
        }
    });

    connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsRemoved,
            this, [this](const QModelIndex& parent, int, int) {
        //Only a node's disappearance can move the format the project needs. The
        //removed rows are already gone by now, so the cheap test is the parent:
        //a trip removal shares it and costs one short-circuiting stampVersion
        //walk, while the note and scrap rows a load pours out are skipped.
        const bool underNode = !parent.isValid()
                || parent.data(cwRegionTreeModel::TypeRole).toInt() == cwRegionTreeModel::CaveType;
        if (underNode) {
            restampProjectIfVersionChanged();
        }
    });

    connect(d->m_regionTreeModel, &cwRegionTreeModel::rowsAboutToBeRemoved,
            this, [this](const QModelIndex &parent, int first, int last) {

        auto removeFile = [this](const QObject* object) {
            d->addFileSystemJob(cwSaveLoadPrivate::Job
                                {
                                    object,
                                    cwSaveLoadPrivate::Job::Kind::File,
                                    cwSaveLoadPrivate::Job::Action::Remove
                                },
                                this);
        };

        auto removeResolvedFile = [this](const QString& path) {
            if (path.isEmpty()) {
                return;
            }

            cwSaveLoadPrivate::Job removeJob;
            removeJob.kind = cwSaveLoadPrivate::Job::Kind::File;
            removeJob.action = cwSaveLoadPrivate::Job::Action::Remove;
            removeJob.oldPath = path;
            d->addExplicitFileSystemJob(removeJob, this);
        };

        for(int i = first; i <= last; i++) {
            auto index = d->m_regionTreeModel->index(i, 0, parent);
            auto object = index.data(cwRegionTreeModel::ObjectRole).value<QObject*>();

            //A move announces the whole subtree's rows as removed and then
            //re-inserts them, so every one of these rows would otherwise be
            //read as a deletion — taking the trips' directories and the note
            //images with it. One Directory Move, queued from
            //parentNodeChanged, is what carries them instead.
            if (d->isInsideMovingNode(object)) {
                continue;
            }

            switch(index.data(cwRegionTreeModel::TypeRole).toInt()) {
            case cwRegionTreeModel::CaveType: {
                auto cave = d->m_regionTreeModel->cave(index);
                d->moveDirectoryToTrash(this, cave, cave->id());
                break;
            }
            case cwRegionTreeModel::TripType: {
                auto trip = d->m_regionTreeModel->trip(index);
                d->moveDirectoryToTrash(this, trip, trip->id());
                break;
            }
            case cwRegionTreeModel::NoteType: {
                auto* note = d->m_regionTreeModel->note(index);
                removeFile(note);
                removeResolvedFile(absolutePathPrivate(note, note->image().path()));
                break;
            }
            case cwRegionTreeModel::NoteLiDARType: {
                auto* noteLiDAR = d->m_regionTreeModel->noteLiDAR(index);
                removeFile(noteLiDAR);
                removeResolvedFile(absolutePathPrivate(noteLiDAR, noteLiDAR->filename()));
                break;
            }
            case cwRegionTreeModel::SketchType: {
                auto* sketch = d->m_regionTreeModel->sketch(index);
                removeFile(sketch);
                break;
            }
            default:
                break;
            }

            disconnect(object, nullptr, this, nullptr);
            if (d->isTrackedConnected(object)) {
                d->trackDisconnected(object);
                d->connectionChecker.remove(object);
            }

            //That disconnect names this as the receiver, which is what the
            //destruction watch connects to, so it takes the watch down with
            //it. Without the watch back up, the object's death goes unheard
            //and its trashed directory stays on disk for good.
            d->rewatchObjectLifetime(object, this);
        }

        if (!d->suppressLocalMutationTracking) {
            emit localMutationOccurred();
        }
    });

    connectObjects();

    // React to project name changes: rename the dataRoot directory and .cwproj descriptor
    // on disk when the region name is edited by the user. Uses the same watch pattern as
    // cave/trip nameChanged handlers. transferProjectTo sets d->projectMetadata.dataRoot to
    // the new name before the caller calls region->setName(), so that rename takes the
    // matching-dataRoot path and only rewrites the project file with the new display name.
    //
    // The actual filesystem renames are queued as Custom jobs so they run on the background
    // thread via the job queue, matching the async pattern used for cave/trip renames.
    if (auto* region = d->m_regionTreeModel->cavingRegion()) {
        const auto saveMetadata = [this, region]() {
            saveProject(projectRootDir(), region);
        };

        connect(region, &cwCavingRegion::nameChanged, this, [this, region, saveMetadata]() {
            if (!d->hasDurableHome) {
                return;
            }

            const QString newName = region->name();
            const QString sanitizedName = sanitizeFileName(newName);
            if (d->projectMetadata.dataRoot == sanitizedName) {
                // Only the display name changed, so the project file is the one thing to rewrite.
                saveMetadata();
                return;
            }

            const QDir rootDir = projectRootDir();
            const QString oldDataRoot = d->projectMetadata.dataRoot;
            const QString oldDataRootPath = rootDir.absoluteFilePath(oldDataRoot);
            const QString newDataRootPath = rootDir.absoluteFilePath(sanitizedName);

            // Update in-memory metadata optimistically so resetObjectStates and
            // subsequent saves use the new paths immediately.
            d->projectMetadata.dataRoot = sanitizedName;
            emit dataRootChanged();

            // Rebuild m_objectStates so all child paths reflect the new dataRoot.
            // compressPendingJobs will drop stale write jobs superseded by new ones.
            d->resetObjectStates(this);

            // Update the .cwproj descriptor filename optimistically.
            const QString currentFile = fileName();
            const QString newDescriptorPath = rootDir.absoluteFilePath(sanitizedName + QStringLiteral(".cwproj"));
            if (currentFile != newDescriptorPath) {
                setFileName(newDescriptorPath);
            }

            // Queue a background job to rename the dataRoot directory.
            cwSaveLoadPrivate::Job moveJob(nullptr, cwSaveLoadPrivate::Job::Kind::Directory, cwSaveLoadPrivate::Job::Action::Custom,
                                  [oldDataRootPath, newDataRootPath]() -> Monad::ResultBase {
                if (!QFileInfo::exists(oldDataRootPath) || QFileInfo::exists(newDataRootPath)) {
                    return Monad::ResultBase();
                }
                return cwSaveLoadPrivate::moveDirectoryRobust(oldDataRootPath, newDataRootPath);
            });
            // The emit above fired before the tree moved, so listeners that
            // re-derive paths from disk (the external-centerline watcher, the
            // image provider binder) saw the new root empty. Announce again
            // once the move has landed; onDone runs on the main thread.
            moveJob.onDone = [this](const Monad::ResultBase&) {
                emit dataRootChanged();
            };
            d->addExplicitFileSystemJob(std::move(moveJob), this);

            // Queue a background job to rename the .cwproj descriptor file.
            if (currentFile != newDescriptorPath) {
                d->addExplicitFileSystemJob(
                            cwSaveLoadPrivate::Job(nullptr, cwSaveLoadPrivate::Job::Kind::File, cwSaveLoadPrivate::Job::Action::Custom,
                                      [currentFile, newDescriptorPath]() -> Monad::ResultBase {
                    if (!QFileInfo::exists(currentFile) || QFileInfo::exists(newDescriptorPath)) {
                        return Monad::ResultBase();
                    }
                    if (!QFile::rename(currentFile, newDescriptorPath)) {
                        return Monad::ResultBase(
                                    QStringLiteral("Failed to rename descriptor: %1 -> %2")
                                    .arg(currentFile, newDescriptorPath));
                    }
                    return Monad::ResultBase();
                }),
                            this);
            }

            saveMetadata();
        });

        // The local projection lives in the project metadata file and is the one
        // piece of geo-reference state that is *not* recomputable from the data
        // — it is deliberately stored rather than re-derived — so a change to it
        // that never reached disk would be lost outright. Without this handler
        // the save pipeline wouldn't see the change, so the dirty bit (and any
        // autosave keyed off it) wouldn't fire.
        connect(region->geoReference(), &cwGeoReference::localProjectionChanged, this, saveMetadata);
        connect(region->geoReference(), &cwGeoReference::verticalDatumChanged, this, saveMetadata);

        // The project's default unitSystem lives in the same metadata file, so it
        // needs the same handler — without it a units change made in the UI never
        // reaches disk or flips the dirty bit, and is dropped on close.
        connect(region, &cwCavingRegion::unitSystemChanged, this, saveMetadata);

        // Every equate persists in the same region top-level file (the
        // Project message), so an equate added/removed on the region must flush
        // it, exactly like a units or CS change. These connections are wired
        // before the region is populated, so WatchReset::No is mandatory here:
        // setEquates() (the bulk load path) resets the model, and saving then
        // would overwrite the on-disk project with a half-built one mid-load.
        connectListModelSaveTrigger(region->equates(), WatchReset::No, saveMetadata);

        // LAZ layers live in "GIS Layers/" inside the project root and are
        // discovered by directory scan, so adds go through cwProject::addFiles
        // (which already flushes + commits after copy). Watch removal here so
        // the queued delete + metadata flush + commit all fire together.
        if (auto* lazLayers = region->lazLayers()) {
            // User-initiated removal: enqueue file deletions before
            // beginRemoveRows fires, so addFileSystemJob can still resolve
            // the .cwlaz path through the live m_objectStates entry. The
            // rowsAboutToBeRemoved handler below then drops the entry.
            connect(lazLayers, &cwLazLayerModel::aboutToRemoveLayerByUser,
                    this, [this](cwLazLayer* layer) {
                if (layer == nullptr) {
                    return;
                }
                // .laz source file: path comes from the layer directly
                // (not tracked by m_objectStates — the LAZ is the
                // user-supplied sibling artifact, not the metadata file
                // owned by cwSaveLoad).
                const QString sourcePath = layer->sourcePath();
                if (!sourcePath.isEmpty()) {
                    cwSaveLoadPrivate::Job job;
                    job.action = cwSaveLoadPrivate::Job::Action::Remove;
                    job.kind = cwSaveLoadPrivate::Job::Kind::File;
                    job.oldPath = sourcePath;
                    d->addExplicitFileSystemJob(job, this);
                }
                // .cwlaz: path resolved through m_objectStates by
                // addFileSystemJob so a prior in-flight rename is honored.
                d->addFileSystemJob(cwSaveLoadPrivate::Job{
                    layer,
                    cwSaveLoadPrivate::Job::Kind::File,
                    cwSaveLoadPrivate::Job::Action::Remove
                }, this);
            });

            // User-initiated rename: the model has already updated the
            // layer's in-memory sourcePath to newSourcePath and emitted
            // this signal carrying the old path. Enqueue two tagged Move
            // jobs so the .laz and .cwlaz move together but never collapse
            // into each other under compression:
            //   * tag "source" — explicit .laz move (not m_objectStates-
            //     tracked, since the user-supplied source file is not the
            //     metadata file cwSaveLoad owns; addExplicitFileSystemJob
            //     carries both paths verbatim).
            //   * tag ""       — .cwlaz move via the standard tracked path
            //     (m_objectStates holds the old .cwlaz path; addFileSystemJob
            //     reads absolutePathPrivate(layer) for the new one — that
            //     resolves to the new basename because renameSourcePath
            //     ran before this signal fired).
            connect(lazLayers, &cwLazLayerModel::layerRenamed,
                    this, [this](cwLazLayer* layer,
                                 const QString& oldSourcePath,
                                 const QString& newSourcePath) {
                if (layer == nullptr) {
                    return;
                }

                if (!oldSourcePath.isEmpty() && !newSourcePath.isEmpty()
                        && oldSourcePath != newSourcePath) {
                    cwSaveLoadPrivate::Job sourceMove(layer,
                                                      cwSaveLoadPrivate::Job::Kind::File,
                                                      cwSaveLoadPrivate::Job::Action::Move);
                    sourceMove.tag = QStringLiteral("source");
                    sourceMove.oldPath = oldSourcePath;
                    sourceMove.path = newSourcePath;
                    d->addExplicitFileSystemJob(sourceMove, this);
                }

                d->addFileSystemJob(cwSaveLoadPrivate::Job{
                    layer,
                    cwSaveLoadPrivate::Job::Kind::File,
                    cwSaveLoadPrivate::Job::Action::Move
                }, this);
            });

            // Bookkeeping: drop the m_objectStates entry for every removed
            // layer regardless of cause (user remove OR rescan-driven
            // file-vanished removal). File-kind Remove jobs don't clean it
            // up themselves (only Directory-kind Remove does), and a layer
            // can outlive its row (a project transfer keeps layers alive),
            // so the destroyed-based cleanup alone would leave the entry
            // stale for as long as the object lives.
            //
            // Either cause is a user-visible mutation of the project's
            // on-disk shape, so emit localMutationOccurred — this is what
            // flips cwProject::modified() to true so the save / discard
            // affordances become available. Rescan-driven removal (a .laz
            // vanished from GIS Layers/ outside the app) has no other path
            // to dirty the project, and without this the user has no
            // signal that the working tree has diverged from HEAD.
            connect(lazLayers, &QAbstractItemModel::rowsAboutToBeRemoved,
                    this, [this, lazLayers](const QModelIndex& parent, int first, int last) {
                Q_UNUSED(parent);
                for (int row = first; row <= last; ++row) {
                    if (cwLazLayer* layer = lazLayers->layerAt(row)) {
                        d->m_objectStates.remove(layer);
                    }
                }
                if (!d->suppressLocalMutationTracking) {
                    emit localMutationOccurred();
                }
            });
            connect(lazLayers, &QAbstractItemModel::rowsRemoved, this, saveMetadata);

            // For each new layer: register its expected .cwlaz path in
            // m_objectStates so saves/renames resolve through the standard
            // pipeline, eagerly write the .cwlaz when one isn't on disk
            // (so the auto-generated UUID is persisted on first sight), and
            // wire enabledChanged → save(layer) so toggling visibility
            // writes only the .cwlaz, not the whole project metadata.
            auto wireLayer = [this](cwLazLayer* layer) {
                if (layer == nullptr) {
                    return;
                }
                const QString metadataPath = absolutePathPrivate(layer);
                if (!metadataPath.isEmpty()) {
                    d->seedStatePathFromLoaded(this, layer, metadataPath);
                    // Write the .cwlaz only while the .laz it describes is
                    // still there. A reconcile re-wires rows in the window
                    // between git deleting a layer's files and the rescan
                    // dropping its row, and writing then resurrects the .cwlaz
                    // as an orphan the next commit picks up.
                    if (!QFileInfo::exists(metadataPath)
                            && QFileInfo::exists(layer->sourcePath())) {
                        save(layer);
                    }
                }
                connect(layer, &cwLazLayer::enabledChanged, this, [this, layer]() {
                    save(layer);
                });
            };
            connect(lazLayers, &QAbstractItemModel::rowsInserted,
                    this, [lazLayers, wireLayer](const QModelIndex& parent, int first, int last) {
                Q_UNUSED(parent);
                for (int row = first; row <= last; ++row) {
                    wireLayer(lazLayers->layerAt(row));
                }
            });
            for (cwLazLayer* layer : lazLayers->layers()) {
                wireLayer(layer);
            }

            // Re-rescan after a Git-driven on-disk mutation. discardChanges
            // restores deleted .laz files via `git reset --hard HEAD` and
            // deletes uncommitted ones via cleanUntracked; the model needs a
            // kick to re-discover the restored ones and re-pair with any
            // orphan .cwlaz sibling so the original UUID + enabled bit are
            // adopted. Queued so the rescan runs after discardCompleted's
            // observers have settled (the project's modified bit clears
            // first, then we repopulate).
            connect(this, &cwSaveLoad::discardCompleted,
                    this, &cwSaveLoad::rescanLazLayersAfterGitWrite,
                    Qt::QueuedConnection);
        }
    }
}

void cwSaveLoad::rescanLazLayersAfterGitWrite()
{
    const auto* region = cavingRegion();
    if (region == nullptr) {
        return;
    }

    auto* lazLayers = region->lazLayers();
    if (lazLayers == nullptr) {
        return;
    }

    // Dropping a row still has to run — the rowsAboutToBeRemoved handler is
    // what releases the m_objectStates entry of a layer that outlives its
    // row. Only its localMutationOccurred emit is unwanted, and rescan()
    // emits its row signals synchronously, so the rollback covers exactly the
    // rows this scan touches.
    QScopedValueRollback<bool> suppressTracking(d->suppressLocalMutationTracking, true);

    lazLayers->rescan();
}

void cwSaveLoad::enqueueProjectRenameJobs(const QString& oldDescriptorPath,
                                          const QString& newDescriptorPath)
{
    // Still gated on hasDurableHome, unlike the enqueueExternalCenterline* jobs.
    // These mutate the project's durable on-disk identity (the .cwproj
    // descriptor and dataRoot directory name), which doesn't exist until the
    // first save - Save As renames it as part of the move. The external-
    // centerline jobs write inside the dataRoot tree, which a temp project
    // already has, so they don't wait. Same reasoning at the two cleanups below.
    if (!d->hasDurableHome) {
        return;
    }

    const QDir rootDir = projectRootDir();
    const QString oldDataRoot = QFileInfo(oldDescriptorPath).completeBaseName();
    const QString newDataRoot = QFileInfo(newDescriptorPath).completeBaseName();

    if (oldDataRoot.isEmpty() || newDataRoot.isEmpty() || oldDataRoot == newDataRoot) {
        return;
    }

    const QString oldDataRootPath = rootDir.absoluteFilePath(oldDataRoot);
    const QString newDataRootPath = rootDir.absoluteFilePath(newDataRoot);

    // Rebuild m_objectStates so all child paths reflect the new dataRoot.
    d->resetObjectStates(this);

    // Queue a background job to rename the dataRoot directory.
    d->addExplicitFileSystemJob(
                cwSaveLoadPrivate::Job(nullptr, cwSaveLoadPrivate::Job::Kind::Directory, cwSaveLoadPrivate::Job::Action::Custom,
                          [oldDataRootPath, newDataRootPath]() -> Monad::ResultBase {
        if (!QFileInfo::exists(oldDataRootPath)) {
            return Monad::ResultBase();
        }
        if (QFileInfo::exists(newDataRootPath)) {
            // New dataRoot already exists (git placed files there during
            // fast-forward / merge). Clean up the old directory if git left
            // it behind as an empty artifact.
            if (QDir(oldDataRootPath).isEmpty()) {
                QDir().rmdir(oldDataRootPath);
            }
            return Monad::ResultBase();
        }
        return cwSaveLoadPrivate::moveDirectoryRobust(oldDataRootPath, newDataRootPath);
    }),
                this);

    // Queue a background job to rename the .cwproj descriptor file.
    if (oldDescriptorPath != newDescriptorPath) {
        d->addExplicitFileSystemJob(
                    cwSaveLoadPrivate::Job(nullptr, cwSaveLoadPrivate::Job::Kind::File, cwSaveLoadPrivate::Job::Action::Custom,
                              [oldDescriptorPath, newDescriptorPath]() -> Monad::ResultBase {
            if (!QFileInfo::exists(oldDescriptorPath) || QFileInfo::exists(newDescriptorPath)) {
                return Monad::ResultBase();
            }
            if (!QFile::rename(oldDescriptorPath, newDescriptorPath)) {
                return Monad::ResultBase(
                            QStringLiteral("Failed to rename descriptor: %1 -> %2")
                            .arg(oldDescriptorPath, newDescriptorPath));
            }
            return Monad::ResultBase();
        }),
                    this);
    }
}

void cwSaveLoad::enqueueConflictingProjectCleanup(const QString& conflictingDescriptorRelPath)
{
    // Durable-home-gated - see enqueueProjectRenameJobs for why the durable-identity
    // jobs wait for the first save while the external-centerline jobs don't.
    if (!d->hasDurableHome || conflictingDescriptorRelPath.isEmpty()) {
        return;
    }

    const QDir rootDir = projectRootDir();
    const QString conflictingDescPath =
            rootDir.absoluteFilePath(conflictingDescriptorRelPath);
    const QString conflictingDataRootPath =
            rootDir.absoluteFilePath(QFileInfo(conflictingDescriptorRelPath).completeBaseName());

    // Delete the conflicting .cwproj descriptor.
    d->addExplicitFileSystemJob(
                cwSaveLoadPrivate::Job(nullptr, cwSaveLoadPrivate::Job::Kind::File, cwSaveLoadPrivate::Job::Action::Custom,
                          [conflictingDescPath]() -> Monad::ResultBase {
                              if (!QFileInfo::exists(conflictingDescPath)) {
                                  return Monad::ResultBase();
                              }
                              if (!QFile::remove(conflictingDescPath)) {
                                  return Monad::ResultBase(
                                  QStringLiteral("Failed to remove conflicting descriptor: %1")
                                  .arg(conflictingDescPath));
                              }
                              return Monad::ResultBase();
                          }),
                this);

    // Delete the conflicting data directory.
    d->addExplicitFileSystemJob(
                cwSaveLoadPrivate::Job(nullptr, cwSaveLoadPrivate::Job::Kind::Directory, cwSaveLoadPrivate::Job::Action::Custom,
                          [conflictingDataRootPath]() -> Monad::ResultBase {
                              if (!QFileInfo::exists(conflictingDataRootPath)) {
                                  return Monad::ResultBase();
                              }
                              if (!QDir(conflictingDataRootPath).removeRecursively()) {
                                  return Monad::ResultBase(
                                  QStringLiteral("Failed to remove conflicting data directory: %1")
                                  .arg(conflictingDataRootPath));
                              }
                              return Monad::ResultBase();
                          }),
                this);
}

void cwSaveLoad::enqueueOrphanDirectoryCleanup(const QString& orphanDirRelPath)
{
    // Durable-home-gated - see enqueueProjectRenameJobs.
    if (!d->hasDurableHome || orphanDirRelPath.isEmpty()) {
        return;
    }

    const QString orphanDirAbsPath = projectRootDir().absoluteFilePath(orphanDirRelPath);

    d->addExplicitFileSystemJob(
                cwSaveLoadPrivate::Job(nullptr, cwSaveLoadPrivate::Job::Kind::Directory, cwSaveLoadPrivate::Job::Action::Custom,
                          [orphanDirAbsPath]() -> Monad::ResultBase {
                              if (!QFileInfo::exists(orphanDirAbsPath)) {
                                  return Monad::ResultBase();
                              }
                              if (!cwSaveLoadPrivate::fsRemoveDirRecursive(orphanDirAbsPath)) {
                              #ifdef Q_OS_WIN
                                  // On Windows, files checked out by the rebase may still
                                  // be held open by the LFS filter or git index.  Log a
                                  // warning and continue — the orphan files are inert and
                                  // will be cleaned up on the next save or sync once the
                                  // handles are released.
                                  qWarning() << "Orphan directory could not be removed (file handles still open):"
                                  << orphanDirAbsPath;
                              #else
                                  return Monad::ResultBase(
                                  QStringLiteral("Failed to remove orphan directory: %1")
                                  .arg(orphanDirAbsPath));
                              #endif
                              }
                              return Monad::ResultBase();
                          }),
                this);
}

void cwSaveLoad::enqueueExternalCenterlineCopyIfNewer(const QString& sourcePath,
                                                      const QString& destinationPath)
{
    enqueueExternalCenterlineCopy(sourcePath, destinationPath, true);
}

void cwSaveLoad::enqueueExternalCenterlineCopyOverwriting(const QString& sourcePath,
                                                          const QString& destinationPath)
{
    enqueueExternalCenterlineCopy(sourcePath, destinationPath, false);
}

void cwSaveLoad::enqueueExternalCenterlineCopy(const QString& sourcePath,
                                               const QString& destinationPath,
                                               bool keepMatchingDestination)
{
    // Deliberately not gated on d->hasDurableHome, unlike the project-rename
    // and cleanup jobs above. A temporary project already has a real root
    // dir, and Save As moves or re-zips that whole tree, so an attachment
    // copied in before the first save travels with it.
    if (sourcePath.isEmpty() || destinationPath.isEmpty()) {
        return;
    }

    // An external-centerline copy mutates the project's on-disk shape
    // (an attach or a Replace landing mid-session), so flip
    // cwProject::modified() via localMutationOccurred — otherwise the
    // copy is silently lost in a bundled .cw on quit-without-save.
    // Under CopyPolicy::SkipUpToDate reconcile()'s planner skips
    // up-to-date destinations, so a no-op reconcile never reaches this
    // and the bit stays untouched.
    if (!d->suppressLocalMutationTracking) {
        emit localMutationOccurred();
    }

    // Whether the job body mutated the destination, set on the worker and
    // read by onDone on the main thread — the job future's completion orders
    // the two.
    auto wroteDestination = std::make_shared<bool>(false);
    cwSaveLoadPrivate::Job job(
                nullptr,
                cwSaveLoadPrivate::Job::Kind::File,
                cwSaveLoadPrivate::Job::Action::Custom,
                [sourcePath, destinationPath, keepMatchingDestination,
                 wroteDestination]() -> Monad::ResultBase {
        if (sourcePath == destinationPath) {
            // Identical paths is almost certainly a caller bug, but removing
            // and re-copying the same file would silently delete it. Treat
            // as no-op so reconcile can recover on the next pass.
            return Monad::ResultBase();
        }

        const QFileInfo srcInfo(sourcePath);
        if (!srcInfo.exists() || !srcInfo.isFile()) {
            return Monad::ResultBase(
                        QStringLiteral("externalCenterlineCopy: missing source: %1")
                        .arg(sourcePath));
        }

        const QFileInfo dstInfo(destinationPath);
        if (dstInfo.exists()) {
            const bool sameSize = srcInfo.size() == dstInfo.size();
            const bool srcOlderOrEqual = srcInfo.lastModified() <= dstInfo.lastModified();
            if (keepMatchingDestination && sameSize && srcOlderOrEqual) {
                return Monad::ResultBase();
            }
            if (!QFile::remove(destinationPath)) {
                return Monad::ResultBase(
                            QStringLiteral("externalCenterlineCopy: failed to remove stale destination: %1")
                            .arg(destinationPath));
            }
            // The delete is itself a write the watcher sees, even if the
            // copy below then fails.
            *wroteDestination = true;
        }

        const auto ensureResult = cwSaveLoadPrivate::ensurePathForFile(destinationPath);
        if (ensureResult.hasError()) {
            return ensureResult;
        }

        if (!QFile::copy(sourcePath, destinationPath)) {
            return Monad::ResultBase(
                        QStringLiteral("externalCenterlineCopy: failed to copy %1 -> %2")
                        .arg(sourcePath, destinationPath));
        }
        *wroteDestination = true;
        return Monad::ResultBase();
    });
    // The copy lands on a path the external-centerline watcher is armed on,
    // and the watcher reads our own write exactly as it reads a user's edit
    // in a text editor. The mutation above already covers this write, so the
    // echo would be a second report of one change — and one that can arrive
    // after the following save has cleared the dirty bit, leaving the project
    // asking to be saved with nothing to write. Open the settle tail when the
    // bytes land so reportProjectFileChangedOnDisk() skips the echo. A job
    // that skipped the copy (destination already newer, same path, dropped
    // before running) wrote nothing, so it has no echo to skip — and the
    // window is global, so opening it anyway would swallow a genuine editor
    // save to any attachment inside the tail.
    job.onDone = [this, wroteDestination](const ResultBase&) {
        if (*wroteDestination) {
            d->selfWrite.noteWriteCompleted();
        }
    };
    // Set path for diagnostics + scheduling; ensureInsideRoot isn't enforced
    // for Action::Custom, so this is informational rather than a precondition.
    job.path = destinationPath;
    job.oldPath = sourcePath;
    d->addExplicitFileSystemJob(std::move(job), this);
}

void cwSaveLoad::enqueueExternalCenterlineRemoveFile(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }

    // GC of a stranded attachment file is a project mutation too; see
    // enqueueExternalCenterlineCopyIfNewer (including why this runs on
    // temporary projects).
    if (!d->suppressLocalMutationTracking) {
        emit localMutationOccurred();
    }

    cwSaveLoadPrivate::Job job(nullptr,
                               cwSaveLoadPrivate::Job::Kind::File,
                               cwSaveLoadPrivate::Job::Action::Remove);
    // Deleting a watched file notifies the watcher too; see
    // enqueueExternalCenterlineCopyIfNewer for why the echo is skipped.
    job.onDone = [this](const ResultBase&) {
        d->selfWrite.noteWriteCompleted();
    };
    job.oldPath = path;
    d->addExplicitFileSystemJob(std::move(job), this);
}

void cwSaveLoad::enqueueExternalCenterlineRemoveTree(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }

    // Removing an attachment dir (detach) is a project mutation too; see
    // enqueueExternalCenterlineCopyIfNewer (including why this runs on
    // temporary projects).
    if (!d->suppressLocalMutationTracking) {
        emit localMutationOccurred();
    }

    cwSaveLoadPrivate::Job job(nullptr,
                               cwSaveLoadPrivate::Job::Kind::Directory,
                               cwSaveLoadPrivate::Job::Action::Remove);
    // Every watched file under the tree notifies on the way out; see
    // enqueueExternalCenterlineCopyIfNewer for why the echo is skipped.
    job.onDone = [this](const ResultBase&) {
        d->selfWrite.noteWriteCompleted();
    };
    job.oldPath = path;
    d->addExplicitFileSystemJob(std::move(job), this);
}

void cwSaveLoad::reportProjectFileChangedOnDisk()
{
    if (d->suppressLocalMutationTracking || d->selfWrite.isOpen()) {
        return;
    }
    emit localMutationOccurred();
}

void cwSaveLoad::disconnectObjects()
{
    // Disconnect trip-owned objects that are not represented as region-tree objects.
    auto trips = d->m_regionTreeModel->all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip);
    for (auto trip : trips) {
        if (trip == nullptr) {
            continue;
        }

        for (int i = 0; i < trip->chunkCount(); ++i) {
            auto* chunk = trip->chunk(i);
            if (chunk == nullptr) {
                continue;
            }
            disconnect(chunk, nullptr, this, nullptr);
            if (d->isTrackedConnected(chunk)) {
                d->trackDisconnected(chunk);
                d->connectionChecker.remove(chunk);
            }
        }

        if (auto* teamModel = trip->team()) {
            disconnect(teamModel, nullptr, this, nullptr);
            if (d->isTrackedConnected(teamModel)) {
                d->trackDisconnected(teamModel);
                d->connectionChecker.remove(teamModel);
            }
        }

        if (auto* calibrations = trip->calibrations()) {
            disconnect(calibrations, nullptr, this, nullptr);
            if (d->isTrackedConnected(calibrations)) {
                d->trackDisconnected(calibrations);
                d->connectionChecker.remove(calibrations);
            }
        }
    }

    //Disconnect from all the region-tree objects
    QList<QObject*> objects = d->m_regionTreeModel->all<QObject*>(QModelIndex(), &cwRegionTreeModel::object);
    for(auto obj : objects) {
        disconnect(obj, nullptr, this, nullptr);
        if (d->isTrackedConnected(obj)) {
            d->trackDisconnected(obj);
            d->connectionChecker.remove(obj);
        }
    }
}

void cwSaveLoad::connectObjects()
{

    {
        auto caves = d->m_regionTreeModel->all<cwCave*>(QModelIndex(), &cwRegionTreeModel::cave);
        for(auto cave : caves) {
            connectCave(cave);
        }
    }

    {
        auto trips = d->m_regionTreeModel->all<cwTrip*>(QModelIndex(), &cwRegionTreeModel::trip);
        for(auto trip : trips) {
            connectTrip(trip);
        }
    }

    {
        auto notes = d->m_regionTreeModel->all<cwNote*>(QModelIndex(), &cwRegionTreeModel::note);
        for(auto note : notes) {
            connectNote(note);
        }
    }

    {
        auto notes = d->m_regionTreeModel->all<cwNoteLiDAR*>(QModelIndex(), &cwRegionTreeModel::noteLiDAR);
        for(auto note : notes) {
            connectNoteLiDAR(note);
        }
    }

    {
        auto sketches = d->m_regionTreeModel->all<cwSketch*>(QModelIndex(), &cwRegionTreeModel::sketch);
        for(auto sketch : sketches) {
            connectSketch(sketch);
        }
    }

}

void cwSaveLoad::connectListModelSaveTrigger(QAbstractItemModel* model,
                                             WatchReset watchReset,
                                             const std::function<void()>& onChanged)
{
    if (model == nullptr) {
        return;
    }
    connect(model, &QAbstractItemModel::rowsInserted, this, onChanged);
    connect(model, &QAbstractItemModel::rowsRemoved, this, onChanged);
    connect(model, &QAbstractItemModel::dataChanged, this, onChanged);
    if (watchReset == WatchReset::Yes) {
        connect(model, &QAbstractItemModel::modelReset, this, onChanged);
    }
}

void cwSaveLoad::connectCave(cwCave *cave)
{
    if (cave == nullptr) {
        return;
    }

    auto saveCaveName = [cave, this]() {
        d->renameDirectoryAndFile(this, cave);
    };

    if(!d->trackConnected(cave)) {
        return;
    }
    d->connectionChecker.add(cave);

    connect(cave, &cwCave::nameChanged, this, saveCaveName);

    auto saveCave = [cave, this]() {
        d->saveObject(this, cave);
    };
    connect(cave->length(), &cwUnitValue::unitChanged, this, saveCave);
    connect(cave->depth(), &cwUnitValue::unitChanged, this, saveCave);
    connect(cave, &cwCave::externalCenterlineChanged, this, saveCave);
    //A node's kind or source is part of what decides the project's file format,
    //so these rewrite the whole project when they move it (§6.8).
    const auto saveCaveAndRestamp = [saveCave, this]() {
        saveCave();
        restampProjectIfVersionChanged();
    };
    connect(cave, &cwCave::kindChanged, this, saveCaveAndRestamp);
    connect(cave, &cwCave::sourceChanged, this, saveCaveAndRestamp);

    //A move reaches the tree model as a row removal and a row insertion, and a
    //row removal is otherwise this node's directory being deleted (§6.5). The
    //node says which of the two is happening, and the Move itself is queued from
    //parentNodeChanged — inside the insert, while the object state still names
    //the old directory and dirPrivate() already names the new one.
    connect(cave, &cwCave::beginMoveNode, this, [cave, this]() {
        d->m_movingNodes.insert(cave);
    });
    connect(cave, &cwCave::parentNodeChanged, this, [cave, this]() {
        d->moveDirectory(this, cave);
    });
    connect(cave, &cwCave::nodeMoved, this, [cave, this]() {
        d->m_movingNodes.remove(cave);
        //The node is back on the tree, so the shape the stamp answers for is
        //final: a move that nests the last flat node, or frees the last nested
        //one, changes the format the whole project needs (§6.8).
        restampProjectIfVersionChanged();
    });

    // connectCave runs after the cave's data is loaded, so watching modelReset
    // is safe here (the load-time setFixStations() reset fires before these
    // connections exist).
    connectListModelSaveTrigger(cave->fixStations(), WatchReset::Yes, saveCave);
}


void cwSaveLoad::connectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    const auto rebindIfTracked = [this](QObject* object) -> bool {
        if (object == nullptr) {
            return false;
        }

        disconnect(object, nullptr, this, nullptr);
        if (d->isTrackedConnected(object)) {
            d->trackDisconnected(object);
            d->connectionChecker.remove(object);
        }
        if (!d->trackConnected(object)) {
            return false;
        }
        d->connectionChecker.add(object);
        return true;
    };

    // Lambda that saves this specific trip
    const auto saveTrip = [this, trip]() {
        save(trip);
    };

    // Helper to connect a survey chunk to save on any data change
    const auto connectChunk = [this, saveTrip, rebindIfTracked](cwSurveyChunk* chunk) {
        if (chunk == nullptr) {
            return;
        }

        if(!rebindIfTracked(chunk)) {
            return;
        }

        connect(chunk, &cwSurveyChunk::added, this, saveTrip);
        connect(chunk, &cwSurveyChunk::aboutToRemove, this, saveTrip);
        connect(chunk, &cwSurveyChunk::removed, this, saveTrip);

        connect(chunk, &cwSurveyChunk::dataChanged, this, saveTrip);
        connect(chunk, &cwSurveyChunk::stationSplaysChanged, this, saveTrip);
    };

    if(!rebindIfTracked(trip)) {
        return;
    }

    auto saveTripName = [trip, this]() {
        d->renameDirectoryAndFile(this, trip);
    };
    connect(trip, &cwTrip::nameChanged, this, saveTripName);

    // Trip-level changes
    connect(trip, &cwTrip::dateChanged, this, saveTrip);
    connect(trip, &cwTrip::externalCenterlineChanged, this, saveTrip);
    connect(trip, &cwTrip::stationPrefixChanged, this, saveTrip);
    //A trip's stored source path is part of what decides the project's file
    //format, so it rewrites the whole project when it appears (§6.8).
    const auto saveTripAndRestamp = [saveTrip, this]() {
        saveTrip();
        restampProjectIfVersionChanged();
    };
    connect(trip, &cwTrip::sourcePathChanged, this, saveTripAndRestamp);
    // connect(trip, &cwTrip::numberOfChunksChanged, this, saveTrip);
    connect(trip, &cwTrip::chunksAboutToBeRemoved, this, saveTrip);
    connect(trip, &cwTrip::chunksRemoved, this, saveTrip);

    // Newly inserted chunks → connect them and save
    connect(trip, &cwTrip::chunksInserted, this,
            [this, trip, connectChunk, saveTrip](int begin, int end) {
        for (int i = begin; i <= end; ++i) {
            if (auto* chunk = trip->chunk(i)) { // adjust if accessor differs
                connectChunk(chunk);
            }
        }
        saveTrip();
    });

    // Connect all existing chunks
    for (int i = 0; i < trip->chunkCount(); ++i) {
        if (auto* chunk = trip->chunk(i)) {
            connectChunk(chunk);
        }
    }

    // Team model changes → save
    if (QAbstractItemModel* const teamModel = trip->team()) {

        if(rebindIfTracked(teamModel)) {
            connect(teamModel, &QAbstractItemModel::dataChanged, this, saveTrip);
            connect(teamModel, &QAbstractItemModel::rowsInserted, this, saveTrip);
            connect(teamModel, &QAbstractItemModel::rowsRemoved, this, saveTrip);
            connect(teamModel, &QAbstractItemModel::modelReset, this, saveTrip);
            connect(teamModel, &QAbstractItemModel::layoutChanged, this, saveTrip);
        }
    }

    // Notes model changes → save
    // Notes model is update through cwCavingRegionTree


    // Trip calibration changes → save
    if (cwTripCalibration* const cal = trip->calibrations()) {

        if(!rebindIfTracked(cal)) {
            return;
        }

        connect(cal, &cwTripCalibration::correctedCompassBacksightChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::correctedClinoBacksightChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::correctedCompassFrontsightChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::correctedClinoFrontsightChanged, this, saveTrip);

        connect(cal, &cwTripCalibration::tapeCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::frontCompassCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::frontClinoCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::backCompassCalibrationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::backClinoCalibrationChanged, this, saveTrip);

        connect(cal, &cwTripCalibration::declinationChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::distanceUnitChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::supportedUnitsChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::frontSightsChanged, this, saveTrip);
        connect(cal, &cwTripCalibration::backSightsChanged, this, saveTrip);
    }

    // parentCaveChanged intentionally omitted (no re-parenting)
}



void cwSaveLoad::connectNote(cwNote *note)
{
    if (note == nullptr) {
        return;
    }

    auto renameAndSaveNote = [this, note]() {
        //Only rename if saved before
        // auto currentFileName = d->m_fileLookup.value(note);
        auto fileMove = cwSaveLoadPrivate::Job {note, cwSaveLoadPrivate::Job::Kind::File, cwSaveLoadPrivate::Job::Action::Move};
        d->addFileSystemJob(fileMove, this);

        save(note);
    };

    if(!d->trackConnected(note)) {
        return;
    }
    d->connectionChecker.add(note);

    // Note-level changes
    connect(note, &cwNote::nameChanged, this, renameAndSaveNote);

    // Lambda to save this specific note
    const auto saveNote = [this, note]() {
        save(note);
    };
    connect(note, &cwNote::imageChanged, this, saveNote);
    connect(note, &cwNote::rotateChanged, this, saveNote);
    connect(note->imageResolution(), &cwImageResolution::valueChanged, this, saveNote);
    connect(note->imageResolution(), &cwImageResolution::unitChanged, this, saveNote);

    connect(note, &cwNote::insertedScraps, this, saveNote);
    connect(note, &cwNote::removedScraps, this, saveNote);
    connect(note, &cwNote::scrapsReset, this, saveNote);

    //Connect scraps
    const auto scraps = note->scraps();
    for(auto scrap : scraps) {
        connectScrap(scrap);
    }
}

void cwSaveLoad::connectScrap(cwScrap *scrap)
{
    if (scrap == nullptr) {
        return;
    }

    // Lambda to save this specific note
    const auto saveNote = [this, scrap]() {
        cwNote* parentNote = scrap->parentNote();
        if (parentNote == nullptr) {
            return;
        }
        save(parentNote);
    };

    if(!d->trackConnected(scrap)) {
        return;
    }
    d->connectionChecker.add(scrap);

    // Scrap outline changes
    connect(scrap, &cwScrap::insertedPoints, this, saveNote);
    connect(scrap, &cwScrap::removedPoints, this, saveNote);
    connect(scrap, &cwScrap::pointChanged, this, saveNote);
    connect(scrap, &cwScrap::pointsReset, this, saveNote);
    connect(scrap, &cwScrap::closeChanged, this, saveNote);

    // Scrap stations
    connect(scrap, &cwScrap::stationAdded, this, saveNote);
    connect(scrap, &cwScrap::stationPositionChanged, this, saveNote);
    connect(scrap, &cwScrap::stationNameChanged, this, saveNote);
    connect(scrap, &cwScrap::stationRemoved, this, saveNote);
    connect(scrap, &cwScrap::stationsReset, this, saveNote);

    // Scrap leads
    connect(scrap, &cwScrap::leadsBeginInserted, this, saveNote);
    connect(scrap, &cwScrap::leadsInserted, this, saveNote);
    connect(scrap, &cwScrap::leadsBeginRemoved, this, saveNote);
    connect(scrap, &cwScrap::leadsRemoved, this, saveNote);
    connect(scrap, &cwScrap::leadsReset, this, saveNote);
    connect(scrap, &cwScrap::leadsDataChanged,
            this, [saveNote](int begin, int end, QList<int> roles) {
        Q_UNUSED(begin);
        Q_UNUSED(end);

        // LeadPosition is derived from triangulation and should not trigger note persistence.
        const bool hasPersistentLeadRole = roles.contains(cwScrap::LeadPositionOnNote)
                || roles.contains(cwScrap::LeadDesciption)
                || roles.contains(cwScrap::LeadSize)
                || roles.contains(cwScrap::LeadCompleted);
        if (!hasPersistentLeadRole) {
            return;
        }

        saveNote();
    });

    // Transformations / type / view matrix
    connect(scrap, &cwScrap::noteTransformationChanged, this, saveNote);
    connect(scrap, &cwScrap::calculateNoteTransformChanged, this, saveNote);
    connect(scrap, &cwScrap::viewMatrixChanged, this, saveNote);
    connect(scrap, &cwScrap::typeChanged, this, saveNote);
    const auto saveManualScrapTransform = [saveNote, scrap]() {
        if(scrap->calculateNoteTransform()) {
            return;
        }
        saveNote();
    };
    connect(scrap->noteTransformation(), &cwNoteTranformation::northUpChanged, this, saveManualScrapTransform);
    //A manual scale's units are stored data, so persist on the data signal — the
    //ratio-only scaleChanged would miss the user picking ft over m
    connect(scrap->noteTransformation(), &cwNoteTranformation::scaleDataChanged, this, saveManualScrapTransform);

    auto connectProjectedViewMatrixSignals = [this, saveManualScrapTransform, scrap]() {
        if (auto projected = qobject_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()))
        {
            connect(projected, &cwProjectedProfileScrapViewMatrix::azimuthChanged, this, saveManualScrapTransform);
            connect(projected, &cwProjectedProfileScrapViewMatrix::directionChanged, this, saveManualScrapTransform);
        }
    };

    connectProjectedViewMatrixSignals();
    connect(scrap, &cwScrap::viewMatrixChanged, this, connectProjectedViewMatrixSignals);
}

void cwSaveLoad::connectNoteLiDAR(cwNoteLiDAR *lidarNote)
{
    if (lidarNote == nullptr) {
        return;
    }

    // Lambda to save this specific note
    const auto saveNote = [this, lidarNote]() {
        // qDebug() << "Save lidar:" << lidarNote << lidarNote->noteTransformation()->upSign();
        save(lidarNote);
    };

    if(!d->trackConnected(lidarNote)) {
        return;
    }
    d->connectionChecker.add(lidarNote);

    connect(lidarNote, &cwNoteLiDAR::nameChanged, this, saveNote);
    connect(lidarNote, &cwNoteLiDAR::filenameChanged, this, saveNote);
    connect(lidarNote, &cwNoteLiDAR::dataChanged,
            this, [saveNote](const QModelIndex &topLeft,
            const QModelIndex &bottomRight,
            const QList<int> &roles) {
        if(roles.contains(cwNoteLiDAR::NameRole)
                || roles.contains(cwNoteLiDAR::PositionOnNoteRole)) {
            Q_UNUSED(topLeft);
            Q_UNUSED(bottomRight);
            Q_UNUSED(roles);
            saveNote();
        }
    });
    connect(lidarNote, &cwNoteLiDAR::rowsInserted,
            this, [saveNote](const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent);
        Q_UNUSED(first);
        Q_UNUSED(last);
        saveNote();
    });
    connect(lidarNote, &cwNoteLiDAR::rowsRemoved,
            this, [saveNote](const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent);
        Q_UNUSED(first);
        Q_UNUSED(last);
        saveNote();
    });
    connect(lidarNote, &cwNoteLiDAR::autoCalculateNorthChanged, this, saveNote);

    if(!d->trackConnected(lidarNote->noteTransformation())) {
        return;
    }
    d->connectionChecker.add(lidarNote->noteTransformation());

    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::upSignChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::upModeChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::upCustomChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::northUpChanged, this, saveNote);
    connect(lidarNote->noteTransformation(), &cwNoteLiDARTransformation::scaleDataChanged, this, saveNote);
}

void cwSaveLoad::connectSketch(cwSketch *sketch)
{
    if (sketch == nullptr) {
        return;
    }

    const auto saveSketch = [this, sketch]() {
        save(sketch);
    };

    if (!d->trackConnected(sketch)) {
        return;
    }
    d->connectionChecker.add(sketch);

    connect(sketch, &cwSketch::nameChanged, this, saveSketch);
    connect(sketch, &cwSketch::viewTypeChanged, this, saveSketch);
    connect(sketch, &cwSketch::strokesReset, this, saveSketch);
    // beginStroke/appendPoint/endStroke mutate the stroke list directly
    // and push a QUndoCommand whose first redo() is a no-op — strokesReset
    // never fires during a fresh stroke. Persist on strokeEnded so drawn
    // strokes survive save/load.
    connect(sketch, &cwSketch::strokeEnded, this, saveSketch);

    if (auto* scale = sketch->mapScale()) {
        if (d->trackConnected(scale)) {
            d->connectionChecker.add(scale);
            connect(scale, &cwScale::dataChanged, this, saveSketch);
        }
    }
}


void cwSaveLoad::markDurableHome()
{
    if (d->hasDurableHome) {
        return;
    }
    d->hasDurableHome = true;
    emit hasDurableHomeChanged();
}

void cwSaveLoad::markTemporaryHome()
{
    if (!d->hasDurableHome) {
        return;
    }
    d->hasDurableHome = false;
    emit hasDurableHomeChanged();
}

QString cwSaveLoad::randomName() const
{
    quint32 randomValue = QRandomGenerator::global()->generate();
    return QStringLiteral("cavewhereTmp-") + QStringLiteral("%1").arg(randomValue, 8, 16, QChar(u'0'));
}

QString cwSaveLoad::friendlyProjectName()
{
    static const QStringList adjectives = {
        QStringLiteral("Misty"), QStringLiteral("Thunder"), QStringLiteral("Obsidian"),
        QStringLiteral("Glacier"), QStringLiteral("Crystal"), QStringLiteral("Amber"),
        QStringLiteral("Crimson"), QStringLiteral("Silver"), QStringLiteral("Golden"),
        QStringLiteral("Iron"), QStringLiteral("Marble"), QStringLiteral("Jade"),
        QStringLiteral("Onyx"), QStringLiteral("Sapphire"), QStringLiteral("Copper"),
        QStringLiteral("Azure"), QStringLiteral("Ivory"), QStringLiteral("Scarlet"),
        QStringLiteral("Shadow"), QStringLiteral("Storm"), QStringLiteral("Frost"),
        QStringLiteral("Ember"), QStringLiteral("Dawn"), QStringLiteral("Dusk"),
        QStringLiteral("Ancient"), QStringLiteral("Hidden"), QStringLiteral("Silent"),
        QStringLiteral("Forgotten"), QStringLiteral("Wild"), QStringLiteral("Mystic")
    };
    static const QStringList nouns = {
        QStringLiteral("Peak"), QStringLiteral("Ridge"), QStringLiteral("Summit"),
        QStringLiteral("Pass"), QStringLiteral("Crest"), QStringLiteral("Pinnacle"),
        QStringLiteral("Cliff"), QStringLiteral("Gorge"), QStringLiteral("Canyon"),
        QStringLiteral("Valley"), QStringLiteral("Hollow"), QStringLiteral("Cavern"),
        QStringLiteral("Bluff"), QStringLiteral("Butte"), QStringLiteral("Crag"),
        QStringLiteral("Tor"), QStringLiteral("Knoll"), QStringLiteral("Basin"),
        QStringLiteral("Dome"), QStringLiteral("Spire")
    };

    const quint32 adjectiveIndex = QRandomGenerator::global()->bounded(static_cast<quint32>(adjectives.size()));
    const quint32 nounIndex = QRandomGenerator::global()->bounded(static_cast<quint32>(nouns.size()));
    return adjectives.at(adjectiveIndex) + QStringLiteral(" ") + nouns.at(nounIndex);
}

QDir cwSaveLoad::createTemporaryDirectory(const QString &subDirName)
{
    QTemporaryDir tempDir;
    tempDir.setAutoRemove(false);

    setOwnedTempDir(tempDir.path());

    QDir dir(tempDir.path());
    dir.mkdir(subDirName);
    dir.cd(subDirName);

    return dir;
}

QFuture<void> cwSaveLoad::completeSaveJobs()
{
    return d->m_pendingJobsDeferred.future();
}

QFuture<void> cwSaveLoad::pendingJobsFinished()
{
    // Shielded because the drain future is shared project-wide state:
    // AsyncFuture propagates cancel() upstream through context chains,
    // so an unshielded caller canceling its derived chain would cancel
    // m_pendingJobsDeferred's future underneath the re-arm polls in
    // cwSaveLoadPrivate and every sibling observer, starving the queue.
    // Note the shielded future settles via the event loop, so it is
    // never already-finished at return - observe it, don't poll it.
    return AsyncFuture::shield(completeSaveJobs());
}

QString cwSaveLoad::sanitizeFileName(QString input) {
    return cwNameUtils::sanitizeFileName(std::move(input));
}

QString cwSaveLoad::relativeNodeDir(const QStringList& nodePath)
{
    QString relativeDir;
    for (const QString& name : nodePath) {
        if (!relativeDir.isEmpty()) {
            relativeDir += QLatin1Char('/') + kSubNodeDirName + QLatin1Char('/');
        }
        relativeDir += sanitizeFileName(name);
    }
    return relativeDir;
}

int cwSaveLoad::stampVersion(const cwCavingRegion* region)
{
    if (region == nullptr) {
        return kFlatProjectVersion;
    }

    const cwSurveyNode* root = region->rootNode();
    if (root == nullptr) {
        return kFlatProjectVersion;
    }

    //Every field the writer can store for a node in the tree counts toward the
    //stamp: a file that carries one is a file an older build would drop on its
    //next save. Only a cwCave's trips reach disk, so the walk below asks this
    //of the caves it visits and the root's own trips stay out of it.
    const auto holdsTripSourcePath = [](const cwSurveyNode* node) {
        const QList<cwTrip*> trips = node->trips();
        for (const cwTrip* trip : trips) {
            if (!trip->sourcePath().isEmpty()) {
                return true;
            }
        }
        return false;
    };

    //Stops at the first node that needs the tree format: this runs once per
    //file a save writes, so it walks no more of the region than it has to. A
    //child of a node that is not the root is already at depth two.
    const auto holdsHierarchy = [holdsTripSourcePath](const cwSurveyNode* node, auto&& self) -> bool {
        const QList<cwSurveyNode*> children = node->childNodes();
        for (const cwSurveyNode* child : children) {
            const bool needsTree = !node->isRoot()
                    || child->kind() != cwSurveyNode::Kind::Cave
                    || child->isReadOnly()
                    || !child->sourceId().isNull()
                    || !child->sourcePath().isEmpty()
                    || holdsTripSourcePath(child);
            if (needsTree || self(child, self)) {
                return true;
            }
        }
        return false;
    };

    return holdsHierarchy(root, holdsHierarchy) ? cwRegionIOTask::protoVersion() : kFlatProjectVersion;
}


QString cwSaveLoad::lastDirectoryForProjectFile(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (filePath.endsWith(QStringLiteral(".cwproj"), Qt::CaseInsensitive)) {
        // The .cwproj file lives one level inside the project folder.
        // Strip the filename to reach the project folder, then strip the
        // project folder name to reach the folder the user navigated to.
        return QFileInfo(info.path()).path();
    }
    // For .cw bundles and all other file types, strip only the filename.
    return info.path();
}

void cwSaveLoad::initializeGitRepository(const QDir& repoDir)
{
    QQuickGit::GitRepository repository;
    repository.setLfsPolicy(cavewhereLfsPolicy());
    repository.setDirectory(repoDir);
    repository.initRepository();
    ensureGitExcludeHasLocalEntries(repoDir);
}

QDir cwSaveLoad::projectRootDir() const
{
    return projectRootDirForFile(d->projectFileName);
}

QDir cwSaveLoad::dataRootDir() const
{
    auto region = d->m_regionTreeModel->cavingRegion();
    QString dataRootName = d->projectMetadata.dataRoot;
    if (dataRootName.isEmpty()) {
        dataRootName = cwSaveLoadPrivate::defaultDataRoot(region ? region->name() : QString());
    }
    return QDir(projectRootDir().absoluteFilePath(dataRootName));
}

QDir cwSaveLoad::dir(cwSurveyNoteModel* notes) const
{
    return dirPrivate(notes);
}

QDir cwSaveLoad::dir(cwSurveyNoteLiDARModel* notes) const
{
    return dirPrivate(notes);
}

QString cwSaveLoad::absolutePath(const cwNote* note, const QString& imageFilename) const
{
    return absolutePathPrivate(note, imageFilename);
}

QString cwSaveLoad::absolutePath(const cwNoteLiDAR* note, const QString& lidarFilename) const
{
    return absolutePathPrivate(note, lidarFilename);
}

cwImage cwSaveLoad::absolutePathNoteImage(const cwNote* note) const
{
    if (!note) {
        return cwImage();
    }

    const QString path = absolutePath(note, note->image().path());
    if (path.isEmpty()) {
        return cwImage();
    }

    cwImage image = note->image();
    image.setPath(path);
    return image;
}

QString cwSaveLoad::fileNamePrivate(const cwCave* cave) const
{
    return sanitizeFileName(cave->name() + QStringLiteral(".cwcave"));
}

QString cwSaveLoad::absolutePathPrivate(const cwCave* cave) const
{
    return dirPrivate(cave).absoluteFilePath(fileNamePrivate(cave));
}

QDir cwSaveLoad::dirPrivate(const cwCave* cave) const
{
    if (cave == nullptr || cave->parentRegion() == nullptr) {
        return QDir();
    }

    //cwSurveyNode::path() is the walk up to the root, and relativeNodeDir turns
    //it into the sub/ layout: the root's own children are the data root's
    //directories, and everything deeper sits in its parent's sub/.
    return QDir(dataRootDir().absoluteFilePath(relativeNodeDir(cave->path())));
}

QString cwSaveLoad::fileNamePrivate(const cwTrip* trip) const
{
    return sanitizeFileName(trip->name() + QStringLiteral(".cwtrip"));
}

QString cwSaveLoad::absolutePathPrivate(const cwTrip* trip) const
{
    return dirPrivate(trip).absoluteFilePath(fileNamePrivate(trip));
}

QDir cwSaveLoad::dirPrivate(const cwTrip* trip) const
{
    if (trip->parentCave()) {
        return tripDirHelper(dirPrivate(trip->parentCave()), trip);
    }
    return QDir();
}

QDir cwSaveLoad::dirPrivate(cwSurveyNoteModel* notes) const
{
    if (notes->parentTrip()) {
        return noteDirHelper(dirPrivate(notes->parentTrip()));
    }
    return QDir();
}

QDir cwSaveLoad::dirPrivate(cwSurveyNoteLiDARModel* notes) const
{
    if (notes->parentTrip()) {
        return noteDirHelper(dirPrivate(notes->parentTrip()));
    }
    return QDir();
}

QString cwSaveLoad::fileNamePrivate(const cwNote* note) const
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote"));
}

QString cwSaveLoad::absolutePathPrivate(const cwNote* note) const
{
    return dirPrivate(note).absoluteFilePath(fileNamePrivate(note));
}

QString cwSaveLoad::absolutePathPrivate(const cwNote* note, const QString& imageFilename) const
{
    if (note == nullptr) {
        return QString();
    }

    if (!imageFilename.isEmpty()) {
        return dirPrivate(note).absoluteFilePath(imageFilename);
    }

    return QString();
}

QDir cwSaveLoad::dirPrivate(const cwNote* note) const
{
    if (note->parentTrip()) {
        return noteDirHelper(dirPrivate(note->parentTrip()));
    }
    return QDir();
}

QString cwSaveLoad::fileNamePrivate(const cwNoteLiDAR* note) const
{
    return sanitizeFileName(note->name() + QStringLiteral(".cwnote3d"));
}

QString cwSaveLoad::absolutePathPrivate(const cwNoteLiDAR* note) const
{
    return dirPrivate(note).absoluteFilePath(fileNamePrivate(note));
}

QString cwSaveLoad::absolutePathPrivate(const cwNoteLiDAR* note, const QString& lidarFilename) const
{
    if (note == nullptr) {
        return QString();
    }

    if (lidarFilename.isEmpty()) {
        return QString();
    }

    return dirPrivate(note).absoluteFilePath(lidarFilename);
}

QDir cwSaveLoad::dirPrivate(const cwNoteLiDAR* note) const
{
    if (note->parentTrip()) {
        return noteDirHelper(dirPrivate(note->parentTrip()));
    }
    return QDir();
}

QString cwSaveLoad::fileNamePrivate(const cwSketch* sketch) const
{
    return sanitizeFileName(sketch->name() + QStringLiteral(".cwsketch"));
}

QString cwSaveLoad::absolutePathPrivate(const cwSketch* sketch) const
{
    return dirPrivate(sketch).absoluteFilePath(fileNamePrivate(sketch));
}

QString cwSaveLoad::absolutePathPrivate(const cwSketch* sketch, const QString& sketchFilename) const
{
    if (sketch == nullptr || sketchFilename.isEmpty()) {
        return QString();
    }
    return dirPrivate(sketch).absoluteFilePath(sketchFilename);
}

QString cwSaveLoad::fileNamePrivate(const cwLazLayer* layer) const
{
    if (layer == nullptr) {
        return QString();
    }
    // Pair the .cwlaz to its sibling .laz/.las by basename. completeBaseName
    // is used (not baseName) so compound names like "alpha.beta.laz" pair
    // with "alpha.beta.cwlaz" rather than collapsing onto "alpha.cwlaz".
    const QString basename = QFileInfo(layer->sourcePath()).completeBaseName();
    if (basename.isEmpty()) {
        return QString();
    }
    return sanitizeFileName(basename + QStringLiteral(".cwlaz"));
}

QString cwSaveLoad::absolutePathPrivate(const cwLazLayer* layer) const
{
    const QString filename = fileNamePrivate(layer);
    if (filename.isEmpty()) {
        return QString();
    }
    return dirPrivate(layer).absoluteFilePath(filename);
}

QDir cwSaveLoad::dirPrivate(const cwLazLayer* /*layer*/) const
{
    // LAZ layers live in a flat "GIS Layers/" directory alongside the
    // dataRoot inside the project root. The directory is the same for every
    // layer; only the basename varies.
    return QDir(projectRootDir().absoluteFilePath(cwLazLayerModel::folderName()));
}

QDir cwSaveLoad::dirPrivate(const cwSketch* sketch) const
{
    if (sketch != nullptr && sketch->parentTrip() != nullptr) {
        return noteDirHelper(dirPrivate(sketch->parentTrip()));
    }
    return QDir();
}

void cwSaveLoad::saveProtoMessage(std::unique_ptr<const google::protobuf::Message> message,
                                  const QObject* objectId)
{
    Q_ASSERT(message);
    d->saveProtoMessage(this,
                        std::move(message),
                        objectId);

}

QDir cwSaveLoad::caveDirHelper(const QDir &projectDir, const cwCave *cave)
{
    QString caveDirName = sanitizeFileName(cave->name());
    return QDir(projectDir.absoluteFilePath(caveDirName));
}

QDir cwSaveLoad::nodeDirHelper(const QDir &parentNodeDir, const cwCave *node)
{
    return QDir(parentNodeDir.absoluteFilePath(kSubNodeDirName + QLatin1Char('/') + sanitizeFileName(node->name())));
}

QDir cwSaveLoad::tripDirHelper(const QDir &caveDir, const cwTrip *trip)
{
    return QDir(caveDir.absoluteFilePath(QStringLiteral("trips/") + sanitizeFileName(trip->name())));
}

QDir cwSaveLoad::noteDirHelper(const QDir &tripDir)
{
    return QDir(tripDir.absoluteFilePath("notes"));
}

QDir cwSaveLoad::externalCenterlineDirHelper(const QDir &ownerDir)
{
    return QDir(ownerDir.absoluteFilePath(QStringLiteral("external-centerline")));
}

QDir cwSaveLoad::externalCenterlineDir(const cwCave *cave) const
{
    if (cave == nullptr || cave->parentRegion() == nullptr) {
        return QDir();
    }
    return externalCenterlineDirHelper(dirPrivate(cave));
}

QDir cwSaveLoad::externalCenterlineDir(const cwTrip *trip) const
{
    if (trip == nullptr || trip->parentCave() == nullptr) {
        return QDir();
    }
    return externalCenterlineDirHelper(dirPrivate(trip));
}

bool cwSaveLoad::hasDurableHome() const
{
    return d->hasDurableHome;
}

QString cwSaveLoad::dataRoot() const
{
    return d->projectMetadata.dataRoot;
}

QString cwSaveLoad::projectId() const
{
    return d->projectMetadata.projectId;
}

void cwSaveLoad::setDataRoot(const QString &dataRoot)
{
    QString normalized = dataRoot.trimmed();
    if (normalized.isEmpty()) {
        auto region = d->m_regionTreeModel->cavingRegion();
        normalized = cwSaveLoadPrivate::defaultDataRoot(region ? region->name() : QString());
    }

    if (d->projectMetadata.dataRoot == normalized) {
        return;
    }

    d->projectMetadata.dataRoot = normalized;
    emit dataRootChanged();
    if (!d->projectFileName.isEmpty()) {
        projectRootDir().mkpath(normalized);
    }

    if (d->saveEnabled && !d->projectFileName.isEmpty()) {
        saveProject(projectRootDir(), d->m_regionTreeModel->cavingRegion());
    }
}

bool cwSaveLoad::syncEnabled() const
{
    return d->projectMetadata.syncEnabled;
}

void cwSaveLoad::setSyncEnabled(bool enabled)
{
    if (d->projectMetadata.syncEnabled == enabled) {
        return;
    }

    d->projectMetadata.syncEnabled = enabled;

    if (d->saveEnabled && !d->projectFileName.isEmpty()) {
        saveProject(projectRootDir(), d->m_regionTreeModel->cavingRegion());
    }
}

Monad::ResultBase cwSaveLoad::commitProjectChanges(const QString& subject,
                                                   const QString& description)
{
    if (d->projectFileName.isEmpty()) {
        return ResultBase();
    }

    auto* repo = d->repository;
    if (repo == nullptr) {
        return ResultBase(QStringLiteral("Git repository is unavailable."));
    }

    repo->checkStatus();
    if (repo->modifiedFileCount() <= 0) {
        return ResultBase();
    }

    auto* account = repo->account();
    if (account == nullptr || !account->isValid()) {
        return ResultBase(QStringLiteral("Git account is not configured. Please set your name and email in CaveWhere."));
    }

    const QString commitSubject = subject.trimmed().isEmpty()
            ? defaultCommitSubject(QStringLiteral("Save"))
            : subject.trimmed();
    const QString commitDescription = description.trimmed().isEmpty()
            ? defaultCommitDescription()
            : description.trimmed();

    try {
        repo->commitAll(commitSubject, commitDescription);
        repo->checkStatus();
    } catch (const std::exception& error) {
        return ResultBase(QString::fromUtf8(error.what()));
    }

    return ResultBase();
}

bool cwSaveLoad::requiresProviderCredentials() const
{
    const QUrl remote = d->repository->remoteUrl();
    return remote.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
}

void cwSaveLoad::setAuthProvider(cwRemoteAuthProvider* provider)
{
    if (m_authProvider == provider) {
        return;
    }
    if (m_authProvider) {
        disconnect(m_authProvider, &cwRemoteAuthProvider::accessTokenChanged,
                   this, nullptr);
    }
    m_authProvider = provider;
    auto updateCredentials = [this]() {
        const QString token = m_authProvider
                ? m_authProvider->accessToken()
                : QString{};
        d->repository->setCredentials(QQuickGit::GitCredentials{token});
    };
    if (provider) {
        connect(provider, &cwRemoteAuthProvider::accessTokenChanged, this, updateCredentials);
    }
    updateCredentials();
}

namespace {

// Surface a phased reconcile operation's live progress on `deferred` as each
// phase runs: the network prepare (pull/checkout + LFS hydration) during
// prepareFuture, then the push during finalizeFuture. Tracked in sequence —
// tracking finalize eagerly would reset the bar to its not-yet-started empty
// range mid-prepare — so finalize is tracked only once reconcile completes,
// immediately before it runs. Shared by sync() and gitOperationAndReconcile()
// so every prepare/reconcile/finalize path surfaces progress the same way.
void trackPhasedProgress(QObject* context,
                         const std::shared_ptr<AsyncFuture::Deferred<ResultBase>>& deferred,
                         const QFuture<ResultBase>& prepareFuture,
                         const QFuture<ResultBase>& reconcileFuture,
                         const QFuture<ResultBase>& finalizeFuture)
{
    deferred->track(prepareFuture);
    AsyncFuture::observe(reconcileFuture).context(context, [deferred, finalizeFuture]() {
        deferred->track(finalizeFuture);
    });
}

} // namespace

QFuture<Monad::ResultBase> cwSaveLoad::sync()
{
    d->lastSyncReport.reset();

    if (!syncEnabled()) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Project sync is disabled in metadata.")));
    }

    if (d->projectFileName.isEmpty()) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Project has no filename.")));
    }

    auto* repo = d->repository;
    if (repo == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Git repository is unavailable.")));
    }

    const QString repoPath = repo->directory().absolutePath();
    const quint64 syncGeneration = d->operationGeneration;

    // A pull writes a teammate's edits straight into the working tree,
    // where the watcher sees them. cwProject re-asserts the dirty bit from
    // the sync report when the sync ends, so a watcher event arriving after
    // that would contradict a decision made with the full picture.
    d->selfWrite.begin();

    auto syncDeferred = std::make_shared<AsyncFuture::Deferred<ResultBase>>();
    auto scheduleAttempt = std::make_shared<std::function<void(int)>>();
    // Preserved across retries so each sync report's diff spans from the
    // very first attempt's pre-pull HEAD to the current attempt's post-pull
    // HEAD. Without this, retries after a pull that merged remote changes
    // see an empty diff and skip reconciling those already-merged changes.
    auto firstAttemptBeforeHead = std::make_shared<std::optional<QString>>();

    *scheduleAttempt = [this, repo, repoPath, syncGeneration, syncDeferred, scheduleAttempt, firstAttemptBeforeHead](int retryCount) {
        if (syncDeferred->future().isFinished()) {
            return;
        }

        if (d->operationGeneration != syncGeneration || d->retiring) {
            syncDeferred->complete(ResultBase(QStringLiteral("Operation canceled.")));
            return;
        }

        auto attemptState = std::make_shared<ReconcileAttemptState>();

        auto saveFlushFuture = d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::SaveFlush, [this]() {
            return saveFlushImpl();
        });

        auto syncPrepareFuture = d->enqueueOperation(
                    this,
                    cwSaveLoadPrivate::Operation::Type::SyncProject,
                    [this, repo, repoPath, syncGeneration, saveFlushFuture, attemptState, firstAttemptBeforeHead]() -> QFuture<ResultBase> {
            Q_ASSERT(saveFlushFuture.isFinished());
            const auto saveFlushResult = saveFlushFuture.result();
            if (saveFlushResult.hasError()) {
                return AsyncFuture::completed(saveFlushResult);
            }

            if (d->operationGeneration != syncGeneration || d->retiring) {
                return AsyncFuture::completed(ResultBase(QStringLiteral("Operation canceled.")));
            }

            auto commitResult = commitProjectChanges(defaultCommitSubject(QStringLiteral("Sync")),
                                                     defaultCommitDescription());
            if (commitResult.hasError()) {
                return AsyncFuture::completed(commitResult);
            }
            const auto postCommitHeadResult = QQuickGit::GitRepository::headCommitOid(repoPath);

            if (!hasRemoteConfigured(repo)) {
                return AsyncFuture::completed(
                            ResultBase(QStringLiteral("No git remote is configured for this project.")));
            }

            const auto beforeHeadResult = QQuickGit::GitRepository::headCommitOid(repoPath);
            if (beforeHeadResult.hasError()) {
                return AsyncFuture::completed(ResultBase(beforeHeadResult.errorMessage()));
            }
            if (!firstAttemptBeforeHead->has_value()) {
                *firstAttemptBeforeHead = beforeHeadResult.value();
            }
            const QString beforeHead = **firstAttemptBeforeHead;

            auto beforeSnapshotFuture = QtConcurrent::run([repoPath]() {
                return captureLfsSnapshot(repoPath);
            });

            auto pullFuture = repo->pullRebaseOrMerge();
            return AsyncFuture::observe(pullFuture)
                    .context(this, [this, repoPath, beforeHead, beforeSnapshotFuture, pullFuture, syncGeneration, attemptState]() -> QFuture<ResultBase> {
                if (d->operationGeneration != syncGeneration || d->retiring) {
                    return AsyncFuture::completed(ResultBase(QStringLiteral("Operation canceled.")));
                }

                const auto pullResult = pullFuture.result();
                if (pullResult.hasError()) {
                    return AsyncFuture::completed(ResultBase(pullResult.errorMessage(), pullResult.errorCode()));
                }

                const auto pullState = toPullState(pullResult.value().state());
                if (pullState == SyncReport::PullState::MergeConflicts) {
                    return AsyncFuture::completed(ResultBase(QStringLiteral("Merge Conflicts need to be resolved")));
                }

                // If the .cwproj descriptor was renamed by the pull, update the
                // in-memory filename before loading — otherwise loadProject will fail
                // because the old filename no longer exists on disk.
                updateFileNameFromSingleCwproj(repoPath);

                const auto pulledProjectResult = cwSaveLoad::loadProject(d->projectFileName);
                if (pulledProjectResult.hasError()) {
                    return AsyncFuture::completed(ResultBase(pulledProjectResult.errorMessage(),
                                                             pulledProjectResult.errorCode()));
                }

                const int pulledVersion = pulledProjectResult.value().maxFileVersion;
                const int supportedVersion = cwRegionIOTask::protoVersion();
                if (pulledVersion > supportedVersion) {
                    return AsyncFuture::completed(ResultBase(
                                                      QStringLiteral("Project file version %1 is newer than supported version %2.")
                                                      .arg(pulledVersion)
                                                      .arg(supportedVersion),
                                                      static_cast<int>(SyncErrorCode::IncompatibleProjectVersion)));
                }

                const auto afterHeadResult = QQuickGit::GitRepository::headCommitOid(repoPath);
                if (afterHeadResult.hasError()) {
                    return AsyncFuture::completed(ResultBase(afterHeadResult.errorMessage()));
                }
                const QString afterHead = afterHeadResult.value();
                attemptState->planEpoch = d->modelMutationEpoch;
                attemptState->localMutationPlanEpoch = d->localMutationEpoch;

                auto reportFuture = QtConcurrent::run([repoPath, beforeHead, afterHead, pullState, beforeSnapshotFuture]() {
                    return buildSyncReport(repoPath,
                                           beforeHead,
                                           afterHead,
                                           pullState,
                                           beforeSnapshotFuture.result());
                });

                return AsyncFuture::observe(reportFuture)
                        .context(this, [this, reportFuture, attemptState, syncGeneration]() -> ResultBase {
                    if (d->operationGeneration != syncGeneration || d->retiring) {
                        return ResultBase(QStringLiteral("Operation canceled."));
                    }

                    attemptState->report = reportFuture.result();
                    d->lastSyncReport = attemptState->report;
                    if (!attemptState->report.has_value()) {
                        return ResultBase(QStringLiteral("Sync report is unavailable."));
                    }

                    return ResultBase();
                })
                        .future();
            })
                    .future();
        });

        auto reconcileFuture = enqueueReconcilePhase(syncPrepareFuture,
                                                     syncGeneration,
                                                     attemptState,
                                                     ReconcileApplyMode::Merge);
        auto finalizeFuture = enqueueFinalizePhase(reconcileFuture,
                                                   syncGeneration,
                                                   repo,
                                                   attemptState,
                                                   FinalizeMode::SyncPush);

        trackPhasedProgress(this, syncDeferred, syncPrepareFuture, reconcileFuture, finalizeFuture);

        AsyncFuture::observe(finalizeFuture)
                .context(this, [this, finalizeFuture, retryCount, scheduleAttempt, syncDeferred]() {
            const auto attemptResult = finalizeFuture.result();
            d->remoteApplyGuard.end();
            if (!attemptResult.hasError()) {
                syncDeferred->complete(attemptResult);
                return;
            }

            QString retryReason;
            if (isPushRejectedByRemoteAdvance(attemptResult)) {
                retryReason = QStringLiteral("remote advanced during push");
            } else if (attemptResult.errorCodeTo<SyncErrorCode>() == SyncErrorCode::RetryEpochChanged) {
                retryReason = QStringLiteral("model changed before reconcile apply");
            }

            if (retryReason.isEmpty()) {
                if (attemptResult.errorCode() == static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed)) {
                    syncDeferred->complete(ResultBase(attemptResult.errorMessage(),
                                                      static_cast<int>(SyncErrorCode::HttpAuthFailed)));
                } else {
                    syncDeferred->complete(attemptResult);
                }
                return;
            }

            if (retryCount >= SyncMaxRetriesPerSession) {
                syncDeferred->complete(syncRetryLimitResult(retryReason));
                return;
            }

            (*scheduleAttempt)(retryCount + 1);
        });
    };

    // Retries re-enter scheduleAttempt, so the window closes on the
    // deferred rather than on any one attempt.
    AsyncFuture::observe(syncDeferred->future()).context(this, [this]() {
        d->selfWrite.end();
    });

    (*scheduleAttempt)(0);

    return syncDeferred->future();
}

QFuture<Monad::ResultBase> cwSaveLoad::gitOperationAndReconcile(const QString& operationLabel,
                                                                const GitOperationFn& gitOp)
{
    d->lastSyncReport.reset();

    if (!syncEnabled()) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Project sync is disabled in metadata.")));
    }

    if (d->projectFileName.isEmpty()) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Project has no filename.")));
    }

    auto* repo = d->repository;
    if (repo == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Git repository is unavailable.")));
    }

    const QString repoPath = repo->directory().absolutePath();
    const quint64 syncGeneration = d->operationGeneration;
    auto attemptState = std::make_shared<ReconcileAttemptState>();

    // Checkout, branch reset, and restore-to-commit all rewrite the working
    // tree; see the window in sync().
    d->selfWrite.begin();

    auto saveFlushFuture = d->enqueueOperation(this, cwSaveLoadPrivate::Operation::Type::SaveFlush, [this]() {
        return saveFlushImpl();
    });

    auto checkoutPrepareFuture = d->enqueueOperation(
                this,
                cwSaveLoadPrivate::Operation::Type::SyncProject,
                [this, repo, repoPath, operationLabel, gitOp, syncGeneration, saveFlushFuture, attemptState]() -> QFuture<ResultBase> {
        Q_ASSERT(saveFlushFuture.isFinished());
        const auto saveFlushResult = saveFlushFuture.result();
        if (saveFlushResult.hasError()) {
            return AsyncFuture::completed(saveFlushResult);
        }

        if (d->operationGeneration != syncGeneration || d->retiring) {
            return AsyncFuture::completed(ResultBase(QStringLiteral("Operation canceled.")));
        }

        repo->checkStatus();
        if (repo->modifiedFileCount() > 0) {
            return AsyncFuture::completed(ResultBase(
                                              QStringLiteral("Working directory must be clean before checkout and reconcile.")));
        }

        const auto beforeHeadResult = QQuickGit::GitRepository::headCommitOid(repoPath);
        if (beforeHeadResult.hasError()) {
            return AsyncFuture::completed(ResultBase(beforeHeadResult.errorMessage()));
        }
        const QString beforeHead = beforeHeadResult.value();

        auto beforeSnapshotFuture = QtConcurrent::run([repoPath]() {
            return captureLfsSnapshot(repoPath);
        });

        auto gitFuture = gitOp(repo);
        return AsyncFuture::observe(gitFuture)
                .context(this, [this, repoPath, operationLabel, beforeHead, beforeSnapshotFuture, gitFuture, syncGeneration, attemptState]() -> QFuture<ResultBase> {
            if (d->operationGeneration != syncGeneration || d->retiring) {
                return AsyncFuture::completed(ResultBase(QStringLiteral("Operation canceled.")));
            }

            const auto checkoutResult = gitFuture.result();
            if (checkoutResult.hasError()) {
                return AsyncFuture::completed(checkoutResult);
            }

            // If the .cwproj descriptor was renamed by the operation, update the
            // in-memory filename before loading — otherwise loadProject will fail
            // because the old filename no longer exists on disk.
            updateFileNameFromSingleCwproj(repoPath);

            const auto checkedOutProjectResult = cwSaveLoad::loadProject(d->projectFileName);
            if (checkedOutProjectResult.hasError()) {
                return AsyncFuture::completed(ResultBase(checkedOutProjectResult.errorMessage(),
                                                         checkedOutProjectResult.errorCode()));
            }

            const auto afterHeadResult = QQuickGit::GitRepository::headCommitOid(repoPath);
            if (afterHeadResult.hasError()) {
                return AsyncFuture::completed(ResultBase(afterHeadResult.errorMessage()));
            }
            const QString afterHead = afterHeadResult.value();
            attemptState->planEpoch = d->modelMutationEpoch;
            attemptState->localMutationPlanEpoch = d->localMutationEpoch;

            auto reportFuture = QtConcurrent::run([repoPath, beforeHead, afterHead, beforeSnapshotFuture]() {
                return buildSyncReport(repoPath,
                                       beforeHead,
                                       afterHead,
                                       SyncReport::PullState::Unknown,
                                       beforeSnapshotFuture.result());
            });

            return AsyncFuture::observe(reportFuture)
                    .context(this, [this, operationLabel, reportFuture, attemptState, syncGeneration]() -> ResultBase {
                if (d->operationGeneration != syncGeneration || d->retiring) {
                    return ResultBase(QStringLiteral("Operation canceled."));
                }

                attemptState->report = reportFuture.result();
                if (attemptState->report.has_value()) {
                    attemptState->report->diagnostics.append(operationLabel);
                }
                d->lastSyncReport = attemptState->report;
                if (!attemptState->report.has_value()) {
                    return ResultBase(QStringLiteral("Sync report is unavailable."));
                }

                return ResultBase();
            })
                    .future();
        })
                .future();
    });

    auto reconcileFuture = enqueueReconcilePhase(checkoutPrepareFuture,
                                                 syncGeneration,
                                                 attemptState,
                                                 ReconcileApplyMode::TargetCommitWins);
    auto finalizeFuture = enqueueFinalizePhase(reconcileFuture,
                                               syncGeneration,
                                               repo,
                                               attemptState,
                                               FinalizeMode::CheckoutLocal);

    auto checkoutDeferred = std::make_shared<AsyncFuture::Deferred<ResultBase>>();
    trackPhasedProgress(this, checkoutDeferred, checkoutPrepareFuture, reconcileFuture, finalizeFuture);
    AsyncFuture::observe(finalizeFuture)
            .context(this, [this, finalizeFuture, checkoutDeferred]() {
        d->remoteApplyGuard.end();
        d->selfWrite.end();
        checkoutDeferred->complete(finalizeFuture.result());
    });

    if (d->futureToken.isValid()) {
        d->futureToken.addJob(checkoutDeferred->future(),
                              operationLabel);
    }

    return checkoutDeferred->future();
}

QFuture<Monad::ResultBase> cwSaveLoad::resetBranchAndReconcile(const QString& refSpec, BranchResetMode resetMode)
{
    const QString normalizedRefSpec = refSpec.trimmed();
    if (normalizedRefSpec.isEmpty()) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Checkout ref is empty.")));
    }

    return gitOperationAndReconcile(
                QStringLiteral("checkout reconcile target: %1").arg(normalizedRefSpec),
                [normalizedRefSpec, resetMode](QQuickGit::GitRepository* repo) -> QFuture<Monad::ResultBase> {
        const QString currentBranch = repo->headBranchName();
        if (currentBranch.isEmpty()) {
            return AsyncFuture::completed(Monad::ResultBase(
                                              QStringLiteral("Cannot reset branch: HEAD is detached.")));
        }

        QQuickGit::GitRepository::ResetMode mode = QQuickGit::GitRepository::ResetMode::Hard;
        if (resetMode == BranchResetMode::Soft) {
            mode = QQuickGit::GitRepository::ResetMode::Soft;
        } else if (resetMode == BranchResetMode::Mixed) {
            mode = QQuickGit::GitRepository::ResetMode::Mixed;
        }
        return repo->reset(normalizedRefSpec, mode);
    });
}

QFuture<Monad::ResultBase> cwSaveLoad::resetBranchAndReconcile(const QString& refSpec, int resetMode)
{
    return resetBranchAndReconcile(refSpec, static_cast<BranchResetMode>(resetMode));
}

QFuture<Monad::ResultBase> cwSaveLoad::checkoutAndReconcile(const QString& refSpec, int checkoutMode)
{
    return resetBranchAndReconcile(refSpec, static_cast<BranchResetMode>(checkoutMode));
}

QFuture<Monad::ResultBase> cwSaveLoad::restoreToCommitAndReconcile(const QString& targetSha)
{
    const QString normalizedSha = targetSha.trimmed();
    if (normalizedSha.isEmpty()) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("Restore target SHA is empty.")));
    }

    return gitOperationAndReconcile(
                QStringLiteral("restore to commit: %1").arg(normalizedSha),
                [normalizedSha](QQuickGit::GitRepository* repo) {
        return repo->restoreToCommit(normalizedSha);
    });
}

std::optional<cwSaveLoad::SyncReport> cwSaveLoad::lastSyncReport() const
{
    return d->lastSyncReport;
}

QList<cwError> cwSaveLoad::lastLoadErrors() const
{
    return d->lastLoadErrors;
}

int cwSaveLoad::lastLoadMaxFileVersion() const
{
    return d->lastLoadMaxFileVersion;
}

QFuture<Monad::Result<cwSaveLoad::ReconcileExternalResult>> cwSaveLoad::reconcileExternalImpl(const SyncReport& report,
                                                                                              quint64 syncGeneration,
                                                                                              quint64 planEpoch,
                                                                                              ReconcileApplyMode applyMode)
{
    const auto mergeOutcomeToString = [](cwReconcileMergeResult::Outcome outcome) {
        switch (outcome) {
        case cwReconcileMergeResult::Outcome::NotApplicable:
            return QStringLiteral("NotApplicable");
        case cwReconcileMergeResult::Outcome::Applied:
            return QStringLiteral("Applied");
        case cwReconcileMergeResult::Outcome::RequiresFullReload:
            return QStringLiteral("RequiresFullReload");
        }
        return QStringLiteral("Unknown");
    };

    const bool needsApply = !report.changedPaths.isEmpty() || report.beforeHead != report.afterHead;
    if (!needsApply) {
        ReconcileExternalResult result;
        result.outcome = ReconcileExternalResult::Outcome::NoOp;
        return AsyncFuture::completed(Monad::Result<ReconcileExternalResult>(result));
    }

    auto loadFuture = cwSaveLoad::loadAll(d->projectFileName);
    return AsyncFuture::observe(loadFuture)
            .context(this, [this, loadFuture, report, syncGeneration, planEpoch, applyMode, mergeOutcomeToString]() -> Monad::Result<ReconcileExternalResult> {
        const auto loadResult = loadFuture.result();
        if (loadResult.hasError()) {
            return Monad::Result<ReconcileExternalResult>(loadResult.errorMessage(), loadResult.errorCode());
        }

        if (d->operationGeneration != syncGeneration || d->retiring) {
            return Monad::Result<ReconcileExternalResult>(QStringLiteral("Operation canceled."));
        }

        if (d->modelMutationEpoch != planEpoch) {
            return Monad::Result<ReconcileExternalResult>(
                        QStringLiteral("Sync plan is stale because the model changed before apply."),
                        static_cast<int>(SyncErrorCode::RetryEpochChanged));
        }

        auto* region = d->m_regionTreeModel->cavingRegion();
        if (region == nullptr) {
            return Monad::Result<ReconcileExternalResult>(QStringLiteral("No caving region is available for reconcile."));
        }

        d->remoteApplyGuard.begin(d->localMutationEpoch);

        const bool previousSaveEnabled = d->saveEnabled;
        disconnectTreeModel();
        setSaveEnabled(false);
        auto reconnectGuard = qScopeGuard([this, previousSaveEnabled]() {
            setSaveEnabled(previousSaveEnabled);
            connectTreeModel();
            // loadAll never enumerates "GIS Layers/", so loadData carries
            // nothing about LAZ layers for a merge handler to diff — the model
            // would keep a row whose file is gone, and its stale point cloud,
            // until something else happened to rescan. Runs after
            // connectTreeModel so the row handlers are live to release each
            // removed layer's m_objectStates entry.
            rescanLazLayersAfterGitWrite();
        });

        const auto& loadData = loadResult.value();
        d->seedObjectStatesFromLoadedData(this, loadData.region, loadData.metadata.dataRoot);
        const auto previousMetadata = d->projectMetadata;
        d->projectMetadata = loadData.metadata;
        d->pendingIdentityRepairSave = loadData.identityRepair.required;

        bool modelMutated = false;
        bool requiresPersistence = false;
        bool persistNoteDescriptors = false;
        bool persistLiDARNoteDescriptors = false;
        QString reconcileDiagnostic;
        QStringList mergeDiagnostics;
        QStringList restoreDiagnostics;
        SyncReport handlerReport = report;
        const cwReconcileMergeContext mergeContext {
            this,
            region,
                    &loadData,
                    &handlerReport,
                    applyMode == ReconcileApplyMode::TargetCommitWins
                    ? cwReconcileApplyMode::TargetCommitWins
                    : cwReconcileApplyMode::Merge,
                    projectRootDir()
        };
        const cwReconcileMergeResult mergeResult = cwSyncMergeRegistry::instance().reconcile(mergeContext);
        const bool fullReloadApplied = (mergeResult.outcome != cwReconcileMergeResult::Outcome::Applied);
        if (!fullReloadApplied) {
            // A subtree a local change won whole is written back only once the registry
            // settled on Applied, for the same reason as the orphan removals below: a
            // later handler's full reload discards this result, and files already written
            // would outlive that decision while the model went back to the loaded data.
            for (const cwRestoreFilesFromCommit& restore : mergeResult.filesToRestore) {
                const auto restoreOutcome = restoreFilesFromCommit(projectRootDir(),
                                                                   restore.commit,
                                                                   restore.relativePaths,
                                                                   d->repository != nullptr
                                                                   ? d->repository->lfsStore()
                                                                   : nullptr);
                restoreDiagnostics.append(QStringLiteral("restore from %1 wrote %2 files")
                                          .arg(restore.commit)
                                          .arg(restoreOutcome.writtenFileCount));
                if (!restoreOutcome.skippedPaths.isEmpty()) {
                    restoreDiagnostics.append(QStringLiteral("restore from %1 could not write: %2")
                                              .arg(restore.commit,
                                                   restoreOutcome.skippedPaths.join(QStringLiteral(", "))));
                }
            }

            // Orphan removals are queued only once the registry settled on Applied: a
            // handler that reported one may still be overruled by a later handler's
            // full reload, and a queued recursive delete would outlive that decision.
            for (const QString& orphanDir : mergeResult.orphanDirectoriesToRemove) {
                enqueueOrphanDirectoryCleanup(orphanDir);
            }

            for (QObject* object : mergeResult.objectsPathReady) {
                if (object != nullptr) {
                    emit objectPathReady(object);
                }
            }

            modelMutated = mergeResult.modelMutated;

            // For fast-forward pulls, rebases, and checkouts (Unknown), git already
            // placed all project files at the correct on-disk locations. Individual
            // handlers may report diskAlreadySynchronized=false because they don't
            // inspect pullState, so override that here. Only conflict cleanup forces
            // persistence regardless of pullState.
            const bool gitPlacedAllFiles =
                    report.pullState == SyncReport::PullState::FastForward
                    || report.pullState == SyncReport::PullState::Rebased
                    || report.pullState == SyncReport::PullState::Unknown;
            const bool effectiveDiskAlreadySynchronized =
                    mergeResult.diskAlreadySynchronized || gitPlacedAllFiles;

            requiresPersistence = (mergeResult.modelMutated && !effectiveDiskAlreadySynchronized)
                    || mergeResult.pendingConflictCleanup;
            persistNoteDescriptors = mergeResult.persistNoteDescriptors;
            persistLiDARNoteDescriptors = mergeResult.persistLiDARNoteDescriptors;
            reconcileDiagnostic = QStringLiteral("reconcile handler %1 applied (%2)")
                    .arg(mergeResult.handlerName,
                         modelMutated
                         ? QStringLiteral("mutated")
                         : QStringLiteral("projection-only"));
        } else {
            region->setData(loadData.region);
            seedStampedVersion();
            modelMutated = true;
            // Full-reload fallback applies merged commit content from disk directly into
            // the model. Unless identity repair is required, there is nothing to persist.
            requiresPersistence = false;
            persistNoteDescriptors = false;
            persistLiDARNoteDescriptors = false;

            if (mergeResult.outcome == cwReconcileMergeResult::Outcome::RequiresFullReload) {
                reconcileDiagnostic = QStringLiteral("reconcile fallback to full reload (%1)")
                        .arg(mergeResult.fallbackReason);
            } else {
                reconcileDiagnostic = QStringLiteral("reconcile fallback to full reload (no handler matched changed paths)");
            }
        }

        mergeDiagnostics = mergeResult.diagnostics;
        mergeDiagnostics.append(restoreDiagnostics);

        if (!reconcileDiagnostic.isEmpty() && d->lastSyncReport.has_value()) {
            d->lastSyncReport->diagnostics.append(reconcileDiagnostic);
        }
        if (!mergeDiagnostics.isEmpty() && d->lastSyncReport.has_value()) {
            d->lastSyncReport->diagnostics.append(mergeDiagnostics);
        }

        if (modelMutated && fullReloadApplied) {
            d->resetObjectStates(this);
        }
        if (modelMutated) {
            ++d->modelMutationEpoch;
        }

        if (previousMetadata.dataRoot != d->projectMetadata.dataRoot
                || previousMetadata.syncEnabled != d->projectMetadata.syncEnabled) {
            emit dataRootChanged();
        }

        ReconcileExternalResult reconcileResult;
        reconcileResult.outcome = (requiresPersistence || d->pendingIdentityRepairSave)
                ? ReconcileExternalResult::Outcome::MutatedRequiresPrePushPersistence
                : ReconcileExternalResult::Outcome::NoOp;
        reconcileResult.persistNoteDescriptors = persistNoteDescriptors;
        reconcileResult.persistLiDARNoteDescriptors = persistLiDARNoteDescriptors;
        return Monad::Result<ReconcileExternalResult>(reconcileResult);
    })
            .future();
}

QFuture<void> cwSaveLoad::retire()
{
    if (!d->retiring) {
        d->retiring = true;
        d->cancelPendingOperations(QStringLiteral("Project is retiring."));
        setSaveEnabled(false);
        disconnectTreeModel();
        d->m_regionTreeModel->setCavingRegion(nullptr);

        auto saveJobsFuture = completeSaveJobs();

        QFuture<void> retireFuture;
        if (!d->m_ownedTempDirs.isEmpty()) {
            const QStringList capturedTempDirs = d->m_ownedTempDirs;
            d->m_ownedTempDirs.clear();  // Prevent destructor double-cleanup
            retireFuture = AsyncFuture::observe(saveJobsFuture)
                .context(this, [this, capturedTempDirs]() {
                    return QtConcurrent::run(&d->m_saveThreadPool, [capturedTempDirs]() {
                        for (const QString& dir : capturedTempDirs) {
                            removeTemporaryProjectDir(dir);
                        }
                    });
                }).future();
        } else {
            retireFuture = saveJobsFuture;
        }

        d->retireFuture = retireFuture;
        d->futureToken.addJob(d->retireFuture,
                              QStringLiteral("Finishing saves"));

        AsyncFuture::observe(d->retireFuture)
                .context(this, [this]() {
            deleteLater();
        });
    }

    // Shielded per caller: retireFuture chains on the shared save-job
    // drain, and the tempdir-cleanup + deleteLater observers watch the
    // same future, so a consumer cancel must reach neither.
    return AsyncFuture::shield(d->retireFuture);
}
