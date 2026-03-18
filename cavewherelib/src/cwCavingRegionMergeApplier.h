#pragma once

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCavingRegionMergePlanBuilder.h"

class CAVEWHERE_LIB_EXPORT cwCavingRegionMergeApplier
{
public:
    static Monad::ResultBase applyRegionMergePlan(const cwCavingRegionMergePlan& plan);
};
