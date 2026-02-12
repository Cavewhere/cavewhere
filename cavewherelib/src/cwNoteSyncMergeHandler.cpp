#include "cwNoteSyncMergeHandler.h"

#include "cwCavingRegion.h"
#include "cwDiff.h"
#include "cwGlobals.h"
#include "cwImageResolution.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <optional>
#include <utility>

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
    NoteLiDARDescriptor,
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

    if (path.endsWith(QStringLiteral(".cwnote3d"), Qt::CaseInsensitive)) {
        return NoteChangedPathKind::NoteLiDARDescriptor;
    }

    const QString extension = QFileInfo(path).suffix().trimmed().toLower();
    if (trackedExtensions.contains(extension)) {
        return NoteChangedPathKind::Asset;
    }

    return NoteChangedPathKind::Unsupported;
}

template<typename Container, typename IdAccessor>
std::optional<std::vector<QUuid>> collectOrderedUniqueIds(const Container& items, IdAccessor idAccessor)
{
    std::vector<QUuid> orderedIds;
    orderedIds.reserve(static_cast<size_t>(items.size()));
    QSet<QUuid> seenIds;

    for (const auto& item : items) {
        const QUuid id = idAccessor(item);
        if (id.isNull() || seenIds.contains(id)) {
            return std::nullopt;
        }
        seenIds.insert(id);
        orderedIds.push_back(id);
    }

    return orderedIds;
}

enum class NoteDescriptorApplyMode {
    FullModelReplace,
    StructuralMerge,
    Ambiguous
};

struct NoteStructuralMergePlan {
    cwNote* note = nullptr;
    const cwNoteData* loadedNoteData = nullptr;
    std::vector<QUuid> mergedScrapOrder;
};

struct NoteStructuralMergePreparation {
    QList<NoteStructuralMergePlan> plans;
    QList<cwNote*> orderedNotes;
};

bool haveSameIdsIgnoringOrder(const std::vector<QUuid>& currentIds, const std::vector<QUuid>& loadedIds)
{
    if (currentIds.size() != loadedIds.size()) {
        return false;
    }

    QSet<QUuid> currentIdSet(currentIds.begin(), currentIds.end());
    QSet<QUuid> loadedIdSet(loadedIds.begin(), loadedIds.end());
    return currentIdSet == loadedIdSet;
}

NoteDescriptorApplyMode determineNoteDescriptorApplyMode(cwSurveyNoteModel* noteModel,
                                                         const cwSurveyNoteModelData& loadedNoteModelData)
{
    if (noteModel == nullptr) {
        return NoteDescriptorApplyMode::Ambiguous;
    }

    const auto currentNoteIds = collectOrderedUniqueIds(
        noteModel->notes(),
        [](const cwNote* note) {
            return note != nullptr ? note->id() : QUuid();
        });
    const auto loadedNoteIds = collectOrderedUniqueIds(
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

    if (haveSameIdsIgnoringOrder(currentNoteIds.value(), loadedNoteIds.value())) {
        return NoteDescriptorApplyMode::StructuralMerge;
    }

    return NoteDescriptorApplyMode::FullModelReplace;
}

std::optional<std::vector<QUuid>> mergedScrapOrderForNote(const cwNote* note, const cwNoteData& loadedNoteData)
{
    if (note == nullptr) {
        return std::nullopt;
    }

    const auto currentScrapIds = collectOrderedUniqueIds(
        note->scraps(),
        [](const cwScrap* scrap) {
            return scrap != nullptr ? scrap->id() : QUuid();
        });
    const auto loadedScrapIds = collectOrderedUniqueIds(
        loadedNoteData.scraps,
        [](const cwScrapData& scrapData) {
            return scrapData.id;
        });

    if (!currentScrapIds.has_value() || !loadedScrapIds.has_value()) {
        return std::nullopt;
    }

    const auto edits = cwDiff::diff<QUuid>(currentScrapIds.value(), loadedScrapIds.value());
    const auto mergedOrder = cwDiff::applyEditScript(edits, currentScrapIds.value(), loadedScrapIds.value());
    if (mergedOrder != loadedScrapIds.value()) {
        return std::nullopt;
    }

    return mergedOrder;
}

std::optional<NoteStructuralMergePreparation> buildNoteStructuralMergePreparation(cwSurveyNoteModel* noteModel,
                                                                                  const cwSurveyNoteModelData& loadedNoteModelData)
{
    if (noteModel == nullptr) {
        return std::nullopt;
    }

    QHash<QUuid, cwNote*> currentNotesById;
    currentNotesById.reserve(noteModel->rowCount());
    for (cwNote* note : noteModel->notes()) {
        if (note == nullptr || note->id().isNull() || currentNotesById.contains(note->id())) {
            return std::nullopt;
        }
        currentNotesById.insert(note->id(), note);
    }

    if (currentNotesById.size() != loadedNoteModelData.notes.size()) {
        return std::nullopt;
    }

    NoteStructuralMergePreparation preparation;
    preparation.plans.reserve(loadedNoteModelData.notes.size());
    preparation.orderedNotes.reserve(loadedNoteModelData.notes.size());
    QSet<QUuid> seenLoadedNoteIds;
    for (const cwNoteData& loadedNoteData : loadedNoteModelData.notes) {
        if (loadedNoteData.id.isNull()
            || seenLoadedNoteIds.contains(loadedNoteData.id)
            || !currentNotesById.contains(loadedNoteData.id)) {
            return std::nullopt;
        }

        seenLoadedNoteIds.insert(loadedNoteData.id);
        cwNote* const currentNote = currentNotesById.value(loadedNoteData.id);
        auto mergedScrapOrder = mergedScrapOrderForNote(currentNote, loadedNoteData);
        if (!mergedScrapOrder.has_value()) {
            return std::nullopt;
        }

        NoteStructuralMergePlan plan;
        plan.note = currentNote;
        plan.loadedNoteData = &loadedNoteData;
        plan.mergedScrapOrder = std::move(mergedScrapOrder.value());
        preparation.plans.append(std::move(plan));
        preparation.orderedNotes.append(currentNote);
    }

    return preparation;
}

void applyNoteStructuralMergePlan(const NoteStructuralMergePlan& plan)
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

        scrap->setData(*loadedScrapData);
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
                scrap->setData(*loadedScrapData);
            }
        }
    }
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
        bool noteLiDARDescriptorChanged = false;
        bool assetChanged = false;
        NoteDescriptorApplyMode noteDescriptorApplyMode = NoteDescriptorApplyMode::FullModelReplace;
        QList<NoteStructuralMergePlan> noteStructuralMergePlans;
        QList<cwNote*> reorderedNotes;
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
            return {};
        }

        const NoteChangedPathKind changedKind = classifyNoteChangedPath(normalizedPath, trackedExtensions);
        if (changedKind == NoteChangedPathKind::Unsupported) {
            return {};
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
        } else if (changedKind == NoteChangedPathKind::NoteLiDARDescriptor) {
            updateIt->noteLiDARDescriptorChanged = true;
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
            const auto mergePreparation = buildNoteStructuralMergePreparation(update.trip->notes(),
                                                                              update.loadedTripData->noteModel);
            if (!mergePreparation.has_value()) {
                cwReconcileMergeResult result;
                result.outcome = cwReconcileMergeResult::Outcome::RequiresFullReload;
                result.handlerName = name();
                result.fallbackReason = QStringLiteral("Unable to build deterministic note scrap merge plan.");
                return result;
            }
            update.noteStructuralMergePlans = mergePreparation->plans;
            update.reorderedNotes = mergePreparation->orderedNotes;
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
                for (const NoteStructuralMergePlan& plan : update.noteStructuralMergePlans) {
                    applyNoteStructuralMergePlan(plan);
                }

                QList<QObject*> reorderedNotes;
                reorderedNotes.reserve(update.reorderedNotes.size());
                for (cwNote* note : update.reorderedNotes) {
                    reorderedNotes.append(note);
                }
                const bool reorderApplied = update.trip->notes()->reorderNotes(reorderedNotes);
                Q_ASSERT(reorderApplied);
            } else {
                update.trip->notes()->setData(update.loadedTripData->noteModel);
            }
            result.modelMutated = true;
        }

        if (update.noteLiDARDescriptorChanged) {
            update.trip->notesLiDAR()->setData(update.loadedTripData->noteLiDARModel);
            result.modelMutated = true;
        }

        if (update.noteDescriptorChanged || update.noteLiDARDescriptorChanged || update.assetChanged) {
            objectPathReadySet.insert(update.trip);
        }
    }

    result.objectsPathReady.reserve(objectPathReadySet.size());
    for (QObject* object : std::as_const(objectPathReadySet)) {
        result.objectsPathReady.append(object);
    }

    return result;
}
