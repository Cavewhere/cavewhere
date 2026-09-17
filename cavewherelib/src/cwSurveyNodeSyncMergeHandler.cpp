#include "cwSurveyNodeSyncMergeHandler.h"

#include "cwCave.h"
#include "cwCaveData.h"
#include "cwCaveMergeApplier.h"
#include "cwCaveMergePlanBuilder.h"
#include "cwCavingRegion.h"
#include "cwSaveLoad.h"
#include "cwSurveyNode.h"
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

    const std::string jsonPayload(content.constData(),
                                  static_cast<size_t>(content.size()));
    static const auto parseOptions = [] {
        google::protobuf::util::JsonParseOptions opts;
        opts.ignore_unknown_fields = true;
        return opts;
    }();
    const auto parseStatus = google::protobuf::util::JsonStringToMessage(jsonPayload, cave, parseOptions);
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
    if (protoCave.has_kind()) {
        baseCaveData.kind = cwSurveyNodeKind::fromSavedValue(protoCave.kind());
    }
    if (protoCave.has_read_only()) {
        baseCaveData.readOnly = protoCave.read_only();
    }
    if (protoCave.has_source_id()) {
        baseCaveData.sourceId = uuidFromProtoString(protoCave.source_id());
    }
    if (protoCave.has_source_path()) {
        baseCaveData.sourcePath = QString::fromStdString(protoCave.source_path());
    }

    return std::make_optional(std::make_pair(baseCaveData.id, baseCaveData));
}

//! The node's directory relative to the repository root, e.g.
//! "DataRoot/Kentucky field seasons/nodes/Side Cave".
QString relativeNodeDirectory(const QString& dataRootName, const cwSurveyNode* node)
{
    const QString nodeDir = cwSaveLoad::relativeNodeDir(node->path());
    if (nodeDir.isEmpty()) {
        return QString();
    }
    return normalizeSyncPath(QDir(dataRootName).filePath(nodeDir));
}

//! True when \a relativeDir names a directory at least one segment below the data root.
//! A descriptor sitting at the data root or the repository root composes a directory that
//! must never be handed to a recursive removal.
bool isBelowDataRoot(const QString& relativeDir, const QString& dataRootName)
{
    if (relativeDir.isEmpty() || QDir::isAbsolutePath(relativeDir)) {
        return false;
    }
    const QString prefix = dataRootName + QLatin1Char('/');
    return relativeDir.startsWith(prefix) && relativeDir.size() > prefix.size();
}

//! True when \a absoluteDir holds a node descriptor of its own.
bool holdsNodeDescriptor(const QString& absoluteDir)
{
    return !QDir(absoluteDir).entryList({QStringLiteral("*.cwcave")}, QDir::Files).isEmpty();
}

} // namespace

QString cwSurveyNodeSyncMergeHandler::name() const
{
    return QStringLiteral("cwSurveyNodeSyncMergeHandler");
}

cwReconcileMergeResult cwSurveyNodeSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
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
    const cwSurveyNode* const rootNode = context.region->rootNode();
    if (rootNode == nullptr) {
        return {};
    }
    const QList<cwSurveyNode*> allNodes = rootNode->allNodes();
    for (cwSurveyNode* node : allNodes) {
        auto* cave = qobject_cast<cwCave*>(node);
        if (cave == nullptr || cave->id().isNull()) {
            continue;
        }
        currentCavesById.insert(cave->id(), cave);
    }

    QHash<QUuid, const cwCaveData*> loadedCavesById;
    walkCaveDataTree(context.loadData->region.caves,
                     [&](const cwCaveData& nodeData, const QStringList&) {
        if (!nodeData.id.isNull()) {
            loadedCavesById.insert(nodeData.id, &nodeData);
        }
    });

    QList<cwCave*> changedCurrentCaves;
    QList<const cwCaveData*> changedLoadedCaves;
    QList<QPair<QString, cwCave*>> changedDescriptors;
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

        const auto currentCaveIt = currentCavesById.constFind(*caveId);
        if (currentCaveIt == currentCavesById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("No current cave matches changed cave descriptor id.");
            return result;
        }

        // Every changed descriptor is recorded, including the second path a
        // rename/rename conflict produces for one node: that path is the orphan.
        changedDescriptors.append(qMakePair(normalizedPath, currentCaveIt.value()));

        if (seenCaveIds.contains(*caveId)) {
            continue;
        }
        seenCaveIds.insert(*caveId);

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

    // Clean up orphaned node directories left by rename/rename conflicts.
    // After the merge the winning name is set on each changed node, so a changed
    // descriptor sitting somewhere other than its node's own directory is a candidate.
    // It is removed only when the node's own directory already holds a descriptor, which
    // means this one is the losing duplicate git checked out from the peer. When the
    // node's own directory holds no descriptor, this directory is the only copy of the
    // subtree — a peer moved the node, or git did not follow an ancestor rename — and a
    // later Directory Move job relocates it. Only the descriptor's own directory is ever
    // removed: an ancestor's would take its siblings with it.
    const QString dataRootName = context.dataRootName();
    if (!dataRootName.isEmpty()) {
        QSet<QString> winningNodeDirs;
        for (cwCave* cave : std::as_const(changedCurrentCaves)) {
            const QString nodeDir = relativeNodeDirectory(dataRootName, cave);
            if (!nodeDir.isEmpty()) {
                winningNodeDirs.insert(nodeDir);
            }
        }

        QSet<QString> checkedNodeDirs;
        for (const auto& [changedPath, cave] : std::as_const(changedDescriptors)) {
            const QString orphanDir = QFileInfo(changedPath).dir().path();
            if (!isBelowDataRoot(orphanDir, dataRootName)
                || winningNodeDirs.contains(orphanDir)
                || checkedNodeDirs.contains(orphanDir)) {
                continue;
            }
            checkedNodeDirs.insert(orphanDir);

            // In a normal rename git removes the old descriptor, so the losing file is
            // absent even when the directory structure lingers.
            if (!QFileInfo::exists(context.repoRoot.absoluteFilePath(changedPath))) {
                continue;
            }

            const QString liveNodeDir = relativeNodeDirectory(dataRootName, cave);
            if (liveNodeDir.isEmpty()
                || !holdsNodeDescriptor(context.repoRoot.absoluteFilePath(liveNodeDir))) {
                continue;
            }

            result.orphanDirectoriesToRemove.append(orphanDir);
            result.pendingConflictCleanup = true;
        }
    }

    return result;
}
