#pragma once

#include "cwSurveyChunkMergeApplier.h"
#include "cwSurveyChunkMergePlanBuilder.h"
#include "cwSyncMergeApplyUtils.h"

class cwSurveyChunkSyncMergeHandler
{
public:
    static Monad::Result<cwSurveyChunkMergePlan> buildSurveyChunkMergePlan(
        cwSurveyChunk* currentChunk,
        const cwSurveyChunkData* loadedChunkData,
        const std::optional<cwSurveyChunkData>& baseChunkData,
        cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);

    static Monad::ResultBase applySurveyChunkMergePlan(const cwSurveyChunkMergePlan& plan);
};
