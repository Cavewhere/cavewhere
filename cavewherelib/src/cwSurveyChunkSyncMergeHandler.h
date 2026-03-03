#pragma once

#include "CaveWhereLibExport.h"
#include "cwSurveyChunkMergeApplier.h"
#include "cwSurveyChunkMergePlanBuilder.h"
#include "cwSyncMergeApplyUtils.h"

class CAVEWHERE_LIB_EXPORT cwSurveyChunkSyncMergeHandler
{
public:
    static Monad::Result<cwSurveyChunkMergePlan> buildSurveyChunkMergePlan(
        cwSurveyChunk* currentChunk,
        const cwSurveyChunkData* loadedChunkData,
        const std::optional<cwSurveyChunkData>& baseChunkData,
        cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);

    static Monad::ResultBase applySurveyChunkMergePlan(const cwSurveyChunkMergePlan& plan);
};
