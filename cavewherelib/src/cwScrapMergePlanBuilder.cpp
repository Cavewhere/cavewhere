#include "cwScrapMergePlanBuilder.h"

#include "cwDiff.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwSyncIdUtils.h"
#include "cwSurveyNoteModel.h"

#include <QHash>
#include <QSet>

#include <optional>
#include <utility>

namespace {

std::optional<std::vector<QUuid>> mergedScrapOrderForNote(const cwNote* note, const cwNoteData& loadedNoteData)
{
    if (note == nullptr) {
        return std::nullopt;
    }

    const auto currentScrapIds = cwSyncIdUtils::collectOrderedUniqueIds(
        note->scraps(),
        [](const cwScrap* scrap) {
            return scrap != nullptr ? scrap->id() : QUuid();
        });
    const auto loadedScrapIds = cwSyncIdUtils::collectOrderedUniqueIds(
        loadedNoteData.scraps,
        [](const cwScrapData& scrapData) {
            return scrapData.id;
        });

    if (!currentScrapIds.has_value() || !loadedScrapIds.has_value()) {
        return std::nullopt;
    }

    const auto edits = cwDiff::diff<QUuid>(currentScrapIds.value(), loadedScrapIds.value());
    const auto mergedOrder = cwDiff::applyEditScript(edits, currentScrapIds.value(), loadedScrapIds.value());
    if (mergedOrder != loadedScrapIds.value()) {
        return std::nullopt;
    }

    return mergedOrder;
}

} // namespace

Monad::Result<cwNoteStructuralMergePreparation> cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(
    cwSurveyNoteModel* noteModel,
    const cwSurveyNoteModelData& loadedNoteModelData)
{
    if (noteModel == nullptr) {
        return Monad::Result<cwNoteStructuralMergePreparation>(QStringLiteral("Note model is null."));
    }

    const auto currentNotesById = cwSyncIdUtils::buildUniqueIdPointerMap(
        noteModel->notes(),
        [](cwNote* note) { return note; },
        [](const cwNote* note) { return note->id(); });
    if (!currentNotesById.has_value()) {
        return Monad::Result<cwNoteStructuralMergePreparation>(QStringLiteral("Ambiguous current note ids."));
    }

    if (currentNotesById->size() != loadedNoteModelData.notes.size()) {
        return Monad::Result<cwNoteStructuralMergePreparation>(
            QStringLiteral("Loaded note count does not match current note count."));
    }

    cwNoteStructuralMergePreparation preparation;
    preparation.plans.reserve(loadedNoteModelData.notes.size());
    preparation.orderedNotes.reserve(loadedNoteModelData.notes.size());
    QSet<QUuid> seenLoadedNoteIds;
    for (const cwNoteData& loadedNoteData : loadedNoteModelData.notes) {
        if (loadedNoteData.id.isNull()
            || seenLoadedNoteIds.contains(loadedNoteData.id)
            || !currentNotesById->contains(loadedNoteData.id)) {
            return Monad::Result<cwNoteStructuralMergePreparation>(
                QStringLiteral("Ambiguous loaded note ids."));
        }

        seenLoadedNoteIds.insert(loadedNoteData.id);
        cwNote* const currentNote = currentNotesById->value(loadedNoteData.id);
        auto mergedScrapOrder = mergedScrapOrderForNote(currentNote, loadedNoteData);
        if (!mergedScrapOrder.has_value()) {
            return Monad::Result<cwNoteStructuralMergePreparation>(
                QStringLiteral("Unable to build deterministic scrap order for note."));
        }

        cwNoteStructuralMergePlan plan;
        plan.note = currentNote;
        plan.loadedNoteData = &loadedNoteData;
        plan.mergedScrapOrder = std::move(mergedScrapOrder.value());
        preparation.plans.append(std::move(plan));
        preparation.orderedNotes.append(currentNote);
    }

    return Monad::Result<cwNoteStructuralMergePreparation>(preparation);
}
