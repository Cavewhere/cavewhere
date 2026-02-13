#include "cwCaveMergePlanBuilder.h"

#include "cwCave.h"
#include "cwSyncIdUtils.h"

#include <QSet>

Monad::Result<cwCaveMergePreparation> cwCaveMergePlanBuilder::build(
    const QList<cwCave*>& currentCaves,
    const QList<const cwCaveData*>& loadedCaves,
    const QHash<QUuid, cwCaveData>& baseCaveById)
{
    const auto currentCavesById = cwSyncIdUtils::buildUniqueIdPointerMap(
        currentCaves,
        [](cwCave* cave) { return cave; },
        [](const cwCave* cave) { return cave->id(); });
    if (!currentCavesById.has_value()) {
        return Monad::Result<cwCaveMergePreparation>(QStringLiteral("Ambiguous current cave ids."));
    }

    cwCaveMergePreparation preparation;
    preparation.plans.reserve(loadedCaves.size());

    QSet<QUuid> seenLoadedIds;
    for (const cwCaveData* loadedCaveData : loadedCaves) {
        if (loadedCaveData == nullptr
            || loadedCaveData->id.isNull()
            || seenLoadedIds.contains(loadedCaveData->id)) {
            return Monad::Result<cwCaveMergePreparation>(QStringLiteral("Ambiguous loaded cave ids."));
        }
        seenLoadedIds.insert(loadedCaveData->id);

        const auto currentCaveIt = currentCavesById->constFind(loadedCaveData->id);
        if (currentCaveIt == currentCavesById->constEnd()) {
            return Monad::Result<cwCaveMergePreparation>(
                QStringLiteral("Missing current cave object for incremental merge."));
        }

        cwCaveMergePlan plan;
        plan.currentCave = *currentCaveIt;
        plan.loadedCaveData = loadedCaveData;

        const auto baseCaveIt = baseCaveById.constFind(loadedCaveData->id);
        if (baseCaveIt != baseCaveById.constEnd()) {
            plan.baseCaveData = *baseCaveIt;
        }

        preparation.plans.append(std::move(plan));
    }

    return Monad::Result<cwCaveMergePreparation>(preparation);
}
