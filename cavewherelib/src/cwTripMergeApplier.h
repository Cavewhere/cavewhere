#pragma once

#include "CaveWhereLibExport.h"
#include "cwTripMergePlanBuilder.h"
#include "Monad/Result.h"

class CAVEWHERE_LIB_EXPORT cwTripMergeApplier
{
public:
    static Monad::ResultBase applyTripMergePlan(const cwTripMergePlan& plan);
};
