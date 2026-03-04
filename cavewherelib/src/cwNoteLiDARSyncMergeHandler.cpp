#include "cwNoteLiDARSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwGlobals.h"
#include "cwNoteLiDARMergeApplier.h"
#include "cwNoteLiDARMergePlanBuilder.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwTrip.h"
#include "cwSaveLoad.h"
#include "cwSyncPathResolver.h"
#include "GitRepository.h"
#include "cwSyncMergeApplyUtils.h"

#include <QHash>
#include <QUuid>

#include <optional>

namespace {
std::optional<std::pair<QUuid, cwNoteLiDARData>> loadBaseLiDARDataForNote(
    const QDir& repoRoot,
    const QString& mergeBaseHead,
    const QString& relativeNotePath)
{
    if (mergeBaseHead.isEmpty()) {
        return std::nullopt;
    }

    const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(
        repoRoot.absolutePath(),
        mergeBaseHead,
        relativeNotePath);
    if (contentResult.hasError() || contentResult.value().isEmpty()) {
        return std::nullopt;
    }

    const QString absoluteNotePath = repoRoot.absoluteFilePath(relativeNotePath);
    const auto noteResult = cwSaveLoad::loadNoteLiDAR(contentResult.value(), absoluteNotePath, repoRoot);
    if (noteResult.hasError()) {
        return std::nullopt;
    }
    const cwNoteLiDARData noteData = noteResult.value();
    if (noteData.id.isNull()) {
        return std::nullopt;
    }

    QSet<QUuid> seenStationIds;
    for (const cwNoteLiDARStation& station : noteData.stations) {
        if (station.id().isNull() || seenStationIds.contains(station.id())) {
            return std::nullopt;
        }
        seenStationIds.insert(station.id());
    }

    return std::make_optional(std::make_pair(noteData.id, noteData));
}

} // namespace

QString cwNoteLiDARSyncMergeHandler::name() const
{
    return QStringLiteral("cwNoteLiDARSyncMergeHandler");
}

cwReconcileMergeResult cwNoteLiDARSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
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

    struct NoteLiDARTripUpdate {
        cwTrip* trip = nullptr;
        const cwTripData* loadedTripData = nullptr;
        QHash<QUuid, cwNoteLiDARData> baseNoteLiDARByNoteId;
    };

    const QString dataRootName = context.dataRootName();

    const auto currentNoteIndex = cwSyncPathResolver::buildCurrentNoteLiDARIndex(context.repoRoot,
                                                                                 dataRootName,
                                                                                 context.saveLoad,
                                                                                 context.region);
    const auto loadedNoteIndex = cwSyncPathResolver::buildLoadedNoteLiDARIndex(context.repoRoot,
                                                                               dataRootName,
                                                                               context.loadData->region);

    QHash<QUuid, const cwTripData*> loadedTripsById;
    for (const cwCaveData& caveData : context.loadData->region.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            if (!tripData.id.isNull()) {
                loadedTripsById.insert(tripData.id, &tripData);
            }
        }
    }

    const QList<cwSyncPathResolver::TripChangeResolution> resolvedTripChanges =
        cwSyncPathResolver::resolveChangedNoteLiDARPaths(context.repoRoot,
                                                         dataRootName,
                                                         context.saveLoad,
                                                         context.region,
                                                         context.report->changedPaths,
                                                         currentNoteIndex,
                                                         loadedNoteIndex);
    if (resolvedTripChanges.isEmpty()) {
        return {};
    }

    QList<NoteLiDARTripUpdate> noteLiDARTripUpdates;
    noteLiDARTripUpdates.reserve(resolvedTripChanges.size());
    for (const cwSyncPathResolver::TripChangeResolution& tripChange : resolvedTripChanges) {
        NoteLiDARTripUpdate update;
        update.trip = tripChange.trip;
        if (update.trip == nullptr || update.trip->id().isNull()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Trip identity is missing for changed LiDAR payload.");
            return result;
        }

        const auto loadedTripIt = loadedTripsById.constFind(update.trip->id());
        if (loadedTripIt == loadedTripsById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Loaded data is missing changed trip LiDAR payload.");
            return result;
        }

        update.loadedTripData = loadedTripIt.value();

        for (auto basePathIt = tripChange.baseLookupPathByObjectId.cbegin();
             basePathIt != tripChange.baseLookupPathByObjectId.cend();
             ++basePathIt) {
            const auto baseLidarData = loadBaseLiDARDataForNote(context.repoRoot,
                                                                context.report->mergeBaseHead,
                                                                basePathIt.value());
            if (baseLidarData.has_value()) {
                update.baseNoteLiDARByNoteId.insert(baseLidarData->first, baseLidarData->second);
            }
        }

        noteLiDARTripUpdates.append(update);
    }

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();

    for (const NoteLiDARTripUpdate& update : noteLiDARTripUpdates) {
        Q_ASSERT(update.trip != nullptr);
        Q_ASSERT(update.loadedTripData != nullptr);

        const auto applyMode = cwNoteLiDARMergePlanBuilder::determineApplyMode(
            update.trip->notesLiDAR(),
            update.loadedTripData->noteLiDARModel);
        if (applyMode == cwNoteLiDARDescriptorApplyMode::Ambiguous) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = QStringLiteral("Ambiguous LiDAR note descriptor structural mapping.");
            return fullReloadResult;
        }

        if (applyMode == cwNoteLiDARDescriptorApplyMode::FullModelReplace) {
            update.trip->notesLiDAR()->setData(update.loadedTripData->noteLiDARModel);
            result.modelMutated = true;
            result.objectsPathReady.append(update.trip);
            continue;
        }

        const auto mergePreparation = cwNoteLiDARMergePlanBuilder::build(
            update.trip->notesLiDAR(),
            update.loadedTripData->noteLiDARModel,
            update.baseNoteLiDARByNoteId,
            context.applyMode == cwReconcileApplyMode::TargetCommitWins
                ? cwSyncMergeApplyUtils::ApplyMode::LoadedWins
                : cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);
        if (mergePreparation.hasError()) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = mergePreparation.errorMessage().isEmpty()
                                                  ? QStringLiteral("Unable to build deterministic LiDAR note merge plan.")
                                                  : mergePreparation.errorMessage();
            return fullReloadResult;
        }
        const cwNoteLiDARMergePreparation mergePreparationValue = mergePreparation.value();

        for (const cwNoteLiDARMergePlan& plan : mergePreparationValue.plans) {
            const auto applyResult = cwNoteLiDARMergeApplier::applyNoteLiDARMergePlan(plan);
            if (applyResult.hasError()) {
                cwReconcileMergeResult fullReloadResult;
                fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                fullReloadResult.handlerName = name();
                fullReloadResult.fallbackReason = applyResult.errorMessage().isEmpty()
                                                      ? QStringLiteral("Ambiguous LiDAR station mapping during incremental merge.")
                                                      : applyResult.errorMessage();
                return fullReloadResult;
            }
        }

        const bool reorderApplied = update.trip->notesLiDAR()->reorderNotes(mergePreparationValue.orderedNotes);
        if (!reorderApplied) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = QStringLiteral("Unable to apply deterministic LiDAR note order.");
            return fullReloadResult;
        }

        result.modelMutated = true;
        result.objectsPathReady.append(update.trip);
    }

    return result;
}
