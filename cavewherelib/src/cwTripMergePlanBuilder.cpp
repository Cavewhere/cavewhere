#include "cwTripMergePlanBuilder.h"

#include "cwSyncIdUtils.h"
#include "cwTrip.h"

#include <QSet>

Monad::Result<cwTripMergePreparation> cwTripMergePlanBuilder::build(
    const QList<cwTrip*>& currentTrips,
    const QList<const cwTripData*>& loadedTrips,
    const QHash<QUuid, cwTripData>& baseTripById)
{
    const auto currentTripsById = cwSyncIdUtils::buildUniqueIdPointerMap(
        currentTrips,
        [](cwTrip* trip) { return trip; },
        [](const cwTrip* trip) { return trip->id(); });
    if (!currentTripsById.has_value()) {
        return Monad::Result<cwTripMergePreparation>(QStringLiteral("Ambiguous current trip ids."));
    }

    cwTripMergePreparation preparation;
    preparation.plans.reserve(loadedTrips.size());

    QSet<QUuid> seenLoadedIds;
    for (const cwTripData* loadedTripData : loadedTrips) {
        if (loadedTripData == nullptr
            || loadedTripData->id.isNull()
            || seenLoadedIds.contains(loadedTripData->id)) {
            return Monad::Result<cwTripMergePreparation>(QStringLiteral("Ambiguous loaded trip ids."));
        }
        seenLoadedIds.insert(loadedTripData->id);

        const auto currentTripIt = currentTripsById->constFind(loadedTripData->id);
        if (currentTripIt == currentTripsById->constEnd()) {
            return Monad::Result<cwTripMergePreparation>(
                QStringLiteral("Missing current trip object for incremental merge."));
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

    return Monad::Result<cwTripMergePreparation>(preparation);
}
