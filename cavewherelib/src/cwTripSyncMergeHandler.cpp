#include "cwTripSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwGlobals.h"
#include "cwRegionLoadTask.h"
#include "cwSaveLoad.h"
#include "cwSyncPathResolver.h"
#include "cwSyncMergeApplyUtils.h"
#include "cwTrip.h"
#include "cwTripMergeApplier.h"
#include "cwTripMergePlanBuilder.h"
#include "GitRepository.h"

#include <QFile>
#include <QDebug>
#include <QHash>
#include <QSet>
#include <QUuid>

#include <optional>

namespace {

QString normalizeSyncPath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

std::optional<std::pair<QUuid, cwTripData>> loadBaseTripDataForPath(const QDir& repoRoot,
                                                                     const QString& mergeBaseHead,
                                                                     const QString& relativeTripPath)
{
    if (mergeBaseHead.isEmpty()) {
        return std::nullopt;
    }

    const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(
        repoRoot.absolutePath(),
        mergeBaseHead,
        relativeTripPath);
    if (contentResult.hasError() || contentResult.value().isEmpty()) {
        return std::nullopt;
    }

    const auto tripResult = cwSaveLoad::loadTrip(contentResult.value());
    if (tripResult.hasError()) {
        return std::nullopt;
    }
    const cwTripData baseTripData = tripResult.value();
    if (baseTripData.id.isNull()) {
        return std::nullopt;
    }

    return std::make_optional(std::make_pair(baseTripData.id, baseTripData));
}

std::optional<QUuid> loadTripIdFromPath(const QDir& repoRoot, const QString& relativeTripPath)
{
    QFile file(repoRoot.absoluteFilePath(relativeTripPath));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const auto tripResult = cwSaveLoad::loadTrip(file.readAll());
    if (tripResult.hasError() || tripResult.value().id.isNull()) {
        return std::nullopt;
    }
    return tripResult.value().id;
}

std::optional<std::pair<QUuid, cwTripData>> loadBaseTripDataForCandidatePaths(
    const QDir& repoRoot,
    const QString& mergeBaseHead,
    const QStringList& candidatePaths)
{
    QSet<QString> seenPaths;
    for (const QString& candidatePath : candidatePaths) {
        const QString normalizedPath = normalizeSyncPath(candidatePath);
        if (normalizedPath.isEmpty() || seenPaths.contains(normalizedPath)) {
            continue;
        }
        seenPaths.insert(normalizedPath);

        auto tripData = loadBaseTripDataForPath(repoRoot, mergeBaseHead, normalizedPath);
        if (tripData.has_value()) {
            return tripData;
        }
    }

    return std::nullopt;
}

QStringList caveDirectoryCandidatesFromChangedCavePaths(const QStringList& changedPaths)
{
    QSet<QString> caveDirs;
    for (const QString& changedPath : changedPaths) {
        const QString normalizedPath = normalizeSyncPath(changedPath);
        if (!normalizedPath.endsWith(QStringLiteral(".cwcave"), Qt::CaseInsensitive)) {
            continue;
        }

        const QString caveDir = QFileInfo(normalizedPath).dir().dirName();
        if (!caveDir.isEmpty()) {
            caveDirs.insert(caveDir);
        }
    }

    return caveDirs.values();
}

QString synthesizeTripPathWithCaveDir(const QString& dataRootName,
                                      const QString& caveDirName,
                                      const QString& tripDescriptorPath)
{
    const QString normalizedTripPath = normalizeSyncPath(tripDescriptorPath);
    const QStringList segments = normalizedTripPath.split(QChar('/'), Qt::SkipEmptyParts);
    const int tripsIndex = segments.indexOf(QStringLiteral("trips"));
    if (tripsIndex <= 0 || tripsIndex + 2 >= segments.size()) {
        return QString();
    }

    const QString tripDirName = segments.at(tripsIndex + 1);
    const QString tripFileName = segments.at(tripsIndex + 2);
    if (tripDirName.isEmpty() || tripFileName.isEmpty()) {
        return QString();
    }

    const QString dataRootDir = dataRootName.isEmpty() ? segments.first() : dataRootName;
    return normalizeSyncPath(
        QDir(dataRootDir).filePath(
            QDir(caveDirName).filePath(
                QDir(QStringLiteral("trips")).filePath(
                    QDir(tripDirName).filePath(tripFileName)))));
}

} // namespace

QString cwTripSyncMergeHandler::name() const
{
    return QStringLiteral("cwTripSyncMergeHandler");
}

cwReconcileMergeResult cwTripSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
{
    if (context.saveLoad == nullptr
        || context.region == nullptr
        || context.loadData == nullptr
        || context.report == nullptr) {
        return {};
    }

    if (context.report->changedPaths.isEmpty()) {
        return {};
    }

    QHash<QUuid, cwTrip*> currentTripsById;
    for (cwCave* cave : context.region->caves()) {
        if (cave == nullptr) {
            continue;
        }

        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr || trip->id().isNull()) {
                continue;
            }
            currentTripsById.insert(trip->id(), trip);
        }
    }

    QHash<QUuid, const cwTripData*> loadedTripsById;
    QHash<QUuid, QString> loadedCaveNameByTripId;
    for (const cwCaveData& caveData : context.loadData->region.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            if (!tripData.id.isNull()) {
                loadedTripsById.insert(tripData.id, &tripData);
                loadedCaveNameByTripId.insert(tripData.id, caveData.name);
            }
        }
    }

    const QString dataRootName = context.dataRootName();

    const auto currentTripIndex = cwSyncPathResolver::buildCurrentTripIndex(context.repoRoot,
                                                                            dataRootName,
                                                                            context.region);
    const auto loadedTripIndex = cwSyncPathResolver::buildLoadedTripIndex(context.repoRoot,
                                                                          dataRootName,
                                                                          context.loadData->region);

    QList<cwTrip*> changedCurrentTrips;
    QList<const cwTripData*> changedLoadedTrips;
    QHash<QUuid, cwTripData> baseTripById;
    QSet<QUuid> seenTripIds;

    for (const QString& changedPath : context.report->changedPaths) {
        const QString normalizedPath = normalizeSyncPath(changedPath);
        if (!normalizedPath.endsWith(QStringLiteral(".cwtrip"), Qt::CaseInsensitive)) {
            continue;
        }

        const QSet<QUuid> resolvedTripIds = cwSyncPathResolver::resolveTripIdsForChangedPath(context.repoRoot,
                                                                                              dataRootName,
                                                                                              normalizedPath,
                                                                                              currentTripIndex,
                                                                                              loadedTripIndex);
        QSet<QUuid> effectiveResolvedTripIds = resolvedTripIds;
        if (effectiveResolvedTripIds.isEmpty()) {
            const auto baseTripData = loadBaseTripDataForPath(context.repoRoot,
                                                              context.report->mergeBaseHead,
                                                              normalizedPath);
            if (baseTripData.has_value()) {
                effectiveResolvedTripIds.insert(baseTripData->first);
                baseTripById.insert(baseTripData->first, baseTripData->second);
            }
        }

        if (effectiveResolvedTripIds.isEmpty()) {
            if (!QFileInfo::exists(context.repoRoot.absoluteFilePath(normalizedPath))) {
                // Deleted/renamed-away descriptor paths can remain in changedPaths after merge.
                // Skip these and rely on the surviving descriptor path for id resolution.
                continue;
            }
            // resolveTripIdsForChangedPath already tried reading the descriptor from disk
            // via its own loadTripIdFromDescriptorPath fallback — if that failed the id
            // is genuinely unresolvable.
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Unable to resolve changed trip identity from descriptor.");
            return result;
        }
        if (effectiveResolvedTripIds.size() > 1) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Changed trip descriptor resolved to multiple identities.");
            return result;
        }

        // If the resolved ID comes from loadedTripIndex it may carry a UUID that was
        // reassigned by repairTopLevelIds (which runs in-memory but has not yet been
        // persisted to disk at reconcile time).  Fall back to the on-disk UUID — it
        // still holds the original value — so an orphan descriptor from a
        // rename/rename conflict can be matched against the current in-memory model.
        if (!effectiveResolvedTripIds.isEmpty()) {
            const QUuid candidate = *effectiveResolvedTripIds.cbegin();
            if (currentTripsById.constFind(candidate) == currentTripsById.constEnd()) {
                const auto diskId = loadTripIdFromPath(context.repoRoot, normalizedPath);
                if (diskId.has_value()
                    && currentTripsById.constFind(*diskId) != currentTripsById.constEnd()) {
                    effectiveResolvedTripIds.clear();
                    effectiveResolvedTripIds.insert(*diskId);
                }
            }
        }

        const QUuid tripId = *effectiveResolvedTripIds.cbegin();

        if (seenTripIds.contains(tripId)) {
            continue;
        }
        seenTripIds.insert(tripId);

        const auto currentTripIt = currentTripsById.constFind(tripId);
        if (currentTripIt == currentTripsById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("No current trip matches changed trip descriptor id.");
            return result;
        }

        cwTrip* const currentTrip = currentTripIt.value();

        const auto loadedTripIt = loadedTripsById.constFind(tripId);
        if (loadedTripIt == loadedTripsById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Loaded data is missing changed trip payload.");
            return result;
        }

        changedCurrentTrips.append(currentTrip);
        changedLoadedTrips.append(loadedTripIt.value());

        QStringList baseLookupPaths;
        baseLookupPaths.append(normalizedPath);

        const QString currentTripPath =
            cwSyncPathResolver::currentTripDescriptorPath(context.repoRoot, dataRootName, currentTrip);
        if (!currentTripPath.isEmpty()) {
            baseLookupPaths.append(currentTripPath);
        }

        const QString loadedCaveName = loadedCaveNameByTripId.value(tripId);
        if (!loadedCaveName.isEmpty()) {
            const QString loadedTripPath = cwSyncPathResolver::loadedTripDescriptorPath(
                context.repoRoot,
                dataRootName,
                loadedCaveName,
                loadedTripIt.value()->name);
            if (!loadedTripPath.isEmpty()) {
                baseLookupPaths.append(loadedTripPath);
            }
        }

        auto baseTripData = loadBaseTripDataForCandidatePaths(context.repoRoot,
                                                              context.report->mergeBaseHead,
                                                              baseLookupPaths);
        if (!baseTripData.has_value()) {
            const QStringList caveDirCandidates =
                caveDirectoryCandidatesFromChangedCavePaths(context.report->changedPaths);
            for (const QString& caveDirCandidate : caveDirCandidates) {
                const QString synthesizedPath = synthesizeTripPathWithCaveDir(dataRootName,
                                                                              caveDirCandidate,
                                                                              normalizedPath);
                if (synthesizedPath.isEmpty()) {
                    continue;
                }

                const auto synthesizedBaseData = loadBaseTripDataForPath(context.repoRoot,
                                                                          context.report->mergeBaseHead,
                                                                          synthesizedPath);
                if (synthesizedBaseData.has_value() && synthesizedBaseData->first == tripId) {
                    baseTripData = synthesizedBaseData;
                    break;
                }
            }
        }

        if (!baseTripData.has_value()) {
            for (const QString& reportPath : context.report->changedPaths) {
                const QString reportNormalizedPath = normalizeSyncPath(reportPath);
                if (!reportNormalizedPath.endsWith(QStringLiteral(".cwtrip"), Qt::CaseInsensitive)) {
                    continue;
                }

                const auto baseFromReportPath = loadBaseTripDataForPath(context.repoRoot,
                                                                         context.report->mergeBaseHead,
                                                                         reportNormalizedPath);
                if (baseFromReportPath.has_value() && baseFromReportPath->first == tripId) {
                    baseTripData = baseFromReportPath;
                    break;
                }
            }
        }
        if (baseTripData.has_value()) {
            baseTripById.insert(baseTripData->first, baseTripData->second);
        }

    }

    if (changedCurrentTrips.isEmpty()) {
        return {};
    }

    const auto mergePreparation = cwTripMergePlanBuilder::build(changedCurrentTrips,
                                                                changedLoadedTrips,
                                                                baseTripById,
                                                                context.applyMode == cwReconcileApplyMode::TargetCommitWins
                                                                    ? cwSyncMergeApplyUtils::ApplyMode::LoadedWins
                                                                    : cwSyncMergeApplyUtils::ApplyMode::ThreeWayMerge);
    if (mergePreparation.hasError()) {
        cwReconcileMergeResult result;
        result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
        result.handlerName = name();
        result.fallbackReason = mergePreparation.errorMessage().isEmpty()
                                    ? QStringLiteral("Unable to build deterministic trip merge plan.")
                                    : mergePreparation.errorMessage();
        return result;
    }
    const cwTripMergePreparation mergePreparationValue = mergePreparation.value();

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();

    QSet<QString> winningTripDirNames;
    for (const cwTripMergePlan& plan : mergePreparationValue.plans) {
        const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
        if (applyResult.hasError()) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = applyResult.errorMessage().isEmpty()
                                                  ? QStringLiteral("Unable to apply deterministic trip merge plan.")
                                                  : applyResult.errorMessage();
            return fullReloadResult;
        }

        result.modelMutated = true;
        result.objectsPathReady.append(plan.currentTrip);
        winningTripDirNames.insert(cwSaveLoad::sanitizeFileName(plan.currentTrip->name()));
    }

    // Clean up orphaned trip directories left by rename/rename conflicts.
    // After the merge, the winning trip name is set on each applied trip.
    // Any .cwtrip path in changedPaths whose parent directory name does not match
    // a winning trip's sanitized name is an orphan and must be removed.
    if (!dataRootName.isEmpty()) {
        QSet<QString> checkedTripDirs;
        for (const QString& changedPath : context.report->changedPaths) {
            const QString normalizedPath = normalizeSyncPath(changedPath);
            if (!normalizedPath.endsWith(QStringLiteral(".cwtrip"), Qt::CaseInsensitive)) {
                continue;
            }
            const QString tripDirName = QFileInfo(normalizedPath).dir().dirName();
            if (tripDirName.isEmpty() || checkedTripDirs.contains(tripDirName)) {
                continue;
            }
            checkedTripDirs.insert(tripDirName);

            if (!winningTripDirNames.contains(tripDirName)) {
                // Only enqueue cleanup if the descriptor file is still on disk.
                // In a rename/rename conflict both descriptors exist simultaneously
                // (git checked out the losing side as a new file from the remote).
                // In a normal rename git removes the old descriptor, so the file
                // will not be present even if the directory structure lingers.
                if (QFileInfo::exists(context.repoRoot.absoluteFilePath(normalizedPath))) {
                    // Relative path to the orphan trip dir, e.g. "DataRoot/Cave A/trips/Peer Trip"
                    const QString relOrphanDir = QFileInfo(normalizedPath).dir().path();
                    context.saveLoad->enqueueOrphanDirectoryCleanup(relOrphanDir);
                    result.pendingConflictCleanup = true;
                }
            }
        }
    }

    return result;
}
