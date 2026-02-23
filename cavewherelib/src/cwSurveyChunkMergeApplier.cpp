#include "cwSurveyChunkMergeApplier.h"

#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSyncIdUtils.h"
#include "cwSyncMergeApplyUtils.h"

#include <QDebug>
#include <QStringList>

#include <optional>

namespace {

QString applyModeToString(cwSyncMergeApplyUtils::ApplyMode applyMode)
{
    switch (applyMode) {
    case cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge:
        return QStringLiteral("ThreeWayMerge");
    case cwSyncMergeApplyUtils::ApplyMode::LoadedWins:
        return QStringLiteral("LoadedWins");
    }

    return QStringLiteral("Unknown");
}

QString summarizeStations(const QList<cwStation>& stations)
{
    QStringList summary;
    summary.reserve(stations.size());
    for (const cwStation& station : stations) {
        summary.append(QStringLiteral("%1:%2").arg(station.id().toString(QUuid::WithoutBraces), station.name()));
    }
    return summary.join(QStringLiteral(", "));
}

QString summarizeShots(const QList<cwShot>& shots)
{
    QStringList summary;
    summary.reserve(shots.size());
    for (const cwShot& shot : shots) {
        summary.append(shot.id().toString(QUuid::WithoutBraces));
    }
    return summary.join(QStringLiteral(", "));
}

cwStation mergeSharedStation(const cwStation& currentStation,
                             const cwStation& loadedStation,
                             const std::optional<cwStation>& baseStation)
{
    cwStation mergedStation = currentStation;
    mergedStation.setName(cwSyncMergeApplyUtils::chooseBundleValue(
        currentStation.name(),
        loadedStation.name(),
        baseStation.has_value() ? std::optional<QString>(baseStation->name()) : std::nullopt,
        [](const QString& lhs, const QString& rhs) { return lhs == rhs; }));
    mergedStation.setLeft(cwSyncMergeApplyUtils::chooseBundleValue(
        currentStation.left(),
        loadedStation.left(),
        baseStation.has_value() ? std::optional<cwDistanceReading>(baseStation->left()) : std::nullopt,
        [](const cwDistanceReading& lhs, const cwDistanceReading& rhs) { return lhs == rhs; }));
    mergedStation.setRight(cwSyncMergeApplyUtils::chooseBundleValue(
        currentStation.right(),
        loadedStation.right(),
        baseStation.has_value() ? std::optional<cwDistanceReading>(baseStation->right()) : std::nullopt,
        [](const cwDistanceReading& lhs, const cwDistanceReading& rhs) { return lhs == rhs; }));
    mergedStation.setUp(cwSyncMergeApplyUtils::chooseBundleValue(
        currentStation.up(),
        loadedStation.up(),
        baseStation.has_value() ? std::optional<cwDistanceReading>(baseStation->up()) : std::nullopt,
        [](const cwDistanceReading& lhs, const cwDistanceReading& rhs) { return lhs == rhs; }));
    mergedStation.setDown(cwSyncMergeApplyUtils::chooseBundleValue(
        currentStation.down(),
        loadedStation.down(),
        baseStation.has_value() ? std::optional<cwDistanceReading>(baseStation->down()) : std::nullopt,
        [](const cwDistanceReading& lhs, const cwDistanceReading& rhs) { return lhs == rhs; }));
    return mergedStation;
}

cwShot mergeSharedShot(const cwShot& currentShot,
                       const cwShot& loadedShot,
                       const std::optional<cwShot>& baseShot)
{
    cwShot mergedShot = currentShot;
    mergedShot.setDistanceIncluded(cwSyncMergeApplyUtils::chooseBundleValue(
        currentShot.isDistanceIncluded(),
        loadedShot.isDistanceIncluded(),
        baseShot.has_value() ? std::optional<bool>(baseShot->isDistanceIncluded()) : std::nullopt,
        [](bool lhs, bool rhs) { return lhs == rhs; }));
    mergedShot.setDistance(cwSyncMergeApplyUtils::chooseBundleValue(
        currentShot.distance(),
        loadedShot.distance(),
        baseShot.has_value() ? std::optional<cwDistanceReading>(baseShot->distance()) : std::nullopt,
        [](const cwDistanceReading& lhs, const cwDistanceReading& rhs) { return lhs == rhs; }));
    mergedShot.setCompass(cwSyncMergeApplyUtils::chooseBundleValue(
        currentShot.compass(),
        loadedShot.compass(),
        baseShot.has_value() ? std::optional<cwCompassReading>(baseShot->compass()) : std::nullopt,
        [](const cwCompassReading& lhs, const cwCompassReading& rhs) { return lhs == rhs; }));
    mergedShot.setBackCompass(cwSyncMergeApplyUtils::chooseBundleValue(
        currentShot.backCompass(),
        loadedShot.backCompass(),
        baseShot.has_value() ? std::optional<cwCompassReading>(baseShot->backCompass()) : std::nullopt,
        [](const cwCompassReading& lhs, const cwCompassReading& rhs) { return lhs == rhs; }));
    mergedShot.setClino(cwSyncMergeApplyUtils::chooseBundleValue(
        currentShot.clino(),
        loadedShot.clino(),
        baseShot.has_value() ? std::optional<cwClinoReading>(baseShot->clino()) : std::nullopt,
        [](const cwClinoReading& lhs, const cwClinoReading& rhs) { return lhs == rhs; }));
    mergedShot.setBackClino(cwSyncMergeApplyUtils::chooseBundleValue(
        currentShot.backClino(),
        loadedShot.backClino(),
        baseShot.has_value() ? std::optional<cwClinoReading>(baseShot->backClino()) : std::nullopt,
        [](const cwClinoReading& lhs, const cwClinoReading& rhs) { return lhs == rhs; }));
    return mergedShot;
}

} // namespace

Monad::ResultBase cwSurveyChunkMergeApplier::applySurveyChunkMergePlan(const cwSurveyChunkMergePlan& plan)
{
    if (plan.currentChunk == nullptr || plan.loadedChunkData == nullptr) {
        return Monad::ResultBase(QStringLiteral("Survey chunk merge plan is missing required objects."));
    }

    const cwSurveyChunkData currentChunkData = plan.currentChunk->data();
    const cwSurveyChunkData& loadedChunkData = *plan.loadedChunkData;
    const std::optional<cwSurveyChunkData> baseChunkData = plan.baseChunkData;
    const auto applyMode = plan.applyMode;
    const cwSyncIdUtils::CurrentOnlyItemPolicy currentOnlyItemPolicy =
        applyMode == cwSyncMergeApplyUtils::ApplyMode::LoadedWins
            ? cwSyncIdUtils::CurrentOnlyItemPolicy::DropAlways
            : cwSyncIdUtils::CurrentOnlyItemPolicy::KeepWhenNotInBase;
    const cwSyncIdUtils::LoadedOnlyItemPolicy loadedOnlyItemPolicy =
        applyMode == cwSyncMergeApplyUtils::ApplyMode::LoadedWins
            ? cwSyncIdUtils::LoadedOnlyItemPolicy::KeepAlways
            : cwSyncIdUtils::LoadedOnlyItemPolicy::KeepWhenNotInBase;

    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge begin applyMode=%1 currentStations=%2 loadedStations=%3 baseStations=%4")
               .arg(applyModeToString(applyMode))
               .arg(currentChunkData.stations.size())
               .arg(loadedChunkData.stations.size())
               .arg(baseChunkData.has_value() ? baseChunkData->stations.size() : -1);
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge begin applyMode=%1 currentShots=%2 loadedShots=%3 baseShots=%4")
               .arg(applyModeToString(applyMode))
               .arg(currentChunkData.shots.size())
               .arg(loadedChunkData.shots.size())
               .arg(baseChunkData.has_value() ? baseChunkData->shots.size() : -1);
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge stations current=[%1]")
               .arg(summarizeStations(currentChunkData.stations));
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge stations loaded=[%1]")
               .arg(summarizeStations(loadedChunkData.stations));
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge shots current=[%1]")
               .arg(summarizeShots(currentChunkData.shots));
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge shots loaded=[%1]")
               .arg(summarizeShots(loadedChunkData.shots));
    if (baseChunkData.has_value()) {
        qDebug().noquote()
            << QStringLiteral("[TripSyncDebug] chunk merge stations base=[%1]")
                   .arg(summarizeStations(baseChunkData->stations));
        qDebug().noquote()
            << QStringLiteral("[TripSyncDebug] chunk merge shots base=[%1]")
                   .arg(summarizeShots(baseChunkData->shots));
    } else {
        qDebug().noquote() << QStringLiteral("[TripSyncDebug] chunk merge base chunk is missing for this chunk id");
    }

    const auto mergedStations = cwSyncIdUtils::buildMergedOrderedList<cwStation>(
        currentChunkData.stations,
        loadedChunkData.stations,
        baseChunkData.has_value()
            ? std::optional<QList<cwStation>>(baseChunkData->stations)
            : std::nullopt,
        [](const cwStation& station) { return station.id(); },
        [](const cwStation& currentStation, const cwStation& loadedStation, const std::optional<cwStation>& baseStation) {
            return mergeSharedStation(currentStation, loadedStation, baseStation);
        },
        [applyMode](const std::vector<QUuid>& currentIds,
           const std::vector<QUuid>& loadedIds,
           const std::optional<std::vector<QUuid>>& baseIds) {
            return cwSyncMergeApplyUtils::chooseBundleValue(
                currentIds,
                loadedIds,
                baseIds,
                [](const std::vector<QUuid>& lhs, const std::vector<QUuid>& rhs) { return lhs == rhs; },
                applyMode);
        },
        QStringLiteral("chunk payload list"),
        currentOnlyItemPolicy,
        loadedOnlyItemPolicy);
    if (mergedStations.hasError()) {
        qDebug().noquote() << QStringLiteral("[TripSyncDebug] chunk merge station error: %1")
                                  .arg(mergedStations.errorMessage());
        return Monad::ResultBase(mergedStations.errorMessage());
    }

    const auto mergedShots = cwSyncIdUtils::buildMergedOrderedList<cwShot>(
        currentChunkData.shots,
        loadedChunkData.shots,
        baseChunkData.has_value()
            ? std::optional<QList<cwShot>>(baseChunkData->shots)
            : std::nullopt,
        [](const cwShot& shot) { return shot.id(); },
        [](const cwShot& currentShot, const cwShot& loadedShot, const std::optional<cwShot>& baseShot) {
            return mergeSharedShot(currentShot, loadedShot, baseShot);
        },
        [applyMode](const std::vector<QUuid>& currentIds,
           const std::vector<QUuid>& loadedIds,
           const std::optional<std::vector<QUuid>>& baseIds) {
            return cwSyncMergeApplyUtils::chooseBundleValue(
                currentIds,
                loadedIds,
                baseIds,
                [](const std::vector<QUuid>& lhs, const std::vector<QUuid>& rhs) { return lhs == rhs; },
                applyMode);
        },
        QStringLiteral("chunk payload list"),
        currentOnlyItemPolicy,
        loadedOnlyItemPolicy);
    if (mergedShots.hasError()) {
        qDebug().noquote() << QStringLiteral("[TripSyncDebug] chunk merge shot error: %1")
                                  .arg(mergedShots.errorMessage());
        return Monad::ResultBase(mergedShots.errorMessage());
    }

    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge result stations count=%1 values=[%2]")
               .arg(mergedStations.value().size())
               .arg(summarizeStations(mergedStations.value()));
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge result shots count=%1 values=[%2]")
               .arg(mergedShots.value().size())
               .arg(summarizeShots(mergedShots.value()));

    cwSurveyChunkData mergedChunkData = currentChunkData;
    mergedChunkData.stations = mergedStations.value();
    mergedChunkData.shots = mergedShots.value();
    plan.currentChunk->setData(mergedChunkData);

    const cwSurveyChunkData appliedChunkData = plan.currentChunk->data();
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge applied stations count=%1 values=[%2]")
               .arg(appliedChunkData.stations.size())
               .arg(summarizeStations(appliedChunkData.stations));
    qDebug().noquote()
        << QStringLiteral("[TripSyncDebug] chunk merge applied shots count=%1 values=[%2]")
               .arg(appliedChunkData.shots.size())
               .arg(summarizeShots(appliedChunkData.shots));

    return Monad::ResultBase();
}
