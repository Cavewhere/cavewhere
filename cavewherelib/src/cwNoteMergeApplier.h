#pragma once

#include "CaveWhereLibExport.h"
#include "cwNoteMergePlanBuilder.h"
#include "Monad/Result.h"

class CAVEWHERE_LIB_EXPORT cwNoteMergeApplier
{
public:
    static Monad::ResultBase applyNoteMergePlan(const cwNoteMergePlan& plan);
};
