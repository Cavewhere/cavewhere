#include "cwTripMergePlanBuilder.h"

#include "cwSyncIdUtils.h"
#include "cwTrip.h"

#include <QSet>

std::optional<cwTripMergePreparation> cwTripMergePlanBuilder::build(
    const QList<cwTrip*>& currentTrips,
    const QList<const cwTripData*>& loadedTrips,
    const QHash<QUuid, cwTripData>& baseTripById,
    QString* failureReason)
{
    const auto currentTripsById = cwSyncIdUtils::buildUniqueIdPointerMap(
        currentTrips,
        [](cwTrip* trip) { return trip; },
        [](const cwTrip* trip) { return trip->id(); });
    if (!currentTripsById.has_value()) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("Ambiguous current trip ids.");
        }
        return std::nullopt;
    }

    cwTripMergePreparation preparation;
    preparation.plans.reserve(loadedTrips.size());

    QSet<QUuid> seenLoadedIds;
    for (const cwTripData* loadedTripData : loadedTrips) {
        if (loadedTripData == nullptr
            || loadedTripData->id.isNull()
            || seenLoadedIds.contains(loadedTripData->id)) {
            if (failureReason != nullptr) {
                *failureReason = QStringLiteral("Ambiguous loaded trip ids.");
            }
            return std::nullopt;
        }
        seenLoadedIds.insert(loadedTripData->id);

        const auto currentTripIt = currentTripsById->constFind(loadedTripData->id);
        if (currentTripIt == currentTripsById->constEnd()) {
            if (failureReason != nullptr) {
                *failureReason = QStringLiteral("Missing current trip object for incremental merge.");
            }
            return std::nullopt;
        }

        cwTripMergePlan plan;
        plan.currentTrip = *currentTripIt;
        plan.loadedTripData = loadedTripData;

        const auto baseTripIt = baseTripById.constFind(loadedTripData->id);
        if (baseTripIt != baseTripById.constEnd()) {
            plan.baseTripData = *baseTripIt;
        }

        preparation.plans.append(std::move(plan));
    }

    return preparation;
}

