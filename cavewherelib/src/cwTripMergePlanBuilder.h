#pragma once

#include "cwTripData.h"
#include "Monad/Result.h"

#include <QHash>
#include <QList>
#include <QUuid>

#include <optional>

class cwTrip;

struct cwTripMergePlan {
    cwTrip* currentTrip = nullptr;
    const cwTripData* loadedTripData = nullptr;
    std::optional<cwTripData> baseTripData;
};

struct cwTripMergePreparation {
    QList<cwTripMergePlan> plans;
};

class cwTripMergePlanBuilder
{
public:
    static Monad::Result<cwTripMergePreparation> build(
        const QList<cwTrip*>& currentTrips,
        const QList<const cwTripData*>& loadedTrips,
        const QHash<QUuid, cwTripData>& baseTripById);
};
