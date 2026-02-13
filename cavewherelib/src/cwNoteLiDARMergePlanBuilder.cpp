#include "cwNoteLiDARMergePlanBuilder.h"

#include "cwNoteLiDAR.h"
#include "cwSurveyNoteLiDARModel.h"

#include <QSet>

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

bool haveSameIdsIgnoringOrder(const std::vector<QUuid>& currentIds, const std::vector<QUuid>& loadedIds)
{
    if (currentIds.size() != loadedIds.size()) {
        return false;
    }

    QSet<QUuid> currentIdSet(currentIds.begin(), currentIds.end());
    QSet<QUuid> loadedIdSet(loadedIds.begin(), loadedIds.end());
    return currentIdSet == loadedIdSet;
}

} // namespace

cwNoteLiDARDescriptorApplyMode cwNoteLiDARMergePlanBuilder::determineApplyMode(
    cwSurveyNoteLiDARModel* noteLiDARModel,
    const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData)
{
    if (noteLiDARModel == nullptr) {
        return cwNoteLiDARDescriptorApplyMode::Ambiguous;
    }

    const auto currentNoteIds = collectOrderedUniqueIds(
        noteLiDARModel->notes(),
        [](const QObject* noteObject) {
            const auto* note = qobject_cast<const cwNoteLiDAR*>(noteObject);
            return note != nullptr ? note->id() : QUuid();
        });
    const auto loadedNoteIds = collectOrderedUniqueIds(
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

    if (haveSameIdsIgnoringOrder(currentNoteIds.value(), loadedNoteIds.value())) {
        return cwNoteLiDARDescriptorApplyMode::IncrementalMerge;
    }

    return cwNoteLiDARDescriptorApplyMode::FullModelReplace;
}

std::optional<cwNoteLiDARMergePreparation> cwNoteLiDARMergePlanBuilder::build(
    cwSurveyNoteLiDARModel* noteLiDARModel,
    const cwSurveyNoteLiDARModelData& loadedNoteLiDARModelData,
    const QHash<QUuid, cwNoteLiDARData>& baseNoteLiDARByNoteId,
    QString* failureReason)
{
    if (noteLiDARModel == nullptr) {
        if (failureReason != nullptr) {
            *failureReason = QStringLiteral("LiDAR note model is null.");
        }
        return std::nullopt;
    }

    QHash<QUuid, cwNoteLiDAR*> currentNotesById;
    for (QObject* noteObject : noteLiDARModel->notes()) {
        auto* note = qobject_cast<cwNoteLiDAR*>(noteObject);
        if (note == nullptr || note->id().isNull() || currentNotesById.contains(note->id())) {
            if (failureReason != nullptr) {
                *failureReason = QStringLiteral("Ambiguous current LiDAR note ids.");
            }
            return std::nullopt;
        }
        currentNotesById.insert(note->id(), note);
    }

    cwNoteLiDARMergePreparation preparation;
    preparation.orderedNotes.reserve(loadedNoteLiDARModelData.notes.size());
    preparation.plans.reserve(loadedNoteLiDARModelData.notes.size());

    for (const cwNoteLiDARData& loadedNoteData : loadedNoteLiDARModelData.notes) {
        auto currentNoteIt = currentNotesById.constFind(loadedNoteData.id);
        if (currentNoteIt == currentNotesById.constEnd()) {
            if (failureReason != nullptr) {
                *failureReason = QStringLiteral("Missing current LiDAR note object for incremental merge.");
            }
            return std::nullopt;
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

    return preparation;
}
