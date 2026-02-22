#include "cwTripMergeApplier.h"

#include "cwSaveLoad.h"
#include "cwSurveyChunkSyncMergeHandler.h"
#include "cwSyncIdUtils.h"
#include "cwSyncMergeApplyUtils.h"
#include "cwTeamSyncMergeHandler.h"
#include "cwTrip.h"
#include "cwTripCalibrationMergeApplier.h"
#include "cwTripCalibrationMergePlanBuilder.h"
#include "cavewhere.pb.h"

#include <optional>

namespace {

std::unique_ptr<CavewhereProto::Trip> normalizedTripProtoForObject(const cwTrip* trip)
{
    auto protoTrip = cwSaveLoad::toProtoTrip(trip);
    protoTrip->clear_name();
    protoTrip->clear_date();
    protoTrip->clear_tripcalibration();
    for (int i = 0; i < protoTrip->chunks_size(); ++i) {
        auto* chunk = protoTrip->mutable_chunks(i);
        chunk->clear_stations();
        chunk->clear_shots();
        chunk->clear_calibrations();
        chunk->clear_leg();
    }
    return protoTrip;
}

std::unique_ptr<CavewhereProto::Trip> normalizedTripProtoForData(const cwTripData& tripData)
{
    cwTrip tempTrip;
    tempTrip.setData(tripData);
    auto protoTrip = cwSaveLoad::toProtoTrip(&tempTrip);
    protoTrip->clear_name();
    protoTrip->clear_date();
    protoTrip->clear_tripcalibration();
    for (int i = 0; i < protoTrip->chunks_size(); ++i) {
        auto* chunk = protoTrip->mutable_chunks(i);
        chunk->clear_stations();
        chunk->clear_shots();
        chunk->clear_calibrations();
        chunk->clear_leg();
    }
    return protoTrip;
}

bool hasOnlyMergeableDiff(const cwTrip* currentTrip, const cwTripData& loadedTripData)
{
    const auto currentNormalized = normalizedTripProtoForObject(currentTrip);
    const auto loadedNormalized = normalizedTripProtoForData(loadedTripData);
    currentNormalized->clear_team();
    loadedNormalized->clear_team();
    currentNormalized->clear_chunks();
    loadedNormalized->clear_chunks();
    return currentNormalized->SerializeAsString() == loadedNormalized->SerializeAsString();
}

Monad::Result<cwSurveyChunkData> mergeChunkDataViaHandler(
    const cwSurveyChunkData& currentChunkData,
    const cwSurveyChunkData& loadedChunkData,
    const std::optional<cwSurveyChunkData>& baseChunkData)
{
    cwSurveyChunk tempChunk;
    tempChunk.setData(currentChunkData);

    const auto chunkPlan = cwSurveyChunkSyncMergeHandler::buildSurveyChunkMergePlan(
        &tempChunk,
        &loadedChunkData,
        baseChunkData);
    if (chunkPlan.hasError()) {
        return Monad::Result<cwSurveyChunkData>(chunkPlan.errorMessage());
    }

    const auto chunkApplyResult = cwSurveyChunkSyncMergeHandler::applySurveyChunkMergePlan(chunkPlan.value());
    if (chunkApplyResult.hasError()) {
        return Monad::Result<cwSurveyChunkData>(chunkApplyResult.errorMessage());
    }

    return Monad::Result<cwSurveyChunkData>(tempChunk.data());
}

} // namespace

Monad::ResultBase cwTripMergeApplier::applyTripMergePlan(const cwTripMergePlan& plan)
{
    Q_ASSERT(plan.currentTrip != nullptr);
    Q_ASSERT(plan.loadedTripData != nullptr);
    if (plan.currentTrip == nullptr || plan.loadedTripData == nullptr) {
        return Monad::ResultBase(QStringLiteral("Trip merge plan is missing required objects."));
    }

    cwTrip* const currentTrip = plan.currentTrip;
    const cwTripData& loadedTripData = *plan.loadedTripData;

    if (!hasOnlyMergeableDiff(currentTrip, loadedTripData)) {
        return Monad::ResultBase(
            QStringLiteral("Trip subobject merge is not implemented for incremental trip merge."));
    }

    const std::optional<QString> baseName = plan.baseTripData.has_value()
        ? std::optional<QString>(plan.baseTripData->name)
        : std::nullopt;
    const std::optional<QDateTime> baseDate = plan.baseTripData.has_value()
        ? std::optional<QDateTime>(plan.baseTripData->date)
        : std::nullopt;
    const std::optional<cwTripCalibrationData> baseCalibration = plan.baseTripData.has_value()
        ? std::optional<cwTripCalibrationData>(plan.baseTripData->calibrations)
        : std::nullopt;
    const std::optional<cwTeamData> baseTeamData = plan.baseTripData.has_value()
        ? std::optional<cwTeamData>(plan.baseTripData->team)
        : std::nullopt;
    const std::optional<QList<cwSurveyChunkData>> baseChunks = plan.baseTripData.has_value()
        ? std::optional<QList<cwSurveyChunkData>>(plan.baseTripData->chunks)
        : std::nullopt;

    const QString mergedName = cwSyncMergeApplyUtils::chooseBundleValue(
        currentTrip->name(),
        loadedTripData.name,
        baseName,
        [](const QString& lhs, const QString& rhs) { return lhs == rhs; },
        plan.applyMode);
    currentTrip->setName(mergedName);

    const QDateTime mergedDate = cwSyncMergeApplyUtils::chooseBundleValue(
        currentTrip->date(),
        loadedTripData.date,
        baseDate,
        [](const QDateTime& lhs, const QDateTime& rhs) { return lhs == rhs; },
        plan.applyMode);
    currentTrip->setDate(mergedDate);

    const auto calibrationMergePlan = cwTripCalibrationMergePlanBuilder::build(
        currentTrip->calibrations(),
        &loadedTripData.calibrations,
        baseCalibration,
        plan.applyMode);
    if (calibrationMergePlan.hasError()) {
        return Monad::ResultBase(calibrationMergePlan.errorMessage());
    }

    const auto applyCalibrationResult =
        cwTripCalibrationMergeApplier::applyTripCalibrationMergePlan(calibrationMergePlan.value());
    if (applyCalibrationResult.hasError()) {
        return Monad::ResultBase(applyCalibrationResult.errorMessage());
    }

    const auto teamMergePlan = cwTeamSyncMergeHandler::buildTeamMergePlan(
        currentTrip->team(),
        &loadedTripData.team,
        baseTeamData);
    if (teamMergePlan.hasError()) {
        return Monad::ResultBase(teamMergePlan.errorMessage());
    }

    const auto applyTeamResult = cwTeamSyncMergeHandler::applyTeamMergePlan(teamMergePlan.value());
    if (applyTeamResult.hasError()) {
        return Monad::ResultBase(applyTeamResult.errorMessage());
    }

    const QList<cwSurveyChunk*> currentChunks = currentTrip->chunks();
    QList<cwSurveyChunkData> currentChunkDataList;
    currentChunkDataList.reserve(currentChunks.size());
    for (cwSurveyChunk* chunk : currentChunks) {
        if (chunk == nullptr) {
            return Monad::ResultBase(QStringLiteral("Current trip has null survey chunk object."));
        }
        currentChunkDataList.append(chunk->data());
    }

    const auto mergedChunkDataList = cwSyncIdUtils::buildMergedOrderedList<cwSurveyChunkData>(
        currentChunkDataList,
        loadedTripData.chunks,
        baseChunks,
        [](const cwSurveyChunkData& chunkData) { return chunkData.id; },
        [applyMode = plan.applyMode](const cwSurveyChunkData& currentChunkData,
           const cwSurveyChunkData& loadedChunkData,
           const std::optional<cwSurveyChunkData>& baseChunkData) {
            if (applyMode == cwSyncMergeApplyUtils::ApplyMode::LoadedWins) {
                return Monad::Result<cwSurveyChunkData>(loadedChunkData);
            }
            return mergeChunkDataViaHandler(currentChunkData, loadedChunkData, baseChunkData);
        },
        [applyMode = plan.applyMode](const std::vector<QUuid>& currentIds,
           const std::vector<QUuid>& loadedIds,
           const std::optional<std::vector<QUuid>>& baseIds) {
            return cwSyncMergeApplyUtils::chooseBundleValue(
                currentIds,
                loadedIds,
                baseIds,
                [](const std::vector<QUuid>& lhs, const std::vector<QUuid>& rhs) { return lhs == rhs; },
                applyMode);
        },
        QStringLiteral("trip chunk list"));
    if (mergedChunkDataList.hasError()) {
        return Monad::ResultBase(mergedChunkDataList.errorMessage());
    }

    const QList<cwSurveyChunkData> mergedChunks = mergedChunkDataList.value();
    const auto currentChunkIds = cwSyncIdUtils::collectOrderedUniqueIds(
        currentChunkDataList,
        [](const cwSurveyChunkData& chunkData) { return chunkData.id; });
    const auto mergedChunkIds = cwSyncIdUtils::collectOrderedUniqueIds(
        mergedChunks,
        [](const cwSurveyChunkData& chunkData) { return chunkData.id; });
    if (!currentChunkIds.has_value() || !mergedChunkIds.has_value()) {
        return Monad::ResultBase(QStringLiteral("Ambiguous merged survey chunk ids."));
    }

    if (*currentChunkIds == *mergedChunkIds) {
        const auto currentChunksById = cwSyncIdUtils::buildUniqueIdPointerMap(
            currentChunks,
            [](cwSurveyChunk* chunk) { return chunk; },
            [](const cwSurveyChunk* chunk) { return chunk->id(); });
        if (!currentChunksById.has_value()) {
            return Monad::ResultBase(QStringLiteral("Ambiguous current survey chunk ids."));
        }

        for (const cwSurveyChunkData& mergedChunkData : mergedChunks) {
            const auto currentChunkIt = currentChunksById->constFind(mergedChunkData.id);
            if (currentChunkIt == currentChunksById->constEnd()) {
                return Monad::ResultBase(QStringLiteral("Missing current survey chunk object for incremental merge."));
            }
            (*currentChunkIt)->setData(mergedChunkData);
        }
    } else {
        currentTrip->removeChunks(0, currentTrip->chunkCount() - 1);
        for (const cwSurveyChunkData& mergedChunkData : mergedChunks) {
            auto* chunk = new cwSurveyChunk();
            chunk->setData(mergedChunkData);
            currentTrip->addChunk(chunk);
        }
    }

    return Monad::ResultBase();
}
