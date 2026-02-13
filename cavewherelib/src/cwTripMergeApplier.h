#pragma once

#include "cwTripMergePlanBuilder.h"

class cwTripMergeApplier
{
public:
    static bool applyTripMergePlan(const cwTripMergePlan& plan, QString* failureReason = nullptr);
};

