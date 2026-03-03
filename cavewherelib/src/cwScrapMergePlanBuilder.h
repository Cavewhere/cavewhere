#pragma once

#include "CaveWhereLibExport.h"
#include "cwScrapSyncMergeHandler.h"
#include "Monad/Result.h"

class cwSurveyNoteModel;
struct cwSurveyNoteModelData;

class CAVEWHERE_LIB_EXPORT cwScrapMergePlanBuilder
{
public:
    static Monad::Result<cwNoteStructuralMergePreparation> buildNoteStructuralMergePreparation(
        cwSurveyNoteModel* noteModel,
        const cwSurveyNoteModelData& loadedNoteModelData);
};
