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

    const QByteArray& content = contentResult.value();
    CavewhereProto::Project protoProject;

    if (!protoProject.ParseFromArray(content.constData(), content.size())) {
        const std::string jsonPayload(content.constData(), static_cast<size_t>(content.size()));
        const auto parseStatus = google::protobuf::util::JsonStringToMessage(jsonPayload, &protoProject);
        if (!parseStatus.ok()) {
            return std::nullopt;
        }
    }

    if (!protoProject.has_name()) {
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

    // Find a changed .cwproj descriptor in the changed paths.
    QString changedProjectPath;
    for (const QString& changedPath : context.report->changedPaths) {
        const QString normalizedPath = cwSyncPathResolver::normalizePath(changedPath);
        if (normalizedPath.endsWith(QStringLiteral(".cwproj"), Qt::CaseInsensitive)) {
            changedProjectPath = normalizedPath;
            break;
        }
    }

    if (changedProjectPath.isEmpty()) {
        return {};
    }

    const std::optional<QString> baseName = loadBaseProjectName(
        context.repoRoot,
        context.report->mergeBaseHead,
        changedProjectPath);

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

    const bool nameChanged = (context.region->name() != nameBeforeApply);

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

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();
    result.modelMutated = nameChanged;
    return result;
}
