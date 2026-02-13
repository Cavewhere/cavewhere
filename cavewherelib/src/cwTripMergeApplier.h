#pragma once

#include "cwTripMergePlanBuilder.h"
#include "Monad/Result.h"

class cwTripMergeApplier
{
public:
    static Monad::ResultBase applyTripMergePlan(const cwTripMergePlan& plan);
};
