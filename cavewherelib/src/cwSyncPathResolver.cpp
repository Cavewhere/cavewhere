#include "cwSyncPathResolver.h"

#include "cwNote.h"
#include "cwNoteData.h"
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARData.h"
#include "cwSaveLoad.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwSurveyNoteModel.h"
#include "cwTripData.h"
#include "cwTrip.h"

#include <QFileInfo>
#include <QImageReader>

namespace {

QString relativeNotesDirPath(const QDir& repoRoot,
                             const QString& caveName,
                             const QString& tripName)
{
    const QString dataRootDirName = cwSaveLoad::sanitizeFileName(repoRoot.dirName());
    const QString caveDirName = cwSaveLoad::sanitizeFileName(caveName);
    const QString tripDirName = cwSaveLoad::sanitizeFileName(tripName);
    return QDir::cleanPath(QDir::fromNativeSeparators(
        QDir(dataRootDirName).filePath(
            QDir(caveDirName).filePath(
                QDir(QStringLiteral("trips")).filePath(
                    QDir(tripDirName).filePath(QStringLiteral("notes")))))));
}

QString relativeAssetPath(const QString& notesDirPath, const QString& fileName)
{
    if (notesDirPath.isEmpty() || fileName.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(QDir::fromNativeSeparators(QDir(notesDirPath).filePath(QFileInfo(fileName).fileName())));
}

enum class NoteChangedPathKind {
    Unsupported,
    Descriptor,
    Asset
};

QHash<QString, cwTrip*> buildTripsByNotesDir(const QDir& repoRoot,
                                             const cwSaveLoad* saveLoad,
                                             const cwCavingRegion* region,
                                             bool lidar)
{
    QHash<QString, cwTrip*> tripsByNotesDir;
    if (saveLoad == nullptr || region == nullptr) {
        return tripsByNotesDir;
    }

    for (cwCave* cave : region->caves()) {
        if (cave == nullptr) {
            continue;
        }

        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr) {
                continue;
            }

            QString notesDirRelativePath;
            if (lidar) {
                if (trip->notesLiDAR() == nullptr) {
                    continue;
                }
                notesDirRelativePath = cwSyncPathResolver::normalizePath(
                    repoRoot.relativeFilePath(saveLoad->dir(trip->notesLiDAR()).absolutePath()));
            } else {
                if (trip->notes() == nullptr) {
                    continue;
                }
                notesDirRelativePath = cwSyncPathResolver::normalizePath(
                    repoRoot.relativeFilePath(saveLoad->dir(trip->notes()).absolutePath()));
            }
            if (!notesDirRelativePath.isEmpty()) {
                tripsByNotesDir.insert(notesDirRelativePath, trip);
            }
        }
    }

    return tripsByNotesDir;
}

std::optional<QString> notesDirectoryPathForChangedFile(const QString& path)
{
    const QString normalized = cwSyncPathResolver::normalizePath(path);
    const QString marker = QStringLiteral("/notes/");
    const int markerIndex = normalized.indexOf(marker, 0, Qt::CaseInsensitive);
    if (markerIndex < 0) {
        return std::nullopt;
    }

    return normalized.left(markerIndex + QStringLiteral("/notes").size());
}

QSet<QString> trackedNoteAssetExtensions()
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

NoteChangedPathKind classifyNoteChangedPath(const QString& path, const QSet<QString>& trackedExtensions)
{
    if (path.endsWith(QStringLiteral(".cwnote"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral(".cwnote3d"), Qt::CaseInsensitive)) {
        return NoteChangedPathKind::Descriptor;
    }

    const QString extension = QFileInfo(path).suffix().trimmed().toLower();
    if (trackedExtensions.contains(extension)) {
        return NoteChangedPathKind::Asset;
    }

    return NoteChangedPathKind::Unsupported;
}

template <typename CurrentIndex, typename LoadedIndex, typename ResolveIdsFn, typename CurrentDescriptorPathFn>
QList<cwSyncPathResolver::TripChangeResolution> resolveChangedPathsImpl(
    const QDir& repoRoot,
    const QHash<QString, cwTrip*>& tripsByNotesDir,
    const QStringList& changedPaths,
    const CurrentIndex& currentIndex,
    const LoadedIndex& loadedIndex,
    ResolveIdsFn resolveIds,
    CurrentDescriptorPathFn currentDescriptorPath)
{
    QHash<QString, cwSyncPathResolver::TripChangeResolution> tripUpdatesByNotesDir;
    const QSet<QString> trackedExtensions = trackedNoteAssetExtensions();

    for (const QString& changedPath : changedPaths) {
        const QString normalizedPath = cwSyncPathResolver::normalizePath(changedPath);
        const NoteChangedPathKind changedKind = classifyNoteChangedPath(normalizedPath, trackedExtensions);
        if (changedKind == NoteChangedPathKind::Unsupported) {
            continue;
        }

        const QSet<QUuid> resolvedObjectIds = resolveIds(normalizedPath, currentIndex, loadedIndex);
        bool mappedViaIdentity = false;
        for (const QUuid& objectId : resolvedObjectIds) {
            const auto currentTripIt = currentIndex.tripByNoteId.constFind(objectId);
            if (currentTripIt == currentIndex.tripByNoteId.constEnd()) {
                continue;
            }

            cwTrip* const trip = currentTripIt.value();
            if (trip == nullptr) {
                continue;
            }

            const auto currentObjectIt = currentIndex.noteById.constFind(objectId);
            QString baseLookupPath = normalizedPath;
            if (currentObjectIt != currentIndex.noteById.constEnd()) {
                const QString currentPath = currentDescriptorPath(repoRoot, currentObjectIt.value());
                if (!currentPath.isEmpty()) {
                    baseLookupPath = currentPath;
                }
            }

            const QString notesDirRelativePath = tripsByNotesDir.key(trip);
            auto updateIt = tripUpdatesByNotesDir.find(notesDirRelativePath);
            if (updateIt == tripUpdatesByNotesDir.end()) {
                cwSyncPathResolver::TripChangeResolution update;
                update.trip = trip;
                updateIt = tripUpdatesByNotesDir.insert(notesDirRelativePath, update);
            }

            if (changedKind == NoteChangedPathKind::Descriptor) {
                updateIt->descriptorChanged = true;
                updateIt->baseLookupPathByObjectId.insert(objectId, baseLookupPath);
            } else {
                updateIt->assetChanged = true;
            }
            mappedViaIdentity = true;
        }

        if (mappedViaIdentity) {
            continue;
        }

        const std::optional<QString> notesDirPath = notesDirectoryPathForChangedFile(normalizedPath);
        if (!notesDirPath.has_value()) {
            continue;
        }

        const auto tripIt = tripsByNotesDir.constFind(*notesDirPath);
        if (tripIt == tripsByNotesDir.constEnd()) {
            continue;
        }

        auto updateIt = tripUpdatesByNotesDir.find(*notesDirPath);
        if (updateIt == tripUpdatesByNotesDir.end()) {
            cwSyncPathResolver::TripChangeResolution update;
            update.trip = tripIt.value();
            updateIt = tripUpdatesByNotesDir.insert(*notesDirPath, update);
        }

        if (changedKind == NoteChangedPathKind::Descriptor) {
            updateIt->descriptorChanged = true;
        } else {
            updateIt->assetChanged = true;
        }
    }

    return tripUpdatesByNotesDir.values();
}

QString currentTripDescriptorPathInternal(const QDir& repoRoot, const cwTrip* trip)
{
    if (trip == nullptr || trip->parentCave() == nullptr) {
        return QString();
    }

    const QString dataRootDirName = cwSaveLoad::sanitizeFileName(repoRoot.dirName());
    const QString caveDirName = cwSaveLoad::sanitizeFileName(trip->parentCave()->name());
    const QString tripDirName = cwSaveLoad::sanitizeFileName(trip->name());
    const QString tripFileName = cwSaveLoad::sanitizeFileName(trip->name() + QStringLiteral(".cwtrip"));
    return QDir::cleanPath(QDir::fromNativeSeparators(
        QDir(dataRootDirName).filePath(
            QDir(caveDirName).filePath(
                QDir(QStringLiteral("trips")).filePath(
                    QDir(tripDirName).filePath(tripFileName))))));
}

QString currentNoteDescriptorPathInternal(const QDir& repoRoot, const cwNote* note)
{
    if (note == nullptr || note->parentTrip() == nullptr || note->parentTrip()->parentCave() == nullptr) {
        return QString();
    }

    const QString notesDirPath = relativeNotesDirPath(repoRoot,
                                                      note->parentTrip()->parentCave()->name(),
                                                      note->parentTrip()->name());
    const QString noteFileName = cwSaveLoad::sanitizeFileName(note->name() + QStringLiteral(".cwnote"));
    return QDir::cleanPath(QDir::fromNativeSeparators(QDir(notesDirPath).filePath(noteFileName)));
}

QString currentNoteLiDARDescriptorPathInternal(const QDir& repoRoot, const cwNoteLiDAR* note)
{
    if (note == nullptr || note->parentTrip() == nullptr || note->parentTrip()->parentCave() == nullptr) {
        return QString();
    }

    const QString notesDirPath = relativeNotesDirPath(repoRoot,
                                                      note->parentTrip()->parentCave()->name(),
                                                      note->parentTrip()->name());
    const QString noteFileName = cwSaveLoad::sanitizeFileName(note->name() + QStringLiteral(".cwnote3d"));
    return QDir::cleanPath(QDir::fromNativeSeparators(QDir(notesDirPath).filePath(noteFileName)));
}

std::optional<QUuid> loadNoteIdFromDescriptorPath(const QDir& repoRoot, const QString& changedPath)
{
    const QString absolutePath = repoRoot.absoluteFilePath(changedPath);
    const auto noteResult = cwSaveLoad::loadNote(absolutePath, repoRoot);
    if (noteResult.hasError()) {
        return std::nullopt;
    }

    const QUuid noteId = noteResult.value().id;
    return noteId.isNull() ? std::nullopt : std::make_optional(noteId);
}

std::optional<QUuid> loadNoteLiDARIdFromDescriptorPath(const QDir& repoRoot, const QString& changedPath)
{
    const QString absolutePath = repoRoot.absoluteFilePath(changedPath);
    const auto noteResult = cwSaveLoad::loadNoteLiDAR(absolutePath, repoRoot);
    if (noteResult.hasError()) {
        return std::nullopt;
    }

    const QUuid noteId = noteResult.value().id;
    return noteId.isNull() ? std::nullopt : std::make_optional(noteId);
}

std::optional<QUuid> loadTripIdFromDescriptorPath(const QDir& repoRoot, const QString& changedPath)
{
    const QString absolutePath = repoRoot.absoluteFilePath(changedPath);
    const auto tripResult = cwSaveLoad::loadTrip(absolutePath);
    if (tripResult.hasError()) {
        return std::nullopt;
    }

    const QUuid tripId = tripResult.value().id;
    return tripId.isNull() ? std::nullopt : std::make_optional(tripId);
}

void addIdsForPath(const QString& path, const QHash<QString, QSet<QUuid>>& idsByPath, QSet<QUuid>* ids)
{
    if (ids == nullptr) {
        return;
    }

    const auto it = idsByPath.constFind(path);
    if (it == idsByPath.constEnd()) {
        return;
    }

    ids->unite(it.value());
}

QString loadedTripDescriptorPathInternal(const QDir& repoRoot,
                                         const QString& caveName,
                                         const QString& tripName)
{
    const QString dataRootDirName = cwSaveLoad::sanitizeFileName(repoRoot.dirName());
    const QString caveDirName = cwSaveLoad::sanitizeFileName(caveName);
    const QString tripDirName = cwSaveLoad::sanitizeFileName(tripName);
    const QString tripFileName = cwSaveLoad::sanitizeFileName(tripName + QStringLiteral(".cwtrip"));
    return QDir::cleanPath(QDir::fromNativeSeparators(
        QDir(dataRootDirName).filePath(
            QDir(caveDirName).filePath(
                QDir(QStringLiteral("trips")).filePath(
                    QDir(tripDirName).filePath(tripFileName))))));
}

} // namespace

namespace cwSyncPathResolver {

QString normalizePath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

TripCurrentIndex buildCurrentTripIndex(const QDir& repoRoot,
                                       const cwCavingRegion* region)
{
    TripCurrentIndex index;
    if (region == nullptr) {
        return index;
    }

    for (cwCave* cave : region->caves()) {
        if (cave == nullptr) {
            continue;
        }

        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr || trip->id().isNull()) {
                continue;
            }

            const QString descriptorPath = currentTripDescriptorPathInternal(repoRoot, trip);
            if (!descriptorPath.isEmpty()) {
                index.tripIdsByDescriptorPath.insert(descriptorPath, trip->id());
            }
        }
    }

    return index;
}

TripLoadedIndex buildLoadedTripIndex(const QDir& repoRoot,
                                     const cwCavingRegionData& regionData)
{
    TripLoadedIndex index;

    for (const cwCaveData& caveData : regionData.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            if (tripData.id.isNull()) {
                continue;
            }

            const QString descriptorPath = loadedTripDescriptorPathInternal(repoRoot,
                                                                            caveData.name,
                                                                            tripData.name);
            if (!descriptorPath.isEmpty()) {
                index.tripIdsByDescriptorPath.insert(descriptorPath, tripData.id);
            }
        }
    }

    return index;
}

NoteCurrentIndex buildCurrentNoteIndex(const QDir& repoRoot,
                                       const cwSaveLoad* saveLoad,
                                       const cwCavingRegion* region)
{
    NoteCurrentIndex index;
    if (saveLoad == nullptr || region == nullptr) {
        return index;
    }

    for (cwCave* cave : region->caves()) {
        if (cave == nullptr) {
            continue;
        }

        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr || trip->notes() == nullptr) {
                continue;
            }

            for (cwNote* note : trip->notes()->notes()) {
                if (note == nullptr || note->id().isNull()) {
                    continue;
                }

                index.tripByNoteId.insert(note->id(), trip);
                index.noteById.insert(note->id(), note);

                const QString absoluteAssetPath = saveLoad->absolutePath(note, note->image().path());
                const QString relativePath = normalizePath(repoRoot.relativeFilePath(absoluteAssetPath));
                if (!relativePath.isEmpty() && !relativePath.startsWith(QStringLiteral(".."))) {
                    index.noteIdsByAssetPath[relativePath].insert(note->id());
                }
            }
        }
    }

    return index;
}

NoteLoadedIndex buildLoadedNoteIndex(const QDir& repoRoot,
                                     const cwCavingRegionData& regionData)
{
    NoteLoadedIndex index;

    for (const cwCaveData& caveData : regionData.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            const QString notesDirPath = relativeNotesDirPath(repoRoot, caveData.name, tripData.name);
            for (const cwNoteData& noteData : tripData.noteModel.notes) {
                if (noteData.id.isNull()) {
                    continue;
                }

                const QString relativePath = relativeAssetPath(notesDirPath, noteData.image.path());
                if (!relativePath.isEmpty()) {
                    index.noteIdsByAssetPath[relativePath].insert(noteData.id);
                }
            }
        }
    }

    return index;
}

NoteLiDARCurrentIndex buildCurrentNoteLiDARIndex(const QDir& repoRoot,
                                                 const cwSaveLoad* saveLoad,
                                                 const cwCavingRegion* region)
{
    NoteLiDARCurrentIndex index;
    if (saveLoad == nullptr || region == nullptr) {
        return index;
    }

    for (cwCave* cave : region->caves()) {
        if (cave == nullptr) {
            continue;
        }

        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr || trip->notesLiDAR() == nullptr) {
                continue;
            }

            const QList<QObject*> notes = trip->notesLiDAR()->notes();
            for (QObject* noteObject : notes) {
                auto* note = qobject_cast<cwNoteLiDAR*>(noteObject);
                if (note == nullptr || note->id().isNull()) {
                    continue;
                }

                index.tripByNoteId.insert(note->id(), trip);
                index.noteById.insert(note->id(), note);

                const QString absoluteAssetPath = saveLoad->absolutePath(note, note->filename());
                const QString relativePath = normalizePath(repoRoot.relativeFilePath(absoluteAssetPath));
                if (!relativePath.isEmpty() && !relativePath.startsWith(QStringLiteral(".."))) {
                    index.noteIdsByAssetPath[relativePath].insert(note->id());
                }
            }
        }
    }

    return index;
}

NoteLiDARLoadedIndex buildLoadedNoteLiDARIndex(const QDir& repoRoot,
                                               const cwCavingRegionData& regionData)
{
    NoteLiDARLoadedIndex index;

    for (const cwCaveData& caveData : regionData.caves) {
        for (const cwTripData& tripData : caveData.trips) {
            const QString notesDirPath = relativeNotesDirPath(repoRoot, caveData.name, tripData.name);
            for (const cwNoteLiDARData& noteData : tripData.noteLiDARModel.notes) {
                if (noteData.id.isNull()) {
                    continue;
                }

                const QString relativePath = relativeAssetPath(notesDirPath, noteData.filename);
                if (!relativePath.isEmpty()) {
                    index.noteIdsByAssetPath[relativePath].insert(noteData.id);
                }
            }
        }
    }

    return index;
}

QSet<QUuid> resolveNoteIdsForChangedPath(const QDir& repoRoot,
                                         const QString& changedPath,
                                         const NoteCurrentIndex& currentIndex,
                                         const NoteLoadedIndex& loadedIndex)
{
    QSet<QUuid> resolvedIds;
    const QString normalizedPath = normalizePath(changedPath);

    if (normalizedPath.endsWith(QStringLiteral(".cwnote"), Qt::CaseInsensitive)) {
        const auto noteId = loadNoteIdFromDescriptorPath(repoRoot, normalizedPath);
        if (noteId.has_value()) {
            resolvedIds.insert(*noteId);
        }
        return resolvedIds;
    }

    addIdsForPath(normalizedPath, currentIndex.noteIdsByAssetPath, &resolvedIds);
    addIdsForPath(normalizedPath, loadedIndex.noteIdsByAssetPath, &resolvedIds);
    return resolvedIds;
}

QSet<QUuid> resolveTripIdsForChangedPath(const QDir& repoRoot,
                                         const QString& changedPath,
                                         const TripCurrentIndex& currentIndex,
                                         const TripLoadedIndex& loadedIndex)
{
    QSet<QUuid> resolvedIds;
    const QString normalizedPath = normalizePath(changedPath);

    const auto currentIt = currentIndex.tripIdsByDescriptorPath.constFind(normalizedPath);
    if (currentIt != currentIndex.tripIdsByDescriptorPath.constEnd()) {
        resolvedIds.insert(currentIt.value());
    }

    const auto loadedIt = loadedIndex.tripIdsByDescriptorPath.constFind(normalizedPath);
    if (loadedIt != loadedIndex.tripIdsByDescriptorPath.constEnd()) {
        resolvedIds.insert(loadedIt.value());
    }

    if (!resolvedIds.isEmpty()) {
        return resolvedIds;
    }

    const auto tripId = loadTripIdFromDescriptorPath(repoRoot, normalizedPath);
    if (tripId.has_value()) {
        resolvedIds.insert(*tripId);
    }

    return resolvedIds;
}

QSet<QUuid> resolveNoteLiDARIdsForChangedPath(const QDir& repoRoot,
                                              const QString& changedPath,
                                              const NoteLiDARCurrentIndex& currentIndex,
                                              const NoteLiDARLoadedIndex& loadedIndex)
{
    QSet<QUuid> resolvedIds;
    const QString normalizedPath = normalizePath(changedPath);

    if (normalizedPath.endsWith(QStringLiteral(".cwnote3d"), Qt::CaseInsensitive)) {
        const auto noteId = loadNoteLiDARIdFromDescriptorPath(repoRoot, normalizedPath);
        if (noteId.has_value()) {
            resolvedIds.insert(*noteId);
        }
        return resolvedIds;
    }

    addIdsForPath(normalizedPath, currentIndex.noteIdsByAssetPath, &resolvedIds);
    addIdsForPath(normalizedPath, loadedIndex.noteIdsByAssetPath, &resolvedIds);
    return resolvedIds;
}

QList<TripChangeResolution> resolveChangedNotePaths(const QDir& repoRoot,
                                                    const cwSaveLoad* saveLoad,
                                                    const cwCavingRegion* region,
                                                    const QStringList& changedPaths,
                                                    const NoteCurrentIndex& currentIndex,
                                                    const NoteLoadedIndex& loadedIndex)
{
    const auto tripsByNotesDir = buildTripsByNotesDir(repoRoot, saveLoad, region, false);
    return resolveChangedPathsImpl(
        repoRoot,
        tripsByNotesDir,
        changedPaths,
        currentIndex,
        loadedIndex,
        [&](const QString& normalizedPath, const NoteCurrentIndex& current, const NoteLoadedIndex& loaded) {
            return resolveNoteIdsForChangedPath(repoRoot, normalizedPath, current, loaded);
        },
        [](const QDir& root, const cwNote* note) {
            return currentNoteDescriptorPath(root, note);
        });
}

QList<TripChangeResolution> resolveChangedNoteLiDARPaths(const QDir& repoRoot,
                                                         const cwSaveLoad* saveLoad,
                                                         const cwCavingRegion* region,
                                                         const QStringList& changedPaths,
                                                         const NoteLiDARCurrentIndex& currentIndex,
                                                         const NoteLiDARLoadedIndex& loadedIndex)
{
    const auto tripsByNotesDir = buildTripsByNotesDir(repoRoot, saveLoad, region, true);
    return resolveChangedPathsImpl(
        repoRoot,
        tripsByNotesDir,
        changedPaths,
        currentIndex,
        loadedIndex,
        [&](const QString& normalizedPath, const NoteLiDARCurrentIndex& current, const NoteLiDARLoadedIndex& loaded) {
            return resolveNoteLiDARIdsForChangedPath(repoRoot, normalizedPath, current, loaded);
        },
        [](const QDir& root, const cwNoteLiDAR* note) {
            return currentNoteLiDARDescriptorPath(root, note);
        });
}

QString currentTripDescriptorPath(const QDir& repoRoot, const cwTrip* trip)
{
    return currentTripDescriptorPathInternal(repoRoot, trip);
}

QString loadedTripDescriptorPath(const QDir& repoRoot,
                                 const QString& caveName,
                                 const QString& tripName)
{
    return loadedTripDescriptorPathInternal(repoRoot, caveName, tripName);
}

QString currentNoteDescriptorPath(const QDir& repoRoot, const cwNote* note)
{
    return currentNoteDescriptorPathInternal(repoRoot, note);
}

QString currentNoteLiDARDescriptorPath(const QDir& repoRoot, const cwNoteLiDAR* note)
{
    return currentNoteLiDARDescriptorPathInternal(repoRoot, note);
}

} // namespace cwSyncPathResolver
