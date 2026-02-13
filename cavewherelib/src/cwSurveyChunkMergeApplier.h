#pragma once

#include "Monad/Result.h"
#include "cwSurveyChunkMergePlanBuilder.h"

class cwSurveyChunkMergeApplier
{
public:
    static Monad::ResultBase applySurveyChunkMergePlan(const cwSurveyChunkMergePlan& plan);
};
