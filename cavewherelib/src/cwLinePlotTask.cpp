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
#include "cwData.h"

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
cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup(cwSurveyNode* node)
{
    // Keys are matched against the changed-station names reported by
    // setStationAsChanged, which are the node-local lookup keys. For an
    // externally-attached trip those retain the trip scope
    // ("<tripLabel>.<tail>") while chunk / note / scrap stations carry only the
    // tail, so every key inserted here is scoped to match (a no-op for a native
    // trip). Resolved once per trip rather than per station: cwTrip::scopePrefix
    // has to look at the trip's siblings to know its label.
    for(cwTrip* trip : node->trips()) {
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

        initializeScopeLabels();
        initializeNodeStationLookups();

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
        updateStationPositionForNodes(parsed.lookup, result);
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

        updateDepthLength(geometry.nodeLengthAndDepths, result);
        updateNodeNetworks(result);

        return result;
    }

private:
    cwLinePlotTask::Input InputData;
    cwCavingRegion Region;
    // Every node of Region's tree, pre-order, gathered once: the region's caves
    // and every node below them. Each solves into a lookup of its own.
    QList<cwSurveyNode*> Nodes;
    // All node-keyed bookkeeping uses cwSurveyNode::id() rather than an integer
    // position: the driver scopes every station under its node's label path, so
    // indexes have no representation in the cavern output; UUIDs do. The
    // result likewise identifies changed nodes/trips/scraps by id(), so the
    // worker never holds a pointer into the main-thread-owned objects.
    QHash<QUuid, cwStationPositionLookup> NodeStationLookups;
    QHash<QUuid, cwLinePlotTask::StationTripScrapLookup> TripLookups;
    // Index from node UUID to the worker-internal node owned by Region. Built
    // once in initializeNodeStationLookups() so the rest of the worker can stay
    // UUID-keyed.
    QHash<QUuid, cwSurveyNode*> InternalNodeById;
    // The survey label each node's and trip's "*begin" carries. The exporter
    // assigns the same labels from the same ordered snapshot, so this is not a
    // map handed across a boundary — it is the same pure function evaluated on
    // both sides, which is what lets the decode below recover a node's id from
    // a name cavern echoed back.
    cwScopeLabels ScopeLabels;

    void initializeScopeLabels()
    {
        // Caller contract: every node id must be non-null - the manager
        // satisfies this via cwCavingRegion::data(); synthetic callers
        // (cwTripLinePlotTask) generate a UUID before building Input.
        walkCaveDataTree(InputData.regionData.caves,
                         []([[maybe_unused]] const cwCaveData& node, const QStringList&) {
                             Q_ASSERT(!node.id.isNull());
                         });

        ScopeLabels = cwScopeLabels(InputData.regionData);
    }

    // The labels of this node's externally-attached trips, which are the only
    // trip scopes the exporter opens inside a node block. A native-prefixed
    // (Scope) trip is deliberately absent: its stations come from chunks, which
    // the exporter emits unscoped.
    QSet<QString> externalTripLabelsFor(const cwSurveyNode* node) const
    {
        const QHash<QUuid, QString>& labels = ScopeLabels.tripLabels(node->id());

        QSet<QString> externalLabels;
        for (const cwTrip* trip : node->trips()) {
            if (!trip->externalCenterline().isEmpty()) {
                externalLabels.insert(labels.value(trip->id()));
            }
        }
        return externalLabels;
    }

    void initializeNodeStationLookups()
    {
        Nodes = Region.rootNode()->allNodes();
        NodeStationLookups.reserve(Nodes.size());
        InternalNodeById.reserve(Nodes.size());

        for (cwSurveyNode* node : std::as_const(Nodes)) {
            const QUuid id = node->id();
            InternalNodeById.insert(id, node);
            NodeStationLookups.insert(id, node->stationPositionLookup());
        }
    }

    bool exportSurvex(const QString& svxPath, cwLinePlotTask::LinePlotResultData& result)
    {
        // exportRegion assigns the scope labels itself, from the same ordered
        // snapshot initializeScopeLabels() reads, which is what lets the decode
        // below recover a node from a name cavern echoed back. The
        // attachment-dir maps come straight from the Input the caller built.
        cwSurvexExporterRegion::Options exportOptions;
        exportOptions.caveAttachmentDirs = InputData.caveAttachmentDirs;
        exportOptions.tripAttachmentDirs = InputData.tripAttachmentDirs;
        exportOptions.tripInjectedDeclinations = InputData.tripInjectedDeclinations;
        exportOptions.excludedExternalOwners = InputData.excludedExternalOwners;

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

        for (cwSurveyNode* node : std::as_const(Nodes)) {
            // Each node is its own *begin block, so its chunks join only each
            // other: a child node's trips are checked as that child. Built from
            // the node's own trips, since node->data() would copy its subtree.
            cwCaveData nodeSnapshot;
            nodeSnapshot.id = node->id();
            nodeSnapshot.name = node->name();
            nodeSnapshot.trips = cwData::toDataList<cwTripData>(node->trips());

            const Monad::Result<QList<cwFindUnconnectedSurveyChunks::Result>> unconnectedResult =
                cwFindUnconnectedSurveyChunks::find(nodeSnapshot);
            if (unconnectedResult.hasError()) {
                continue;
            }
            const QList<cwFindUnconnectedSurveyChunks::Result> errorResults = unconnectedResult.value();
            if (!errorResults.isEmpty()) {
                cwLinePlotTask::LinePlotCaveData& nodeData = createLinePlotCaveDataFor(node->id(), result);
                nodeData.setUnconnectedChunkError(errorResults);
                unconnectedChunkCount += errorResults.size();
                offendingCaveNames.append(node->name());
                result.FloatingSurveys.append(
                    cwFindFloatingSurveys::fromUnconnectedChunks(nodeSnapshot, errorResults));
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

    // Returns the result entry for nodeId, creating an empty one on first
    // access. nodeId always comes from a node in Nodes or from a cavern prefix
    // already checked against InternalNodeById, so it is always valid.
    cwLinePlotTask::LinePlotCaveData& createLinePlotCaveDataFor(const QUuid& nodeId,
                                                               cwLinePlotTask::LinePlotResultData& result)
    {
        return result.Caves[nodeId];
    }

    void addEmptyStationLookup(const QUuid& nodeId, cwLinePlotTask::LinePlotResultData& result)
    {
        if (!result.Caves.contains(nodeId)) {
            result.Caves.insert(nodeId, cwLinePlotTask::LinePlotCaveData());
        }
    }

    void indexStations()
    {
        TripLookups.clear();
        TripLookups.reserve(Nodes.size());

        for (cwSurveyNode* node : std::as_const(Nodes)) {
            TripLookups.insert(node->id(), cwLinePlotTask::StationTripScrapLookup(node));
        }
    }

    // The node a cavern name belongs to, and the nodes above it up to the
    // region's caves, deepest first. Empty when no node wears the name's leading
    // scope.
    //
    // resolve() walks the label path as deep as the labels reach. The walk stops
    // at a node with an attachment of its own: that node is one *include, its
    // children are never blocks (§7.1), so a deeper label match there is a
    // coincidence of names, and the station belongs to the attached node.
    QList<const cwSurveyNode*> owningChain(const QString& scopedName) const
    {
        const cwScopeLabels::Resolution resolution = ScopeLabels.resolve(scopedName);

        QList<const cwSurveyNode*> chain;
        const cwSurveyNode* node = InternalNodeById.value(resolution.nodeId, nullptr);
        while (node != nullptr && !node->isRoot()) {
            if (!node->externalCenterline().isEmpty()) {
                chain.clear();
            }
            chain.append(node);
            node = node->parentNode();
        }
        return chain;
    }

    // Parses cavern-emitted scoped station names of the form
    //   "<nodeLabel>.<nodeLabel>...<station-name>"
    // back into one position lookup per node, keyed by the node's id. A
    // station lands in its deepest matching node's slice and in every
    // ancestor's, keyed in each by the name below that node: a node's slice is
    // its subtree, as a cave's lookup has always held every one of its trips'
    // stations. Stations whose leading scope is not one of this region's cave
    // labels are dropped (they would not match any node in the region; this
    // keeps the split robust against accidental orphan prefixes without
    // poisoning the whole result).
    QHash<QUuid, cwStationPositionLookup> splitLookupByNode(
        const cwStationPositionLookup& stationPostions) const
    {
        // Round positions to millimetre precision to absorb cavern's
        // double-to-text rounding when comparing against the previous run.
        constexpr int kPositionPrecisionDigits = 3;
        const double positionFactor = std::pow(10.0, kPositionPrecisionDigits);

        QHash<QUuid, cwStationPositionLookup> nodeStations;
        nodeStations.reserve(InternalNodeById.size());

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

            // Node labels are lowercase by construction, but cavern echoes back
            // whatever case the included file used for a nested scope, so
            // resolve() matches each segment case-insensitively.
            const QList<const cwSurveyNode*> chain = owningChain(name);
            if (chain.isEmpty()) {
                qDebug() << "Cavern emitted station with unknown node scope:" << name << LOCATION;
                continue;
            }

            //Walls' empty-name quirk can put a bare "<nodeLabel>." (or one with
            //only spaces after the separator) in the .3d, and neither
            //setPosition nor cwStation::canonicalKey trims, so an unguarded tail
            //would pollute the lookup with a blank key that no chunk station can
            //ever match.
            const QString tail = name.sliced(ScopeLabels.prefix(chain.first()->id()).size());
            if (tail.trimmed().isEmpty()) {
                qDebug() << "Cavern station name has no station under its node scope:"
                         << name << LOCATION;
                continue;
            }

            // A prefix is the label path down to its node, and resolve()
            // consumed exactly those segments, so each slice's key is the name
            // with that node's prefix length cut off — in the case cavern used.
            for (const cwSurveyNode* node : chain) {
                const QString key = name.sliced(ScopeLabels.prefix(node->id()).size());
                nodeStations[node->id()].setPosition(key, position);
            }
        }

        return nodeStations;
    }

    void setStationAsChanged(const QUuid& nodeId, const QString& stationName,
                             cwLinePlotTask::LinePlotResultData& result)
    {
        addEmptyStationLookup(nodeId, result);

        const cwLinePlotTask::StationTripScrapLookup lookup = TripLookups.value(nodeId);
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

    void updateInternalNodeStationLookups(const QHash<QUuid, cwStationPositionLookup>& nodeStations,
                                          cwLinePlotTask::LinePlotResultData& result)
    {
        // Iterate every node so nodes with no positions in `nodeStations`
        // (e.g. a node whose entire centerline failed to solve) still get
        // their stale lookup cleared and the result populated.
        for (const cwSurveyNode* node : std::as_const(Nodes)) {
            const QUuid nodeId = node->id();

            const cwStationPositionLookup newLookup = nodeStations.value(nodeId);
            const cwStationPositionLookup oldLookup = NodeStationLookups.value(nodeId);

            if (newLookup.positions().size() != oldLookup.positions().size()) {
                addEmptyStationLookup(nodeId, result);
            }

            const QMap<QString, QVector3D> newPositions = newLookup.positions();
            const QMap<QString, QVector3D> oldPositions = oldLookup.positions();

            for (auto it = newPositions.constBegin(); it != newPositions.constEnd(); ++it) {
                const QString& stationName = it.key();
                const QVector3D newPoint = it.value();
                if (oldPositions.contains(stationName)) {
                    if (oldPositions.value(stationName) != newPoint) {
                        setStationAsChanged(nodeId, stationName, result);
                    }
                } else {
                    setStationAsChanged(nodeId, stationName, result);
                }
            }

            NodeStationLookups[nodeId] = newLookup;
        }
    }

    void updateExternalNodeStationLookups(cwLinePlotTask::LinePlotResultData& result)
    {
        for (cwSurveyNode* internalNode : std::as_const(Nodes)) {
            const QUuid nodeId = internalNode->id();
            if (!result.Caves.contains(nodeId)) {
                continue;
            }

            const cwStationPositionLookup updatedLookup = NodeStationLookups.value(nodeId);
            cwLinePlotTask::LinePlotCaveData& nodeData = result.Caves[nodeId];
            nodeData.setStationPositions(updatedLookup);
            internalNode->setStationPositionLookup(updatedLookup);
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

    void updateStationPositionForNodes(const cwStationPositionLookup& stationPostions,
                                       cwLinePlotTask::LinePlotResultData& result)
    {
        indexStations();

        const QHash<QUuid, cwStationPositionLookup> nodeStationLookups = splitLookupByNode(stationPostions);

        updateInternalNodeStationLookups(nodeStationLookups, result);
        updateExternalNodeStationLookups(result);
    }

    void updateDepthLength(const QHash<QUuid, cwLinePlotGeometry::LengthAndDepth>& lengths,
                           cwLinePlotTask::LinePlotResultData& result)
    {
        for (const cwSurveyNode* node : std::as_const(Nodes)) {
            const QUuid nodeId = node->id();
            Q_ASSERT(lengths.contains(nodeId));
            cwLinePlotTask::LinePlotCaveData& nodeData = createLinePlotCaveDataFor(nodeId, result);
            //Always a real measurement — 0/0 means the node resolved no
            //centerline, so it copies through to the live node as-is.
            const cwLinePlotGeometry::LengthAndDepth measured = lengths.value(nodeId);
            nodeData.setLength(measured.length());
            nodeData.setDepth(measured.depth());
        }
    }

    void updateNodeNetworks(cwLinePlotTask::LinePlotResultData& result)
    {
        // The solved region network carries every scope's topology, including
        // externally-attached trips that own no cwSurveyChunk. Keyed
        // "<nodePrefix><tail>" (native) and "<nodePrefix><tripLabel>.<tail>"
        // (external), where nodePrefix is the label path down to the node.
        const cwSurveyNetwork regionNetwork = result.regionNetwork();

        auto createNetwork = [&regionNetwork, this](const cwSurveyNode* node) {
            cwSurveyNetwork network;

            for (cwTrip* trip : node->trips()) {
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
            // node network under the node-local scope ("<tripLabel>.<tail>", the
            // same keying splitLookupByNode gives the position lookup) so the
            // note-editing sites can resolve external neighbors. Native-to-native
            // edges are left to the chunk loop above.
            //
            // Which names are external is a membership question, not a spelling
            // one: a trip label is an ordinary survey name, so nothing about
            // "topo1.a1" marks it as scoped except that this node has a trip
            // labeled topo1.
            const QString nodePrefix = ScopeLabels.prefix(node->id());
            const QSet<QString> externalTripLabels = externalTripLabelsFor(node);

            for (const QString& scopedStation : regionNetwork.stations()) {
                if (!scopedStation.startsWith(nodePrefix)) {
                    continue;
                }
                const QString nodeLocalStation = scopedStation.mid(nodePrefix.size());
                if (!externalTripLabels.contains(cwCavernNaming::scopeHeadOf(nodeLocalStation))) {
                    continue; //native station, already covered by the chunk loop
                }
                for (const QString& scopedNeighbor : regionNetwork.neighbors(scopedStation)) {
                    const QString nodeLocalNeighbor = scopedNeighbor.startsWith(nodePrefix)
                            ? scopedNeighbor.mid(nodePrefix.size())
                            : scopedNeighbor;
                    network.addShot(nodeLocalStation, nodeLocalNeighbor);
                }
            }

            return network;
        };

        for (const cwSurveyNode* node : std::as_const(Nodes)) {
            const cwSurveyNetwork network = createNetwork(node);
            if (network == node->network()) {
                continue;
            }
            const QUuid nodeId = node->id();
            result.Caves[nodeId].setNetwork(network);

            const auto changedStations = cwSurveyNetwork::changedStations(node->network(), network);
            for (const auto& station : changedStations) {
                setStationAsChanged(nodeId, station, result);
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
    input.excludedExternalOwners = external.excludedExternalOwners;
    const QHash<QUuid, bool>& fileOwnsDeclination = external.fileOwnsDeclination;

    if (region != nullptr) {
        // Resolve the injected declination here, on the main thread: the
        // resolved value (IGRF auto or manual fallback) lives on the live
        // cwTripCalibration and isn't part of the worker snapshot. Owners
        // missing from fileOwnsDeclination stay uninjected — same outcome
        // as a file that owns its declination.
        for (cwTrip* trip : region->rootNode()->allTrips()) {
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
