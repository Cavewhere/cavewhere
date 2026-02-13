#include "cwSurveyChunkMergePlanBuilder.h"

#include "cwSurveyChunk.h"

Monad::Result<cwSurveyChunkMergePlan> cwSurveyChunkMergePlanBuilder::build(
    cwSurveyChunk* currentChunk,
    const cwSurveyChunkData* loadedChunkData,
    const std::optional<cwSurveyChunkData>& baseChunkData)
{
    if (currentChunk == nullptr || loadedChunkData == nullptr) {
        return Monad::Result<cwSurveyChunkMergePlan>(
            QStringLiteral("Survey chunk merge plan is missing required objects."));
    }

    if (currentChunk->id().isNull() || loadedChunkData->id.isNull() || currentChunk->id() != loadedChunkData->id) {
        return Monad::Result<cwSurveyChunkMergePlan>(
            QStringLiteral("Survey chunk merge plan requires matching non-null chunk ids."));
    }

    cwSurveyChunkMergePlan plan;
    plan.currentChunk = currentChunk;
    plan.loadedChunkData = loadedChunkData;
    plan.baseChunkData = baseChunkData;
    return Monad::Result<cwSurveyChunkMergePlan>(plan);
}
