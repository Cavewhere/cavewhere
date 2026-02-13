#include "cwNoteMergePlanBuilder.h"

#include "cwNote.h"
#include "cwSyncIdUtils.h"
#include "cwSurveyNoteModel.h"

#include <QSet>

Monad::Result<cwNoteMergePreparation> cwNoteMergePlanBuilder::build(
    cwSurveyNoteModel* noteModel,
    const cwSurveyNoteModelData& loadedNoteModelData,
    const QHash<QUuid, cwNoteData>& baseNoteById)
{
    if (noteModel == nullptr) {
        return Monad::Result<cwNoteMergePreparation>(QStringLiteral("Note model is null."));
    }

    const auto currentNotesById = cwSyncIdUtils::buildUniqueIdPointerMap(
        noteModel->notes(),
        [](cwNote* note) { return note; },
        [](const cwNote* note) { return note->id(); });
    if (!currentNotesById.has_value()) {
        return Monad::Result<cwNoteMergePreparation>(QStringLiteral("Ambiguous current note ids."));
    }

    cwNoteMergePreparation preparation;
    preparation.plans.reserve(loadedNoteModelData.notes.size());

    QSet<QUuid> seenLoadedIds;
    for (const cwNoteData& loadedNoteData : loadedNoteModelData.notes) {
        if (loadedNoteData.id.isNull() || seenLoadedIds.contains(loadedNoteData.id)) {
            return Monad::Result<cwNoteMergePreparation>(QStringLiteral("Ambiguous loaded note ids."));
        }
        seenLoadedIds.insert(loadedNoteData.id);

        const auto currentNoteIt = currentNotesById->constFind(loadedNoteData.id);
        if (currentNoteIt == currentNotesById->constEnd()) {
            return Monad::Result<cwNoteMergePreparation>(
                QStringLiteral("Missing current note object for incremental merge."));
        }

        cwNoteMergePlan plan;
        plan.currentNote = *currentNoteIt;
        plan.loadedNoteData = &loadedNoteData;

        const auto baseNoteIt = baseNoteById.constFind(loadedNoteData.id);
        if (baseNoteIt != baseNoteById.constEnd()) {
            plan.baseNoteData = *baseNoteIt;
        }

        preparation.plans.append(std::move(plan));
    }

    return Monad::Result<cwNoteMergePreparation>(preparation);
}
