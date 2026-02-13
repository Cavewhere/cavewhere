#include "cwScrapMergeApplier.h"

#include "cwImageResolution.h"
#include "cwNote.h"
#include "cwScrap.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <functional>

namespace {

bool almostEqual(qreal lhs, qreal rhs)
{
    return qAbs(lhs - rhs) <= 1.0e-6;
}

bool areStationsEquivalent(const cwNoteStation& lhs, const cwNoteStation& rhs)
{
    return lhs.name() == rhs.name()
           && almostEqual(lhs.positionOnNote().x(), rhs.positionOnNote().x())
           && almostEqual(lhs.positionOnNote().y(), rhs.positionOnNote().y());
}

bool areLeadsEquivalent(const cwLead& lhs, const cwLead& rhs)
{
    return lhs.desciption() == rhs.desciption()
           && almostEqual(lhs.positionOnNote().x(), rhs.positionOnNote().x())
           && almostEqual(lhs.positionOnNote().y(), rhs.positionOnNote().y())
           && almostEqual(lhs.size().width(), rhs.size().width())
           && almostEqual(lhs.size().height(), rhs.size().height())
           && lhs.completed() == rhs.completed();
}

template<typename ItemT, typename ItemEqualsFn, typename IdAccessor>
QList<ItemT> mergeUnorderedByIdPreferOurs(const QList<ItemT>& ours,
                                          const QList<ItemT>& loaded,
                                          const QSet<QUuid>* baseIds,
                                          const QHash<QUuid, ItemT>* baseItemsById,
                                          ItemEqualsFn itemEquals,
                                          IdAccessor idAccessor)
{
    QHash<QUuid, ItemT> oursById;
    oursById.reserve(ours.size());
    for (const ItemT& item : ours) {
        const QUuid id = idAccessor(item);
        if (id.isNull() || oursById.contains(id)) {
            return loaded;
        }
        oursById.insert(id, item);
    }

    QHash<QUuid, ItemT> loadedById;
    loadedById.reserve(loaded.size());
    for (const ItemT& item : loaded) {
        const QUuid id = idAccessor(item);
        if (id.isNull() || loadedById.contains(id)) {
            return loaded;
        }
        loadedById.insert(id, item);
    }

    QList<ItemT> merged;
    merged.reserve(std::max(ours.size(), loaded.size()));

    for (const ItemT& oursItem : ours) {
        const QUuid id = idAccessor(oursItem);
        if (!loadedById.contains(id)) {
            // Local-only id means local add or local keep; both preserve ours.
            merged.append(oursItem);
            continue;
        }

        const ItemT loadedItem = loadedById.value(id);
        if (baseItemsById != nullptr && baseItemsById->contains(id)) {
            const ItemT baseItem = baseItemsById->value(id);
            const bool oursChanged = !itemEquals(oursItem, baseItem);
            const bool loadedChanged = !itemEquals(loadedItem, baseItem);
            if (!oursChanged && loadedChanged) {
                // Remote-only change.
                merged.append(loadedItem);
            } else if (oursChanged && !loadedChanged) {
                // Local-only change.
                merged.append(oursItem);
            } else if (!oursChanged && !loadedChanged) {
                merged.append(oursItem);
            } else {
                // Conflict changed on both sides.
                merged.append(oursItem);
            }
        } else {
            // Without base item payload we preserve local value on shared ids.
            merged.append(oursItem);
        }
    }

    for (const ItemT& loadedItem : loaded) {
        const QUuid id = idAccessor(loadedItem);
        if (oursById.contains(id)) {
            continue;
        }

        // Remote-only id present in base implies local delete vs remote modify -> keep delete.
        if (baseIds != nullptr && baseIds->contains(id)) {
            continue;
        }

        // Concurrent remote add with distinct id.
        merged.append(loadedItem);
    }

    return merged;
}

cwScrapData mergedScrapDataPreferOursForStationsAndLeads(const cwScrap* currentScrap,
                                                         const cwScrapData& loadedScrapData,
                                                         const cwScrapBaseIdentityData* baseScrapIdentity)
{
    if (currentScrap == nullptr) {
        return loadedScrapData;
    }

    const QSet<QUuid>* baseStationIds = baseScrapIdentity != nullptr ? &baseScrapIdentity->stationIds : nullptr;
    const QSet<QUuid>* baseLeadIds = baseScrapIdentity != nullptr ? &baseScrapIdentity->leadIds : nullptr;
    const QHash<QUuid, cwNoteStation>* baseStationsById = baseScrapIdentity != nullptr ? &baseScrapIdentity->stationsById : nullptr;
    const QHash<QUuid, cwLead>* baseLeadsById = baseScrapIdentity != nullptr ? &baseScrapIdentity->leadsById : nullptr;
    cwScrapData mergedData = loadedScrapData;
    mergedData.stations = mergeUnorderedByIdPreferOurs(
        currentScrap->stations(),
        loadedScrapData.stations,
        baseStationIds,
        baseStationsById,
        [](const cwNoteStation& lhs, const cwNoteStation& rhs) { return areStationsEquivalent(lhs, rhs); },
        [](const cwNoteStation& station) { return station.id(); });
    mergedData.leads = mergeUnorderedByIdPreferOurs(
        currentScrap->leads(),
        loadedScrapData.leads,
        baseLeadIds,
        baseLeadsById,
        [](const cwLead& lhs, const cwLead& rhs) { return areLeadsEquivalent(lhs, rhs); },
        [](const cwLead& lead) { return lead.id(); });
    return mergedData;
}

} // namespace

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

        const auto baseIdentityIt = plan.baseScrapIdentityByScrapId.constFind(targetScrapId);
        const cwScrapBaseIdentityData* baseIdentity =
            (baseIdentityIt != plan.baseScrapIdentityByScrapId.constEnd()) ? &baseIdentityIt.value() : nullptr;
        scrap->setData(mergedScrapDataPreferOursForStationsAndLeads(scrap, *loadedScrapData, baseIdentity));
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
                const auto baseIdentityIt = plan.baseScrapIdentityByScrapId.constFind(targetScrapId);
                const cwScrapBaseIdentityData* baseIdentity =
                    (baseIdentityIt != plan.baseScrapIdentityByScrapId.constEnd()) ? &baseIdentityIt.value() : nullptr;
                scrap->setData(mergedScrapDataPreferOursForStationsAndLeads(scrap, *loadedScrapData, baseIdentity));
            }
        }
    }
}
