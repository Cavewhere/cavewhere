#include "cwNoteMergePlanBuilder.h"

#include "cwNote.h"
#include "cwSyncIdUtils.h"
#include "cwSurveyNoteModel.h"

#include <QSet>

std::optional<cwNoteMergePreparation> cwNoteMergePlanBuilder::build(
    cwSurveyNoteModel* noteModel,
    const cwSurveyNoteModelData& loadedNoteModelData,
    const QHash<QUuid, cwNoteData>& baseNoteById,
    QString* failureReason)
{
    if (noteModel == nullptr) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("Note model is null.");
        }
        return std::nullopt;
    }

    const auto currentNotesById = cwSyncIdUtils::buildUniqueIdPointerMap(
        noteModel->notes(),
        [](cwNote* note) { return note; },
        [](const cwNote* note) { return note->id(); });
    if (!currentNotesById.has_value()) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("Ambiguous current note ids.");
        }
        return std::nullopt;
    }

    cwNoteMergePreparation preparation;
    preparation.plans.reserve(loadedNoteModelData.notes.size());

    QSet<QUuid> seenLoadedIds;
    for (const cwNoteData& loadedNoteData : loadedNoteModelData.notes) {
        if (loadedNoteData.id.isNull() || seenLoadedIds.contains(loadedNoteData.id)) {
            if (failureReason != nullptr) {
                *failureReason = QStringLiteral("Ambiguous loaded note ids.");
            }
            return std::nullopt;
        }
        seenLoadedIds.insert(loadedNoteData.id);

        const auto currentNoteIt = currentNotesById->constFind(loadedNoteData.id);
        if (currentNoteIt == currentNotesById->constEnd()) {
            if (failureReason != nullptr) {
                *failureReason = QStringLiteral("Missing current note object for incremental merge.");
            }
            return std::nullopt;
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

    return preparation;
}

