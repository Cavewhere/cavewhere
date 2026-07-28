/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwLinePlotTask.h"
#include "cwCavernNaming.h"
#include "cwScopeLabels.h"
#include "cwConcurrent.h"
#include "cwSurvexExporterRegion.h"
#include "cwCavernRunner.h"
#include "cwSurvex3DFileReader.h"
#include "cwLinePlotGeometry.h"
#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwScrap.h"
#include "cwSurveyChunk.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwErrorModel.h"

// Qt includes
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QtGlobal>
#include <cmath>

cwLinePlotTask::LinePlotCaveData::LinePlotCaveData() :
    DepthLengthChanged(false),
    Depth(0.0),
    Length(0.0),
    StationPostionsChanged(false),
    NetworkChanged(false)
{
}
cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup(cwCave *cave)
{
    // Keys are matched against the changed-station names reported by
    // setStationAsChanged, which are the cave-local lookup keys. For an
    // externally-attached trip those retain the trip scope
    // ("<tripLabel>.<tail>") while chunk / note / scrap stations carry only the
    // tail, so every key inserted here is scoped to match (a no-op for a native
    // trip). Resolved once per trip rather than per station: cwTrip::scopePrefix
    // has to look at the trip's siblings to know its label.
    for(int tripIndex = 0; tripIndex < cave->tripCount(); tripIndex++) {
        cwTrip* trip = cave->trip(tripIndex);
        const QUuid tripId = trip->id();
        const bool external = !trip->externalCenterline().isEmpty();
        //Native-prefixed (Scope) trips are deliberately left unscoped here:
        //their stations come from chunks, which the exporter emits unprefixed.
        const QString tripScope = external ? trip->scopePrefix() : QString();

        foreach(cwSurveyChunk* surveyChunk, trip->chunks()) {
            foreach(cwStation station, surveyChunk->stations()) {
                MapStationToTrip.insert((tripScope + station.name()).toUpper(), tripId);
            }
        }

        foreach(cwNote* note, trip->notes()->notes()) {
            for(int i = 0; i < note->scraps().size(); i++) {
                cwScrap* scrap = note->scrap(i);
                const QUuid scrapId = scrap->id();

                foreach(cwNoteStation noteStation, scrap->stations()) {
                    MapStationToScrap.insert((tripScope + noteStation.name()).toUpper(),
                                             std::make_pair(tripId, scrapId));
                }
            }
        }

        // An external trip owns no chunk, so nothing above mapped it into
        // MapStationToTrip. Its LiDAR-carpet notes still need the trip flagged
        // when their tie-in stations move (a scrap already propagates to its
        // parent trip in setStationAsChanged, so scraps need no extra mapping).
        if(external) {
            foreach(QObject* obj, trip->notesLiDAR()->notes()) {
                auto* lidarNote = qobject_cast<cwNoteLiDAR*>(obj);
                if(lidarNote == nullptr) {
                    continue;
                }
                foreach(const cwNoteLiDARStation& noteStation, lidarNote->stations()) {
                    MapStationToTrip.insert((tripScope + noteStation.name()).toUpper(), tripId);
                }
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
        if (InputData.regionData.caves.isEmpty()) {
            //Nothing to solve, so nothing can be floating — a real empty
            //answer rather than the absence of one.
            return cwLinePlotTask::LinePlotResultData::cleared();
        }

        // Every return below is a path that never reaches the post-solve
        // floating-survey pass, and so is any added later: the result asserts
        // nothing about external scopes until that pass actually runs.
        cwLinePlotTask::LinePlotResultData result;

        // Prepare working copy of region data
        Region.setData(InputData.regionData);

        initializeCaveLabels();
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
        result.setRegionNetwork(parsed.network);

        // The other half of the floating-survey answer. An attached centerline
        // owns no chunk, so checkForErrors above never saw it; only a completed
        // run knows which scopes cavern placed and which it dropped.
        result.FloatingSurveys.append(
            cwFindFloatingSurveys::fromExternalScopes(InputData.regionData,
                                                      parsed.network,
                                                      ScopeLabels));
        result.ExternalScopesChecked = true;

        // The network carries the shot topology for externally-attached scopes,
        // which have no cwSurveyChunk of their own. cwSurveyNetwork is
        // implicitly shared, so this is a refcount bump, not a deep copy.
        cwLinePlotGeometry::Result geometry = generateGeometry(parsed.network);
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
    // All cave-keyed bookkeeping uses cwCave::id() rather than an integer
    // position: the driver scopes every station under its cave's label, so
    // indexes have no representation in the cavern output; UUIDs do. The
    // result likewise identifies changed caves/trips/scraps by id(), so the
    // worker never holds a pointer into the main-thread-owned objects.
    QHash<QUuid, cwStationPositionLookup> CaveStationLookups;
    QHash<QUuid, cwLinePlotTask::StationTripScrapLookup> TripLookups;
    // Index from cave UUID to the worker-internal cwCave* owned by Region.
    // Built once in initializeCaveStationLookups() so the rest of the worker
    // can stay UUID-keyed.
    QHash<QUuid, cwCave*> InternalCaveByUuid;
    // The survey label each cave's and trip's "*begin" carries. The exporter
    // assigns the same labels from the same ordered snapshot, so this is not a
    // map handed across a boundary — it is the same pure function evaluated on
    // both sides, which is what lets the decode below recover cwCave::id() from
    // a name cavern echoed back.
    cwScopeLabels ScopeLabels;

    void initializeCaveLabels()
    {
        // Caller contract: cave.id must be non-null - the manager satisfies
        // this via cwCavingRegion::data(); synthetic callers
        // (cwTripLinePlotTask) generate a UUID before building Input.
        for (const cwCaveData& cave : std::as_const(InputData.regionData.caves)) {
            Q_ASSERT(!cave.id.isNull());
        }

        ScopeLabels = cwScopeLabels(InputData.regionData);
    }

    // The labels of this cave's externally-attached trips, which are the only
    // scopes the exporter opens inside a cave block. A native-prefixed (Scope)
    // trip is deliberately absent: its stations come from chunks, which the
    // exporter emits unscoped.
    QSet<QString> externalTripLabelsFor(cwCave* cave) const
    {
        const QHash<QUuid, QString>& labels = ScopeLabels.tripLabels(cave->id());

        QSet<QString> externalLabels;
        for (const cwTrip* trip : cave->trips()) {
            if (!trip->externalCenterline().isEmpty()) {
                externalLabels.insert(labels.value(trip->id()));
            }
        }
        return externalLabels;
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
    }

    bool exportSurvex(const QString& svxPath, cwLinePlotTask::LinePlotResultData& result)
    {
        // exportRegion assigns the cave labels itself, from the same ordered
        // snapshot initializeCaveLabels() reads, which is what lets the decode
        // below recover a cave from a name cavern echoed back. The
        // attachment-dir maps come straight from the Input the caller built.
        cwSurvexExporterRegion::Options exportOptions;
        exportOptions.caveAttachmentDirs = InputData.caveAttachmentDirs;
        exportOptions.tripAttachmentDirs = InputData.tripAttachmentDirs;
        exportOptions.tripInjectedDeclinations = InputData.tripInjectedDeclinations;

        const Monad::ResultBase r =
            cwSurvexExporterRegion::exportRegion(InputData.regionData, svxPath, exportOptions);
        if (r.hasError()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Export;
            error.message = r.errorMessage();
            result.setSolveError(error);
            return false;
        }

        // Capture the driver text before cavern runs so the manager can
        // surface it even when the solve fails downstream.
        QFile driverFile(svxPath);
        if (driverFile.open(QFile::ReadOnly)) {
            result.DriverSource = QString::fromUtf8(driverFile.readAll());
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
        result.CavernWarningCount = cavern.warningCount;

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

    cwLinePlotGeometry::Result generateGeometry(const cwSurveyNetwork& network)
    {
        const Monad::Result<cwLinePlotGeometry::Result> result =
            cwLinePlotGeometry::generate(Region.data(), network);
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

            const cwCaveData caveSnapshot = cave->data();
            const Monad::Result<QList<cwFindUnconnectedSurveyChunks::Result>> unconnectedResult =
                cwFindUnconnectedSurveyChunks::find(caveSnapshot);
            if (unconnectedResult.hasError()) {
                continue;
            }
            const QList<cwFindUnconnectedSurveyChunks::Result> errorResults = unconnectedResult.value();
            if (!errorResults.isEmpty()) {
                cwLinePlotTask::LinePlotCaveData& caveData = createLinePlotCaveDataFor(cave->id(), result);
                caveData.setUnconnectedChunkError(errorResults);
                unconnectedChunkCount += errorResults.size();
                offendingCaveNames.append(cave->name());
                result.FloatingSurveys.append(
                    cwFindFloatingSurveys::fromUnconnectedChunks(caveSnapshot, errorResults));
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

    // Parses cavern-emitted scoped station names of the form
    //   "<caveLabel>.<station-name>"
    // back into a per-cave position lookup keyed by cwCave::id(). Stations
    // whose leading scope is not one of this region's cave labels are dropped
    // (they would not match any cave in the region; this keeps
    // splitLookupByCave robust against accidental orphan prefixes without
    // poisoning the whole result).
    QHash<QUuid, cwStationPositionLookup> splitLookupByCave(
        const cwStationPositionLookup& stationPostions) const
    {
        // Round positions to millimetre precision to absorb cavern's
        // double-to-text rounding when comparing against the previous run.
        constexpr int kPositionPrecisionDigits = 3;
        const double positionFactor = std::pow(10.0, kPositionPrecisionDigits);

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

            // Cave labels are lowercase by construction, but cavern echoes back
            // whatever case the included file used for a nested scope, so match
            // the leading scope case-insensitively.
            const QString caveLabel = cwCavernNaming::scopeHeadOf(name).toLower();
            if (caveLabel.isEmpty()) {
                qDebug() << "Cavern station name carries no cave scope:" << name
                         << "This is a bug!" << LOCATION;
                continue;
            }

            const QUuid caveId = ScopeLabels.caveId(caveLabel);
            if (caveId.isNull() || !InternalCaveByUuid.contains(caveId)) {
                qDebug() << "Cavern emitted station with unknown cave scope:" << caveLabel << LOCATION;
                continue;
            }

            //Walls' empty-name quirk can put a bare "<caveLabel>." (or one with
            //only spaces after the separator) in the .3d, and neither
            //setPosition nor cwStation::canonicalKey trims, so an unguarded tail
            //would pollute the lookup with a blank key that no chunk station can
            //ever match.
            const QString tail = cwCavernNaming::removeScopeHead(name);
            if (tail.trimmed().isEmpty()) {
                qDebug() << "Cavern station name has no station under its cave scope:"
                         << name << LOCATION;
                continue;
            }

            cwStationPositionLookup& lookup = caveStations[caveId];
            lookup.setPosition(tail, position);
        }

        return caveStations;
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

    void updateExteralCaveStationLookups(cwLinePlotTask::LinePlotResultData& result)
    {
        for (int i = 0; i < Region.caveCount(); i++) {
            cwCave* internalCave = Region.cave(i);
            const QUuid caveId = internalCave->id();
            if (!result.Caves.contains(caveId)) {
                continue;
            }

            const cwStationPositionLookup updatedLookup = CaveStationLookups.value(caveId);
            cwLinePlotTask::LinePlotCaveData& caveData = result.Caves[caveId];
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
            cwLinePlotTask::LinePlotCaveData& caveData = createLinePlotCaveDataFor(cave->id(), result);
            caveData.setLength(lengths.at(i).length());
            caveData.setDepth(lengths.at(i).depth());
        }
    }

    void updateCaveNetworks(cwLinePlotTask::LinePlotResultData& result)
    {
        // The solved region network carries every scope's topology, including
        // externally-attached trips that own no cwSurveyChunk. Keyed
        // "<caveLabel>.<tail>" (native) and "<caveLabel>.<tripLabel>.<tail>" (external).
        const cwSurveyNetwork regionNetwork = result.regionNetwork();

        auto createNetwork = [&regionNetwork, this](cwCave* cave) {
            cwSurveyNetwork network;

            for (cwTrip* trip : cave->trips()) {
                for (cwSurveyChunk* chunk : trip->chunks()) {
                    const QList<cwStation> stations = chunk->stations();
                    for (int i = 0; i < stations.size() - 1; i++) {
                        network.addShot(stations.at(i).name(), stations.at(i + 1).name());
                    }
                }
            }

            // An externally-attached trip owns no chunk, so the loop above adds
            // none of its adjacency. Its solved topology exists only in the
            // region network; copy each edge that touches a trip scope into the
            // cave network under the cave-local scope ("<tripLabel>.<tail>", the
            // same keying splitLookupByCave gives the position lookup) so the
            // note-editing sites can resolve external neighbors. Native-to-native
            // edges are left to the chunk loop above.
            //
            // Which names are external is a membership question, not a spelling
            // one: a trip label is an ordinary survey name, so nothing about
            // "topo1.a1" marks it as scoped except that this cave has a trip
            // labeled topo1.
            const QString cavePrefix = ScopeLabels.cavePrefix(cave->id());
            const QSet<QString> externalTripLabels = externalTripLabelsFor(cave);

            for (const QString& scopedStation : regionNetwork.stations()) {
                if (!scopedStation.startsWith(cavePrefix)) {
                    continue;
                }
                const QString caveLocalStation = scopedStation.mid(cavePrefix.size());
                if (!externalTripLabels.contains(cwCavernNaming::scopeHeadOf(caveLocalStation))) {
                    continue; //native station, already covered by the chunk loop
                }
                for (const QString& scopedNeighbor : regionNetwork.neighbors(scopedStation)) {
                    const QString caveLocalNeighbor = scopedNeighbor.startsWith(cavePrefix)
                            ? scopedNeighbor.mid(cavePrefix.size())
                            : scopedNeighbor;
                    network.addShot(caveLocalStation, caveLocalNeighbor);
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
            result.Caves[caveId].setNetwork(network);

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
    }
    return input;
}

cwLinePlotTask::Input cwLinePlotTask::buildInput(const cwCavingRegion* region,
                                                 const ExternalCenterlineInputs& external)
{
    Input input = buildInput(region);
    input.caveAttachmentDirs = external.caveAttachmentDirs;
    input.tripAttachmentDirs = external.tripAttachmentDirs;
    const QHash<QUuid, bool>& fileOwnsDeclination = external.fileOwnsDeclination;

    if (region != nullptr) {
        // Resolve the injected declination here, on the main thread: the
        // resolved value (IGRF auto or manual fallback) lives on the live
        // cwTripCalibration and isn't part of the worker snapshot. Owners
        // missing from fileOwnsDeclination stay uninjected — same outcome
        // as a file that owns its declination.
        for (cwCave* cave : region->caves()) {
            for (cwTrip* trip : cave->trips()) {
                if (trip->externalCenterline().isEmpty()) {
                    continue;
                }
                if (fileOwnsDeclination.value(trip->id(), true)) {
                    continue;
                }
                input.tripInjectedDeclinations.insert(trip->id(),
                                                      trip->calibrations()->declination());
            }
        }
    }
    return input;
}

QFuture<cwLinePlotTask::LinePlotResultData> cwLinePlotTask::run(cwLinePlotTask::Input input)
{
    return cwConcurrent::run([input = std::move(input)]() mutable {
        QElapsedTimer timer;
        timer.start();
        cwLinePlotTask::LinePlotWorker worker(std::move(input));
        auto result = worker.run();
        result.SolveDurationSeconds = timer.elapsed() / 1000.0;
        return result;
    });
}
