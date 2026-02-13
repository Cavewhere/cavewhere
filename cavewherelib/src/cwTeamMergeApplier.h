#pragma once

#include "cwTeamMergePlanBuilder.h"
#include "Monad/Result.h"

class cwTeamMergeApplier
{
public:
    static Monad::ResultBase applyTeamMergePlan(const cwTeamMergePlan& plan);
};

