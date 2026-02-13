#pragma once

#include "cwTripCalibrationMergePlanBuilder.h"
#include "Monad/Result.h"

class cwTripCalibrationMergeApplier
{
public:
    static Monad::ResultBase applyTripCalibrationMergePlan(const cwTripCalibrationMergePlan& plan);
};
