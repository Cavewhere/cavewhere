#include "cwSurveyChunkSyncMergeHandler.h"

Monad::Result<cwSurveyChunkMergePlan> cwSurveyChunkSyncMergeHandler::buildSurveyChunkMergePlan(
    cwSurveyChunk* currentChunk,
    const cwSurveyChunkData* loadedChunkData,
    const std::optional<cwSurveyChunkData>& baseChunkData,
    cwSyncMergeApplyUtils::ApplyMode applyMode)
{
    return cwSurveyChunkMergePlanBuilder::build(currentChunk, loadedChunkData, baseChunkData, applyMode);
}

Monad::ResultBase cwSurveyChunkSyncMergeHandler::applySurveyChunkMergePlan(const cwSurveyChunkMergePlan& plan)
{
    return cwSurveyChunkMergeApplier::applySurveyChunkMergePlan(plan);
}
