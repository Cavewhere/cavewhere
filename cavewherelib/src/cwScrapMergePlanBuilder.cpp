#include "cwScrapMergePlanBuilder.h"

#include "cwDiff.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"

#include <QHash>
#include <QSet>

#include <optional>
#include <utility>

namespace {

template<typename Container, typename IdAccessor>
std::optional<std::vector<QUuid>> collectOrderedUniqueIds(const Container& items, IdAccessor idAccessor)
{
    std::vector<QUuid> orderedIds;
    orderedIds.reserve(static_cast<size_t>(items.size()));
    QSet<QUuid> seenIds;

    for (const auto& item : items) {
        const QUuid id = idAccessor(item);
        if (id.isNull() || seenIds.contains(id)) {
            return std::nullopt;
        }
        seenIds.insert(id);
        orderedIds.push_back(id);
    }

    return orderedIds;
}

std::optional<std::vector<QUuid>> mergedScrapOrderForNote(const cwNote* note, const cwNoteData& loadedNoteData)
{
    if (note == nullptr) {
        return std::nullopt;
    }

    const auto currentScrapIds = collectOrderedUniqueIds(
        note->scraps(),
        [](const cwScrap* scrap) {
            return scrap != nullptr ? scrap->id() : QUuid();
        });
    const auto loadedScrapIds = collectOrderedUniqueIds(
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

std::optional<cwNoteStructuralMergePreparation> cwScrapMergePlanBuilder::buildNoteStructuralMergePreparation(
    cwSurveyNoteModel* noteModel,
    const cwSurveyNoteModelData& loadedNoteModelData)
{
    if (noteModel == nullptr) {
        return std::nullopt;
    }

    QHash<QUuid, cwNote*> currentNotesById;
    currentNotesById.reserve(noteModel->rowCount());
    for (cwNote* note : noteModel->notes()) {
        if (note == nullptr || note->id().isNull() || currentNotesById.contains(note->id())) {
            return std::nullopt;
        }
        currentNotesById.insert(note->id(), note);
    }

    if (currentNotesById.size() != loadedNoteModelData.notes.size()) {
        return std::nullopt;
    }

    cwNoteStructuralMergePreparation preparation;
    preparation.plans.reserve(loadedNoteModelData.notes.size());
    preparation.orderedNotes.reserve(loadedNoteModelData.notes.size());
    QSet<QUuid> seenLoadedNoteIds;
    for (const cwNoteData& loadedNoteData : loadedNoteModelData.notes) {
        if (loadedNoteData.id.isNull()
            || seenLoadedNoteIds.contains(loadedNoteData.id)
            || !currentNotesById.contains(loadedNoteData.id)) {
            return std::nullopt;
        }

        seenLoadedNoteIds.insert(loadedNoteData.id);
        cwNote* const currentNote = currentNotesById.value(loadedNoteData.id);
        auto mergedScrapOrder = mergedScrapOrderForNote(currentNote, loadedNoteData);
        if (!mergedScrapOrder.has_value()) {
            return std::nullopt;
        }

        cwNoteStructuralMergePlan plan;
        plan.note = currentNote;
        plan.loadedNoteData = &loadedNoteData;
        plan.mergedScrapOrder = std::move(mergedScrapOrder.value());
        preparation.plans.append(std::move(plan));
        preparation.orderedNotes.append(currentNote);
    }

    return preparation;
}
