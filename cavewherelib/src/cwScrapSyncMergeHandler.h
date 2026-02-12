#pragma once

#include <QList>
#include <QtCore/quuid.h>

#include <optional>
#include <vector>

class cwNote;
class cwSurveyNoteModel;
struct cwNoteData;
struct cwSurveyNoteModelData;

struct cwNoteStructuralMergePlan {
    cwNote* note = nullptr;
    const cwNoteData* loadedNoteData = nullptr;
    std::vector<QUuid> mergedScrapOrder;
};

struct cwNoteStructuralMergePreparation {
    QList<cwNoteStructuralMergePlan> plans;
    QList<cwNote*> orderedNotes;
};

class cwScrapSyncMergeHandler
{
public:
    static std::optional<cwNoteStructuralMergePreparation> buildNoteStructuralMergePreparation(
        cwSurveyNoteModel* noteModel,
        const cwSurveyNoteModelData& loadedNoteModelData);

    static void applyNoteStructuralMergePlan(const cwNoteStructuralMergePlan& plan);
};
