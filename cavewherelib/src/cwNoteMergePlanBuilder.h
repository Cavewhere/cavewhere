#pragma once

#include "CaveWhereLibExport.h"
#include "cwNoteData.h"
#include "cwSurveyNoteModelData.h"
#include "Monad/Result.h"

#include <QHash>
#include <QList>
#include <QUuid>

#include <optional>

class cwNote;
class cwSurveyNoteModel;

struct cwNoteMergePlan {
    cwNote* currentNote = nullptr;
    const cwNoteData* loadedNoteData = nullptr;
    std::optional<cwNoteData> baseNoteData;
};

struct cwNoteMergePreparation {
    QList<cwNoteMergePlan> plans;
};

class CAVEWHERE_LIB_EXPORT cwNoteMergePlanBuilder
{
public:
    static Monad::Result<cwNoteMergePreparation> build(
        cwSurveyNoteModel* noteModel,
        const cwSurveyNoteModelData& loadedNoteModelData,
        const QHash<QUuid, cwNoteData>& baseNoteById);
};
