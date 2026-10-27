#include "cwCavingRegionSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwCavingRegionMergeApplier.h"
#include "cwCavingRegionMergePlanBuilder.h"
#include "cwProtoUtils.h"
#include "cwSaveLoad.h"
#include "cwSyncPathResolver.h"
#include "GitRepository.h"
#include "cavewhere.pb.h"
#include "google/protobuf/util/json_util.h"

#include <QDir>
#include <QFileInfo>

namespace {

bool parseProjectProto(const QByteArray& content, CavewhereProto::Project& protoProject)
{
    if (content.isEmpty()) {
        return false;
    }
    const std::string jsonPayload(content.constData(), static_cast<size_t>(content.size()));
    static const auto parseOptions = [] {
        google::protobuf::util::JsonParseOptions opts;
        opts.ignore_unknown_fields = true;
        return opts;
    }();
    return google::protobuf::util::JsonStringToMessage(jsonPayload, &protoProject, parseOptions).ok();
}

//! The project file as \a commit holds it, read from the first of
//! \a relativeProjectPaths that exists there. A rename moves the file, so the
//! caller lists every path it may have had.
std::optional<CavewhereProto::Project> loadProjectAtCommit(const QDir& repoRoot,
                                                           const QString& commit,
                                                           const QStringList& relativeProjectPaths)
{
    if (commit.isEmpty()) {
        return std::nullopt;
    }

    for (const QString& relativePath : relativeProjectPaths) {
        const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(
            repoRoot.absolutePath(), commit, relativePath);
        if (contentResult.hasError() || contentResult.value().isEmpty()) {
            continue;
        }

        CavewhereProto::Project protoProject;
        if (parseProjectProto(contentResult.value(), protoProject)) {
            return protoProject;
        }
    }
    return std::nullopt;
}

//! The commit a merge brought in: the second parent of \a mergeCommit.
QString mergedInCommit(const QDir& repoRoot, const QString& mergeCommit)
{
    const auto parentsResult =
        QQuickGit::GitRepository::commitParentOids(repoRoot.absolutePath(), mergeCommit);
    if (parentsResult.hasError() || parentsResult.value().size() < 2) {
        return QString();
    }
    return parentsResult.value().at(1);
}

} // namespace

QString cwCavingRegionSyncMergeHandler::name() const
{
    return QStringLiteral("cwCavingRegionSyncMergeHandler");
}

cwReconcileMergeResult cwCavingRegionSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
{
    if (context.saveLoad == nullptr
        || context.region == nullptr
        || context.loadData == nullptr
        || context.report == nullptr) {
        return {};
    }

    // Collect ALL changed .cwproj descriptor paths. The diff includes both old_file and
    // new_file paths, so a rename produces two entries: the original path (deleted) and
    // the new path (added). A concurrent rename produces even more. We need all of them to:
    //   (a) find the one that existed at the merge base (for three-way name merge), and
    //   (b) detect peer-added descriptors that need cleanup.
    QStringList changedProjectPaths;
    for (const QString& changedPath : context.report->changedPaths) {
        const QString normalizedPath = cwSyncPathResolver::normalizePath(changedPath);
        if (normalizedPath.endsWith(QStringLiteral(".cwproj"), Qt::CaseInsensitive)) {
            changedProjectPaths.append(normalizedPath);
        }
    }

    // A merge commit can leave the project file exactly as our side wrote it
    // (git resolves a conflicting hunk with our lines), so the peer's new source
    // can sit in a project file that is absent from the changed paths.
    const bool mergeCommitCreated =
        context.report->pullState == cwSaveLoad::SyncReport::PullState::MergeCommitCreated;
    if (changedProjectPaths.isEmpty() && !mergeCommitCreated) {
        return {};
    }

    // Try each changed .cwproj path to find the one that existed at the merge base.
    // The old (deleted) path from a rename delta is the one that will succeed.
    QStringList candidateProjectPaths = changedProjectPaths;
    const QString currentProjectPath =
        cwSyncPathResolver::normalizePath(context.repoRoot.relativeFilePath(context.saveLoad->fileName()));
    if (!candidateProjectPaths.contains(currentProjectPath)) {
        candidateProjectPaths.append(currentProjectPath);
    }

    const auto baseProject = loadProjectAtCommit(context.repoRoot,
                                                 context.report->mergeBaseHead,
                                                 candidateProjectPaths);
    std::optional<QString> baseName;
    std::optional<QList<cwExternalSource>> baseSources;
    if (baseProject.has_value()) {
        if (baseProject->has_name()) {
            baseName = QString::fromStdString(baseProject->name());
        }
        baseSources = cwProtoUtils::fromProtoExternalSources(baseProject->external_sources());
    }

    std::optional<QList<cwExternalSource>> theirSources;
    if (mergeCommitCreated) {
        const auto theirProject = loadProjectAtCommit(context.repoRoot,
                                                      mergedInCommit(context.repoRoot, context.report->afterHead),
                                                      candidateProjectPaths);
        if (theirProject.has_value()) {
            theirSources = cwProtoUtils::fromProtoExternalSources(theirProject->external_sources());
        }
    }

    const QList<cwExternalSource> sourcesBeforeApply = context.region->externalSources()->sources();

    const auto planResult = cwCavingRegionMergePlanBuilder::build(
        context.region,
        &context.loadData->region,
        baseName,
        theirSources,
        baseSources);

    if (planResult.hasError()) {
        cwReconcileMergeResult result;
        result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
        result.handlerName = name();
        result.fallbackReason = planResult.errorMessage().isEmpty()
                                    ? QStringLiteral("Unable to build caving region merge plan.")
                                    : planResult.errorMessage();
        return result;
    }

    const QString nameBeforeApply = context.region->name();
    // Capture the old descriptor path before applyMergePlan may change the name.
    // The nameChanged signal handler early-outs in the sync flow (because
    // d->projectMetadata.dataRoot is pre-set from loaded data at line 5952 of
    // reconcileExternalImpl). We must therefore queue the filesystem rename jobs
    // and update the project filename pointer explicitly.
    const QString oldDescriptorPath = context.saveLoad->fileName();

    const auto applyResult = cwCavingRegionMergeApplier::applyRegionMergePlan(planResult.value());
    if (applyResult.hasError()) {
        cwReconcileMergeResult result;
        result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
        result.handlerName = name();
        result.fallbackReason = applyResult.errorMessage().isEmpty()
                                    ? QStringLiteral("Unable to apply caving region merge plan.")
                                    : applyResult.errorMessage();
        return result;
    }

    // When the apply mode is TargetCommitWins (checkout), force the loaded name even
    // if the three-way merge chose the current name. This ensures the project filename
    // is renamed to reflect the checked-out state (e.g. reverting a rename).
    if (context.applyMode == cwReconcileApplyMode::TargetCommitWins) {
        if (!context.loadData->region.name.isEmpty()
            && context.region->name() != context.loadData->region.name) {
            context.region->setName(context.loadData->region.name);
        }
        if (context.region->externalSources()->sources() != context.loadData->region.externalSources) {
            context.region->externalSources()->setSources(context.loadData->region.externalSources);
        }
    }

    const QList<cwExternalSource>& mergedSources = context.region->externalSources()->sources();
    const bool sourcesChanged = (mergedSources != sourcesBeforeApply);
    // The merge kept a source the file on disk lacks, so the project file is
    // written back before the push.
    const bool sourcesDifferFromDisk = (mergedSources != context.loadData->region.externalSources);

    if (changedProjectPaths.isEmpty()) {
        if (!sourcesChanged && !sourcesDifferFromDisk) {
            return {};
        }
        cwReconcileMergeResult result;
        result.outcome = cwReconcileMergeResult::Outcome::Applied;
        result.handlerName = name();
        result.modelMutated = sourcesChanged;
        result.persistProjectFile = sourcesDifferFromDisk;
        return result;
    }

    const bool nameChanged = (context.region->name() != nameBeforeApply);

    // A .cwproj path in changedPaths means a project rename was part of this sync.
    // Git already placed all files at their correct on-disk locations during the
    // pull/merge, so subsequent handlers must not re-write them (that would only
    // bump cavewhereVersion and produce a spurious Sync Reconcile commit).
    context.gitProjectDirMoved = true;

    if (nameChanged) {
        const QString sanitizedName = cwSaveLoad::sanitizeFileName(context.region->name());
        if (!sanitizedName.isEmpty()) {
            const QString newDescriptorPath =
                context.repoRoot.absoluteFilePath(sanitizedName + QStringLiteral(".cwproj"));

            // Update the in-memory project file pointer.
            context.saveLoad->setFileName(newDescriptorPath);

            // Queue the dataRoot directory rename and .cwproj descriptor rename jobs,
            // and rebuild internal object-state paths for the new dataRoot.
            context.saveLoad->enqueueProjectRenameJobs(oldDescriptorPath, newDescriptorPath);
        }
    }

    // When both sides renamed the project differently, git's merge adds the peer's
    // .cwproj + data directory alongside ours. Scan the repo root for any .cwproj
    // files that do not match our current project name and clean them up.
    bool pendingConflictCleanup = false;
    const QString ourSanitizedName = cwSaveLoad::sanitizeFileName(context.region->name());
    const QStringList allCwprojOnDisk =
        context.repoRoot.entryList(QStringList() << QStringLiteral("*.cwproj"), QDir::Files);
    for (const QString& cwprojFile : allCwprojOnDisk) {
        const QString stem = QFileInfo(cwprojFile).completeBaseName();
        if (!stem.isEmpty() && stem != ourSanitizedName) {
            context.saveLoad->enqueueConflictingProjectCleanup(cwprojFile);
            pendingConflictCleanup = true;
        }
    }

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();
    result.modelMutated = nameChanged || sourcesChanged;
    result.persistProjectFile = sourcesDifferFromDisk;
    result.pendingConflictCleanup = pendingConflictCleanup;
    // Git already renamed all files on disk during pull; no Sync Reconcile commit needed.
    result.diskAlreadySynchronized = nameChanged;
    return result;
}
