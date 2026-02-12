#pragma once

#include "cwScrapSyncMergeHandler.h"

class cwSurveyNoteModel;
struct cwSurveyNoteModelData;

class cwScrapMergePlanBuilder
{
public:
    static std::optional<cwNoteStructuralMergePreparation> buildNoteStructuralMergePreparation(
        cwSurveyNoteModel* noteModel,
        const cwSurveyNoteModelData& loadedNoteModelData);
};
