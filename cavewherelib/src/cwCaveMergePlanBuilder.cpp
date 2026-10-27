#include "cwCaveMergePlanBuilder.h"

#include "cwCave.h"
#include "cwSyncIdUtils.h"

#include <QSet>

namespace {

//! Appends a plan for \a currentCave against \a loadedCaveData, then recurses into the
//! loaded child nodes, matching each to a live child by id the way trips are matched. A
//! node already planned (its own descriptor changed too) is planned once.
//! \a changedIds names the nodes whose own descriptor the merge changed.
void appendPlanTree(cwCave* currentCave,
                    const cwCaveData* loadedCaveData,
                    const QHash<QUuid, cwCaveData>& baseCaveById,
                    const QSet<QUuid>& changedIds,
                    QSet<QUuid>* plannedIds,
                    QList<cwCaveMergePlan>* plans)
{
    if (plannedIds->contains(loadedCaveData->id)) {
        return;
    }
    plannedIds->insert(loadedCaveData->id);

    cwCaveMergePlan plan;
    plan.currentCave = currentCave;
    plan.loadedCaveData = loadedCaveData;

    const auto baseCaveIt = baseCaveById.constFind(loadedCaveData->id);
    if (baseCaveIt != baseCaveById.constEnd()) {
        plan.baseCaveData = *baseCaveIt;
    } else if (!changedIds.contains(loadedCaveData->id)) {
        // Reached by recursion from a changed ancestor: this node's own descriptor did
        // not change, so the loaded values are the merge base. Saying so keeps the live
        // values instead of letting the loaded copy win every scalar untouched by the merge.
        plan.baseCaveData = *loadedCaveData;
    }

    plans->append(std::move(plan));

    QHash<QUuid, cwCave*> currentChildrenById;
    const QList<cwSurveyNode*> childNodes = currentCave->childNodes();
    for (cwSurveyNode* childNode : childNodes) {
        auto* childCave = qobject_cast<cwCave*>(childNode);
        if (childCave != nullptr && !childCave->id().isNull()) {
            currentChildrenById.insert(childCave->id(), childCave);
        }
    }

    for (const cwCaveData& loadedChild : loadedCaveData->nodes) {
        if (loadedChild.id.isNull()) {
            continue;
        }

        const auto currentChildIt = currentChildrenById.constFind(loadedChild.id);
        if (currentChildIt == currentChildrenById.constEnd()) {
            continue;
        }

        appendPlanTree(currentChildIt.value(), &loadedChild, baseCaveById, changedIds, plannedIds, plans);
    }
}

} // namespace

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

    QSet<QUuid> seenLoadedIds;
    for (const cwCaveData* loadedCaveData : loadedCaves) {
        if (loadedCaveData == nullptr
            || loadedCaveData->id.isNull()
            || seenLoadedIds.contains(loadedCaveData->id)) {
            return Monad::Result<cwCaveMergePreparation>(QStringLiteral("Ambiguous loaded cave ids."));
        }
        seenLoadedIds.insert(loadedCaveData->id);

        if (currentCavesById->constFind(loadedCaveData->id) == currentCavesById->constEnd()) {
            return Monad::Result<cwCaveMergePreparation>(
                QStringLiteral("Missing current cave object for incremental merge."));
        }
    }

    cwCaveMergePreparation preparation;
    preparation.plans.reserve(loadedCaves.size());

    QSet<QUuid> plannedIds;
    for (const cwCaveData* loadedCaveData : loadedCaves) {
        appendPlanTree(currentCavesById->value(loadedCaveData->id),
                       loadedCaveData,
                       baseCaveById,
                       seenLoadedIds,
                       &plannedIds,
                       &preparation.plans);
    }

    return Monad::Result<cwCaveMergePreparation>(preparation);
}
