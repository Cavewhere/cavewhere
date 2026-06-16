/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwLinePlotTask.h"
#include "cwConcurrent.h"
#include "cwSurvexExporterRegion.h"
#include "cwCavernRunner.h"
#include "cwSurvex3DFileReader.h"
#include "cwLinePlotGeometry.h"
#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwScrap.h"
#include "cwSurveyChunk.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwErrorModel.h"

// Qt includes
#include <QHash>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QtGlobal>
#include <cmath>

namespace {

// Cave name encoding used by the line-plot driver. Cave names emitted by
// cwSurvexExporterRegion become survex *begin / *end identifiers, and cavern
// prefixes every station in a cave with that identifier. We pick a synthetic
// "cave_<32 hex>" form (the cave UUID in QUuid::Id128 layout) so:
//   - the prefix round-trips: splitLookupByCave can recover the QUuid from a
//     cavern-emitted "cave_<32hex>.<station>" line with no name-collision risk
//     across caves;
//   - the survex syntax is satisfied: cave_<hex> only uses [a-zA-Z0-9_], which
//     matches cwStationValidator::validCharactersRegex();
//   - the user-facing survex exporter (cwSurveyExportManager) is unaffected -
//     this rewrite happens on a worker-local copy of the region snapshot, so
//     human-readable cave names persist in any user-driven export.
constexpr QLatin1String kCaveNamePrefix("cave_");

// QUuid::fromString refuses the 32-hex-no-hyphens form even when wrapped in
// braces; it requires either the dashed RFC-4122 layout (36 chars) or the
// dashed-and-braced layout (38 chars). We emit the no-hyphen form via
// QUuid::Id128 because it is what cavern's name syntax allows (cavern's
// validCharactersRegex permits hex + underscore but not hyphens inside an
// identifier prefix), so on the way back we re-insert hyphens at the
// 8/4/4/4/12 boundaries before handing the string to QUuid.
QString reinsertUuidHyphens(const QString& hex32)
{
    Q_ASSERT(hex32.size() == 32);
    return hex32.mid(0, 8) + QLatin1Char('-')
         + hex32.mid(8, 4) + QLatin1Char('-')
         + hex32.mid(12, 4) + QLatin1Char('-')
         + hex32.mid(16, 4) + QLatin1Char('-')
         + hex32.mid(20, 12);
}

} // namespace

QString cwLinePlotTask::cavernCaveNameFor(const QUuid& caveId)
{
    return kCaveNamePrefix + caveId.toString(QUuid::Id128);
}

QRegularExpression cwLinePlotTask::cavernStationRegex()
{
    // ^cave_<32 hex>\.(\S.*)$ — the tail is intentionally permissive
    // (must start with a non-whitespace, then any trailing text) so it
    // covers:
    //   - native shots:    cave_<uuid>.<station>
    //   - trip-attached:   cave_<uuid>.trip_<uuid>.<file-begin>.<station>
    //   - cave-attached:   cave_<uuid>.<file-begin>.<station>
    // External Survex files can introduce nested *begin scopes that
    // surface as dotted segments inside the tail, and Walls' empty-name
    // quirk can emit trailing spaces; tightening the tail to the native
    // station validator would drop both. The leading \\S guard rejects
    // pure-whitespace tails (e.g. cave_<uuid>. ) so a malformed external
    // file can't pollute the lookup with whitespace keys. The cave UUID
    // prefix is still strictly bounded so the integer-keyed legacy form
    // ("<digit>.station") remains rejected.
    //
    // CaseInsensitiveOption: cwStationPositionLookup keys via
    // cwStation::canonicalKey() which folds station names to lower case
    // (cwStation.h:66). cavernCaveNameFor() already emits lowercase via
    // QUuid::Id128 — but the flag keeps the matcher robust if QUuid::Id128
    // ever changes its case.
    return QRegularExpression(
        QStringLiteral("^cave_([0-9a-fA-F]{32})\\.(\\S.*)$"),
        QRegularExpression::CaseInsensitiveOption);
}

cwLinePlotTask::LinePlotCaveData::LinePlotCaveData() :
    DepthLengthChanged(false),
    Depth(0.0),
    Length(0.0),
    StationPostionsChanged(false),
    NetworkChanged(false)
{
}
void cwLinePlotTask::LinePlotResultData::clear()
{
    Caves.clear();
    Trips.clear();
    Scraps.clear();
    StationPositions.clear();
    TripVertexRanges.clear();
    TripUuids.clear();
}

cwLinePlotTask::TripDataPtrs::TripDataPtrs(cwTrip *trip)
{
    Trip = trip;
    foreach(cwNote* note, trip->notes()->notes()) {
        foreach(cwScrap* scrap, note->scraps()) {
            Scraps.append(scrap);
        }
    }
}

cwLinePlotTask::CaveDataPtrs::CaveDataPtrs(cwCave *cave)
{
    Cave = cave;
    foreach(cwTrip* trip, cave->trips()) {
        Trips.append(cwLinePlotTask::TripDataPtrs(trip));
    }
}

cwLinePlotTask::RegionDataPtrs::RegionDataPtrs(const cwCavingRegion *region)
{
    foreach(cwCave* cave, region->caves()) {
        Caves.append(cwLinePlotTask::CaveDataPtrs(cave));
    }
}

cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup(cwCave *cave)
{
    for(int tripIndex = 0; tripIndex < cave->tripCount(); tripIndex++) {
        cwTrip* trip = cave->trip(tripIndex);
        foreach(cwSurveyChunk* surveyChunk, trip->chunks()) {
            foreach(cwStation station, surveyChunk->stations()) {
                MapStationToTrip.insert(station.name().toUpper(), tripIndex);
            }
        }

        int scrapIndex = 0;
        foreach(cwNote* note, trip->notes()->notes()) {
            for(int i = 0; i < note->scraps().size(); i++) {
                cwScrap* scrap = note->scrap(i);

                foreach(cwNoteStation noteStation, scrap->stations()) {
                    MapStationToScrap.insert(noteStation.name().toUpper(), QPair<int, int>(tripIndex, scrapIndex));
                }

                scrapIndex++;
            }
        }
    }
}

struct cwLinePlotTask::LinePlotWorker {
    explicit LinePlotWorker(cwLinePlotTask::Input input)
        : InputData(std::move(input))
    {
    }

    cwLinePlotTask::LinePlotResultData run()
    {
        cwLinePlotTask::LinePlotResultData result;

        if (InputData.regionData.caves.isEmpty()) {
            return result;
        }

        // Prepare working copy of region data
        encodeCaveNames(InputData.regionData);
        Region.setData(InputData.regionData);

        initializeCaveStationLookups();

        if (!checkForErrors(result)) {
            return result;
        }

        // QTemporaryDir owns the lifecycle of the .svx input, the .3d output,
        // and cavern's .log/.err sidecars. Auto-removed on scope exit, so
        // failure on any step below leaves /tmp clean.
        QTemporaryDir workDir;
        if (!workDir.isValid()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Export;
            error.message = QStringLiteral("Failed to create temporary directory for solve");
            result.setSolveError(error);
            return result;
        }

        const QString svxPath      = workDir.filePath(QStringLiteral("region.svx"));
        const QString output3dPath = workDir.filePath(QStringLiteral("region.3d"));

        if (!exportSurvex(svxPath, result)) {
            return result;
        }

        if (!runCavern(svxPath, output3dPath, result)) {
            return result;
        }

        cwSurvex3DFileReader reader;
        cwSurvex3DFileReader::NetworkAndLookup parsed = reader.readNetworkAndLookup(output3dPath);
        if (parsed.lookup.isEmpty()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Parse;
            error.message = QStringLiteral("Cavern produced no station positions in %1").arg(output3dPath);
            result.setSolveError(error);
            return result;
        }
        applyWorldOriginOffset(parsed.lookup, InputData.regionData.worldOrigin);
        updateStationPositionForCaves(parsed.lookup, result);
        result.setRegionNetwork(std::move(parsed.network));

        cwLinePlotGeometry::Result geometry = generateGeometry();
        result.setPositions(geometry.points);
        result.setTripVertexRanges(geometry.tripVertexRanges);
        result.setTripUuids(geometry.tripUuids);

        updateDepthLength(geometry.cavesLengthAndDepths, result);
        updateCaveNetworks(result);

        return result;
    }

private:
    cwLinePlotTask::Input InputData;
    cwCavingRegion Region;
    // All cave-keyed bookkeeping uses cwCave::id() rather than the integer
    // position in InputData.regionPointers.Caves. The previous int-keyed
    // tables broke down as soon as the driver emitted "cave_<uuid>" prefixes
    // (commit 7) because integer cave indexes no longer have a representation
    // in the cavern output; UUIDs do.
    QHash<QUuid, cwStationPositionLookup> CaveStationLookups;
    QHash<QUuid, cwLinePlotTask::StationTripScrapLookup> TripLookups;
    // Index from cave UUID to the worker-internal cwCave* (owned by Region)
    // and to the original-thread cwCave + scrap/trip pointers carried by
    // InputData.regionPointers. Built once in initializeCaveLookups() so the
    // rest of the worker can stay UUID-keyed.
    QHash<QUuid, cwCave*> InternalCaveByUuid;
    QHash<QUuid, cwLinePlotTask::CaveDataPtrs> ExternalCaveByUuid;

    void encodeCaveNames(cwCavingRegionData& regionData)
    {
        // Cave names are rewritten to cavernCaveNameFor(cave.id) so the
        // exporter emits "*begin cave_<uuid>" and splitLookupByCave can
        // recover cwCave::id() from the cavern station prefix. Caller
        // contract: cave.id must be non-null and must equal the id of the
        // corresponding entry in InputData.regionPointers.Caves[i].Cave -
        // cwLinePlotManager naturally satisfies this via cwCavingRegion::data();
        // synthetic callers (cwTripLinePlotTask) generate a UUID before
        // building Input so the two sides stay aligned.
        for (cwCaveData& cave : regionData.caves) {
            Q_ASSERT(!cave.id.isNull());
            cave.name = cwLinePlotTask::cavernCaveNameFor(cave.id);
        }
    }

    void initializeCaveStationLookups()
    {
        const int numCaves = Region.caveCount();
        CaveStationLookups.reserve(numCaves);
        InternalCaveByUuid.reserve(numCaves);

        for (int i = 0; i < numCaves; i++) {
            cwCave* cave = Region.cave(i);
            const QUuid id = cave->id();
            InternalCaveByUuid.insert(id, cave);
            CaveStationLookups.insert(id, cave->stationPositionLookup());
        }

        // RegionDataPtrs is index-aligned with Region by construction, but the
        // worker drops that coupling: every consumer keys by cwCave::id().
        const QList<cwLinePlotTask::CaveDataPtrs>& external = InputData.regionPointers.Caves;
        ExternalCaveByUuid.reserve(external.size());
        for (const cwLinePlotTask::CaveDataPtrs& cavePtrs : external) {
            if (cavePtrs.Cave != nullptr) {
                ExternalCaveByUuid.insert(cavePtrs.Cave->id(), cavePtrs);
            }
        }
    }

    bool exportSurvex(const QString& svxPath, cwLinePlotTask::LinePlotResultData& result)
    {
        // The line-plot driver always emits InternalUuid-style cave names
        // (encodeCaveNames already rewrote cave.name); the attachment-dir
        // maps come straight from the Input the caller built.
        cwSurvexExporterRegion::Options exportOptions;
        exportOptions.caveAttachmentDirs = InputData.caveAttachmentDirs;
        exportOptions.tripAttachmentDirs = InputData.tripAttachmentDirs;

        const Monad::ResultBase r =
            cwSurvexExporterRegion::exportRegion(InputData.regionData, svxPath, exportOptions);
        if (r.hasError()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Export;
            error.message = r.errorMessage();
            result.setSolveError(error);
            return false;
        }
        return true;
    }

    bool runCavern(const QString& svxPath,
                   const QString& output3dPath,
                   cwLinePlotTask::LinePlotResultData& result)
    {
        const Monad::Result<cwCavernRunner::Result> r =
            cwCavernRunner::run(svxPath, output3dPath);
        if (r.hasError()) {
            // cavern_run reported failure before the log file could be parsed
            // back into a separate field; for this step the error message IS
            // the captured log text (cwCavernRunner sets them equal).
            result.CavernLog = r.errorMessage();

            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Cavern;
            error.exitCode = 1;        // non-zero; cwCavernRunner doesn't expose the precise rc on error
            error.message = r.errorMessage();
            result.setSolveError(error);
            return false;
        }
        const cwCavernRunner::Result cavern = r.value();
        // Always publish cavern's diagnostic output — even on a clean solve
        // the log carries info-level messages (e.g. "I've fixed X at 0,0,0")
        // that CavernOutputPage exposes to the user.
        result.CavernLog = cavern.logText;
        result.LoopClosureStats = cavern.loopClosureStats;

        if (!QFileInfo::exists(cavern.output3dPath)) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Cavern;
            error.exitCode = cavern.exitCode;
            error.message = QStringLiteral("Cavern reported success but produced no .3d output");
            result.setSolveError(error);
            return false;
        }
        return true;
    }

    cwLinePlotGeometry::Result generateGeometry()
    {
        const Monad::Result<cwLinePlotGeometry::Result> result =
            cwLinePlotGeometry::generate(Region.data());
        if (result.hasError()) {
            return cwLinePlotGeometry::Result();
        }
        return result.value();
    }

    bool checkForErrors(cwLinePlotTask::LinePlotResultData& result)
    {
        int unconnectedChunkCount = 0;
        QStringList offendingCaveNames;

        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);

            const Monad::Result<QList<cwFindUnconnectedSurveyChunks::Result>> unconnectedResult =
                cwFindUnconnectedSurveyChunks::find(cave->data());
            if (unconnectedResult.hasError()) {
                continue;
            }
            const QList<cwFindUnconnectedSurveyChunks::Result> errorResults = unconnectedResult.value();
            if (!errorResults.isEmpty()) {
                cwLinePlotTask::LinePlotCaveData* caveData = createLinePlotCaveDataFor(cave->id(), result);
                if (caveData == nullptr) {
                    continue;
                }
                caveData->setUnconnectedChunkError(errorResults);
                unconnectedChunkCount += errorResults.size();
                offendingCaveNames.append(cave->name());
            }
        }

        if (unconnectedChunkCount > 0) {
            // Cavern was never run; surface a SolveError so CavernOutputPage
            // tells the user *why* and which caves need attention, instead of
            // showing "Last solve completed successfully" with an empty log.
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Validation;
            error.message = QStringLiteral(
                "Cannot solve: %1 survey leg(s) are not connected to the cave network (%2). "
                "Open the affected chunks and fix the disconnected station names.")
                .arg(unconnectedChunkCount)
                .arg(offendingCaveNames.join(QStringLiteral(", ")));
            result.setSolveError(error);
            return false;
        }
        return true;
    }

    // Returns a pointer so callers iterating Region.cave(i) can skip caves
    // that have no original-thread counterpart in regionPointers without
    // accidentally seeding result.Caves[nullptr]. Under the documented
    // contract every Region cave maps to an external cave; the null path
    // exists as Release-build defence against a malformed snapshot
    // (Q_ASSERT alone would silently corrupt the result in Release).
    cwLinePlotTask::LinePlotCaveData* createLinePlotCaveDataFor(const QUuid& caveId,
                                                                cwLinePlotTask::LinePlotResultData& result)
    {
        cwCave* externalCave = ExternalCaveByUuid.value(caveId).Cave;
        if (externalCave == nullptr) {
            Q_ASSERT(false);
            return nullptr;
        }

        if (!result.Caves.contains(externalCave)) {
            result.Caves[externalCave] = cwLinePlotTask::LinePlotCaveData();
        }

        return &result.Caves[externalCave];
    }

    void addEmptyStationLookup(const QUuid& caveId, cwLinePlotTask::LinePlotResultData& result)
    {
        cwCave* externalCave = ExternalCaveByUuid.value(caveId).Cave;
        if (externalCave == nullptr) {
            return;
        }
        if (!result.Caves.contains(externalCave)) {
            result.Caves.insert(externalCave, cwLinePlotTask::LinePlotCaveData());
        }
    }

    void indexStations()
    {
        TripLookups.clear();
        TripLookups.reserve(Region.caveCount());

        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);
            TripLookups.insert(cave->id(), cwLinePlotTask::StationTripScrapLookup(cave));
        }
    }

    // Parses cavern-emitted prefixed station names of the form
    //   "cave_<32-hex-uuid>.<station-name>"
    // back into a per-cave position lookup keyed by cwCave::id(). Stations
    // whose prefix UUID is not present in the worker's known cave set are
    // dropped (they would not match any cave in the region; this keeps
    // splitLookupByCave robust against accidental orphan prefixes without
    // poisoning the whole result).
    QHash<QUuid, cwStationPositionLookup> splitLookupByCave(
        const cwStationPositionLookup& stationPostions) const
    {
        // Round positions to millimetre precision to absorb cavern's
        // double-to-text rounding when comparing against the previous run.
        constexpr int kPositionPrecisionDigits = 3;
        const double positionFactor = std::pow(10.0, kPositionPrecisionDigits);

        // Compiled once per splitLookupByCave call. cavernStationRegex()
        // documents the contract (see cwLinePlotTask.h) and is what the
        // [LinePlot][UuidPrefix] tests bind against.
        const QRegularExpression regex = cwLinePlotTask::cavernStationRegex();

        QHash<QUuid, cwStationPositionLookup> caveStations;
        caveStations.reserve(InternalCaveByUuid.size());

        const QMap<QString, QVector3D> positions = stationPostions.positions();
        for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
            const QString& name = iter.key();
            QVector3D position = iter.value();

            // std::round keeps the intermediate value in double; qRound returns
            // int and overflows for UTM-scale coordinates (a 5.47e6m northing
            // multiplied by 1000 already exceeds INT_MAX, and the user-visible
            // crash on projects with no worldOrigin / large fixes traced here).
            position.setX(float(std::round(double(position.x()) * positionFactor) / positionFactor));
            position.setY(float(std::round(double(position.y()) * positionFactor) / positionFactor));
            position.setZ(float(std::round(double(position.z()) * positionFactor) / positionFactor));

            const QRegularExpressionMatch match = regex.match(name);
            if (!match.hasMatch()) {
                qDebug() << "Couldn't match cavern station name:" << name << "This is a bug!" << LOCATION;
                continue;
            }

            // QUuid::fromString requires hyphens; reinsertUuidHyphens turns
            // the 32-hex capture back into the RFC-4122 dashed layout that
            // QUuid::fromString accepts. The regex already restricted the
            // capture to 32 hex chars so the parse never returns null for a
            // matched name.
            const QUuid caveId = QUuid::fromString(reinsertUuidHyphens(match.captured(1)));
            if (caveId.isNull()) {
                qDebug() << "Failed to parse cave UUID from cavern prefix:" << match.captured(1) << LOCATION;
                return {};
            }
            if (!InternalCaveByUuid.contains(caveId)) {
                qDebug() << "Cavern emitted station with unknown cave UUID:" << caveId << LOCATION;
                continue;
            }

            cwStationPositionLookup& lookup = caveStations[caveId];
            lookup.setPosition(match.captured(2), position);
        }

        return caveStations;
    }

    void setStationAsChanged(const QUuid& caveId, const QString& stationName,
                             cwLinePlotTask::LinePlotResultData& result)
    {
        addEmptyStationLookup(caveId, result);

        const cwLinePlotTask::StationTripScrapLookup lookup = TripLookups.value(caveId);
        const QList<int> tripIndexes = lookup.trips(stationName.toUpper());
        const QList<QPair<int, int>> scrapIndexes = lookup.scraps(stationName.toUpper());

        const cwLinePlotTask::CaveDataPtrs cavePtrs = ExternalCaveByUuid.value(caveId);
        if (cavePtrs.Cave == nullptr) {
            // No matching original-side cave (defensive: ExternalCaveByUuid
            // is built from the same snapshot as the worker's Region, so this
            // path is only reachable if regionPointers is out of sync).
            return;
        }

        for (int tripIndex : tripIndexes) {
            cwTrip* externalTrip = cavePtrs.Trips.at(tripIndex).Trip;
            result.Trips.insert(externalTrip);
        }

        for (const QPair<int, int>& index : scrapIndexes) {
            const int tripIndex = index.first;
            const int scrapIndex = index.second;

            const cwLinePlotTask::TripDataPtrs& tripPtrs = cavePtrs.Trips.at(tripIndex);
            result.Scraps.insert(tripPtrs.Scraps.at(scrapIndex));
            result.Trips.insert(tripPtrs.Trip);
        }
    }

    void updateInteralCaveStationLookups(const QHash<QUuid, cwStationPositionLookup>& caveStations,
                                         cwLinePlotTask::LinePlotResultData& result)
    {
        // Iterate Region by index so caves with no positions in `caveStations`
        // (e.g. a cave whose entire centerline failed to solve) still get
        // their stale lookup cleared and the result populated.
        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);
            const QUuid caveId = cave->id();

            const cwStationPositionLookup newLookup = caveStations.value(caveId);
            const cwStationPositionLookup oldLookup = CaveStationLookups.value(caveId);

            if (newLookup.positions().size() != oldLookup.positions().size()) {
                addEmptyStationLookup(caveId, result);
            }

            const QMap<QString, QVector3D> newPositions = newLookup.positions();
            const QMap<QString, QVector3D> oldPositions = oldLookup.positions();

            for (auto it = newPositions.constBegin(); it != newPositions.constEnd(); ++it) {
                const QString& stationName = it.key();
                const QVector3D newPoint = it.value();
                if (oldPositions.contains(stationName)) {
                    if (oldPositions.value(stationName) != newPoint) {
                        setStationAsChanged(caveId, stationName, result);
                    }
                } else {
                    setStationAsChanged(caveId, stationName, result);
                }
            }

            CaveStationLookups[caveId] = newLookup;
        }
    }

    void updateExteralCaveStationLookups(cwLinePlotTask::LinePlotResultData& result)
    {
        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* internalCave = Region.cave(i);
            const QUuid caveId = internalCave->id();
            cwCave* externalCave = ExternalCaveByUuid.value(caveId).Cave;
            if (externalCave == nullptr || !result.Caves.contains(externalCave)) {
                continue;
            }

            const cwStationPositionLookup updatedLookup = CaveStationLookups.value(caveId);
            cwLinePlotTask::LinePlotCaveData& caveData = result.Caves[externalCave];
            caveData.setStationPositions(updatedLookup);
            internalCave->setStationPositionLookup(updatedLookup);
        }
    }

    // Translate every station in lookup by -worldOrigin in place. Cavern
    // emits .3d coordinates in our globalCS; subtracting worldOrigin keeps
    // the position lookup (and downstream geometry) close to (0,0,0) for
    // float precision in shaders. No-op when worldOrigin == (0,0,0), which
    // is the un-fixed-project default.
    static void applyWorldOriginOffset(cwStationPositionLookup& lookup,
                                       const cwGeoPoint& worldOrigin)
    {
        const QVector3D offset = worldOrigin.toVector3D();
        if (offset.isNull()) {
            return;
        }
        const QMap<QString, QVector3D> positions = lookup.positions();
        lookup.clearStations();
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            lookup.setPosition(it.key(), it.value() - offset);
        }
    }

    void updateStationPositionForCaves(const cwStationPositionLookup& stationPostions,
                                       cwLinePlotTask::LinePlotResultData& result)
    {
        indexStations();

        const QHash<QUuid, cwStationPositionLookup> caveStationLookups = splitLookupByCave(stationPostions);

        updateInteralCaveStationLookups(caveStationLookups, result);
        updateExteralCaveStationLookups(result);
    }

    void updateDepthLength(const QVector<cwLinePlotGeometry::CaveLengthAndDepth>& lengths,
                           cwLinePlotTask::LinePlotResultData& result)
    {
        Q_ASSERT(Region.caveCount() == lengths.size());

        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);
            cwLinePlotTask::LinePlotCaveData* caveData = createLinePlotCaveDataFor(cave->id(), result);
            if (caveData == nullptr) {
                continue;
            }
            caveData->setLength(lengths.at(i).length());
            caveData->setDepth(lengths.at(i).depth());
        }
    }

    void updateCaveNetworks(cwLinePlotTask::LinePlotResultData& result)
    {
        auto createNetwork = [](cwCave* cave) {
            cwSurveyNetwork network;

            for (cwTrip* trip : cave->trips()) {
                for (cwSurveyChunk* chunk : trip->chunks()) {
                    const QList<cwStation> stations = chunk->stations();
                    for (int i = 0; i < stations.size() - 1; i++) {
                        network.addShot(stations.at(i).name(), stations.at(i + 1).name());
                    }
                }
            }

            return network;
        };

        const QList<cwCave*> caves = Region.caves();
        for (cwCave* cave : caves) {
            const cwSurveyNetwork network = createNetwork(cave);
            if (network == cave->network()) {
                continue;
            }
            const QUuid caveId = cave->id();
            cwCave* externalCave = ExternalCaveByUuid.value(caveId).Cave;
            if (externalCave == nullptr) {
                continue;
            }
            result.Caves[externalCave].setNetwork(network);

            const auto changedStations = cwSurveyNetwork::changedStations(cave->network(), network);
            for (const auto& station : changedStations) {
                setStationAsChanged(caveId, station, result);
            }
        }
    }
};

cwLinePlotTask::Input cwLinePlotTask::buildInput(const cwCavingRegion *region)
{
    Input input;
    if(region != nullptr) {
        input.regionData = region->data();
        input.regionPointers = RegionDataPtrs(region);
    }
    return input;
}

cwLinePlotTask::Input cwLinePlotTask::buildInput(const cwCavingRegion* region,
                                                 const QHash<QUuid, QString>& caveAttachmentDirs,
                                                 const QHash<QUuid, QString>& tripAttachmentDirs)
{
    Input input = buildInput(region);
    input.caveAttachmentDirs = caveAttachmentDirs;
    input.tripAttachmentDirs = tripAttachmentDirs;
    return input;
}

QFuture<cwLinePlotTask::LinePlotResultData> cwLinePlotTask::run(cwLinePlotTask::Input input)
{
    return cwConcurrent::run([input = std::move(input)]() mutable {
        cwLinePlotTask::LinePlotWorker worker(std::move(input));
        return worker.run();
    });
}
