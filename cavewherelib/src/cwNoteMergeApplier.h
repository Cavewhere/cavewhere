#pragma once

#include "cwNoteMergePlanBuilder.h"
#include "Monad/Result.h"

class cwNoteMergeApplier
{
public:
    static Monad::ResultBase applyNoteMergePlan(const cwNoteMergePlan& plan);
};
