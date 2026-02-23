#include "cwTripSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwGlobals.h"
#include "cwRegionLoadTask.h"
#include "cwSaveLoad.h"
#include "cwSyncMergeApplyUtils.h"
#include "cwTrip.h"
#include "cwTripMergeApplier.h"
#include "cwTripMergePlanBuilder.h"
#include "GitRepository.h"
#include "cavewhere.pb.h"
#include "google/protobuf/util/json_util.h"

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QUuid>

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

std::optional<QUuid> loadTripIdFromPath(const QDir& repoRoot, const QString& relativeTripPath)
{
    QFile file(repoRoot.absoluteFilePath(relativeTripPath));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray content = file.readAll();
    if (content.isEmpty()) {
        return std::nullopt;
    }

    CavewhereProto::Trip protoTrip;
    if (!protoTrip.ParseFromArray(content.constData(), content.size())) {
        const std::string jsonPayload(content.constData(),
                                      static_cast<size_t>(content.size()));
        const auto parseStatus = google::protobuf::util::JsonStringToMessage(jsonPayload, &protoTrip);
        if (!parseStatus.ok()) {
            return std::nullopt;
        }
    }

    if (!protoTrip.has_id()) {
        return std::nullopt;
    }

    const QUuid id = uuidFromProtoString(protoTrip.id());
    return id.isNull() ? std::nullopt : std::make_optional(id);
}

std::optional<std::pair<QUuid, cwTripData>> loadBaseTripDataForPath(const QDir& repoRoot,
                                                                     const QString& mergeBaseHead,
                                                                     const QString& relativeTripPath)
{
    if (mergeBaseHead.isEmpty()) {
        return std::nullopt;
    }

    const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(
        repoRoot.absolutePath(),
        mergeBaseHead,
        relativeTripPath);
    if (contentResult.hasError() || contentResult.value().isEmpty()) {
        return std::nullopt;
    }

    const auto tripResult = cwSaveLoad::loadTrip(contentResult.value());
    if (tripResult.hasError()) {
        return std::nullopt;
    }
    const cwTripData baseTripData = tripResult.value();
    if (baseTripData.id.isNull()) {
        return std::nullopt;
    }

    return std::make_optional(std::make_pair(baseTripData.id, baseTripData));
}

} // namespace

QString cwTripSyncMergeHandler::name() const
{
    return QStringLiteral("cwTripSyncMergeHandler");
}

cwReconcileMergeResult cwTripSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
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

    QHash<QUuid, cwTrip*> currentTripsById;
    for (cwCave* cave : context.region->caves()) {
        if (cave == nullptr) {
            continue;
        }

        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr || trip->id().isNull()) {
                continue;
            }
            currentTripsById.insert(trip->id(), trip);
        }
    }

    QHash<QUuid, const cwTripData*> loadedTripsById;
    for (const cwCaveData& caveData : context.loadData->region.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            if (!tripData.id.isNull()) {
                loadedTripsById.insert(tripData.id, &tripData);
            }
        }
    }

    QList<cwTrip*> changedCurrentTrips;
    QList<const cwTripData*> changedLoadedTrips;
    QHash<QUuid, cwTripData> baseTripById;
    QSet<QUuid> seenTripIds;

    for (const QString& changedPath : context.report->changedPaths) {
        const QString normalizedPath = normalizeSyncPath(changedPath);
        if (!normalizedPath.endsWith(QStringLiteral(".cwtrip"), Qt::CaseInsensitive)) {
            continue;
        }

        const auto tripId = loadTripIdFromPath(context.repoRoot, normalizedPath);
        if (!tripId.has_value()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Unable to resolve changed trip identity from descriptor.");
            return result;
        }

        if (seenTripIds.contains(*tripId)) {
            continue;
        }
        seenTripIds.insert(*tripId);

        const auto currentTripIt = currentTripsById.constFind(*tripId);
        if (currentTripIt == currentTripsById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("No current trip matches changed trip descriptor id.");
            return result;
        }

        cwTrip* const currentTrip = currentTripIt.value();

        const auto loadedTripIt = loadedTripsById.constFind(*tripId);
        if (loadedTripIt == loadedTripsById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Loaded data is missing changed trip payload.");
            return result;
        }

        changedCurrentTrips.append(currentTrip);
        changedLoadedTrips.append(loadedTripIt.value());

        const auto baseTripData = loadBaseTripDataForPath(context.repoRoot,
                                                          context.report->mergeBaseHead,
                                                          normalizedPath);
        if (baseTripData.has_value()) {
            qDebug().noquote()
                << QStringLiteral("[TripSyncDebug] base trip resolved path=%1 changedTripId=%2 baseTripId=%3 baseChunkCount=%4")
                       .arg(normalizedPath)
                       .arg(tripId->toString(QUuid::WithoutBraces))
                       .arg(baseTripData->first.toString(QUuid::WithoutBraces))
                       .arg(baseTripData->second.chunks.size());
            baseTripById.insert(baseTripData->first, baseTripData->second);
        } else {
            qDebug().noquote()
                << QStringLiteral("[TripSyncDebug] base trip missing path=%1 changedTripId=%2 mergeBaseHead=%3")
                       .arg(normalizedPath)
                       .arg(tripId->toString(QUuid::WithoutBraces))
                       .arg(context.report->mergeBaseHead);
        }
    }

    if (changedCurrentTrips.isEmpty()) {
        return {};
    }

    const auto mergePreparation = cwTripMergePlanBuilder::build(changedCurrentTrips,
                                                                changedLoadedTrips,
                                                                baseTripById,
                                                                context.applyMode == cwReconcileApplyMode::TargetCommitWins
                                                                    ? cwSyncMergeApplyUtils::ApplyMode::LoadedWins
                                                                    : cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] trip merge preparation changedTrips=%1 loadedTrips=%2 baseTrips=%3 applyMode=%4")
               .arg(changedCurrentTrips.size())
               .arg(changedLoadedTrips.size())
               .arg(baseTripById.size())
               .arg(context.applyMode == cwReconcileApplyMode::TargetCommitWins ? QStringLiteral("LoadedWins")
                                                                                 : QStringLiteral("ThreeWayMerge"));
    if (mergePreparation.hasError()) {
        cwReconcileMergeResult result;
        result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
        result.handlerName = name();
        result.fallbackReason = mergePreparation.errorMessage().isEmpty()
                                    ? QStringLiteral("Unable to build deterministic trip merge plan.")
                                    : mergePreparation.errorMessage();
        return result;
    }
    const cwTripMergePreparation mergePreparationValue = mergePreparation.value();

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();

    for (const cwTripMergePlan& plan : mergePreparationValue.plans) {
        const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
        if (applyResult.hasError()) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = applyResult.errorMessage().isEmpty()
                                                  ? QStringLiteral("Unable to apply deterministic trip merge plan.")
                                                  : applyResult.errorMessage();
            return fullReloadResult;
        }

        result.modelMutated = true;
        result.objectsPathReady.append(plan.currentTrip);
    }

    return result;
}
