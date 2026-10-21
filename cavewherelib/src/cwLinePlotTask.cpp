/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwLinePlotTask.h"
#include "cwScopeLabels.h"
#include "cwCavernNaming.h"
#include "cwConcurrent.h"
#include "cwSurvexExporterRegion.h"
#include "cwCavernRunner.h"
#include "cwSurvex3DFileReader.h"
#include "cwLinePlotGeometry.h"
#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCavingRegion.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwScrap.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwErrorModel.h"
#include "cwData.h"
#include "cwExternalStationHarvest.h"

// Qt includes
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QPromise>
#include <QRegularExpression>
#include <QtGlobal>
#include <cmath>
#include <functional>
#include <optional>

cwLinePlotTask::LinePlotCaveData::LinePlotCaveData() :
    DepthLengthChanged(false),
    Depth(0.0),
    Length(0.0)
{
}
cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup(cwSurveyNode* node,
                                                                const cwScopeLabels& labels)
{
    // Keys are matched against the changed-station names reported by
    // setStationAsChanged, which are node-local cavern names. Every key here is
    // spelled the way the exporter spells it: the trip's scope prefix, then the
    // station name. Resolved once per trip rather than per station:
    // cwTrip::scopePrefix has to look at the trip's siblings to know its label.
    //
    // A source root answers for its whole subtree, and its scopes come from the
    // snapshot's labels: a derived trip's stations sit under its node's cavern
    // path and its own scope, a native trip's under its label in the root's block.
    const bool sourceRoot = labels.sourceRoot(node->id()) == node->id();
    const QList<cwTrip*> trips = sourceRoot ? node->allTrips() : node->trips();
    for(cwTrip* trip : trips) {
        const QUuid tripId = trip->id();
        const QUuid tripNodeId = trip->parentNode()->id();
        const QString tripScope = sourceRoot
            ? (trip->isDerived()
                   ? labels.cavernPath(tripNodeId) + trip->scopePrefix()
                   : cwCavernNaming::scopePrefix(labels.tripLabels(tripNodeId).value(tripId)))
            : trip->scopePrefix();

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

        // A LiDAR-carpet note flags its trip when its tie-in stations move,
        // which matters for a trip whose stations come from a file rather than
        // chunks (a scrap already propagates to its parent trip in
        // setStationAsChanged, so scraps need no extra mapping).
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
        initializeNodes();

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
        indexStations();
        const cwStationPositionLookup stationPositions = regionStationPositions(parsed.lookup);
        updateStationPositions(stationPositions, result);
        const QHash<QUuid, cwSplayTipsByStation> nodeSplayTips =
            splitSplayTipsByNode(parsed.splayTips);
        updateSplayTipsForNodes(nodeSplayTips, result);
        const QHash<QUuid, cwLegErrorsByShot> nodeLegErrors =
            splitLegErrorsByNode(parsed.legErrors);
        result.setRegionNetwork(parsed.network);
        markNetworkChanges(parsed.network, result);

        // The other half of the floating-survey answer. An attached centerline
        // owns no chunk, so checkForErrors above never saw it; only a completed
        // run knows which scopes cavern placed and which it dropped.
        result.FloatingSurveys.append(
            cwFindFloatingSurveys::fromExternalScopes(InputData.regionData,
                                                      parsed.network,
                                                      ScopeLabels));
        result.ExternalScopesChecked = true;
        result.Hanging = findHangingStations(result.CavernLog, parsed.lookup);

        if (IsCanceled()) {
            return result;
        }

        // The network carries the shot topology for externally-attached scopes,
        // which have no cwSurveyChunk of their own. cwSurveyNetwork is
        // implicitly shared, so this is a refcount bump, not a deep copy.
        cwLinePlotGeometry::Result geometry =
            generateGeometry(parsed.network, stationPositions, nodeSplayTips, nodeLegErrors);
        result.setPositions(geometry.points);
        result.setTripVertexRanges(geometry.tripVertexRanges);
        result.setTripSplayVertexRanges(geometry.tripSplayVertexRanges);
        result.setTripUuids(geometry.tripUuids);
        result.setTripLengths(geometry.tripLengths);
        result.setSegmentErrors(geometry.segmentErrors);

        updateDepthLength(geometry.nodeLengthAndDepths, result);

        return result;
    }

private:
    cwLinePlotTask::Input InputData;
    std::function<bool ()> IsCanceled;
    cwCavingRegion Region;
    // Every node of Region's tree, pre-order, gathered once: the region's caves
    // and every node below them.
    QList<cwSurveyNode*> Nodes;
    // All node-keyed bookkeeping uses cwSurveyNode::id() rather than an integer
    // position: the driver scopes every station under its node's label path, so
    // indexes have no representation in the cavern output; UUIDs do. The
    // result likewise identifies changed nodes/trips/scraps by id(), so the
    // worker never holds a pointer into the main-thread-owned objects.
    QHash<QUuid, cwLinePlotTask::StationTripScrapLookup> TripLookups;
    // Index from node UUID to the worker-internal node owned by Region. Built
    // once in initializeNodes() so the rest of the worker can stay UUID-keyed.
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

    void initializeNodes()
    {
        Nodes = Region.rootNode()->allNodes();
        InternalNodeById.reserve(Nodes.size());
        for (cwSurveyNode* node : std::as_const(Nodes)) {
            InternalNodeById.insert(node->id(), node);
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
        exportOptions.externalFixedStations = InputData.externalFixedStations;
        exportOptions.externalOutputSystems = InputData.externalOutputSystems;
        exportOptions.externalBareFixOrigins = InputData.externalBareFixOrigins;
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

    cwLinePlotGeometry::Result generateGeometry(
        const cwSurveyNetwork& network,
        const cwStationPositionLookup& stationPositions,
        const QHash<QUuid, cwSplayTipsByStation>& nodeSplayTips,
        const QHash<QUuid, cwLegErrorsByShot>& nodeLegErrors)
    {
        const Monad::Result<cwLinePlotGeometry::Result> result =
            // The input snapshot rather than Region.data(): the worker's
            // region is rebuilt through setData, which leaves out the derived
            // fields (unit sets, cavernLabels) a sourced subtree draws from.
            cwLinePlotGeometry::generate(InputData.regionData, stationPositions, network,
                                         nodeSplayTips, nodeLegErrors);
        if (result.hasError()) {
            return cwLinePlotGeometry::Result();
        }
        return result.value();
    }

    bool checkForErrors(cwLinePlotTask::LinePlotResultData& result)
    {
        int unconnectedChunkCount = 0;
        QStringList offendingCaveNames;

        const QHash<QUuid, QList<cwFindUnconnectedSurveyChunks::Result>> unconnected =
            cwFindUnconnectedSurveyChunks::find(InputData.regionData,
                                                InputData.excludedExternalOwners);

        for (cwSurveyNode* node : std::as_const(Nodes)) {
            const auto nodeResults = unconnected.constFind(node->id());
            if (nodeResults == unconnected.constEnd()) {
                continue;
            }
            const QList<cwFindUnconnectedSurveyChunks::Result>& errorResults = nodeResults.value();

            // The results index the node's own trips, so the snapshot holds
            // just those; node->data() would copy its subtree too.
            cwCaveData nodeSnapshot;
            nodeSnapshot.id = node->id();
            nodeSnapshot.name = node->name();
            nodeSnapshot.trips = cwData::toDataList<cwTripData>(node->trips());

            cwLinePlotTask::LinePlotCaveData& nodeData = createLinePlotCaveDataFor(node->id(), result);
            nodeData.setUnconnectedChunkError(errorResults);
            unconnectedChunkCount += errorResults.size();
            offendingCaveNames.append(node->name());
            result.FloatingSurveys.append(
                cwFindFloatingSurveys::fromUnconnectedChunks(nodeSnapshot, errorResults));
        }

        if (unconnectedChunkCount > 0) {
            // Cavern was never run; surface a SolveError so CavernOutputPage
            // tells the user *why* and which caves need attention, instead of
            // showing "Last solve completed successfully" with an empty log.
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Validation;
            error.message = QStringLiteral(
                "Cannot solve: %1 survey leg(s) are not connected to the rest of the project (%2). "
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
            TripLookups.insert(node->id(), cwLinePlotTask::StationTripScrapLookup(node, ScopeLabels));
        }
    }

    // The node a cavern name belongs to, or null when no node wears the name's
    // leading scope.
    //
    // The deepest node resolve() reaches, except that the topmost attached node
    // on that path owns everything beneath it: that node is one *include and
    // its children are never blocks (§7.1, the rule DriverTree::index applies
    // in cwSurvexExporterCaveTask), so a deeper label match there is a
    // coincidence of names.
    const cwSurveyNode* owningNode(const QString& scopedName) const
    {
        const cwSurveyNode* deepest =
            InternalNodeById.value(ScopeLabels.resolve(scopedName).nodeId, nullptr);

        const cwSurveyNode* owner = deepest;
        for (const cwSurveyNode* node = deepest; node != nullptr && !node->isRoot();
             node = node->parentNode()) {
            if (!node->externalCenterline().isEmpty()) {
                owner = node;
            }
        }
        return owner;
    }

    // The owning node and the nodes above it up to the region's caves, deepest
    // first. Empty when no node owns the name.
    QList<const cwSurveyNode*> owningChain(const QString& scopedName) const
    {
        QList<const cwSurveyNode*> chain;
        for (const cwSurveyNode* node = owningNode(scopedName); node != nullptr && !node->isRoot();
             node = node->parentNode()) {
            chain.append(node);
        }
        return chain;
    }

    // Where a hanging station's warning lands, and how to read the rest of its
    // survey. A file owner (an attached node or trip) names its stations in
    // the file's own namespace, below filePrefix; a native station names no
    // file owner and is routed to its trips by name instead.
    struct HangingOwner {
        QUuid ownerId;
        QString filePrefix;
        QString entryFile;
        QString fileLocalName;
    };

    std::optional<HangingOwner> fileOwnerOf(const QString& scopedName) const
    {
        const cwSurveyNode* node = owningNode(scopedName);
        if (node == nullptr) {
            return std::nullopt;
        }
        const QString nodePrefix = ScopeLabels.prefix(node->id());
        const QString tail = scopedName.sliced(nodePrefix.size());

        if (!node->externalCenterline().isEmpty()) {
            return HangingOwner{node->id(), nodePrefix,
                                QDir(InputData.caveAttachmentDirs.value(node->id()))
                                    .filePath(node->externalCenterline().entryFile()),
                                tail};
        }

        const QHash<QUuid, QString>& tripLabels = ScopeLabels.tripLabels(node->id());
        for (const cwTrip* trip : node->trips()) {
            if (trip->externalCenterline().isEmpty()) {
                continue;
            }
            const QString tripScope = tripLabels.value(trip->id()) + QLatin1Char('.');
            if (tail.startsWith(tripScope, Qt::CaseInsensitive)) {
                return HangingOwner{trip->id(), nodePrefix + tripScope,
                                    QDir(InputData.tripAttachmentDirs.value(trip->id()))
                                        .filePath(trip->externalCenterline().entryFile()),
                                    tail.sliced(tripScope.size())};
            }
        }
        return std::nullopt;
    }

    // Cavern drops every survey no fixed point reaches (netartic.c warning 45)
    // and names one station of each such component on a line of its own,
    // "<file>:<line>: info: <station>". The label is localized, so the
    // line is recognized by its shape — a message that is one bare station
    // name — and by that station having no position in the .3d. The unused-fix
    // match (listpos.c warning 73) reads cavern's English text, so it finds
    // unused fixes only when cavern's messages are in English.
    //
    // A native station routes to its node's native trips, and each reports the
    // chunk stations the .3d left out; the hasPosition filter is what keeps a
    // tied component of the same node off the list. A file reports the
    // components its named stations seed, read back by solving the file alone
    // with those stations fixed: cavern names only one station per component.
    QList<cwLinePlotTask::LinePlotResultData::HangingStations> findHangingStations(
        const QString& cavernLog,
        const cwStationPositionLookup& solved) const
    {
        static const QRegularExpression stationLine(
            QStringLiteral(R"(^.+:\d+(?::\d+)?: [^:]+: (\S+)\s*$)"));
        static const QRegularExpression unusedFixLine(
            QStringLiteral("Unused fixed point “([^”]+)”"));

        QHash<QUuid, QStringList> stationsByOwner;
        QHash<QUuid, QStringList> seedsByOwner;
        QHash<QUuid, HangingOwner> fileOwners;
        QSet<const cwSurveyNode*> nativeNodesRouted;

        const auto routeNative = [&](const QString& scopedName) {
            const cwSurveyNode* node = owningNode(scopedName);
            if (node == nullptr || nativeNodesRouted.contains(node)) {
                return;
            }
            nativeNodesRouted.insert(node);
            const QString nodePrefix = ScopeLabels.prefix(node->id());
            for (const cwTrip* trip : node->trips()) {
                if (!trip->externalCenterline().isEmpty()) {
                    continue;
                }
                const QString tripPrefix = nodePrefix + trip->scopePrefix();
                for (const cwSurveyChunk* chunk : trip->chunks()) {
                    for (const cwStation& station : chunk->stations()) {
                        if (station.isValid() && !solved.hasPosition(tripPrefix + station.name())) {
                            stationsByOwner[trip->id()].append(station.name());
                        }
                    }
                }
            }
        };

        const QStringList lines = cavernLog.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            const QRegularExpressionMatch unusedFix = unusedFixLine.match(line);
            if (unusedFix.hasMatch()) {
                if (const auto owner = fileOwnerOf(unusedFix.captured(1))) {
                    stationsByOwner[owner->ownerId].append(owner->fileLocalName);
                }
                continue;
            }

            const QRegularExpressionMatch match = stationLine.match(line);
            if (!match.hasMatch()) {
                continue;
            }
            const QString name = match.captured(1);
            if (solved.hasPosition(name)) {
                continue;
            }
            if (const auto owner = fileOwnerOf(name)) {
                fileOwners.insert(owner->ownerId, *owner);
                seedsByOwner[owner->ownerId].append(owner->fileLocalName);
            } else {
                routeNative(name);
            }
        }

        for (auto it = seedsByOwner.constBegin(); it != seedsByOwner.constEnd(); ++it) {
            const HangingOwner& owner = fileOwners.value(it.key());
            QStringList& stations = stationsByOwner[it.key()];
            stations.append(it.value());
            if (IsCanceled()) {
                continue;
            }
            const auto components =
                cwExternalStationHarvest::harvestComponents(owner.entryFile, it.value());
            if (components.hasError()) {
                continue;
            }
            for (const QString& station : components.value()) {
                if (!solved.hasPosition(owner.filePrefix + station)) {
                    stations.append(station);
                }
            }
        }

        QList<cwLinePlotTask::LinePlotResultData::HangingStations> hanging;
        for (auto it = stationsByOwner.constBegin(); it != stationsByOwner.constEnd(); ++it) {
            hanging.append({it.key(), it.value()});
        }
        return hanging;
    }

    // Cavern's lookup as the region keeps it: every station whose leading
    // scope names one of this region's nodes, keyed by its full cavern name,
    // rounded to millimeters. Stations under no node of the region are dropped
    // (they would match no node; this keeps the result robust against
    // accidental orphan prefixes without poisoning the whole result).
    cwStationPositionLookup regionStationPositions(
        const cwStationPositionLookup& stationPostions) const
    {
        // Round positions to millimeter precision to absorb cavern's
        // double-to-text rounding when comparing against the previous run.
        constexpr int kPositionPrecisionDigits = 3;
        const double positionFactor = std::pow(10.0, kPositionPrecisionDigits);

        cwStationPositionLookup lookup;

        const QMap<QString, QVector3D> positions = stationPostions.positions();
        for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
            const QString& name = iter.key();
            QVector3D position = iter.value();

            // std::round keeps the intermediate value in double; qRound returns
            // int and overflows for UTM-scale coordinates (a 5.47e6m northing
            // multiplied by 1000 already exceeds INT_MAX, and a user-visible
            // crash on projects solving in absolute coordinates traced here).
            position.setX(float(std::round(double(position.x()) * positionFactor) / positionFactor));
            position.setY(float(std::round(double(position.y()) * positionFactor) / positionFactor));
            position.setZ(float(std::round(double(position.z()) * positionFactor) / positionFactor));

            // Node labels are lowercase by construction, but cavern echoes back
            // whatever case the included file used for a nested scope, so
            // resolve() matches each segment case-insensitively.
            const cwSurveyNode* owner = owningNode(name);
            if (owner == nullptr) {
                qDebug() << "Cavern emitted station with unknown node scope:" << name << LOCATION;
                continue;
            }

            //Walls' empty-name quirk can put a bare "<nodeLabel>." (or one with
            //only spaces after the separator) in the .3d, and neither
            //setPosition nor cwStation::canonicalKey trims, so an unguarded tail
            //would pollute the lookup with a blank key that no chunk station can
            //ever match.
            const QString tail = name.sliced(ScopeLabels.prefix(owner->id()).size());
            if (tail.trimmed().isEmpty()) {
                qDebug() << "Cavern station name has no station under its node scope:"
                         << name << LOCATION;
                continue;
            }

            lookup.setPosition(name, position);
        }

        return lookup;
    }

    // The owning-node split for the splay tips.
    // A tip lands only in its owning node's slice, keyed node-locally and
    // canonically: that node's own trips are the ones that draw it. Positions
    // are kept as cavern gave them: a tip only ever feeds geometry, so it is
    // never compared against a previous solve the way station positions are.
    QHash<QUuid, cwSplayTipsByStation> splitSplayTipsByNode(
        const cwSplayTipsByStation& splayTips) const
    {
        QHash<QUuid, cwSplayTipsByStation> nodeSplayTips;
        for (auto iter = splayTips.constBegin(); iter != splayTips.constEnd(); ++iter) {
            const cwSurveyNode* owner = owningNode(iter.key());
            if (owner == nullptr) {
                continue;
            }

            const QString tail = iter.key().sliced(ScopeLabels.prefix(owner->id()).size());
            if (tail.trimmed().isEmpty()) {
                continue;
            }

            nodeSplayTips[owner->id()].insert(cwStation::canonicalKey(tail), iter.value());
        }

        return nodeSplayTips;
    }

    // The same owning-node split for the leg errors. A leg is kept only when
    // both ends belong to the same node; its key is rebuilt from the node-local
    // names so it matches the geometry's lookup.
    QHash<QUuid, cwLegErrorsByShot> splitLegErrorsByNode(const cwLegErrorsByShot& legErrors) const
    {
        QHash<QUuid, cwLegErrorsByShot> nodeLegErrors;
        for (auto iter = legErrors.constBegin(); iter != legErrors.constEnd(); ++iter) {
            const cwSurveyNode* owner = owningNode(iter.key().first);
            if (owner == nullptr || owner != owningNode(iter.key().second)) {
                continue;
            }

            const int prefixSize = ScopeLabels.prefix(owner->id()).size();
            nodeLegErrors[owner->id()].insert(
                cwSurvex3DFileReader::legKey(iter.key().first.sliced(prefixSize),
                                             iter.key().second.sliced(prefixSize)),
                iter.value());
        }

        return nodeLegErrors;
    }

    // Splays move exactly when the station they hang off does, so they ride out
    // on the nodes the solve already had something to say about. A node with no
    // tips keeps the empty hash it was built with.
    void updateSplayTipsForNodes(const QHash<QUuid, cwSplayTipsByStation>& nodeSplayTips,
                                 cwLinePlotTask::LinePlotResultData& result)
    {
        for (auto iter = nodeSplayTips.constBegin(); iter != nodeSplayTips.constEnd(); ++iter) {
            const auto it = result.Caves.find(iter.key());
            if (it != result.Caves.end()) {
                it.value().setSplayTips(iter.value());
            }
        }
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

    // Marks \a scopedName changed in its owning node and every node above it,
    // each reading the name below itself, as that node's trips spell it.
    void markStationChanged(const QString& scopedName, cwLinePlotTask::LinePlotResultData& result)
    {
        for (const cwSurveyNode* node : owningChain(scopedName)) {
            setStationAsChanged(node->id(),
                                scopedName.sliced(ScopeLabels.prefix(node->id()).size()),
                                result);
        }
    }

    // Cavern emits .3d coordinates in whatever *cs out named, which for this
    // export is the project's local projection — already centered on the
    // project, already small enough for float in the shaders. Nothing is
    // subtracted on the way in; there is no second frame to reconcile with.
    //
    // Publishing is what tells cwLinePlotManager to write the lookup back to
    // the live region, so it is published only when a station moved, appeared,
    // or vanished.
    void updateStationPositions(const cwStationPositionLookup& stationPositions,
                                cwLinePlotTask::LinePlotResultData& result)
    {
        const QMap<QString, QVector3D> newPositions = stationPositions.positions();
        const QMap<QString, QVector3D> oldPositions = InputData.previousStationPositions.positions();

        if (newPositions == oldPositions) {
            return;
        }

        for (auto it = newPositions.constBegin(); it != newPositions.constEnd(); ++it) {
            const auto old = oldPositions.constFind(it.key());
            if (old == oldPositions.constEnd() || old.value() != it.value()) {
                markStationChanged(it.key(), result);
            }
        }

        // A vanished station flags its nodes, so their trips and scraps hear
        // that it is gone.
        for (auto it = oldPositions.constBegin(); it != oldPositions.constEnd(); ++it) {
            if (!newPositions.contains(it.key())) {
                for (const cwSurveyNode* node : owningChain(it.key())) {
                    addEmptyStationLookup(node->id(), result);
                }
            }
        }

        result.setRegionStationPositions(stationPositions);
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

    // A shot appearing or vanishing changes how a scrap morphs even when no
    // station moved, so the stations whose neighbors changed are flagged too.
    void markNetworkChanges(const cwSurveyNetwork& network,
                            cwLinePlotTask::LinePlotResultData& result)
    {
        const QStringList changedStations =
            cwSurveyNetwork::changedStations(InputData.previousNetwork, network);
        for (const QString& station : changedStations) {
            markStationChanged(station, result);
        }
    }
};

cwLinePlotTask::Input cwLinePlotTask::buildInput(const cwCavingRegion *region)
{
    Input input;
    if(region != nullptr) {
        input.regionData = region->data();

        // Carry the last applied solve across as the change-detection baseline.
        // The live region holds it because cwLinePlotManager writes each result
        // back, so a restarted solve still diffs against what the user last saw
        // rather than against a half-finished run.
        input.previousStationPositions = region->stationPositions();
        input.previousNetwork = region->network();
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
    input.externalFixedStations = external.externalFixedStations;
    input.externalOutputSystems = external.externalOutputSystems;
    input.externalBareFixOrigins = external.externalBareFixOrigins;
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
    // The QPromise form is what makes cancel() reach the worker: the solve polls
    // promise.isCanceled() between its phases, so a restart or a manager being
    // torn down stops the run at the next boundary rather than paying for cavern
    // and the geometry pass. A canceled run publishes no result, which every
    // caller already handles by checking resultCount().
    return cwConcurrent::run([input = std::move(input)]
                             (QPromise<cwLinePlotTask::LinePlotResultData>& promise) mutable {
        QElapsedTimer timer;
        timer.start();
        cwLinePlotTask::LinePlotWorker worker(std::move(input),
                                              [&promise]() { return promise.isCanceled(); });
        auto result = worker.run();
        if (promise.isCanceled()) {
            return;
        }
        result.SolveDurationSeconds = timer.elapsed() / 1000.0;
        promise.addResult(std::move(result));
    });
}
