#include "cwNoteLiDARMergeApplier.h"

#include "cwNoteLiDAR.h"

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

bool lidarTransformsEqual(const cwNoteLiDARTransformationData& lhs, const cwNoteLiDARTransformationData& rhs)
{
    return qFuzzyCompare(1.0 + lhs.north, 1.0 + rhs.north)
           && scalesEqual(lhs.scale, rhs.scale)
           && lhs.upMode == rhs.upMode
           && qFuzzyCompare(1.0f + lhs.upSign, 1.0f + rhs.upSign)
           && lhs.upRotation == rhs.upRotation;
}

bool lidarStationsEqual(const cwNoteLiDARStation& lhs, const cwNoteLiDARStation& rhs)
{
    return lhs.name() == rhs.name()
           && lhs.positionOnNote() == rhs.positionOnNote();
}

template<typename T, typename EqualsFn>
T chooseBundleValue(const T& currentValue,
                    const T& loadedValue,
                    const std::optional<T>& baseValue,
                    const EqualsFn& equals)
{
    if (!baseValue.has_value()) {
        return loadedValue;
    }

    const bool currentMatchesBase = equals(currentValue, *baseValue);
    const bool loadedMatchesBase = equals(loadedValue, *baseValue);

    if (currentMatchesBase && !loadedMatchesBase) {
        return loadedValue;
    }

    return currentValue;
}

std::optional<QHash<QString, cwNoteLiDARStation>> keyedStationsByName(const QList<cwNoteLiDARStation>& stations)
{
    QHash<QString, cwNoteLiDARStation> byName;
    byName.reserve(stations.size());
    for (const cwNoteLiDARStation& station : stations) {
        const QString key = station.name().trimmed();
        if (key.isEmpty() || byName.contains(key)) {
            return std::nullopt;
        }
        byName.insert(key, station);
    }
    return byName;
}

std::optional<QList<cwNoteLiDARStation>> mergeStationsByName(const QList<cwNoteLiDARStation>& currentStations,
                                                             const QList<cwNoteLiDARStation>& loadedStations,
                                                             const std::optional<QList<cwNoteLiDARStation>>& baseStations)
{
    if (!baseStations.has_value()) {
        return loadedStations;
    }

    const auto currentByName = keyedStationsByName(currentStations);
    const auto loadedByName = keyedStationsByName(loadedStations);
    const auto baseByName = keyedStationsByName(*baseStations);
    if (!currentByName.has_value() || !loadedByName.has_value() || !baseByName.has_value()) {
        return std::nullopt;
    }

    QList<QString> orderedNames;
    orderedNames.reserve(currentStations.size() + loadedStations.size());
    QSet<QString> seenNames;
    for (const cwNoteLiDARStation& station : currentStations) {
        const QString name = station.name().trimmed();
        if (!seenNames.contains(name)) {
            seenNames.insert(name);
            orderedNames.append(name);
        }
    }
    for (const cwNoteLiDARStation& station : loadedStations) {
        const QString name = station.name().trimmed();
        if (!seenNames.contains(name)) {
            seenNames.insert(name);
            orderedNames.append(name);
        }
    }

    QList<cwNoteLiDARStation> mergedStations;
    mergedStations.reserve(orderedNames.size());
    for (const QString& name : orderedNames) {
        const auto currentIt = currentByName->constFind(name);
        const auto loadedIt = loadedByName->constFind(name);
        const auto baseIt = baseByName->constFind(name);

        const bool hasCurrent = currentIt != currentByName->constEnd();
        const bool hasLoaded = loadedIt != loadedByName->constEnd();
        const bool hasBase = baseIt != baseByName->constEnd();

        if (hasCurrent && hasLoaded) {
            const std::optional<cwNoteLiDARStation> baseStation = hasBase
                                                                      ? std::optional<cwNoteLiDARStation>(*baseIt)
                                                                      : std::nullopt;
            mergedStations.append(chooseBundleValue(*currentIt,
                                                    *loadedIt,
                                                    baseStation,
                                                    [](const cwNoteLiDARStation& lhs, const cwNoteLiDARStation& rhs) {
                                                        return lidarStationsEqual(lhs, rhs);
                                                    }));
            continue;
        }

        if (hasCurrent) {
            mergedStations.append(*currentIt);
            continue;
        }

        if (hasLoaded && !hasBase) {
            mergedStations.append(*loadedIt);
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

bool cwNoteLiDARMergeApplier::applyNoteLiDARMergePlan(const cwNoteLiDARMergePlan& plan)
{
    if (plan.currentNote == nullptr || plan.loadedNoteData == nullptr) {
        return false;
    }

    const QString mergedFilename = chooseBundleValue<QString>(
        plan.currentNote->filename(),
        plan.loadedNoteData->filename,
        plan.baseNoteData.has_value() ? std::optional<QString>(plan.baseNoteData->filename)
                                      : std::nullopt,
        [](const QString& lhs, const QString& rhs) {
            return lhs == rhs;
        });
    plan.currentNote->setFilename(mergedFilename);

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

    const TransformBundle mergedTransformBundle = chooseBundleValue<TransformBundle>(
        currentTransformBundle,
        loadedTransformBundle,
        baseTransformBundle,
        [](const TransformBundle& lhs, const TransformBundle& rhs) {
            return lhs.autoCalculateNorth == rhs.autoCalculateNorth
                   && lidarTransformsEqual(lhs.transform, rhs.transform);
        });

    plan.currentNote->setAutoCalculateNorth(mergedTransformBundle.autoCalculateNorth);
    plan.currentNote->noteTransformation()->setData(mergedTransformBundle.transform);
    plan.currentNote->noteTransformation()->setUpSign(mergedTransformBundle.transform.upSign);

    const auto mergedStations = mergeStationsByName(
        plan.currentNote->stations(),
        plan.loadedNoteData->stations,
        plan.baseNoteData.has_value() ? std::optional<QList<cwNoteLiDARStation>>(plan.baseNoteData->stations)
                                      : std::nullopt);
    if (!mergedStations.has_value()) {
        return false;
    }

    plan.currentNote->setStations(*mergedStations);
    return true;
}
