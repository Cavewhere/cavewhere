#include "cwNoteSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwGlobals.h"
#include "cwImage.h"
#include "cwImageResolution.h"
#include "cwNoteMergeApplier.h"
#include "cwNoteMergePlanBuilder.h"
#include "cwNote.h"
#include "cwSaveLoad.h"
#include "cwScrapSyncMergeHandler.h"
#include "cwSyncPathResolver.h"
#include "cwSyncIdUtils.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "cwNoteStation.h"
#include "cwLead.h"
#include "GitRepository.h"
#include <QSet>
#include <QHash>
#include <QUuid>

#include <optional>

namespace {
struct BaseNoteMergeData {
    cwNoteData noteData;
    QHash<QUuid, cwScrapBaseIdentityData> baseScrapIdentityByScrapId;
};

cwScrapBaseIdentityData baseScrapIdentityFromScrapData(const cwScrapData& scrapData)
{
    cwScrapBaseIdentityData identityData;
    identityData.hasGeometryData = true;
    identityData.geometry.outlinePoints = QPolygonF(scrapData.outlinePoints);
    identityData.geometry.transform.noteTransformation = scrapData.noteTransformation;
    identityData.geometry.transform.calculateNoteTransform = scrapData.calculateNoteTransform;
    identityData.geometry.transform.viewType = scrapData.viewMatrix
                                                   ? static_cast<cwScrapType::Type>(scrapData.viewMatrix->type())
                                                   : cwScrapType::Plan;

    if (identityData.geometry.transform.viewType == cwScrapType::ProjectedProfile) {
        const auto* projectedData =
            dynamic_cast<const cwProjectedProfileScrapViewMatrix::Data*>(scrapData.viewMatrix.get());
        if (projectedData != nullptr) {
            identityData.geometry.transform.hasProjectedProfileView = true;
            identityData.geometry.transform.projectedAzimuth = projectedData->azimuth();
            identityData.geometry.transform.projectedDirection = static_cast<int>(projectedData->direction());
        }
    }

    for (const cwNoteStation& station : scrapData.stations) {
        if (station.id().isNull()) {
            continue;
        }
        identityData.stationIds.insert(station.id());
        identityData.stationsById.insert(station.id(), station);
    }

    for (const cwLead& lead : scrapData.leads) {
        if (lead.id().isNull()) {
            continue;
        }
        identityData.leadIds.insert(lead.id());
        identityData.leadsById.insert(lead.id(), lead);
    }

    return identityData;
}

std::optional<std::pair<QUuid, BaseNoteMergeData>> loadBaseMergeDataForNote(
    const QDir& repoRoot,
    const QString& mergeBaseHead,
    const QString& relativeNotePath)
{
    if (mergeBaseHead.isEmpty()) {
        return std::nullopt;
    }

    const auto contentResult = QQuickGit::GitRepository::fileContentAtCommit(
        repoRoot.absolutePath(),
        mergeBaseHead,
        relativeNotePath);
    if (contentResult.hasError() || contentResult.value().isEmpty()) {
        return std::nullopt;
    }

    const QString absoluteNotePath = repoRoot.absoluteFilePath(relativeNotePath);
    const auto noteResult = cwSaveLoad::loadNote(contentResult.value(), absoluteNotePath, repoRoot);
    if (noteResult.hasError()) {
        return std::nullopt;
    }

    const cwNoteData noteData = noteResult.value();
    if (noteData.id.isNull()) {
        return std::nullopt;
    }

    BaseNoteMergeData baseMergeData;
    baseMergeData.noteData = noteData;

    for (const cwScrapData& scrapData : noteData.scraps) {
        if (scrapData.id.isNull()) {
            continue;
        }
        baseMergeData.baseScrapIdentityByScrapId.insert(scrapData.id, baseScrapIdentityFromScrapData(scrapData));
    }

    return std::make_optional(std::make_pair(noteData.id, std::move(baseMergeData)));
}

enum class NoteDescriptorApplyMode {
    FullModelReplace,
    StructuralMerge,
    Ambiguous
};

NoteDescriptorApplyMode determineNoteDescriptorApplyMode(cwSurveyNoteModel* noteModel,
                                                         const cwSurveyNoteModelData& loadedNoteModelData)
{
    if (noteModel == nullptr) {
        return NoteDescriptorApplyMode::Ambiguous;
    }

    const auto currentNoteIds = cwSyncIdUtils::collectOrderedUniqueIds(
        noteModel->notes(),
        [](const cwNote* note) {
            return note != nullptr ? note->id() : QUuid();
        });
    const auto loadedNoteIds = cwSyncIdUtils::collectOrderedUniqueIds(
        loadedNoteModelData.notes,
        [](const cwNoteData& noteData) {
            return noteData.id;
        });

    if (!currentNoteIds.has_value() || !loadedNoteIds.has_value()) {
        return NoteDescriptorApplyMode::Ambiguous;
    }

    if (currentNoteIds.value() == loadedNoteIds.value()) {
        return NoteDescriptorApplyMode::StructuralMerge;
    }

    if (cwSyncIdUtils::haveSameIdsIgnoringOrder(currentNoteIds.value(), loadedNoteIds.value())) {
        return NoteDescriptorApplyMode::StructuralMerge;
    }

    return NoteDescriptorApplyMode::FullModelReplace;
}

} // namespace

QString cwNoteSyncMergeHandler::name() const
{
    return QStringLiteral("cwNoteSyncMergeHandler");
}

cwReconcileMergeResult cwNoteSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
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

    struct NoteTripUpdate {
        cwTrip* trip = nullptr;
        const cwTripData* loadedTripData = nullptr;
        bool noteDescriptorChanged = false;
        bool assetChanged = false;
        NoteDescriptorApplyMode noteDescriptorApplyMode = NoteDescriptorApplyMode::FullModelReplace;
        QList<cwNoteMergePlan> noteMergePlans;
        QList<cwNoteStructuralMergePlan> noteStructuralMergePlans;
        QList<cwNote*> reorderedNotes;
        QHash<QUuid, cwNoteData> baseNoteDataByNoteId;
        QHash<QUuid, QHash<QUuid, cwScrapBaseIdentityData>> baseScrapIdentityByNoteId;
    };

    const QString dataRootName = context.dataRootName();

    const auto currentNoteIndex = cwSyncPathResolver::buildCurrentNoteIndex(context.repoRoot,
                                                                            dataRootName,
                                                                            context.saveLoad,
                                                                            context.region);
    const auto loadedNoteIndex = cwSyncPathResolver::buildLoadedNoteIndex(context.repoRoot,
                                                                          dataRootName,
                                                                          context.loadData->region);

    QHash<QUuid, const cwTripData*> loadedTripsById;
    for (const cwCaveData& caveData : context.loadData->region.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            if (!tripData.id.isNull()) {
                loadedTripsById.insert(tripData.id, &tripData);
            }
        }
    }

    QList<NoteTripUpdate> noteTripUpdates;
    const QList<cwSyncPathResolver::TripChangeResolution> resolvedTripChanges =
        cwSyncPathResolver::resolveChangedNotePaths(context.repoRoot,
                                                    dataRootName,
                                                    context.saveLoad,
                                                    context.region,
                                                    context.report->changedPaths,
                                                    currentNoteIndex,
                                                    loadedNoteIndex);
    noteTripUpdates.reserve(resolvedTripChanges.size());
    for (const cwSyncPathResolver::TripChangeResolution& tripChange : resolvedTripChanges) {
        NoteTripUpdate update;
        update.trip = tripChange.trip;
        update.noteDescriptorChanged = tripChange.descriptorChanged;
        update.assetChanged = tripChange.assetChanged;
        if (update.trip == nullptr || update.trip->id().isNull()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Trip identity is missing for changed note payload.");
            return result;
        }

        const auto loadedTripIt = loadedTripsById.constFind(update.trip->id());
        if (loadedTripIt == loadedTripsById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Loaded data is missing changed trip payload.");
            return result;
        }

        update.loadedTripData = loadedTripIt.value();
        for (auto basePathIt = tripChange.baseLookupPathByObjectId.cbegin();
             basePathIt != tripChange.baseLookupPathByObjectId.cend();
             ++basePathIt) {
            const auto baseMergeData = loadBaseMergeDataForNote(context.repoRoot,
                                                                context.report->mergeBaseHead,
                                                                basePathIt.value());
            if (baseMergeData.has_value()) {
                update.baseNoteDataByNoteId.insert(baseMergeData->first, baseMergeData->second.noteData);
                update.baseScrapIdentityByNoteId.insert(baseMergeData->first,
                                                       baseMergeData->second.baseScrapIdentityByScrapId);
            }
        }

        if (!update.noteDescriptorChanged && !update.assetChanged) {
            continue;
        }
        noteTripUpdates.append(update);
    }

    if (noteTripUpdates.isEmpty()) {
        cwReconcileMergeResult result;
        result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
        result.handlerName = name();
        result.fallbackReason = QStringLiteral("No note updates were mapped for changed note paths.");
        return result;
    }

    for (NoteTripUpdate& update : noteTripUpdates) {
        Q_ASSERT(update.trip != nullptr);
        Q_ASSERT(update.loadedTripData != nullptr);

        if (!update.noteDescriptorChanged) {
            continue;
        }

        update.noteDescriptorApplyMode = determineNoteDescriptorApplyMode(update.trip->notes(),
                                                                           update.loadedTripData->noteModel);
        if (update.noteDescriptorApplyMode == NoteDescriptorApplyMode::Ambiguous) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Ambiguous note descriptor structural mapping.");
            return result;
        }

        if (update.noteDescriptorApplyMode == NoteDescriptorApplyMode::StructuralMerge) {
            const auto noteMergePreparation = cwNoteMergePlanBuilder::build(
                update.trip->notes(),
                update.loadedTripData->noteModel,
                update.baseNoteDataByNoteId);
            if (noteMergePreparation.hasError()) {
                cwReconcileMergeResult result;
                result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                result.handlerName = name();
                result.fallbackReason = noteMergePreparation.errorMessage().isEmpty()
                                            ? QStringLiteral("Unable to build deterministic note merge plan.")
                                            : noteMergePreparation.errorMessage();
                return result;
            }
            const cwNoteMergePreparation noteMergePreparationValue = noteMergePreparation.value();
            update.noteMergePlans = noteMergePreparationValue.plans;

            const auto mergePreparation = cwScrapSyncMergeHandler::buildNoteStructuralMergePreparation(
                update.trip->notes(),
                update.loadedTripData->noteModel);
            if (mergePreparation.hasError()) {
                cwReconcileMergeResult result;
                result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                result.handlerName = name();
                result.fallbackReason = mergePreparation.errorMessage().isEmpty()
                                            ? QStringLiteral("Unable to build deterministic note scrap merge plan.")
                                            : mergePreparation.errorMessage();
                return result;
            }
            const cwNoteStructuralMergePreparation scrapMergePreparationValue = mergePreparation.value();
            update.noteStructuralMergePlans = scrapMergePreparationValue.plans;
            for (cwNoteStructuralMergePlan& plan : update.noteStructuralMergePlans) {
                if (plan.loadedNoteData == nullptr) {
                    continue;
                }

                plan.applyMode = context.applyMode;

                const auto baseIdentityIt = update.baseScrapIdentityByNoteId.constFind(plan.loadedNoteData->id);
                if (baseIdentityIt != update.baseScrapIdentityByNoteId.constEnd()) {
                    plan.baseScrapIdentityByScrapId = baseIdentityIt.value();
                }
            }
            update.reorderedNotes = scrapMergePreparationValue.orderedNotes;
        }
    }

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();

    QSet<QObject*> objectPathReadySet;
    for (const NoteTripUpdate& update : noteTripUpdates) {
        Q_ASSERT(update.trip != nullptr);
        Q_ASSERT(update.loadedTripData != nullptr);

        if (update.noteDescriptorChanged) {
            if (update.noteDescriptorApplyMode == NoteDescriptorApplyMode::StructuralMerge) {
                for (const cwNoteMergePlan& plan : update.noteMergePlans) {
                    const auto applyResult = cwNoteMergeApplier::applyNoteMergePlan(plan);
                    if (applyResult.hasError()) {
                        cwReconcileMergeResult fullReloadResult;
                        fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                        fullReloadResult.handlerName = name();
                        fullReloadResult.fallbackReason = applyResult.errorMessage().isEmpty()
                                                              ? QStringLiteral("Unable to apply deterministic note merge plan.")
                                                              : applyResult.errorMessage();
                        return fullReloadResult;
                    }
                }

                for (const cwNoteStructuralMergePlan& plan : update.noteStructuralMergePlans) {
                    const auto applyResult = cwScrapSyncMergeHandler::applyNoteStructuralMergePlan(plan);
                    if (applyResult.hasError()) {
                        cwReconcileMergeResult fullReloadResult;
                        fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                        fullReloadResult.handlerName = name();
                        fullReloadResult.fallbackReason = applyResult.errorMessage().isEmpty()
                                                              ? QStringLiteral("Unable to apply deterministic note scrap merge plan.")
                                                              : applyResult.errorMessage();
                        return fullReloadResult;
                    }
                    if (applyResult.value().geometryConflictKeptOurs) {
                        result.diagnostics.append(QStringLiteral("reconcile scrap geometry conflict resolved to ours"));
                    }
                }

                QList<QObject*> reorderedNotes;
                reorderedNotes.reserve(update.reorderedNotes.size());
                for (cwNote* note : update.reorderedNotes) {
                    reorderedNotes.append(note);
                }
                const bool reorderApplied = update.trip->notes()->reorderNotes(reorderedNotes);
                if (!reorderApplied) {
                    cwReconcileMergeResult fullReloadResult;
                    fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                    fullReloadResult.handlerName = name();
                    fullReloadResult.fallbackReason = QStringLiteral("Unable to apply deterministic note order.");
                    return fullReloadResult;
                }
            } else {
                update.trip->notes()->setData(update.loadedTripData->noteModel);
            }
            result.modelMutated = true;
        }

        if (update.noteDescriptorChanged || update.assetChanged) {
            objectPathReadySet.insert(update.trip);
        }
    }

    result.objectsPathReady.reserve(objectPathReadySet.size());
    for (QObject* object : std::as_const(objectPathReadySet)) {
        result.objectsPathReady.append(object);
    }

    return result;
}
