#include "cwScrapMergeApplier.h"

#include "cwImageResolution.h"
#include "cwNote.h"
#include "cwProjectedProfileScrapViewMatrix.h"
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

bool pointsAlmostEqual(const QPointF& lhs, const QPointF& rhs)
{
    return almostEqual(lhs.x(), rhs.x()) && almostEqual(lhs.y(), rhs.y());
}

QVector<QPointF> normalizeOutline(const QPolygonF& outline)
{
    QVector<QPointF> normalized = outline.toVector();
    if (normalized.size() >= 2 && pointsAlmostEqual(normalized.first(), normalized.last())) {
        normalized.removeLast();
    }
    return normalized;
}

bool areStationsEquivalent(const cwNoteStation& lhs, const cwNoteStation& rhs)
{
    return lhs.name() == rhs.name()
           && pointsAlmostEqual(lhs.positionOnNote(), rhs.positionOnNote());
}

bool areLeadsEquivalent(const cwLead& lhs, const cwLead& rhs)
{
    return lhs.desciption() == rhs.desciption()
           && pointsAlmostEqual(lhs.positionOnNote(), rhs.positionOnNote())
           && almostEqual(lhs.size().width(), rhs.size().width())
           && almostEqual(lhs.size().height(), rhs.size().height())
           && lhs.completed() == rhs.completed();
}

bool areGeometryEquivalent(const cwScrapBaseIdentityData::GeometryData& lhs,
                           const cwScrapBaseIdentityData::GeometryData& rhs)
{
    const QVector<QPointF> lhsOutline = normalizeOutline(lhs.outlinePoints);
    const QVector<QPointF> rhsOutline = normalizeOutline(rhs.outlinePoints);
    if (lhsOutline.size() != rhsOutline.size()) {
        return false;
    }
    for (int i = 0; i < lhsOutline.size(); ++i) {
        if (!pointsAlmostEqual(lhsOutline[i], rhsOutline[i])) {
            return false;
        }
    }

    return true;
}

bool areTransformBundleEquivalent(const cwScrapBaseIdentityData::GeometryData::TransformBundle& lhs,
                                  const cwScrapBaseIdentityData::GeometryData::TransformBundle& rhs)
{
    if (!almostEqual(lhs.noteTransformation.north, rhs.noteTransformation.north)) {
        return false;
    }

    const auto& lhsNum = lhs.noteTransformation.scale.scaleNumerator;
    const auto& rhsNum = rhs.noteTransformation.scale.scaleNumerator;
    if (!almostEqual(lhsNum.value, rhsNum.value)
        || lhsNum.unit != rhsNum.unit
        || lhsNum.updateValueWhenUnitChanged != rhsNum.updateValueWhenUnitChanged) {
        return false;
    }

    const auto& lhsDen = lhs.noteTransformation.scale.scaleDenominator;
    const auto& rhsDen = rhs.noteTransformation.scale.scaleDenominator;
    if (!almostEqual(lhsDen.value, rhsDen.value)
        || lhsDen.unit != rhsDen.unit
        || lhsDen.updateValueWhenUnitChanged != rhsDen.updateValueWhenUnitChanged) {
        return false;
    }

    if (lhs.calculateNoteTransform != rhs.calculateNoteTransform
        || lhs.viewType != rhs.viewType
        || lhs.hasProjectedProfileView != rhs.hasProjectedProfileView) {
        return false;
    }

    if (lhs.hasProjectedProfileView) {
        if (!almostEqual(lhs.projectedAzimuth, rhs.projectedAzimuth)
            || lhs.projectedDirection != rhs.projectedDirection) {
            return false;
        }
    }

    return true;
}

cwScrapBaseIdentityData::GeometryData geometryDataFromScrapData(const cwScrapData& scrapData)
{
    cwScrapBaseIdentityData::GeometryData geometry;
    geometry.outlinePoints = scrapData.outlinePoints;
    geometry.transform.noteTransformation = scrapData.noteTransformation;
    geometry.transform.calculateNoteTransform = scrapData.calculateNoteTransform;

    if (scrapData.viewMatrix) {
        geometry.transform.viewType = static_cast<cwScrapType::Type>(scrapData.viewMatrix->type());
        if (scrapData.viewMatrix->type() == cwAbstractScrapViewMatrix::ProjectedProfile) {
            const auto* projectedData =
                dynamic_cast<const cwProjectedProfileScrapViewMatrix::Data*>(scrapData.viewMatrix.get());
            if (projectedData != nullptr) {
                geometry.transform.hasProjectedProfileView = true;
                geometry.transform.projectedAzimuth = projectedData->azimuth();
                geometry.transform.projectedDirection = static_cast<int>(projectedData->direction());
            }
        }
    } else {
        geometry.transform.viewType = cwScrapType::Plan;
    }

    return geometry;
}

void applyOutlineFrom(const cwScrapData& source, cwScrapData& destination)
{
    destination.outlinePoints = source.outlinePoints;
}

void applyTransformFrom(const cwScrapData& source, cwScrapData& destination)
{
    destination.noteTransformation = source.noteTransformation;
    destination.calculateNoteTransform = source.calculateNoteTransform;
    destination.viewMatrix = source.viewMatrix
                                 ? std::unique_ptr<cwAbstractScrapViewMatrix::Data>(source.viewMatrix->clone())
                                 : nullptr;
}

struct MergedScrapDataResult {
    cwScrapData data;
    bool geometryConflictKeptOurs = false;
};

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
            if (baseItemsById != nullptr && baseItemsById->contains(id)) {
                const ItemT baseItem = baseItemsById->value(id);
                const bool oursChanged = !itemEquals(oursItem, baseItem);
                if (oursChanged) {
                    // Remote delete vs local modify keeps the local value.
                    merged.append(oursItem);
                }
                // Remote-only delete with no local change keeps the delete.
            } else {
                // Local-only add or keep without base payload preserves ours.
                merged.append(oursItem);
            }
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

MergedScrapDataResult mergedScrapDataPreferOursForStationsAndLeads(const cwScrapData* currentScrapData,
                                                                   const cwScrapData& loadedScrapData,
                                                                   const cwScrapBaseIdentityData* baseScrapIdentity,
                                                                   cwReconcileApplyMode applyMode,
                                                                   const QUuid& scrapId)
{
    if (applyMode == cwReconcileApplyMode::TargetCommitWins) {
        return MergedScrapDataResult{loadedScrapData, false};
    }

    if (currentScrapData == nullptr) {
        return MergedScrapDataResult{loadedScrapData, false};
    }

    const QSet<QUuid>* baseStationIds = baseScrapIdentity != nullptr ? &baseScrapIdentity->stationIds : nullptr;
    const QSet<QUuid>* baseLeadIds = baseScrapIdentity != nullptr ? &baseScrapIdentity->leadIds : nullptr;
    const QHash<QUuid, cwNoteStation>* baseStationsById = baseScrapIdentity != nullptr ? &baseScrapIdentity->stationsById : nullptr;
    const QHash<QUuid, cwLead>* baseLeadsById = baseScrapIdentity != nullptr ? &baseScrapIdentity->leadsById : nullptr;
    cwScrapData mergedData = loadedScrapData;
    mergedData.stations = mergeUnorderedByIdPreferOurs(
        currentScrapData->stations,
        loadedScrapData.stations,
        baseStationIds,
        baseStationsById,
        [](const cwNoteStation& lhs, const cwNoteStation& rhs) { return areStationsEquivalent(lhs, rhs); },
        [](const cwNoteStation& station) { return station.id(); });
    mergedData.leads = mergeUnorderedByIdPreferOurs(
        currentScrapData->leads,
        loadedScrapData.leads,
        baseLeadIds,
        baseLeadsById,
        [](const cwLead& lhs, const cwLead& rhs) { return areLeadsEquivalent(lhs, rhs); },
        [](const cwLead& lead) { return lead.id(); });

    bool geometryConflictKeptOurs = false;
    if (baseScrapIdentity != nullptr && baseScrapIdentity->hasGeometryData) {
        const auto currentGeometry = geometryDataFromScrapData(*currentScrapData);
        const auto loadedGeometry = geometryDataFromScrapData(loadedScrapData);
        const bool oursOutlineChanged = !areGeometryEquivalent(currentGeometry, baseScrapIdentity->geometry);
        const bool loadedOutlineChanged = !areGeometryEquivalent(loadedGeometry, baseScrapIdentity->geometry);
        const bool oursTransformChanged =
            !areTransformBundleEquivalent(currentGeometry.transform, baseScrapIdentity->geometry.transform);
        const bool loadedTransformChanged =
            !areTransformBundleEquivalent(loadedGeometry.transform, baseScrapIdentity->geometry.transform);

        if (oursOutlineChanged && loadedOutlineChanged) {
            applyOutlineFrom(*currentScrapData, mergedData);
            geometryConflictKeptOurs = true;
        } else if (oursOutlineChanged && !loadedOutlineChanged) {
            applyOutlineFrom(*currentScrapData, mergedData);
        } else if (!oursOutlineChanged && loadedOutlineChanged) {
            applyOutlineFrom(loadedScrapData, mergedData);
        }

        if (oursTransformChanged && loadedTransformChanged) {
            applyTransformFrom(*currentScrapData, mergedData);
            geometryConflictKeptOurs = true;
        } else if (oursTransformChanged && !loadedTransformChanged) {
            applyTransformFrom(*currentScrapData, mergedData);
        } else if (!oursTransformChanged && loadedTransformChanged) {
            applyTransformFrom(loadedScrapData, mergedData);
        }
    }

    return MergedScrapDataResult{std::move(mergedData), geometryConflictKeptOurs};
}

} // namespace

Monad::Result<cwScrapMergeApplyResult> cwScrapMergeApplier::applyNoteStructuralMergePlan(
    const cwNoteStructuralMergePlan& plan)
{
    Q_ASSERT(plan.note != nullptr);
    Q_ASSERT(plan.loadedNoteData != nullptr);
    if (plan.note == nullptr || plan.loadedNoteData == nullptr) {
        return Monad::Result<cwScrapMergeApplyResult>(
            QStringLiteral("Note structural merge plan is missing required objects."));
    }

    cwNote* const note = plan.note;
    const cwNoteData& loadedNoteData = *plan.loadedNoteData;

    const QList<cwScrap*> currentScraps = note->scraps();
    QHash<QUuid, const cwScrapData*> loadedScrapsById;
    loadedScrapsById.reserve(loadedNoteData.scraps.size());
    for (const cwScrapData& loadedScrapData : loadedNoteData.scraps) {
        loadedScrapsById.insert(loadedScrapData.id, &loadedScrapData);
    }
    QHash<QUuid, cwScrapData> currentScrapDataById;
    currentScrapDataById.reserve(currentScraps.size());
    for (cwScrap* scrap : currentScraps) {
        if (scrap == nullptr) {
            continue;
        }
        currentScrapDataById.insert(scrap->id(), scrap->data());
    }

    // Validate the planned mapping up front so apply is all-or-nothing.
    QSet<QUuid> seenOrderedIds;
    for (const QUuid& orderedId : plan.mergedScrapOrder) {
        if (orderedId.isNull() || seenOrderedIds.contains(orderedId)) {
            return Monad::Result<cwScrapMergeApplyResult>(
                QStringLiteral("Note structural merge plan contains ambiguous scrap ids."));
        }
        seenOrderedIds.insert(orderedId);
        if (!loadedScrapsById.contains(orderedId)) {
            return Monad::Result<cwScrapMergeApplyResult>(
                QStringLiteral("Note structural merge plan references missing loaded scrap data."));
        }
    }
    for (cwScrap* scrap : currentScraps) {
        if (scrap == nullptr) {
            return Monad::Result<cwScrapMergeApplyResult>(
                QStringLiteral("Note structural merge plan references null current scrap object."));
        }
    }

    bool geometryConflictKeptOurs = false;
    const int currentCount = currentScraps.size();
    const int loadedCount = static_cast<int>(plan.mergedScrapOrder.size());
    const int sharedCount = std::min(currentCount, loadedCount);
    for (int index = 0; index < sharedCount; ++index) {
        cwScrap* const scrap = currentScraps.at(index);
        const QUuid targetScrapId = plan.mergedScrapOrder.at(static_cast<size_t>(index));
        const cwScrapData* loadedScrapData = loadedScrapsById.value(targetScrapId, nullptr);
        Q_ASSERT(scrap != nullptr);
        Q_ASSERT(loadedScrapData != nullptr);

        const auto baseIdentityIt = plan.baseScrapIdentityByScrapId.constFind(targetScrapId);
        const cwScrapBaseIdentityData* baseIdentity =
            (baseIdentityIt != plan.baseScrapIdentityByScrapId.constEnd()) ? &baseIdentityIt.value() : nullptr;
        const auto currentScrapDataIt = currentScrapDataById.constFind(targetScrapId);
        const cwScrapData* currentScrapDataForId =
            currentScrapDataIt != currentScrapDataById.constEnd() ? &currentScrapDataIt.value() : nullptr;
        const auto mergedResult =
            mergedScrapDataPreferOursForStationsAndLeads(currentScrapDataForId,
                                                         *loadedScrapData,
                                                         baseIdentity,
                                                         plan.applyMode,
                                                         targetScrapId);
        scrap->setData(mergedResult.data);
        geometryConflictKeptOurs = geometryConflictKeptOurs || mergedResult.geometryConflictKeptOurs;
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
            const auto baseIdentityIt = plan.baseScrapIdentityByScrapId.constFind(targetScrapId);
            const cwScrapBaseIdentityData* baseIdentity =
                (baseIdentityIt != plan.baseScrapIdentityByScrapId.constEnd()) ? &baseIdentityIt.value() : nullptr;
            const auto currentScrapDataIt = currentScrapDataById.constFind(targetScrapId);
            const cwScrapData* currentScrapDataForId =
                currentScrapDataIt != currentScrapDataById.constEnd() ? &currentScrapDataIt.value() : nullptr;
            const auto mergedResult =
                mergedScrapDataPreferOursForStationsAndLeads(currentScrapDataForId,
                                                             *loadedScrapData,
                                                             baseIdentity,
                                                             plan.applyMode,
                                                             targetScrapId);
            scrap->setData(mergedResult.data);
            geometryConflictKeptOurs = geometryConflictKeptOurs || mergedResult.geometryConflictKeptOurs;
        }
    }

    cwScrapMergeApplyResult result;
    result.geometryConflictKeptOurs = geometryConflictKeptOurs;
    return Monad::Result<cwScrapMergeApplyResult>(result);
}
