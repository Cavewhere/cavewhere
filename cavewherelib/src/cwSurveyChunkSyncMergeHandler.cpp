#include "cwSurveyChunkSyncMergeHandler.h"

Monad::Result<cwSurveyChunkMergePlan> cwSurveyChunkSyncMergeHandler::buildSurveyChunkMergePlan(
    cwSurveyChunk* currentChunk,
    const cwSurveyChunkData* loadedChunkData,
    const std::optional<cwSurveyChunkData>& baseChunkData)
{
    return cwSurveyChunkMergePlanBuilder::build(currentChunk, loadedChunkData, baseChunkData);
}

Monad::ResultBase cwSurveyChunkSyncMergeHandler::applySurveyChunkMergePlan(const cwSurveyChunkMergePlan& plan)
{
    return cwSurveyChunkMergeApplier::applySurveyChunkMergePlan(plan);
}
