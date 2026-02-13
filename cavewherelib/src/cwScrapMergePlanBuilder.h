#pragma once

#include "cwScrapSyncMergeHandler.h"
#include "Monad/Result.h"

class cwSurveyNoteModel;
struct cwSurveyNoteModelData;

class cwScrapMergePlanBuilder
{
public:
    static Monad::Result<cwNoteStructuralMergePreparation> buildNoteStructuralMergePreparation(
        cwSurveyNoteModel* noteModel,
        const cwSurveyNoteModelData& loadedNoteModelData);
};
