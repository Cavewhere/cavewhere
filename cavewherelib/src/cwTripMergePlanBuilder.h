#pragma once

#include "CaveWhereLibExport.h"
#include "cwSyncMergeApplyUtils.h"
#include "cwTripData.h"
#include "Monad/Result.h"

#include <QHash>
#include <QList>
#include <QUuid>

#include <optional>

class cwTrip;

struct cwTripMergePlan {
    cwTrip* currentTrip = nullptr;
    cwTripData loadedTripData;
    std::optional<cwTripData> baseTripData;
    cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge;
};

struct cwTripMergePreparation {
    QList<cwTripMergePlan> plans;
};

class CAVEWHERE_LIB_EXPORT cwTripMergePlanBuilder
{
public:
    static Monad::Result<cwTripMergePreparation> build(
        const QList<cwTrip*>& currentTrips,
        const QList<const cwTripData*>& loadedTrips,
        const QHash<QUuid, cwTripData>& baseTripById,
        cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);
};
