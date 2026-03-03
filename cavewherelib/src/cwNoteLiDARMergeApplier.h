#pragma once

#include "CaveWhereLibExport.h"
#include "cwNoteLiDARMergePlanBuilder.h"
#include "Monad/Result.h"

class CAVEWHERE_LIB_EXPORT cwNoteLiDARMergeApplier
{
public:
    static Monad::ResultBase applyNoteLiDARMergePlan(const cwNoteLiDARMergePlan& plan);
};
