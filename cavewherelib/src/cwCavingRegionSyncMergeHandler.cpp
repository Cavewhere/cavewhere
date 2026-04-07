#include "cwCavingRegionSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwCavingRegionMergeApplier.h"
#include "cwCavingRegionMergePlanBuilder.h"
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

std::optional<QString> loadBaseProjectName(const QDir& repoRoot,
                                           const QString& mergeBaseHead,
                                           const QString& relativeProjectPath)
{
    if (mergeBaseHead.isEmpty()) {
        return std::nullopt;
    }

    const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(
        repoRoot.absolutePath(),
        mergeBaseHead,
        relativeProjectPath);
    if (contentResult.hasError() || contentResult.value().isEmpty()) {
        return std::nullopt;
    }

    CavewhereProto::Project protoProject;
    if (!parseProjectProto(contentResult.value(), protoProject) || !protoProject.has_name()) {
        return std::nullopt;
    }

    return QString::fromStdString(protoProject.name());
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

    if (context.report->changedPaths.isEmpty()) {
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

    if (changedProjectPaths.isEmpty()) {
        return {};
    }

    // Try each changed .cwproj path to find the one that existed at the merge base.
    // The old (deleted) path from a rename delta is the one that will succeed.
    std::optional<QString> baseName;
    for (const QString& cwprojPath : changedProjectPaths) {
        baseName = loadBaseProjectName(context.repoRoot, context.report->mergeBaseHead, cwprojPath);
        if (baseName.has_value()) {
            break;
        }
    }

    const auto planResult = cwCavingRegionMergePlanBuilder::build(
        context.region,
        &context.loadData->region,
        baseName);

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
    if (context.applyMode == cwReconcileApplyMode::TargetCommitWins
        && !context.loadData->region.name.isEmpty()
        && context.region->name() != context.loadData->region.name) {
        context.region->setName(context.loadData->region.name);
    }

    const bool nameChanged = (context.region->name() != nameBeforeApply);

    // A .cwproj path in changedPaths means a project rename was part of this sync.
    // Git already placed all files at their correct on-disk locations during the
    // pull/merge, so subsequent handlers must not re-write them (that would only
    // bump cavewhereVersion and produce a spurious Sync Reconcile commit).
    if (!changedProjectPaths.isEmpty()) {
        context.gitProjectDirMoved = true;
    }

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
    if (!changedProjectPaths.isEmpty()) {
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
    }

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();
    result.modelMutated = nameChanged;
    result.pendingConflictCleanup = pendingConflictCleanup;
    // Git already renamed all files on disk during pull; no Sync Reconcile commit needed.
    result.diskAlreadySynchronized = nameChanged;
    return result;
}
