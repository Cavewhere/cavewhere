#include "cwNoteLiDARMergeApplier.h"

#include "cwNoteLiDAR.h"
#include "cwSyncMergeApplyUtils.h"

#include <QHash>
#include <QSet>

namespace {

bool lengthsEqual(const cwLength::Data& lhs, const cwLength::Data& rhs)
{
    return lhs.unit == rhs.unit
           && qFuzzyCompare(1.0 + lhs.value, 1.0 + rhs.value)
           && lhs.updateValueWhenUnitChanged == rhs.updateValueWhenUnitChanged;
}

bool scalesEqual(const cwScale::Data& lhs, const cwScale::Data& rhs)
{
    return lengthsEqual(lhs.scaleNumerator, rhs.scaleNumerator)
           && lengthsEqual(lhs.scaleDenominator, rhs.scaleDenominator);
}

bool lidarTransformsEqual(const cwNoteLiDARTransformationData& lhs,
                          const cwNoteLiDARTransformationData& rhs,
                          bool ignoreNorth)
{
    return (ignoreNorth || qFuzzyCompare(1.0 + lhs.north, 1.0 + rhs.north))
           && scalesEqual(lhs.scale, rhs.scale)
           && lhs.upMode == rhs.upMode
           && qFuzzyCompare(1.0f + lhs.upSign, 1.0f + rhs.upSign)
           && lhs.upRotation == rhs.upRotation;
}

bool lidarStationsEqual(const cwNoteLiDARStation& lhs, const cwNoteLiDARStation& rhs)
{
    return lhs.id() == rhs.id()
           && lhs.name() == rhs.name()
           && lhs.positionOnNote() == rhs.positionOnNote();
}

std::optional<QHash<QUuid, cwNoteLiDARStation>> keyedStationsById(const QList<cwNoteLiDARStation>& stations)
{
    QHash<QUuid, cwNoteLiDARStation> byId;
    byId.reserve(stations.size());
    for (const cwNoteLiDARStation& station : stations) {
        const QUuid key = station.id();
        if (key.isNull() || byId.contains(key)) {
            return std::nullopt;
        }
        byId.insert(key, station);
    }
    return byId;
}

std::optional<QList<cwNoteLiDARStation>> mergeStationsById(const QList<cwNoteLiDARStation>& currentStations,
                                                           const QList<cwNoteLiDARStation>& loadedStations,
                                                           const std::optional<QList<cwNoteLiDARStation>>& baseStations,
                                                           cwSyncMergeApplyUtils::ApplyMode applyMode)
{
    if (!baseStations.has_value()) {
        if (!keyedStationsById(loadedStations).has_value()) {
            return std::nullopt;
        }
        return applyMode == cwSyncMergeApplyUtils::ApplyMode::LoadedWins
                   ? loadedStations
                   : loadedStations;
    }

    const auto currentById = keyedStationsById(currentStations);
    const auto loadedById = keyedStationsById(loadedStations);
    const auto baseById = keyedStationsById(*baseStations);
    if (!currentById.has_value() || !loadedById.has_value() || !baseById.has_value()) {
        return std::nullopt;
    }

    QList<QUuid> orderedIds;
    orderedIds.reserve(currentStations.size() + loadedStations.size());
    QSet<QUuid> seenIds;
    for (const cwNoteLiDARStation& station : currentStations) {
        const QUuid id = station.id();
        if (!seenIds.contains(id)) {
            seenIds.insert(id);
            orderedIds.append(id);
        }
    }
    for (const cwNoteLiDARStation& station : loadedStations) {
        const QUuid id = station.id();
        if (!seenIds.contains(id)) {
            seenIds.insert(id);
            orderedIds.append(id);
        }
    }

    QList<cwNoteLiDARStation> mergedStations;
    mergedStations.reserve(orderedIds.size());
    for (const QUuid& id : orderedIds) {
        const auto currentIt = currentById->constFind(id);
        const auto loadedIt = loadedById->constFind(id);
        const auto baseIt = baseById->constFind(id);

        const bool hasCurrent = currentIt != currentById->constEnd();
        const bool hasLoaded = loadedIt != loadedById->constEnd();
        const bool hasBase = baseIt != baseById->constEnd();

        if (hasCurrent && hasLoaded) {
            const std::optional<cwNoteLiDARStation> baseStation = hasBase
                                                                      ? std::optional<cwNoteLiDARStation>(*baseIt)
                                                                      : std::nullopt;
            mergedStations.append(cwSyncMergeApplyUtils::chooseBundleValue(
                *currentIt,
                *loadedIt,
                baseStation,
                [](const cwNoteLiDARStation& lhs, const cwNoteLiDARStation& rhs) {
                    return lidarStationsEqual(lhs, rhs);
                },
                applyMode));
            continue;
        }

        if (hasCurrent) {
            if (applyMode == cwSyncMergeApplyUtils::ApplyMode::LoadedWins) {
                continue;
            }

            if (hasBase && lidarStationsEqual(*currentIt, *baseIt)) {
                continue;
            }
            mergedStations.append(*currentIt);
            continue;
        }

        if (hasLoaded) {
            if (!hasBase) {
                mergedStations.append(*loadedIt);
                continue;
            }

            if (applyMode == cwSyncMergeApplyUtils::ApplyMode::LoadedWins) {
                mergedStations.append(*loadedIt);
                continue;
            }

            if (!lidarStationsEqual(*loadedIt, *baseIt)) {
                continue;
            }
        }
    }

    return mergedStations;
}

cwNoteLiDARTransformationData currentTransformData(const cwNoteLiDAR* note)
{
    cwNoteLiDARTransformationData data = note->noteTransformation()->data();
    data.upSign = note->noteTransformation()->upSign();
    return data;
}

} // namespace

Monad::ResultBase cwNoteLiDARMergeApplier::applyNoteLiDARMergePlan(const cwNoteLiDARMergePlan& plan)
{
    if (plan.currentNote == nullptr || plan.loadedNoteData == nullptr) {
        return Monad::ResultBase(QStringLiteral("LiDAR note merge plan is missing required objects."));
    }

    const QString mergedFilename = cwSyncMergeApplyUtils::chooseBundleValue<QString>(
        plan.currentNote->filename(),
        plan.loadedNoteData->filename,
        plan.baseNoteData.has_value() ? std::optional<QString>(plan.baseNoteData->filename)
                                      : std::nullopt,
        [](const QString& lhs, const QString& rhs) {
            return lhs == rhs;
        },
        plan.applyMode);

    struct TransformBundle {
        cwNoteLiDARTransformationData transform;
        bool autoCalculateNorth = true;
    };

    const TransformBundle currentTransformBundle {
        currentTransformData(plan.currentNote),
        plan.currentNote->autoCalculateNorth()
    };
    const TransformBundle loadedTransformBundle {
        plan.loadedNoteData->transfrom,
        plan.loadedNoteData->autoCalculateNorth
    };
    const std::optional<TransformBundle> baseTransformBundle = plan.baseNoteData.has_value()
                                                                   ? std::optional<TransformBundle>(
                                                                         TransformBundle {
                                                                             plan.baseNoteData->transfrom,
                                                                             plan.baseNoteData->autoCalculateNorth
                                                                         })
                                                                   : std::nullopt;

    const TransformBundle mergedTransformBundle = cwSyncMergeApplyUtils::chooseBundleValue<TransformBundle>(
        currentTransformBundle,
        loadedTransformBundle,
        baseTransformBundle,
        [](const TransformBundle& lhs, const TransformBundle& rhs) {
            return lhs.autoCalculateNorth == rhs.autoCalculateNorth
                   && lidarTransformsEqual(lhs.transform,
                                           rhs.transform,
                                           lhs.autoCalculateNorth && rhs.autoCalculateNorth);
        },
        plan.applyMode);

    const auto mergedStations = mergeStationsById(
        plan.currentNote->stations(),
        plan.loadedNoteData->stations,
        plan.baseNoteData.has_value() ? std::optional<QList<cwNoteLiDARStation>>(plan.baseNoteData->stations)
                                      : std::nullopt,
        plan.applyMode);
    if (!mergedStations.has_value()) {
        return Monad::ResultBase(QStringLiteral("Ambiguous LiDAR station mapping during incremental merge."));
    }

    plan.currentNote->setFilename(mergedFilename);
    plan.currentNote->setAutoCalculateNorth(mergedTransformBundle.autoCalculateNorth);
    plan.currentNote->noteTransformation()->setData(mergedTransformBundle.transform);
    plan.currentNote->noteTransformation()->setUpSign(mergedTransformBundle.transform.upSign);
    plan.currentNote->setStations(*mergedStations);
    return Monad::ResultBase();
}
