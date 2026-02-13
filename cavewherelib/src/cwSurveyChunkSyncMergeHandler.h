#pragma once

#include "cwSurveyChunkMergeApplier.h"
#include "cwSurveyChunkMergePlanBuilder.h"

class cwSurveyChunkSyncMergeHandler
{
public:
    static Monad::Result<cwSurveyChunkMergePlan> buildSurveyChunkMergePlan(
        cwSurveyChunk* currentChunk,
        const cwSurveyChunkData* loadedChunkData,
        const std::optional<cwSurveyChunkData>& baseChunkData);

    static Monad::ResultBase applySurveyChunkMergePlan(const cwSurveyChunkMergePlan& plan);
};
