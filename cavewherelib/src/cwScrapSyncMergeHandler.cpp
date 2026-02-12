#include "cwScrapSyncMergeHandler.h"

#include "cwScrapMergeApplier.h"
#include "cwScrapMergePlanBuilder.h"

std::optional<cwNoteStructuralMergePreparation> cwScrapSyncMergeHandler::buildNoteStructuralMergePreparation(
    cwSurveyNoteModel* noteModel,
    const cwSurveyNoteModelData& loadedNoteModelData)
{
    return cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(noteModel, loadedNoteModelData);
}

void cwScrapSyncMergeHandler::applyNoteStructuralMergePlan(const cwNoteStructuralMergePlan& plan)
{
    cwScrapMergeApplier::applyNoteStructuralMergePlan(plan);
}
