#pragma once

#include "cwSyncMergeApplyUtils.h"
#include "cwTripCalibration.h"
#include "Monad/Result.h"

#include <QString>

#include <optional>

struct cwTripCalibrationMergePlan {
    cwTripCalibration* currentCalibration = nullptr;
    const cwTripCalibrationData* loadedCalibrationData = nullptr;
    std::optional<cwTripCalibrationData> baseCalibrationData;
    cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge;
};

class cwTripCalibrationMergePlanBuilder
{
public:
    static Monad::Result<cwTripCalibrationMergePlan> build(
        cwTripCalibration* currentCalibration,
        const cwTripCalibrationData* loadedCalibrationData,
        const std::optional<cwTripCalibrationData>& baseCalibrationData,
        cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);
};
