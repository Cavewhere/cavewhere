#include "cwScrapMergeApplier.h"

#include "cwImageResolution.h"
#include "cwNote.h"
#include "cwScrap.h"

#include <QHash>

#include <algorithm>

void cwScrapMergeApplier::applyNoteStructuralMergePlan(const cwNoteStructuralMergePlan& plan)
{
    Q_ASSERT(plan.note != nullptr);
    Q_ASSERT(plan.loadedNoteData != nullptr);
    if (plan.note == nullptr || plan.loadedNoteData == nullptr) {
        return;
    }

    cwNote* const note = plan.note;
    const cwNoteData& loadedNoteData = *plan.loadedNoteData;

    note->setName(loadedNoteData.name);
    note->setId(loadedNoteData.id);
    note->setRotate(loadedNoteData.rotate);
    note->setImage(loadedNoteData.image);
    note->imageResolution()->setData(loadedNoteData.imageResolution);

    const QList<cwScrap*> currentScraps = note->scraps();
    QHash<QUuid, const cwScrapData*> loadedScrapsById;
    loadedScrapsById.reserve(loadedNoteData.scraps.size());
    for (const cwScrapData& loadedScrapData : loadedNoteData.scraps) {
        loadedScrapsById.insert(loadedScrapData.id, &loadedScrapData);
    }

    const int currentCount = currentScraps.size();
    const int loadedCount = static_cast<int>(plan.mergedScrapOrder.size());
    const int sharedCount = std::min(currentCount, loadedCount);
    for (int index = 0; index < sharedCount; ++index) {
        cwScrap* const scrap = currentScraps.at(index);
        const QUuid targetScrapId = plan.mergedScrapOrder.at(static_cast<size_t>(index));
        const cwScrapData* loadedScrapData = loadedScrapsById.value(targetScrapId, nullptr);
        Q_ASSERT(scrap != nullptr);
        Q_ASSERT(loadedScrapData != nullptr);
        if (scrap == nullptr || loadedScrapData == nullptr) {
            continue;
        }

        scrap->setData(*loadedScrapData);
    }

    if (currentCount > loadedCount) {
        note->removeScraps(loadedCount, currentCount - 1);
    } else if (loadedCount > currentCount) {
        for (int index = currentCount; index < loadedCount; ++index) {
            auto* scrap = new cwScrap();
            note->addScrap(scrap);
            const QUuid targetScrapId = plan.mergedScrapOrder.at(static_cast<size_t>(index));
            const cwScrapData* loadedScrapData = loadedScrapsById.value(targetScrapId, nullptr);
            Q_ASSERT(loadedScrapData != nullptr);
            if (loadedScrapData != nullptr) {
                scrap->setData(*loadedScrapData);
            }
        }
    }
}
