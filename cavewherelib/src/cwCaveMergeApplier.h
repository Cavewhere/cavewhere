#pragma once

#include "Monad/Result.h"
#include "cwCaveMergePlanBuilder.h"

class cwCaveMergeApplier
{
public:
    static Monad::ResultBase applyCaveMergePlan(const cwCaveMergePlan& plan);
};
