#include "cwCaveSyncMergeHandler.h"

#include "cwCave.h"
#include "cwCaveMergeApplier.h"
#include "cwCaveMergePlanBuilder.h"
#include "cwCavingRegion.h"
#include "GitRepository.h"
#include "cavewhere.pb.h"
#include "google/protobuf/util/json_util.h"

#include <QFile>
#include <QHash>
#include <QUuid>
#include <QSet>

#include <optional>

namespace {

QString normalizeSyncPath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QUuid uuidFromProtoString(const std::string& uuidString)
{
    if (uuidString.empty()) {
        return QUuid();
    }

    const QUuid uuid(QString::fromStdString(uuidString));
    return uuid.isNull() ? QUuid() : uuid;
}

bool parseProtoCave(const QByteArray& content, CavewhereProto::Cave* cave)
{
    if (content.isEmpty() || cave == nullptr) {
        return false;
    }

    if (cave->ParseFromArray(content.constData(), content.size())) {
        return true;
    }

    const std::string jsonPayload(content.constData(),
                                  static_cast<size_t>(content.size()));
    const auto parseStatus = google::protobuf::util::JsonStringToMessage(jsonPayload, cave);
    return parseStatus.ok();
}

std::optional<QUuid> loadCaveIdFromPath(const QDir& repoRoot, const QString& relativeCavePath)
{
    QFile file(repoRoot.absoluteFilePath(relativeCavePath));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }

    CavewhereProto::Cave protoCave;
    if (!parseProtoCave(file.readAll(), &protoCave) || !protoCave.has_id()) {
        return std::nullopt;
    }

    const QUuid id = uuidFromProtoString(protoCave.id());
    return id.isNull() ? std::nullopt : std::make_optional(id);
}

std::optional<std::pair<QUuid, cwCaveData>> loadBaseCaveDataForPath(const QDir& repoRoot,
                                                                     const QString& mergeBaseHead,
                                                                     const QString& relativeCavePath)
{
    if (mergeBaseHead.isEmpty()) {
        return std::nullopt;
    }

    const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(
        repoRoot.absolutePath(),
        mergeBaseHead,
        relativeCavePath);
    if (contentResult.hasError() || contentResult.value().isEmpty()) {
        return std::nullopt;
    }

    CavewhereProto::Cave protoCave;
    if (!parseProtoCave(contentResult.value(), &protoCave) || !protoCave.has_id()) {
        return std::nullopt;
    }

    cwCaveData baseCaveData;
    baseCaveData.id = uuidFromProtoString(protoCave.id());
    if (baseCaveData.id.isNull()) {
        return std::nullopt;
    }

    if (protoCave.has_name()) {
        baseCaveData.name = QString::fromStdString(protoCave.name());
    }

    return std::make_optional(std::make_pair(baseCaveData.id, baseCaveData));
}

} // namespace

QString cwCaveSyncMergeHandler::name() const
{
    return QStringLiteral("cwCaveSyncMergeHandler");
}

cwReconcileMergeResult cwCaveSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
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

    QHash<QUuid, cwCave*> currentCavesById;
    for (cwCave* cave : context.region->caves()) {
        if (cave == nullptr || cave->id().isNull()) {
            continue;
        }
        currentCavesById.insert(cave->id(), cave);
    }

    QHash<QUuid, const cwCaveData*> loadedCavesById;
    for (const cwCaveData& caveData : context.loadData->region.caves) {
        if (!caveData.id.isNull()) {
            loadedCavesById.insert(caveData.id, &caveData);
        }
    }

    QList<cwCave*> changedCurrentCaves;
    QList<const cwCaveData*> changedLoadedCaves;
    QHash<QUuid, cwCaveData> baseCaveById;
    QSet<QUuid> seenCaveIds;

    for (const QString& changedPath : context.report->changedPaths) {
        const QString normalizedPath = normalizeSyncPath(changedPath);
        if (!normalizedPath.endsWith(QStringLiteral(".cwcave"), Qt::CaseInsensitive)) {
            continue;
        }

        const auto baseCaveDataForPath = loadBaseCaveDataForPath(context.repoRoot,
                                                                 context.report->mergeBaseHead,
                                                                 normalizedPath);

        std::optional<QUuid> caveId = loadCaveIdFromPath(context.repoRoot, normalizedPath);
        if (!caveId.has_value() && baseCaveDataForPath.has_value()) {
            caveId = std::make_optional(baseCaveDataForPath->first);
        }

        if (!caveId.has_value()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Unable to resolve changed cave identity from descriptor.");
            return result;
        }

        if (seenCaveIds.contains(*caveId)) {
            continue;
        }
        seenCaveIds.insert(*caveId);

        const auto currentCaveIt = currentCavesById.constFind(*caveId);
        if (currentCaveIt == currentCavesById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("No current cave matches changed cave descriptor id.");
            return result;
        }

        const auto loadedCaveIt = loadedCavesById.constFind(*caveId);
        if (loadedCaveIt == loadedCavesById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Loaded data is missing changed cave payload.");
            return result;
        }

        changedCurrentCaves.append(currentCaveIt.value());
        changedLoadedCaves.append(loadedCaveIt.value());

        if (baseCaveDataForPath.has_value()) {
            baseCaveById.insert(baseCaveDataForPath->first, baseCaveDataForPath->second);
        }
    }

    if (changedCurrentCaves.isEmpty()) {
        return {};
    }

    const auto mergePreparation = cwCaveMergePlanBuilder::build(changedCurrentCaves,
                                                                 changedLoadedCaves,
                                                                 baseCaveById);
    if (mergePreparation.hasError()) {
        cwReconcileMergeResult result;
        result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
        result.handlerName = name();
        result.fallbackReason = mergePreparation.errorMessage().isEmpty()
                                    ? QStringLiteral("Unable to build deterministic cave merge plan.")
                                    : mergePreparation.errorMessage();
        return result;
    }

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();

    QSet<QObject*> objectPathReadySet;
    for (const cwCaveMergePlan& plan : mergePreparation.value().plans) {
        const auto applyResult = cwCaveMergeApplier::applyCaveMergePlan(plan);
        if (applyResult.hasError()) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = applyResult.errorMessage().isEmpty()
                                                  ? QStringLiteral("Unable to apply deterministic cave merge plan.")
                                                  : applyResult.errorMessage();
            return fullReloadResult;
        }

        result.modelMutated = true;
        objectPathReadySet.insert(plan.currentCave);
    }

    result.objectsPathReady.reserve(objectPathReadySet.size());
    for (QObject* object : std::as_const(objectPathReadySet)) {
        result.objectsPathReady.append(object);
    }

    return result;
}
