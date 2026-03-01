#include "cwNoteSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwGlobals.h"
#include "cwImage.h"
#include "cwImageResolution.h"
#include "cwNoteMergeApplier.h"
#include "cwNoteMergePlanBuilder.h"
#include "cwNote.h"
#include "cwScrapSyncMergeHandler.h"
#include "cwSyncIdUtils.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "cwNoteStation.h"
#include "cwLead.h"
#include "GitRepository.h"
#include "cavewhere.pb.h"
#include "google/protobuf/util/json_util.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <QHash>
#include <QUuid>

#include <optional>

namespace {

QString normalizeSyncPath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QSet<QString> trackedExtensionsForSyncMerge()
{
    QSet<QString> trackedExtensions;
    const QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    for (const QByteArray& format : supportedFormats) {
        const QString extension = QString::fromLatin1(format).trimmed().toLower();
        if (!extension.isEmpty()) {
            trackedExtensions.insert(extension);
        }
    }

    trackedExtensions.insert(QStringLiteral("svg"));
    trackedExtensions.insert(QStringLiteral("pdf"));
    trackedExtensions.insert(QStringLiteral("glb"));
    return trackedExtensions;
}

enum class NoteChangedPathKind {
    Unsupported,
    NoteDescriptor,
    Asset
};

std::optional<QString> notesDirectoryPathForChangedFile(const QString& path)
{
    const QString normalized = normalizeSyncPath(path);
    const QString marker = QStringLiteral("/notes/");
    const int markerIndex = normalized.indexOf(marker, 0, Qt::CaseInsensitive);
    if (markerIndex < 0) {
        return std::nullopt;
    }

    return normalized.left(markerIndex + QStringLiteral("/notes").size());
}

NoteChangedPathKind classifyNoteChangedPath(const QString& path, const QSet<QString>& trackedExtensions)
{
    if (path.endsWith(QStringLiteral(".cwnote"), Qt::CaseInsensitive)) {
        return NoteChangedPathKind::NoteDescriptor;
    }

    const QString extension = QFileInfo(path).suffix().trimmed().toLower();
    if (trackedExtensions.contains(extension)) {
        return NoteChangedPathKind::Asset;
    }

    return NoteChangedPathKind::Unsupported;
}

QUuid uuidFromProtoString(const std::string& uuidString)
{
    if (uuidString.empty()) {
        return QUuid();
    }

    const QUuid uuid(QString::fromStdString(uuidString));
    return uuid.isNull() ? QUuid() : uuid;
}

cwImage imageFromProtoForMergeBase(const CavewhereProto::Image& protoImage)
{
    cwImage image;
    if (protoImage.has_path()) {
        image.setPath(QString::fromStdString(protoImage.path()));
    }
    if (protoImage.has_page()) {
        image.setPage(protoImage.page());
    }
    if (protoImage.has_size()) {
        image.setOriginalSize(QSize(protoImage.size().width(), protoImage.size().height()));
    }
    if (protoImage.has_dotpermeter()) {
        image.setOriginalDotsPerMeter(protoImage.dotpermeter());
    }
    if (protoImage.has_imageunit()) {
        image.setUnit(static_cast<cwImage::Unit>(protoImage.imageunit()));
    }
    return image;
}

cwImageResolution::Data imageResolutionFromProtoForMergeBase(const CavewhereProto::ImageResolution& protoImageResolution)
{
    cwImageResolution::Data data;
    data.value = protoImageResolution.value();
    data.unit = protoImageResolution.unit();
    data.updateValueWhenUnitChanged = false;
    return data;
}

struct BaseNoteMergeData {
    cwNoteData noteData;
    QHash<QUuid, cwScrapBaseIdentityData> baseScrapIdentityByScrapId;
};

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

    CavewhereProto::Note protoNote;
    if (!protoNote.ParseFromArray(contentResult.value().constData(), contentResult.value().size())) {
        const std::string jsonPayload(contentResult.value().constData(),
                                      static_cast<size_t>(contentResult.value().size()));
        const auto parseStatus = google::protobuf::util::JsonStringToMessage(jsonPayload, &protoNote);
        if (!parseStatus.ok()) {
            return std::nullopt;
        }
    }

    if (!protoNote.has_id()) {
        return std::nullopt;
    }

    const QUuid noteId = uuidFromProtoString(protoNote.id());
    if (noteId.isNull()) {
        return std::nullopt;
    }

    BaseNoteMergeData baseMergeData;
    baseMergeData.noteData.id = noteId;
    if (protoNote.has_name()) {
        baseMergeData.noteData.name = QString::fromStdString(protoNote.name());
    }
    baseMergeData.noteData.rotate = protoNote.rotation();
    if (protoNote.has_image()) {
        baseMergeData.noteData.image = imageFromProtoForMergeBase(protoNote.image());
    }
    if (protoNote.has_imageresolution()) {
        baseMergeData.noteData.imageResolution = imageResolutionFromProtoForMergeBase(protoNote.imageresolution());
    }

    for (const auto& protoScrap : protoNote.scraps()) {
        if (!protoScrap.has_id()) {
            continue;
        }

        const QUuid scrapId = uuidFromProtoString(protoScrap.id());
        if (scrapId.isNull()) {
            continue;
        }

        cwScrapBaseIdentityData identityData;
        identityData.hasGeometryData = true;
        for (const auto& protoPoint : protoScrap.outlinepoints()) {
            identityData.geometry.outlinePoints.append(QPointF(protoPoint.x(), protoPoint.y()));
        }
        if (protoScrap.has_notetransformation()) {
            const auto& protoTransform = protoScrap.notetransformation();
            identityData.geometry.transform.noteTransformation.north = protoTransform.northup();
            if (protoTransform.has_scalenumerator()) {
                identityData.geometry.transform.noteTransformation.scale.scaleNumerator.value = protoTransform.scalenumerator().value();
                identityData.geometry.transform.noteTransformation.scale.scaleNumerator.unit = protoTransform.scalenumerator().unit();
            }
            if (protoTransform.has_scaledenominator()) {
                identityData.geometry.transform.noteTransformation.scale.scaleDenominator.value = protoTransform.scaledenominator().value();
                identityData.geometry.transform.noteTransformation.scale.scaleDenominator.unit = protoTransform.scaledenominator().unit();
            }
        }
        identityData.geometry.transform.calculateNoteTransform = protoScrap.calculatenotetransform();
        if (protoScrap.has_type()) {
            switch (protoScrap.type()) {
            case CavewhereProto::Scrap_ScrapType_Plan:
                identityData.geometry.transform.viewType = cwScrapType::Plan;
                break;
            case CavewhereProto::Scrap_ScrapType_RunningProfile:
                identityData.geometry.transform.viewType = cwScrapType::RunningProfile;
                break;
            case CavewhereProto::Scrap_ScrapType_ProjectedProfile:
                identityData.geometry.transform.viewType = cwScrapType::ProjectedProfile;
                if (protoScrap.has_profileviewmatrix()) {
                    identityData.geometry.transform.hasProjectedProfileView = true;
                    identityData.geometry.transform.projectedAzimuth = protoScrap.profileviewmatrix().azimuth();
                    identityData.geometry.transform.projectedDirection = protoScrap.profileviewmatrix().direction();
                }
                break;
            default:
                identityData.geometry.transform.viewType = cwScrapType::Plan;
                break;
            }
        }

        for (const auto& protoStation : protoScrap.notestations()) {
            if (!protoStation.has_id()) {
                continue;
            }
            const QUuid stationId = uuidFromProtoString(protoStation.id());
            if (!stationId.isNull()) {
                identityData.stationIds.insert(stationId);
                cwNoteStation station;
                station.setId(stationId);
                if (protoStation.has_name()) {
                    station.setName(QString::fromStdString(protoStation.name()));
                }
                station.setPositionOnNote(QPointF(protoStation.positiononnote().x(),
                                                  protoStation.positiononnote().y()));
                identityData.stationsById.insert(stationId, station);
            }
        }

        for (const auto& protoLead : protoScrap.leads()) {
            if (!protoLead.has_id()) {
                continue;
            }
            const QUuid leadId = uuidFromProtoString(protoLead.id());
            if (!leadId.isNull()) {
                identityData.leadIds.insert(leadId);
                cwLead lead;
                lead.setId(leadId);
                lead.setPositionOnNote(QPointF(protoLead.positiononnote().x(),
                                               protoLead.positiononnote().y()));
                if (protoLead.has_description()) {
                    lead.setDescription(QString::fromStdString(protoLead.description()));
                }
                if (protoLead.has_size()) {
                    lead.setSize(QSizeF(protoLead.size().width(),
                                        protoLead.size().height()));
                }
                lead.setCompleted(protoLead.completed());
                identityData.leadsById.insert(leadId, lead);
            }
        }

        baseMergeData.baseScrapIdentityByScrapId.insert(scrapId, std::move(identityData));
    }

    return std::make_optional(std::make_pair(noteId, std::move(baseMergeData)));
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

    QHash<QString, cwTrip*> tripsByNotesDir;
    for (cwCave* cave : context.region->caves()) {
        if (cave == nullptr) {
            continue;
        }

        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr) {
                continue;
            }

            const QString notesDirRelativePath = normalizeSyncPath(
                context.repoRoot.relativeFilePath(context.saveLoad->dir(trip->notes()).absolutePath()));
            if (!notesDirRelativePath.isEmpty()) {
                tripsByNotesDir.insert(notesDirRelativePath, trip);
            }
        }
    }

    QHash<QUuid, const cwTripData*> loadedTripsById;
    for (const cwCaveData& caveData : context.loadData->region.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            if (!tripData.id.isNull()) {
                loadedTripsById.insert(tripData.id, &tripData);
            }
        }
    }

    QHash<QString, NoteTripUpdate> tripUpdatesByNotesDir;
    const QSet<QString> trackedExtensions = trackedExtensionsForSyncMerge();
    for (const QString& changedPath : context.report->changedPaths) {
        const QString normalizedPath = normalizeSyncPath(changedPath);
        const std::optional<QString> notesDirPath = notesDirectoryPathForChangedFile(normalizedPath);
        if (!notesDirPath.has_value()) {
            continue;
        }

        const NoteChangedPathKind changedKind = classifyNoteChangedPath(normalizedPath, trackedExtensions);
        if (changedKind == NoteChangedPathKind::Unsupported) {
            continue;
        }

        const auto tripIt = tripsByNotesDir.constFind(*notesDirPath);
        if (tripIt == tripsByNotesDir.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("No trip matches changed notes directory.");
            return result;
        }

        auto updateIt = tripUpdatesByNotesDir.find(*notesDirPath);
        if (updateIt == tripUpdatesByNotesDir.end()) {
            NoteTripUpdate update;
            update.trip = tripIt.value();
            updateIt = tripUpdatesByNotesDir.insert(*notesDirPath, update);
        }

        if (changedKind == NoteChangedPathKind::NoteDescriptor) {
            updateIt->noteDescriptorChanged = true;
            const auto baseMergeData = loadBaseMergeDataForNote(context.repoRoot,
                                                                context.report->mergeBaseHead,
                                                                normalizedPath);
            if (baseMergeData.has_value()) {
                updateIt->baseNoteDataByNoteId.insert(baseMergeData->first, baseMergeData->second.noteData);
                updateIt->baseScrapIdentityByNoteId.insert(baseMergeData->first,
                                                           baseMergeData->second.baseScrapIdentityByScrapId);
            }
        } else {
            updateIt->assetChanged = true;
        }
    }

    QList<NoteTripUpdate> noteTripUpdates;
    noteTripUpdates.reserve(tripUpdatesByNotesDir.size());
    for (auto updateIt = tripUpdatesByNotesDir.cbegin();
         updateIt != tripUpdatesByNotesDir.cend();
         ++updateIt) {
        NoteTripUpdate update = updateIt.value();
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
