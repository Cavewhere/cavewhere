#pragma once

#include "cwNoteLiDARMergePlanBuilder.h"
#include "Monad/Result.h"

class cwNoteLiDARMergeApplier
{
public:
    static Monad::ResultBase applyNoteLiDARMergePlan(const cwNoteLiDARMergePlan& plan);
};
