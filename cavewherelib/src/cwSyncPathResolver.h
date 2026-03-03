#pragma once

#include "cwCavingRegion.h"

#include <QDir>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QUuid>

class cwNote;
class cwNoteLiDAR;
class cwSaveLoad;
class cwTrip;
struct cwTripData;

namespace cwSyncPathResolver {

struct TripCurrentIndex {
    QHash<QString, QUuid> tripIdsByDescriptorPath;
};

struct TripLoadedIndex {
    QHash<QString, QUuid> tripIdsByDescriptorPath;
};

struct NoteCurrentIndex {
    QHash<QUuid, cwTrip*> tripByNoteId;
    QHash<QUuid, cwNote*> noteById;
    QHash<QString, QSet<QUuid>> noteIdsByAssetPath;
};

struct NoteLoadedIndex {
    QHash<QString, QSet<QUuid>> noteIdsByAssetPath;
};

struct NoteLiDARCurrentIndex {
    QHash<QUuid, cwTrip*> tripByNoteId;
    QHash<QUuid, cwNoteLiDAR*> noteById;
    QHash<QString, QSet<QUuid>> noteIdsByAssetPath;
};

struct NoteLiDARLoadedIndex {
    QHash<QString, QSet<QUuid>> noteIdsByAssetPath;
};

struct TripChangeResolution {
    cwTrip* trip = nullptr;
    bool descriptorChanged = false;
    bool assetChanged = false;
    QHash<QUuid, QString> baseLookupPathByObjectId;
};

QString normalizePath(const QString& path);

TripCurrentIndex buildCurrentTripIndex(const QDir& repoRoot,
                                       const cwCavingRegion* region);
TripLoadedIndex buildLoadedTripIndex(const QDir& repoRoot,
                                     const cwCavingRegionData& regionData);

NoteCurrentIndex buildCurrentNoteIndex(const QDir& repoRoot,
                                       const cwSaveLoad* saveLoad,
                                       const cwCavingRegion* region);
NoteLoadedIndex buildLoadedNoteIndex(const QDir& repoRoot,
                                     const cwCavingRegionData& regionData);

NoteLiDARCurrentIndex buildCurrentNoteLiDARIndex(const QDir& repoRoot,
                                                 const cwSaveLoad* saveLoad,
                                                 const cwCavingRegion* region);
NoteLiDARLoadedIndex buildLoadedNoteLiDARIndex(const QDir& repoRoot,
                                               const cwCavingRegionData& regionData);

QSet<QUuid> resolveNoteIdsForChangedPath(const QDir& repoRoot,
                                         const QString& changedPath,
                                         const NoteCurrentIndex& currentIndex,
                                         const NoteLoadedIndex& loadedIndex);
QSet<QUuid> resolveTripIdsForChangedPath(const QDir& repoRoot,
                                         const QString& changedPath,
                                         const TripCurrentIndex& currentIndex,
                                         const TripLoadedIndex& loadedIndex);

QSet<QUuid> resolveNoteLiDARIdsForChangedPath(const QDir& repoRoot,
                                              const QString& changedPath,
                                              const NoteLiDARCurrentIndex& currentIndex,
                                              const NoteLiDARLoadedIndex& loadedIndex);

QList<TripChangeResolution> resolveChangedNotePaths(const QDir& repoRoot,
                                                    const cwSaveLoad* saveLoad,
                                                    const cwCavingRegion* region,
                                                    const QStringList& changedPaths,
                                                    const NoteCurrentIndex& currentIndex,
                                                    const NoteLoadedIndex& loadedIndex);

QList<TripChangeResolution> resolveChangedNoteLiDARPaths(const QDir& repoRoot,
                                                         const cwSaveLoad* saveLoad,
                                                         const cwCavingRegion* region,
                                                         const QStringList& changedPaths,
                                                         const NoteLiDARCurrentIndex& currentIndex,
                                                         const NoteLiDARLoadedIndex& loadedIndex);

QString currentTripDescriptorPath(const QDir& repoRoot, const cwTrip* trip);
QString loadedTripDescriptorPath(const QDir& repoRoot,
                                 const QString& caveName,
                                 const QString& tripName);
QString currentNoteDescriptorPath(const QDir& repoRoot, const cwNote* note);
QString currentNoteLiDARDescriptorPath(const QDir& repoRoot, const cwNoteLiDAR* note);

} // namespace cwSyncPathResolver
