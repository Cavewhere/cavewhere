#include "cwNoteLiDARSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwGlobals.h"
#include "cwNoteLiDARMergeApplier.h"
#include "cwNoteLiDARMergePlanBuilder.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwTrip.h"
#include "GitRepository.h"
#include "cavewhere.pb.h"
#include "qt.pb.h"
#include "google/protobuf/util/json_util.h"

#include <QFileInfo>
#include <QHash>
#include <QUuid>
#include <QVector3D>

#include <optional>

namespace {

QString normalizeSyncPath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

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

QUuid uuidFromProtoString(const std::string& uuidString)
{
    if (uuidString.empty()) {
        return QUuid();
    }

    const QUuid uuid(QString::fromStdString(uuidString));
    return uuid.isNull() ? QUuid() : uuid;
}

cwLength::Data lengthDataFromProto(const CavewhereProto::Length& protoLength)
{
    cwLength::Data data;
    if (protoLength.has_unit()) {
        data.unit = protoLength.unit();
    }
    if (protoLength.has_value()) {
        data.value = protoLength.value();
    }
    return data;
}

cwNoteLiDARTransformationData lidarTransformationFromProto(
    const CavewhereProto::NoteLiDARTransformation& protoTransform)
{
    cwNoteLiDARTransformationData transform;
    if (protoTransform.has_plantransform()) {
        const auto& protoPlanTransform = protoTransform.plantransform();
        if (protoPlanTransform.has_northup()) {
            transform.north = protoPlanTransform.northup();
        }
        if (protoPlanTransform.has_scalenumerator()) {
            transform.scale.scaleNumerator = lengthDataFromProto(protoPlanTransform.scalenumerator());
        }
        if (protoPlanTransform.has_scaledenominator()) {
            transform.scale.scaleDenominator = lengthDataFromProto(protoPlanTransform.scaledenominator());
        }
    }

    if (protoTransform.has_upmode()) {
        transform.upMode = static_cast<cwNoteLiDARTransformationData::UpMode>(protoTransform.upmode());
    }
    if (protoTransform.has_upsign()) {
        transform.upSign = protoTransform.upsign();
    }
    if (protoTransform.has_upcustom()) {
        const auto& upCustom = protoTransform.upcustom();
        transform.upRotation = QQuaternion(upCustom.scalar(),
                                           upCustom.x(),
                                           upCustom.y(),
                                           upCustom.z());
    }

    return transform;
}

std::optional<cwNoteLiDARData> lidarDataFromProto(const CavewhereProto::NoteLiDAR& protoNote)
{
    if (!protoNote.has_id()) {
        return std::nullopt;
    }

    cwNoteLiDARData noteData;
    noteData.id = uuidFromProtoString(protoNote.id());
    if (noteData.id.isNull()) {
        return std::nullopt;
    }

    noteData.name = QString::fromStdString(protoNote.name());
    const QString rawFilename = QString::fromStdString(protoNote.filename());
    noteData.filename = QFileInfo(rawFilename).fileName().isEmpty()
                            ? rawFilename
                            : QFileInfo(rawFilename).fileName();

    noteData.stations.reserve(protoNote.notestations_size());
    for (const auto& protoStation : protoNote.notestations()) {
        if (!protoStation.has_name() || protoStation.name().empty()) {
            return std::nullopt;
        }
        cwNoteLiDARStation station;
        station.setName(QString::fromStdString(protoStation.name()));
        if (protoStation.has_positiononnote()) {
            const auto& position = protoStation.positiononnote();
            station.setPositionOnNote(QVector3D(position.x(), position.y(), position.z()));
        } else {
            station.setPositionOnNote(QVector3D());
        }
        noteData.stations.append(station);
    }

    if (protoNote.has_notetransformation()) {
        noteData.transfrom = lidarTransformationFromProto(protoNote.notetransformation());
    }
    if (protoNote.has_autocalculatenorth()) {
        noteData.autoCalculateNorth = protoNote.autocalculatenorth();
    }

    return noteData;
}

std::optional<std::pair<QUuid, cwNoteLiDARData>> loadBaseLiDARDataForNote(
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

    CavewhereProto::NoteLiDAR protoNote;
    if (!protoNote.ParseFromArray(contentResult.value().constData(), contentResult.value().size())) {
        const std::string jsonPayload(contentResult.value().constData(),
                                      static_cast<size_t>(contentResult.value().size()));
        const auto parseStatus = google::protobuf::util::JsonStringToMessage(jsonPayload, &protoNote);
        if (!parseStatus.ok()) {
            return std::nullopt;
        }
    }

    auto noteData = lidarDataFromProto(protoNote);
    if (!noteData.has_value()) {
        return std::nullopt;
    }

    return std::make_optional(std::make_pair(noteData->id, *noteData));
}

} // namespace

QString cwNoteLiDARSyncMergeHandler::name() const
{
    return QStringLiteral("cwNoteLiDARSyncMergeHandler");
}

cwReconcileMergeResult cwNoteLiDARSyncMergeHandler::reconcile(const cwReconcileMergeContext& context) const
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

    struct NoteLiDARTripUpdate {
        cwTrip* trip = nullptr;
        const cwTripData* loadedTripData = nullptr;
        QHash<QUuid, cwNoteLiDARData> baseNoteLiDARByNoteId;
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
                context.repoRoot.relativeFilePath(context.saveLoad->dir(trip->notesLiDAR()).absolutePath()));
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

    QHash<QString, NoteLiDARTripUpdate> tripUpdatesByNotesDir;
    for (const QString& changedPath : context.report->changedPaths) {
        const QString normalizedPath = normalizeSyncPath(changedPath);
        if (!normalizedPath.endsWith(QStringLiteral(".cwnote3d"), Qt::CaseInsensitive)) {
            continue;
        }

        const std::optional<QString> notesDirPath = notesDirectoryPathForChangedFile(normalizedPath);
        if (!notesDirPath.has_value()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Changed LiDAR note path is outside notes directory layout.");
            return result;
        }

        const auto tripIt = tripsByNotesDir.constFind(*notesDirPath);
        if (tripIt == tripsByNotesDir.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("No trip matches changed LiDAR notes directory.");
            return result;
        }

        auto updateIt = tripUpdatesByNotesDir.find(*notesDirPath);
        if (updateIt == tripUpdatesByNotesDir.end()) {
            NoteLiDARTripUpdate update;
            update.trip = tripIt.value();
            updateIt = tripUpdatesByNotesDir.insert(*notesDirPath, update);
        }

        const auto baseLidarData = loadBaseLiDARDataForNote(context.repoRoot,
                                                            context.report->mergeBaseHead,
                                                            normalizedPath);
        if (baseLidarData.has_value()) {
            updateIt->baseNoteLiDARByNoteId.insert(baseLidarData->first, baseLidarData->second);
        }
    }

    if (tripUpdatesByNotesDir.isEmpty()) {
        return {};
    }

    QList<NoteLiDARTripUpdate> noteLiDARTripUpdates;
    noteLiDARTripUpdates.reserve(tripUpdatesByNotesDir.size());
    for (auto updateIt = tripUpdatesByNotesDir.cbegin();
         updateIt != tripUpdatesByNotesDir.cend();
         ++updateIt) {
        NoteLiDARTripUpdate update = updateIt.value();
        if (update.trip == nullptr || update.trip->id().isNull()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Trip identity is missing for changed LiDAR payload.");
            return result;
        }

        const auto loadedTripIt = loadedTripsById.constFind(update.trip->id());
        if (loadedTripIt == loadedTripsById.constEnd()) {
            cwReconcileMergeResult result;
            result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            result.handlerName = name();
            result.fallbackReason = QStringLiteral("Loaded data is missing changed trip LiDAR payload.");
            return result;
        }

        update.loadedTripData = loadedTripIt.value();
        noteLiDARTripUpdates.append(update);
    }

    cwReconcileMergeResult result;
    result.outcome = cwReconcileMergeResult::Outcome::Applied;
    result.handlerName = name();

    for (const NoteLiDARTripUpdate& update : noteLiDARTripUpdates) {
        Q_ASSERT(update.trip != nullptr);
        Q_ASSERT(update.loadedTripData != nullptr);

        const auto applyMode = cwNoteLiDARMergePlanBuilder::determineApplyMode(
            update.trip->notesLiDAR(),
            update.loadedTripData->noteLiDARModel);
        if (applyMode == cwNoteLiDARDescriptorApplyMode::Ambiguous) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = QStringLiteral("Ambiguous LiDAR note descriptor structural mapping.");
            return fullReloadResult;
        }

        if (applyMode == cwNoteLiDARDescriptorApplyMode::FullModelReplace) {
            update.trip->notesLiDAR()->setData(update.loadedTripData->noteLiDARModel);
            result.modelMutated = true;
            result.objectsPathReady.append(update.trip);
            continue;
        }

        QString buildFailureReason;
        const auto mergePreparation = cwNoteLiDARMergePlanBuilder::build(
            update.trip->notesLiDAR(),
            update.loadedTripData->noteLiDARModel,
            update.baseNoteLiDARByNoteId,
            &buildFailureReason);
        if (!mergePreparation.has_value()) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = buildFailureReason.isEmpty()
                                                  ? QStringLiteral("Unable to build deterministic LiDAR note merge plan.")
                                                  : buildFailureReason;
            return fullReloadResult;
        }

        for (const cwNoteLiDARMergePlan& plan : mergePreparation->plans) {
            if (!cwNoteLiDARMergeApplier::applyNoteLiDARMergePlan(plan)) {
                cwReconcileMergeResult fullReloadResult;
                fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                fullReloadResult.handlerName = name();
                fullReloadResult.fallbackReason = QStringLiteral("Ambiguous LiDAR station mapping during incremental merge.");
                return fullReloadResult;
            }
        }

        const bool reorderApplied = update.trip->notesLiDAR()->reorderNotes(mergePreparation->orderedNotes);
        if (!reorderApplied) {
            cwReconcileMergeResult fullReloadResult;
            fullReloadResult.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
            fullReloadResult.handlerName = name();
            fullReloadResult.fallbackReason = QStringLiteral("Unable to apply deterministic LiDAR note order.");
            return fullReloadResult;
        }

        result.modelMutated = true;
        result.objectsPathReady.append(update.trip);
    }

    return result;
}
