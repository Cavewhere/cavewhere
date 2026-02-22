#include "cwTripCalibrationMergePlanBuilder.h"

Monad::Result<cwTripCalibrationMergePlan> cwTripCalibrationMergePlanBuilder::build(
    cwTripCalibration* currentCalibration,
    const cwTripCalibrationData* loadedCalibrationData,
    const std::optional<cwTripCalibrationData>& baseCalibrationData,
    cwSyncMergeApplyUtils::ApplyMode applyMode)
{
    if (currentCalibration == nullptr || loadedCalibrationData == nullptr) {
        return Monad::Result<cwTripCalibrationMergePlan>(
            QStringLiteral("Trip calibration merge plan is missing required objects."));
    }

    cwTripCalibrationMergePlan plan;
    plan.currentCalibration = currentCalibration;
    plan.loadedCalibrationData = loadedCalibrationData;
    plan.baseCalibrationData = baseCalibrationData;
    plan.applyMode = applyMode;
    return Monad::Result<cwTripCalibrationMergePlan>(plan);
}
