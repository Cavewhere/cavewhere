#pragma once

#include "cwNoteData.h"
#include "cwSurveyNoteModelData.h"

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

class cwNoteMergePlanBuilder
{
public:
    static std::optional<cwNoteMergePreparation> build(
        cwSurveyNoteModel* noteModel,
        const cwSurveyNoteModelData& loadedNoteModelData,
        const QHash<QUuid, cwNoteData>& baseNoteById,
        QString* failureReason = nullptr);
};

