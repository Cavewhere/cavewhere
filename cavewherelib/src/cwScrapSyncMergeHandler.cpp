#include "cwScrapSyncMergeHandler.h"

#include "cwScrapMergeApplier.h"
#include "cwScrapMergePlanBuilder.h"

Monad::Result<cwNoteStructuralMergePreparation> cwScrapSyncMergeHandler::buildNoteStructuralMergePreparation(
    cwSurveyNoteModel* noteModel,
    const cwSurveyNoteModelData& loadedNoteModelData)
{
    return cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(noteModel, loadedNoteModelData);
}

Monad::Result<cwScrapMergeApplyResult> cwScrapSyncMergeHandler::applyNoteStructuralMergePlan(
    const cwNoteStructuralMergePlan& plan)
{
    return cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
}
