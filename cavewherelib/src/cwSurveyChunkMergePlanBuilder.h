#pragma once

#include "Monad/Result.h"
#include "cwSurveyChunkData.h"
#include "cwSyncMergeApplyUtils.h"

#include <optional>

class cwSurveyChunk;

struct cwSurveyChunkMergePlan {
    cwSurveyChunk* currentChunk = nullptr;
    const cwSurveyChunkData* loadedChunkData = nullptr;
    std::optional<cwSurveyChunkData> baseChunkData;
    cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge;
};

class cwSurveyChunkMergePlanBuilder
{
public:
    static Monad::Result<cwSurveyChunkMergePlan> build(
        cwSurveyChunk* currentChunk,
        const cwSurveyChunkData* loadedChunkData,
        const std::optional<cwSurveyChunkData>& baseChunkData,
        cwSyncMergeApplyUtils::ApplyMode applyMode = cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);
};
