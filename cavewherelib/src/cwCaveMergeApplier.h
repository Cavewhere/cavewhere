#pragma once

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCaveMergePlanBuilder.h"

class CAVEWHERE_LIB_EXPORT cwCaveMergeApplier
{
public:
    static Monad::ResultBase applyCaveMergePlan(const cwCaveMergePlan& plan);
};
