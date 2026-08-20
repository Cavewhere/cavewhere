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
#include "cwStation.h"
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
#include <QPromise>
#include <QtGlobal>
#include <cmath>
#include <functional>

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

cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup(cwCave *cave)
{
    for(int tripIndex = 0; tripIndex < cave->tripCount(); tripIndex++) {
        cwTrip* trip = cave->trip(tripIndex);
        const QUuid tripId = trip->id();
        foreach(cwSurveyChunk* surveyChunk, trip->chunks()) {
            foreach(cwStation station, surveyChunk->stations()) {
                MapStationToTrip.insert(station.name().toUpper(), tripId);
            }
        }

        foreach(cwNote* note, trip->notes()->notes()) {
            for(int i = 0; i < note->scraps().size(); i++) {
                cwScrap* scrap = note->scrap(i);
                const QUuid scrapId = scrap->id();

                foreach(cwNoteStation noteStation, scrap->stations()) {
                    MapStationToScrap.insert(noteStation.name().toUpper(),
                                             std::make_pair(tripId, scrapId));
                }
            }
        }
    }
}

struct cwLinePlotTask::LinePlotWorker {
    // IsCanceled is polled between the solve's phases so a canceled run stops at
    // the next boundary instead of finishing a solve nobody will read.
    LinePlotWorker(cwLinePlotTask::Input input, std::function<bool ()> isCanceled)
        : InputData(std::move(input)),
          IsCanceled(std::move(isCanceled))
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

        if (IsCanceled()) {
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

        if (IsCanceled()) {
            return result;
        }

        if (!runCavern(svxPath, output3dPath, result)) {
            return result;
        }

        if (IsCanceled()) {
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
        updateStationPositionForCaves(parsed.lookup, result);
        const QHash<QUuid, cwSplayTipsByStation> caveSplayTips =
            splitSplayTipsByCave(parsed.splayTips);
        updateSplayTipsForCaves(caveSplayTips, result);
        result.setRegionNetwork(std::move(parsed.network));

        if (IsCanceled()) {
            return result;
        }

        cwLinePlotGeometry::Result geometry = generateGeometry(caveSplayTips);
        result.setPositions(geometry.points);
        result.setTripVertexRanges(geometry.tripVertexRanges);
        result.setTripSplayVertexRanges(geometry.tripSplayVertexRanges);
        result.setTripUuids(geometry.tripUuids);

        updateDepthLength(geometry.cavesLengthAndDepths, result);
        updateCaveNetworks(result);

        return result;
    }

private:
    cwLinePlotTask::Input InputData;
    std::function<bool ()> IsCanceled;
    cwCavingRegion Region;
    // All cave-keyed bookkeeping uses cwCave::id() rather than an integer
    // position: the driver emits "cave_<uuid>" prefixes, so integer cave
    // indexes have no representation in the cavern output; UUIDs do. The
    // result likewise identifies changed caves/trips/scraps by id(), so the
    // worker never holds a pointer into the main-thread-owned objects.
    QHash<QUuid, cwStationPositionLookup> CaveStationLookups;
    QHash<QUuid, cwLinePlotTask::StationTripScrapLookup> TripLookups;
    // cavernStationRegex() documents the "cave_<uuid>.<station>" contract (see
    // cwLinePlotTask.h) and is what the [LinePlot][UuidPrefix] tests bind
    // against. Compiled once per solve rather than per split.
    const QRegularExpression CavernRegex = cwLinePlotTask::cavernStationRegex();
    // Index from cave UUID to the worker-internal cwCave* owned by Region.
    // Built once in initializeCaveStationLookups() so the rest of the worker
    // can stay UUID-keyed.
    QHash<QUuid, cwCave*> InternalCaveByUuid;

    void encodeCaveNames(cwCavingRegionData& regionData)
    {
        // Cave names are rewritten to cavernCaveNameFor(cave.id) so the
        // exporter emits "*begin cave_<uuid>" and splitLookupByCave can
        // recover cwCave::id() from the cavern station prefix. Caller
        // contract: cave.id must be non-null - the manager satisfies this via
        // cwCavingRegion::data(); synthetic callers (cwTripLinePlotTask)
        // generate a UUID before building Input.
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
            // Seed from the caller's snapshot, not from `cave`: Region is
            // rebuilt from cwCaveData, and cwCave::setData doesn't restore
            // station positions, so the internal cave's lookup is always empty.
            CaveStationLookups.insert(id, InputData.previousStationPositions.value(id));
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
        // Cavern's positions come straight back into the scene, so *cs out has
        // to name the frame the scene is in, not one a reader would want.
        exportOptions.outputCSPolicy =
            cwSurvexExporterRegion::OutputCSPolicy::WorkingFrame;

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

    cwLinePlotGeometry::Result generateGeometry(
        const QHash<QUuid, cwSplayTipsByStation>& caveSplayTips)
    {
        const Monad::Result<cwLinePlotGeometry::Result> result =
            cwLinePlotGeometry::generate(Region.data(), caveSplayTips);
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
                cwLinePlotTask::LinePlotCaveData& caveData = createLinePlotCaveDataFor(cave->id(), result);
                caveData.setUnconnectedChunkError(errorResults);
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

    // Returns the result entry for caveId, creating an empty one on first
    // access. caveId always comes from Region.cave(i)->id() or from a cavern
    // prefix already checked against InternalCaveByUuid, so it is always valid.
    cwLinePlotTask::LinePlotCaveData& createLinePlotCaveDataFor(const QUuid& caveId,
                                                               cwLinePlotTask::LinePlotResultData& result)
    {
        return result.Caves[caveId];
    }

    void addEmptyStationLookup(const QUuid& caveId, cwLinePlotTask::LinePlotResultData& result)
    {
        if (!result.Caves.contains(caveId)) {
            result.Caves.insert(caveId, cwLinePlotTask::LinePlotCaveData());
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

    struct CavernName {
        QUuid caveId;
        QString stationName;
    };

    // Splits a cavern-emitted name of the form "cave_<32-hex-uuid>.<station>"
    // into the cave it belongs to and the station name inside that cave, or
    // nothing when the name doesn't name a cave this worker knows about.
    // Shared by the position and splay-tip splits so both read the prefix the
    // same way.
    std::optional<CavernName> parseCavernStationName(const QString& name) const
    {
        const QRegularExpressionMatch match = CavernRegex.match(name);
        if (!match.hasMatch()) {
            qDebug() << "Couldn't match cavern station name:" << name << "This is a bug!" << LOCATION;
            return {};
        }

        // QUuid::fromString requires hyphens; reinsertUuidHyphens turns the
        // 32-hex capture back into the RFC-4122 dashed layout that
        // QUuid::fromString accepts. The regex already restricted the capture
        // to 32 hex chars so the parse never returns null for a matched name.
        const QUuid caveId = QUuid::fromString(reinsertUuidHyphens(match.captured(1)));
        if (caveId.isNull()) {
            qDebug() << "Failed to parse cave UUID from cavern prefix:" << match.captured(1) << LOCATION;
            return {};
        }
        if (!InternalCaveByUuid.contains(caveId)) {
            qDebug() << "Cavern emitted station with unknown cave UUID:" << caveId << LOCATION;
            return {};
        }

        return CavernName{caveId, match.captured(2)};
    }

    // Splits cavern's solved positions into a per-cave lookup keyed by
    // cwCave::id(). Stations whose prefix UUID is absent from the worker's
    // known cave set are dropped, which keeps an accidental orphan prefix from
    // poisoning the whole result.
    QHash<QUuid, cwStationPositionLookup> splitLookupByCave(
        const cwStationPositionLookup& stationPostions) const
    {
        // Round positions to millimeter precision to absorb cavern's
        // double-to-text rounding when comparing against the previous run.
        constexpr int kPositionPrecisionDigits = 3;
        const double positionFactor = std::pow(10.0, kPositionPrecisionDigits);

        QHash<QUuid, cwStationPositionLookup> caveStations;
        caveStations.reserve(InternalCaveByUuid.size());

        const QMap<QString, QVector3D> positions = stationPostions.positions();
        for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
            QVector3D position = iter.value();

            // std::round keeps the intermediate value in double; qRound returns
            // int and overflows for UTM-scale coordinates (a 5.47e6m northing
            // multiplied by 1000 already exceeds INT_MAX, and a user-visible
            // crash on projects solving in absolute coordinates traced here).
            position.setX(float(std::round(double(position.x()) * positionFactor) / positionFactor));
            position.setY(float(std::round(double(position.y()) * positionFactor) / positionFactor));
            position.setZ(float(std::round(double(position.z()) * positionFactor) / positionFactor));

            const auto parsed = parseCavernStationName(iter.key());
            if (!parsed.has_value()) {
                continue;
            }

            cwStationPositionLookup& lookup = caveStations[parsed->caveId];
            lookup.setPosition(parsed->stationName, position);
        }

        return caveStations;
    }

    // Same prefix split as splitLookupByCave, for the splay tips. Positions are
    // kept as cavern gave them: a tip only ever feeds geometry, so it is never
    // compared against a previous solve the way station positions are.
    QHash<QUuid, cwSplayTipsByStation> splitSplayTipsByCave(
        const cwSplayTipsByStation& splayTips) const
    {
        QHash<QUuid, cwSplayTipsByStation> caveSplayTips;
        for (auto iter = splayTips.constBegin(); iter != splayTips.constEnd(); ++iter) {
            const auto parsed = parseCavernStationName(iter.key());
            if (!parsed.has_value()) {
                continue;
            }

            caveSplayTips[parsed->caveId].insert(cwStation::canonicalKey(parsed->stationName),
                                                 iter.value());
        }

        return caveSplayTips;
    }

    // Splays move exactly when the station they hang off does, so they ride out
    // on the caves the solve already had something to say about. A cave with no
    // tips keeps the empty hash it was built with.
    void updateSplayTipsForCaves(const QHash<QUuid, cwSplayTipsByStation>& caveSplayTips,
                                 cwLinePlotTask::LinePlotResultData& result)
    {
        for (auto iter = caveSplayTips.constBegin(); iter != caveSplayTips.constEnd(); ++iter) {
            const auto it = result.Caves.find(iter.key());
            if (it != result.Caves.end()) {
                it.value().setSplayTips(iter.value());
            }
        }
    }

    void setStationAsChanged(const QUuid& caveId, const QString& stationName,
                             cwLinePlotTask::LinePlotResultData& result)
    {
        addEmptyStationLookup(caveId, result);

        const cwLinePlotTask::StationTripScrapLookup lookup = TripLookups.value(caveId);
        const QString upperName = stationName.toUpper();

        for (const QUuid& tripId : lookup.trips(upperName)) {
            result.Trips.insert(tripId);
        }

        // A changed scrap also marks its parent trip as changed.
        for (const auto& [tripId, scrapId] : lookup.scraps(upperName)) {
            result.Scraps.insert(scrapId);
            result.Trips.insert(tripId);
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

    // Deliberately takes no result parameter, so it cannot be gated on
    // something having changed: generateGeometry() rebuilds the whole plot from
    // Region.data(), and cwCave::data() only carries positions that were set on
    // the internal cave. A cave whose stations didn't move this solve still
    // needs them written here or it drops out of the plot entirely, losing its
    // geometry, length and depth along with it.
    void refreshInternalStationLookups()
    {
        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* internalCave = Region.cave(i);
            internalCave->setStationPositionLookup(CaveStationLookups.value(internalCave->id()));
        }
    }

    // Publishing is what tells cwLinePlotManager to write back to the live
    // cave, so only the caves something actually changed on are published.
    void publishChangedStationLookups(cwLinePlotTask::LinePlotResultData& result)
    {
        for (int i = 0; i < Region.caveCount(); i++) {
            const QUuid caveId = Region.cave(i)->id();
            const auto it = result.Caves.find(caveId);
            if (it != result.Caves.end()) {
                it.value().setStationPositions(CaveStationLookups.value(caveId));
            }
        }
    }

    // Cavern emits .3d coordinates in whatever *cs out named, which for this
    // export is the project's local projection — already centered on the
    // project, already small enough for float in the shaders. Nothing is
    // subtracted on the way in; there is no second frame to reconcile with.
    void updateStationPositionForCaves(const cwStationPositionLookup& stationPostions,
                                       cwLinePlotTask::LinePlotResultData& result)
    {
        indexStations();

        const QHash<QUuid, cwStationPositionLookup> caveStationLookups = splitLookupByCave(stationPostions);

        updateInteralCaveStationLookups(caveStationLookups, result);
        refreshInternalStationLookups();
        publishChangedStationLookups(result);
    }

    void updateDepthLength(const QVector<cwLinePlotGeometry::CaveLengthAndDepth>& lengths,
                           cwLinePlotTask::LinePlotResultData& result)
    {
        Q_ASSERT(Region.caveCount() == lengths.size());

        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);
            cwLinePlotTask::LinePlotCaveData& caveData = createLinePlotCaveDataFor(cave->id(), result);
            caveData.setLength(lengths.at(i).length());
            caveData.setDepth(lengths.at(i).depth());
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
            const QUuid caveId = cave->id();
            // As with the station lookups, the internal cave carries no network
            // of its own — cwCaveData has no such field — so the previous one
            // has to come from the caller's snapshot.
            const cwSurveyNetwork previousNetwork = InputData.previousNetworks.value(caveId);
            if (network == previousNetwork) {
                continue;
            }
            result.Caves[caveId].setNetwork(network);

            const auto changedStations = cwSurveyNetwork::changedStations(previousNetwork, network);
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

        // Carry the last applied solve across as the change-detection baseline.
        // The live caves hold it because cwLinePlotManager writes each result
        // back to them, so a restarted solve still diffs against what the user
        // last saw rather than against a half-finished run.
        const QList<cwCave*> caves = region->caves();
        input.previousStationPositions.reserve(caves.size());
        input.previousNetworks.reserve(caves.size());
        for(const cwCave* cave : caves) {
            input.previousStationPositions.insert(cave->id(), cave->stationPositionLookup());
            input.previousNetworks.insert(cave->id(), cave->network());
        }
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
    // The QPromise form is what makes cancel() reach the worker: the solve polls
    // promise.isCanceled() between its phases, so a restart or a manager being
    // torn down stops the run at the next boundary rather than paying for cavern
    // and the geometry pass. A canceled run publishes no result, which every
    // caller already handles by checking resultCount().
    return cwConcurrent::run([input = std::move(input)]
                             (QPromise<cwLinePlotTask::LinePlotResultData>& promise) mutable {
        cwLinePlotTask::LinePlotWorker worker(std::move(input),
                                              [&promise]() { return promise.isCanceled(); });
        auto result = worker.run();
        if (promise.isCanceled()) {
            return;
        }
        promise.addResult(std::move(result));
    });
}
