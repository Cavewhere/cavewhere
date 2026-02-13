#include "cwNoteLiDARMergePlanBuilder.h"

#include "cwNoteLiDAR.h"
#include "cwSyncIdUtils.h"
#include "cwSurveyNoteLiDARModel.h"

cwNoteLiDARDescriptorApplyMode cwNoteLiDARMergePlanBuilder::determineApplyMode(
    cwSurveyNoteLiDARModel* noteLiDARModel,
    const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData)
{
    if (noteLiDARModel == nullptr) {
        return cwNoteLiDARDescriptorApplyMode::Ambiguous;
    }

    const auto currentNoteIds = cwSyncIdUtils::collectOrderedUniqueIds(
        noteLiDARModel->notes(),
        [](const QObject* noteObject) {
            const auto* note = qobject_cast<const cwNoteLiDAR*>(noteObject);
            return note != nullptr ? note->id() : QUuid();
        });
    const auto loadedNoteIds = cwSyncIdUtils::collectOrderedUniqueIds(
        loadedNoteLiDARModelData.notes,
        [](const cwNoteLiDARData& noteData) {
            return noteData.id;
        });

    if (!currentNoteIds.has_value() || !loadedNoteIds.has_value()) {
        return cwNoteLiDARDescriptorApplyMode::Ambiguous;
    }

    if (currentNoteIds.value() == loadedNoteIds.value()) {
        return cwNoteLiDARDescriptorApplyMode::IncrementalMerge;
    }

    if (cwSyncIdUtils::haveSameIdsIgnoringOrder(currentNoteIds.value(), loadedNoteIds.value())) {
        return cwNoteLiDARDescriptorApplyMode::IncrementalMerge;
    }

    return cwNoteLiDARDescriptorApplyMode::FullModelReplace;
}

Monad::Result<cwNoteLiDARMergePreparation> cwNoteLiDARMergePlanBuilder::build(
    cwSurveyNoteLiDARModel* noteLiDARModel,
    const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData,
    const QHash<QUuid, cwNoteLiDARData>& baseNoteLiDARByNoteId)
{
    if (noteLiDARModel == nullptr) {
        return Monad::Result<cwNoteLiDARMergePreparation>(QStringLiteral("LiDAR note model is null."));
    }

    const auto currentNotesById = cwSyncIdUtils::buildUniqueIdPointerMap(
        noteLiDARModel->notes(),
        [](QObject* noteObject) { return qobject_cast<cwNoteLiDAR*>(noteObject); },
        [](const cwNoteLiDAR* note) { return note->id(); });
    if (!currentNotesById.has_value()) {
        return Monad::Result<cwNoteLiDARMergePreparation>(
            QStringLiteral("Ambiguous current LiDAR note ids."));
    }

    cwNoteLiDARMergePreparation preparation;
    preparation.orderedNotes.reserve(loadedNoteLiDARModelData.notes.size());
    preparation.plans.reserve(loadedNoteLiDARModelData.notes.size());

    for (const cwNoteLiDARData& loadedNoteData : loadedNoteLiDARModelData.notes) {
        auto currentNoteIt = currentNotesById->constFind(loadedNoteData.id);
        if (currentNoteIt == currentNotesById->constEnd()) {
            return Monad::Result<cwNoteLiDARMergePreparation>(
                QStringLiteral("Missing current LiDAR note object for incremental merge."));
        }

        cwNoteLiDARMergePlan plan;
        plan.currentNote = *currentNoteIt;
        plan.loadedNoteData = &loadedNoteData;

        const auto baseIt = baseNoteLiDARByNoteId.constFind(loadedNoteData.id);
        if (baseIt != baseNoteLiDARByNoteId.constEnd()) {
            plan.baseNoteData = *baseIt;
        }

        preparation.plans.append(plan);
        preparation.orderedNotes.append(plan.currentNote);
    }

    return Monad::Result<cwNoteLiDARMergePreparation>(preparation);
}
