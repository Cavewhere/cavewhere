#include "cwTripCalibrationMergePlanBuilder.h"

std::optional<cwTripCalibrationMergePlan> cwTripCalibrationMergePlanBuilder::build(
    cwTripCalibration* currentCalibration,
    const cwTripCalibrationData* loadedCalibrationData,
    const std::optional<cwTripCalibrationData>& baseCalibrationData,
    QString* failureReason)
{
    if (currentCalibration == nullptr || loadedCalibrationData == nullptr) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("Trip calibration merge plan is missing required objects.");
        }
        return std::nullopt;
    }

    cwTripCalibrationMergePlan plan;
    plan.currentCalibration = currentCalibration;
    plan.loadedCalibrationData = loadedCalibrationData;
    plan.baseCalibrationData = baseCalibrationData;
    return plan;
}

