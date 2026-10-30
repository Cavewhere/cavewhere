/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinePlotManager.h"
#include "cwRestarterTracking.h"
#include "cwCavernRunner.h"
#include "cwTaskTrace.h"
#include "cwProfileLog.h"
#include "cwProfilePhase.h"
#include "cwCavingRegion.h"
#include "cwGeoReference.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwLinePlotTask.h"
#include "cwRenderLinePlot.h"
#include "cwTripCalibration.h"
#include "cwSurveyNoteModel.h"
#include "cwScrap.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwSurveyChunkSignaler.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwSurveyNetworkSource.h"
#include "cwEntranceModel.h"
#include "cwEquateModel.h"
#include "cwFixStationModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLinePlotTripVisibility.h"
#include "cwNameUtils.h"
#include "cwStation.h"
#include "asyncfuture.h"

#include <QDateTime>
#include <QFileInfo>
#include <QFuture>
#include <QSet>

#include <algorithm>

namespace {

const char* solveErrorStepName(cwLinePlotTask::SolveError::Step step)
{
    switch(step) {
    case cwLinePlotTask::SolveError::Step::Export: return "Export";
    case cwLinePlotTask::SolveError::Step::Cavern: return "Cavern";
    case cwLinePlotTask::SolveError::Step::Parse: return "Parse";
    case cwLinePlotTask::SolveError::Step::Validation: return "Validation";
    }
    return "Unknown";
}

// The worker identifies changed nodes/trips/scraps by UUID. Resolve those
// UUIDs back to the live objects in `region`, dropping any that were deleted
// while the solve was running. Walking the live hierarchy (rather than a flat
// id->object map) also guarantees a trip/scrap is only kept when its owning
// node survived.
struct ResolvedResults {
    QHash<cwSurveyNode*, cwLinePlotTask::LinePlotCaveData> nodes;
    QSet<cwTrip*> trips;
    QSet<cwScrap*> scraps;
};

ResolvedResults resolveResultsToLive(const cwCavingRegion* region,
                                     const cwLinePlotTask::LinePlotResultData& results)
{
    ResolvedResults resolved;
    for (cwSurveyNode* node : region->rootNode()->allNodes()) {
        const auto nodeIt = results.Caves.constFind(node->id());
        if (nodeIt == results.Caves.constEnd()) {
            continue;
        }
        resolved.nodes.insert(node, nodeIt.value());

        for (cwTrip* trip : node->trips()) {
            if (!results.Trips.contains(trip->id())) {
                continue;
            }
            resolved.trips.insert(trip);

            for (cwNote* note : trip->notes()->notes()) {
                for (cwScrap* scrap : note->scraps()) {
                    if (results.Scraps.contains(scrap->id())) {
                        resolved.scraps.insert(scrap);
                    }
                }
            }
        }
    }
    return resolved;
}

QString entryFileName(const cwExternalCenterline& centerline)
{
    return QFileInfo(centerline.entryFile()).fileName();
}

} // namespace


cwLinePlotManager::cwLinePlotManager(QObject *parent) :
    QObject(parent),
    m_restarter(this)
{
    Region = nullptr;
    m_linePlot = nullptr;

    m_surveyNetworkSource = new cwSurveyNetworkSource(this);

    m_floatingSurveyModel = new cwFloatingSurveyModel(this);

    // The external-centerline subsystem owns the watcher, the async scan
    // pipeline, the attachment dirs, and the attached-centerlines model.
    // Its apply requests a solve through solveNeeded after the member
    // swap, so buildInput never reads half-applied declination flags.
    m_externalCenterlineManager = new cwExternalCenterlineManager(this);
    connect(m_externalCenterlineManager, &cwExternalCenterlineManager::solveNeeded,
            this, &cwLinePlotManager::runSurvex);

    SurveySignaler = new cwSurveyChunkSignaler(this);

    SurveySignaler->addConnectionToCaves(SIGNAL(insertedTrips(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToCaves(SIGNAL(removedTrips(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SLOT(runSurvex()));

    SurveySignaler->addConnectionToTrips(SIGNAL(chunksInserted(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTrips(SIGNAL(chunksRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTrips(SIGNAL(chunksMoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTripCalibrations(SIGNAL(calibrationsChanged()), this, SLOT(runSurvex()));
    // declinationChanged covers external inputs that don't dirty stored
    // calibration data — trip date and fix-station edits re-resolve the auto
    // declination (bug #581); calibrationsChanged alone would miss them.
    SurveySignaler->addConnectionToTripCalibrations(SIGNAL(declinationChanged(double)), this, SLOT(runSurvex()));

    SurveySignaler->addConnectionToChunks(SIGNAL(shotsAdded(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(shotsRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(stationsAdded(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(stationsRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(stationSplaysChanged(int)), this, SLOT(runSurvex()));

    cwTrackRestarter(m_futureManagerToken, m_restarter, QStringLiteral("Line plot"));
}

cwLinePlotManager::~cwLinePlotManager() {
    //See cwUpdatable::beginTeardown().
    beginTeardown();

    //Cancel the scan first: a canceled scan's apply never runs, so no
    //solveNeeded can chain a fresh solve onto a manager being torn down.
    m_externalCenterlineManager->cancelScan();

    //Cancel without waiting: the worker solves a value copy of the region
    //(cwLinePlotTask::Input) and polls for cancel between its phases, so nothing
    //it touches dies with this manager. The result continuation is bound with
    //context(this), so it is dropped here rather than delivered to a
    //half-destroyed manager. The first line finishes the outer future; the
    //second is the cancel the worker itself sees, since the drain's own cancel
    //forwarding is bound to this manager too.
    m_restarter.future().cancel();
    m_solveWorker.cancel();

    // m_keywordRegistry's destructor tears down the keyword items
    // synchronously.
}

/**
  \brief Sets the region that this manager will listen to
  */
void cwLinePlotManager::setRegion(cwCavingRegion* region) {
    const QHash<QUuid, QList<cwSolvedEntrance>> oldEntrances = solvedEntrances();
    if (Region != nullptr) {
        disconnect(Region, &cwCavingRegion::solvedEntrancesChanged,
                   this, &cwLinePlotManager::solvedEntrancesChanged);
    }
    Region = region;
    m_pendingLabels.clear();
    m_pendingLiveLabels.clear();

    // Clear any cached cavern output / solve error from a previous region —
    // CavernOutputPage should start blank for the new project (D-2). Empty
    // results also clear per-chunk error markers. Safe even when region is
    // nullptr (publishPerCaveErrors no-ops without a Region).
    publishResults(cwLinePlotTask::LinePlotResultData::cleared());

    // After the clear, never before: cave and trip uuids are persisted, so
    // reopening a project would otherwise let the outgoing region's records
    // resolve against the incoming one and publish a run's worth of rows that
    // describe neither.
    m_floatingSurveyModel->setRegion(region);

    // Connected after the clear above, so the swap announces at most once.
    if (Region != nullptr) {
        connect(Region, &cwCavingRegion::solvedEntrancesChanged,
                this, &cwLinePlotManager::solvedEntrancesChanged);
    }
    if (solvedEntrances() != oldEntrances) {
        emit solvedEntrancesChanged();
    }

    if(Region == nullptr) {
        SurveySignaler->setRegion(nullptr);
        m_externalCenterlineManager->setRegion(nullptr);
        return;
    }

    // A trip or node added or removed at any depth changes what cavern reads,
    // and a node added anywhere brings fix stations to hook.
    connect(Region->rootNode(), &cwSurveyNode::subtreeChanged,
            this, &cwLinePlotManager::handleSubtreeChanged);
    connect(Region, &cwCavingRegion::memberViewsChanged,
            this, &cwLinePlotManager::handleMemberViewsChanged);

    // The local projection is what *cs out names, and cavern reports the solved
    // stations in it — so the scene's coordinates are only meaningful in the
    // frame that was current when the solve ran. A frame that moves invalidates
    // every position and has to re-solve.
    connect(Region->geoReference(), &cwGeoReference::localProjectionChanged, this, &cwLinePlotManager::runSurvex);

    SurveySignaler->setRegion(Region);

    // Hook fix-station edits on every existing node; the subtreeChanged handler
    // above hooks the ones added later. Every equate sits in the region's one
    // list, hooked once here.
    connectEquates(Region->equates());
    connectNodeInputs();

    rerunIfStale(Region);

    // Recompute the external-centerline watch set + missing-source probe.
    // Done after the per-cave/trip connects so any subsequent attach/detach
    // signal recompute lands on a populated region. The initial solve
    // chains behind the scan's apply via solveNeeded.
    m_externalCenterlineManager->setRegion(Region);

    updateLinePlot(cwLinePlotTask::LinePlotResultData());
}

void cwLinePlotManager::connectFixStations(cwSurveyNode* node) {
    if (!node) { return; }
    auto* model = node->fixStations();
    if (!model) { return; }
    connect(model, &cwFixStationModel::dataChanged,  this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(model, &cwFixStationModel::rowsInserted, this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(model, &cwFixStationModel::rowsRemoved,  this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(model, &cwFixStationModel::modelReset,   this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    // The driver writes *entrance for the node's entrances; a label edit
    // changes nothing cavern reads.
    connect(node->entrances(), &cwEntranceModel::entranceStationsChanged,
            this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
}

void cwLinePlotManager::connectEquates(cwEquateModel* equates) {
    if (!equates) { return; }
    connect(equates, &cwEquateModel::dataChanged,  this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(equates, &cwEquateModel::rowsInserted, this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(equates, &cwEquateModel::rowsRemoved,  this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(equates, &cwEquateModel::modelReset,   this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
}

void cwLinePlotManager::handleSubtreeChanged() {
    runSurvex();
    if (m_connectNodeInputsPending) { return; }
    m_connectNodeInputsPending = true;
    QMetaObject::invokeMethod(this, &cwLinePlotManager::connectNodeInputs, Qt::QueuedConnection);
}

void cwLinePlotManager::connectNodeInputs() {
    m_connectNodeInputsPending = false;
    if (Region == nullptr) { return; }
    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        connectFixStations(node);
    }
}

void cwLinePlotManager::setRenderLinePlot(cwRenderLinePlot* linePlot) {
    m_linePlot = linePlot;
    updateLinePlot(cwLinePlotTask::LinePlotResultData());
}

void cwLinePlotManager::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
    m_externalCenterlineManager->setFutureManagerToken(token);
}

void cwLinePlotManager::setKeywordItemModel(cwKeywordItemModel* keywordItemModel)
{
    if (m_keywordRegistry.model() == keywordItemModel) {
        return;
    }

    // setModel tears down the items registered with the old model. Items are
    // (re)created on the next updateLinePlot() against the current geometry.
    m_keywordRegistry.setModel(keywordItemModel);

    for (cwTrip* trip : std::as_const(m_trackedTrips)) {
        disconnect(trip, &QObject::destroyed, this, nullptr);
    }
    m_trackedTrips.clear();
}

void cwLinePlotManager::reconcileTripKeywordItems(
    const QHash<QUuid, cwTrip*>& liveByUuid,
    const QVector<QUuid>& tripUuids,
    const QVector<cwLinePlotGeometry::VertexRange>& tripVertexRanges,
    const QVector<cwLinePlotGeometry::VertexRange>& tripSplayVertexRanges)
{
    // With no keyword item model there are no items to reconcile:
    // setKeywordItemModel() tears everything down when the model goes away.
    if (Region == nullptr || m_keywordRegistry.model() == nullptr) {
        return;
    }

    // Built in lockstep in cwLinePlotGeometry::generate (one append each per
    // trip), so the running id indexes all three tables identically.
    Q_ASSERT(tripVertexRanges.size() == tripUuids.size());
    Q_ASSERT(tripSplayVertexRanges.size() == tripUuids.size());

    // Re-binds an item's proxy to its current vertex span (they shift each
    // solve), then re-hides a keyword-hidden span and re-colors it. setGeometry
    // just reset every vertex to visible with color value 0.
    const auto retarget = [this](cwKeywordItem* item,
                                 cwLinePlotGeometry::VertexRange range) {
        if (item == nullptr) {
            return;
        }
        auto* visibility = qobject_cast<cwLinePlotTripVisibility*>(item->object());
        if (visibility) {
            visibility->setTarget(m_linePlot, range);
            visibility->pushToTarget();
        }
    };

    using TripKeywordKey = QPair<cwTrip*, TripKeywordKind>;

    QSet<cwTrip*> present;
    QList<TripKeywordKey> keys;
    QList<cwLinePlotGeometry::VertexRange> ranges;
    keys.reserve(2 * tripUuids.size());
    ranges.reserve(2 * tripUuids.size());
    for (int i = 0; i < tripUuids.size(); ++i) {
        cwTrip* trip = liveByUuid.value(tripUuids.at(i), nullptr);
        if (trip == nullptr) {
            continue; // trip deleted mid-solve — skip, no dangling deref
        }
        present.insert(trip);

        // The splays ride at the tail of the trip's contiguous span, so the
        // centerline is the prefix before them. The two keyword items address
        // these disjoint ranges, which keeps their toggles independent: hiding
        // Type="Splays" leaves the centerline alone and vice versa.
        const cwLinePlotGeometry::VertexRange fullRange = tripVertexRanges.at(i);
        const cwLinePlotGeometry::VertexRange splayRange = tripSplayVertexRanges.at(i);
        const cwLinePlotGeometry::VertexRange centerlineRange {
            fullRange.start, splayRange.start - fullRange.start};

        if (!m_trackedTrips.contains(trip)) {
            m_trackedTrips.insert(trip);

            // Prompt cleanup if the trip is destroyed before the next solve.
            connect(trip, &QObject::destroyed, this, [this, trip]() {
                removeTripKeywordItems(trip);
            });
        }

        keys.append({trip, TripKeywordKind::Centerline});
        ranges.append(centerlineRange);

        // The splays item exists only while the trip has splay geometry, so
        // an empty filter panel stays free of a "Splays" type that matches
        // nothing.
        if (splayRange.count > 0) {
            keys.append({trip, TripKeywordKind::Splays});
            ranges.append(splayRange);
        } else {
            m_keywordRegistry.drop({trip, TripKeywordKind::Splays});
        }
    }

    // One registry call, so a solve's new items land in the model as one insert.
    const QList<cwKeywordItem*> items = m_keywordRegistry.ensureAll(
        keys, [this](const TripKeywordKey& key) {
            cwTrip* trip = key.first;
            cwKeywordModel* keywordModel = key.second == TripKeywordKind::Splays
                ? trip->splaysKeywordModel()
                : trip->linePlotKeywordModel();
            return makeTripKeywordItem(trip, keywordModel);
        });

    for (int i = 0; i < items.size(); ++i) {
        retarget(items.at(i), ranges.at(i));
    }

    // Drop items for trips that are no longer in the solved geometry.
    const QSet<cwTrip*> tracked = m_trackedTrips;
    for (cwTrip* trip : tracked) {
        if (!present.contains(trip)) {
            removeTripKeywordItems(trip);
        }
    }
}

cwKeywordItem* cwLinePlotManager::makeTripKeywordItem(cwTrip* trip,
                                                      cwKeywordModel* keywordModel)
{
    auto item = new cwKeywordItem();
    // References a trip-owned identity model (Type="Line Plot" or
    // Type="Splays" plus the trip's inherited Trip/Year/Date/Cave/Caver
    // keywords), so filtering the Type keyword toggles the item's whole vertex
    // range. The Type lives on that dedicated model, not trip->keywordModel(),
    // so scraps/notes under the trip don't inherit it. The station labels'
    // keyword item references the same line-plot model.
    item->keywordModel()->addExtension(keywordModel);

    auto visibility = new cwLinePlotTripVisibility(trip, item);
    item->setObject(visibility);

    // The registry's ensure() adds the item to the model after this factory
    // returns; addItem fires resolveVisibility → proxy setVisible, and the
    // caller's retarget seeds the render object.
    return item;
}

void cwLinePlotManager::removeTripKeywordItems(cwTrip* trip)
{
    if (!m_trackedTrips.remove(trip)) {
        return;
    }

    m_keywordRegistry.drop({trip, TripKeywordKind::Centerline});
    m_keywordRegistry.drop({trip, TripKeywordKind::Splays});

    // Drop the destroyed() connection added when the trip was first tracked;
    // otherwise a trip that leaves and re-enters the solved geometry
    // accumulates a duplicate connection on every cycle. (Lambda connections
    // can't use Qt::UniqueConnection, so disconnect explicitly.)
    disconnect(trip, &QObject::destroyed, this, nullptr);
}

QHash<QUuid, QList<cwSolvedEntrance>> cwLinePlotManager::solvedEntrances() const
{
    return Region != nullptr ? Region->solvedEntrances() : QHash<QUuid, QList<cwSolvedEntrance>>();
}

QList<cwSolvedEntrance> cwLinePlotManager::solvedEntrances(const QUuid& nodeId) const
{
    return solvedEntrances().value(nodeId);
}

bool cwLinePlotManager::isSolvedEntrance(const QUuid& nodeId, const QString& stationName) const
{
    const QList<cwSolvedEntrance> entrances = solvedEntrances(nodeId);
    const QString name = stationName.trimmed();
    return std::any_of(entrances.cbegin(), entrances.cend(),
                       [&name](const cwSolvedEntrance& entrance) {
                           return entrance.stationName.compare(name, Qt::CaseInsensitive) == 0;
                       });
}

/**
 * @brief cwLinePlotManager::waitToFinish
 *
 * Will cause the LinePlotManager to block until the underlying task is finished. This is useful
 * for unit testing.
 */
void cwLinePlotManager::waitToFinish()
{
    // The scan's apply continuation can chain into a solve via solveNeeded
    // (and a solve never chains back into a scan), so draining
    // scan-then-solve settles both pipelines. waitForFinished pumps queued
    // continuations before returning, which is what lets the apply's
    // chained runSurvex land in between.
    m_externalCenterlineManager->waitToFinish();
    AsyncFuture::waitForFinished(currentRun());
    AsyncFuture::waitForFinished(m_restarter.future());
}

/**
  \brief Re-solves when the region's station positions are stale
  */
void cwLinePlotManager::rerunIfStale(cwCavingRegion* region) {
    if(region->isStationPositionsStale()) {
        runSurvex();
    }
}

/**
 * @brief cwLinePlotManager::updateUnconnectedChunkErrors
 *
 * This will clear all the survey chunk errors and add survey chunk error's that exist. Currently
 * the only errors that is added to the whole survey chunk, are unconnected survey chunk error.
 */
void cwLinePlotManager::updateUnconnectedChunkErrors(cwSurveyNode* node,
                                                     const cwLinePlotTask::LinePlotCaveData& nodeData)
{

    //Append unconnected errors
    if(nodeData.unconnectedChunkError().size() > 0) {
        for (const auto& errorResult : nodeData.unconnectedChunkError()) {
            cwErrorModel* model = node->trip(errorResult.TripIndex)->chunk(errorResult.SurveyChunkIndex)->errorModel();
            model->errors()->append(errorResult.Error);
            UnconnectedChunks.append(model->errors());
        }
    }
}

/**
 * @brief cwLinePlotManager::clearUnconnectedChunkErrors
 *
 * This goes throught all clears all the connected cwSurveyChunk
 */
void cwLinePlotManager::clearUnconnectedChunkErrors()
{
    foreach(auto errorList, UnconnectedChunks) {
        if(errorList) {
            errorList->clear();
        }
    }
    UnconnectedChunks.clear();
}

void cwLinePlotManager::markNeedsUpdate() {
    if(!m_needsUpdate) {
        m_needsUpdate = true;
        // Clean -> Dirty, or (mid-solve edit) Working -> Dirty. Either is a real
        // state change; whoever is driving runs the pipeline again on the Dirty.
        emit updateStateChanged();
    }
}

/**
  \brief The survey-edit slot: marks the line plot dirty, and solves on the spot
  while standalone.

  Marking is all this does once a coordinator has taken over — whether the solve
  runs now or waits is that coordinator's call.
  */
void cwLinePlotManager::runSurvex() {
    markNeedsUpdate();
    runIfStandalone();
}

/**
  \brief Runs the line plot task now, unconditionally.
  */
QFuture<void> cwLinePlotManager::doRun() {
    // Enter Working and drop the pending-dirty marker, so the pipeline reports
    // Working (not Dirty) while the solve waits for a pending scan and while
    // it runs. That keeps a caller waiting on the returned future from
    // mistaking the pipeline for "finished". The solve snapshots the region
    // later, in startSolveAfterScan(), and clears the marker again there, so
    // an edit that lands before the snapshot rides the same solve.
    const cwUpdatable::State previousState = updateState();
    m_needsUpdate = false;
    const QFuture<void> solve = beginRun();
    if(updateState() != previousState) {
        emit updateStateChanged();
    }

    if(Region != nullptr) {
        // Skip the pipeline only when nothing solvable exists. Native
        // chunks contribute shots; cave/trip-level external attachments
        // contribute *include data that we cannot see without running
        // cavern, so an empty native-chunk set with any external
        // attachment still needs a solve to surface stations or errors.
        // Named lambda returns on the first hit so the three nested
        // loops escape together without a flag-per-level check (per
        // CLAUDE.md "Never use goto").
        const auto hasAnySolvableInput = [this]() {
            for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
                if (!node->externalCenterline().isEmpty()) {
                    return true;
                }
                for (cwTrip* trip : node->trips()) {
                    if (!trip->externalCenterline().isEmpty()) {
                        return true;
                    }
                    for (cwSurveyChunk* chunk : trip->chunks()) {
                        if (chunk->shotCount() > 0) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        if(!hasAnySolvableInput()) {
            publishAttachedFixWarnings({});
            // No-shots path must also clear the cached cavern output / solve
            // error so CavernOutputPage doesn't keep showing the previous
            // run's text (D-1). No async work, so the solve is already done.
            publishResults(cwLinePlotTask::LinePlotResultData::cleared());
            updateLinePlot(cwLinePlotTask::LinePlotResultData());
            finishSolving();
            return solve;
        }

        Region->setStationPositionsStale(true);
        scheduleSolve();
    } else {
        // No region: nothing to solve.
        finishSolving();
    }

    return solve;
}

void cwLinePlotManager::scheduleSolve()
{
    if (m_solveScheduled) {
        return;
    }
    m_solveScheduled = true;

    // Queued so the scan an attach, a load, or setRegion starts later in the
    // same call chain is already pending when startSolveAfterScan() looks.
    QMetaObject::invokeMethod(this, &cwLinePlotManager::startSolveAfterScan, Qt::QueuedConnection);
}

void cwLinePlotManager::startSolveAfterScan()
{
    // A scan fills the attachment dirs, declination flags, and fixed
    // stations solveInputs() hands the driver, so a solve started now would
    // publish a plot the scan's own solve request immediately replaces.
    // A canceled scan reports false too, so the wait always ends.
    if (m_externalCenterlineManager->isScanning()) {
        connect(m_externalCenterlineManager, &cwExternalCenterlineManager::scanningChanged,
                this, &cwLinePlotManager::startSolveAfterScan, Qt::SingleShotConnection);
        return;
    }
    m_solveScheduled = false;

    // doRun()'s empty-region path ended the run this start belonged to, so
    // no solve is owed.
    if (!isRunning()) {
        return;
    }

    if (Region.isNull()) {
        finishSolving();
        return;
    }

    m_restarter.restart([this]() {
        if (Region.isNull()) {
            finishSolving();
            return QFuture<cwLinePlotTask::LinePlotResultData>();
        }

        // The snapshot below covers every edit and solve request made so
        // far, including those since doRun() entered Working.
        if (m_needsUpdate) {
            m_needsUpdate = false;
            emit updateStateChanged();
        }

        cwProfilePhase solvePhase(lcProfileLoad(), QStringLiteral("project"), QStringLiteral("solveKick"));

        const auto externalInputs = m_externalCenterlineManager->solveInputs();
        publishAttachedFixWarnings(externalInputs);
        auto input = cwLinePlotTask::buildInput(Region.data(), externalInputs);
        m_memberViewsMovedSinceSnapshot = false;
        cwTaskContextScope traceScope(QStringLiteral("lineplot"));
        auto worker = cwLinePlotTask::run(std::move(input));
        m_solveWorker = worker;
        solvePhase.finish();

        // The restarter tracks the drain, not the worker, so the job lasts
        // until the labels are installed. A restart cancels the drain, and the
        // worker has to hear that to stop cavern early. The drain completes
        // without a value; the result travels through installSolve().
        AsyncFuture::Deferred<cwLinePlotTask::LinePlotResultData> drain;
        AsyncFuture::observe(drain.future())
            .context(this, []() {}, [worker]() mutable { worker.cancel(); });
        AsyncFuture::observe(worker)
            .context(this,
                     [this, drain](cwLinePlotTask::LinePlotResultData result) {
                         installSolve(drain, std::move(result));
                     },
                     [drain]() mutable { drain.cancel(); });

        return drain.future();
    });
}

void cwLinePlotManager::installSolve(AsyncFuture::Deferred<cwLinePlotTask::LinePlotResultData> drain,
                                     cwLinePlotTask::LinePlotResultData result)
{
    if (drain.future().isCanceled()) {
        return;
    }

    // publishResults updates CavernOutputPage state (log/stats/error +
    // per-chunk markers) on every path; updateLinePlot only runs on the
    // success path so an error doesn't wipe the last good line plot. Station
    // positions and cave length/depth therefore keep their last good values
    // while a solve error stands — the long-standing contract for native
    // trips, which external owners share.
    publishResults(result);
    if (result.hasSolveError()) {
        const auto& error = result.solveError();
        qWarning() << "Line plot solve failed at step"
                   << solveErrorStepName(error.step)
                   << "exit code" << error.exitCode << ":" << error.message;
    } else {
        m_externalCenterlineManager->markSolved(QDateTime::currentDateTime());
        updateLinePlot(std::move(result));
    }

    m_labelDrain = drain;
    const qsizetype pending = m_pendingLabels.size() + m_pendingLiveLabels.size();
    const int batches = static_cast<int>((pending + kLabelBatchSize - 1) / kLabelBatchSize);
    m_labelDrain.setProgressRange(0, batches);
    if (batches == 0) {
        completeLabelDrain();
    } else {
        queueLabelBatch();
    }
}

void cwLinePlotManager::queueLabelBatch()
{
    if (m_labelDrainPending) {
        return;
    }
    m_labelDrainPending = true;
    QMetaObject::invokeMethod(this, &cwLinePlotManager::drainLabelBatch, Qt::QueuedConnection);
}

void cwLinePlotManager::drainLabelBatch()
{
    m_labelDrainPending = false;

    // A restart canceled this solve, and its successor takes the pending
    // labels; or a solve with nothing to drain already finished the job.
    if (m_labelDrain.future().isCanceled() || m_labelDrain.future().isFinished()) {
        return;
    }

    if (Region.isNull() || (m_pendingLabels.isEmpty() && m_pendingLiveLabels.isEmpty())) {
        completeLabelDrain();
        return;
    }

    cwCavingRegion::TripSolvedStations batch;
    batch.reserve(kLabelBatchSize);
    for (auto it = m_pendingLabels.begin();
         it != m_pendingLabels.end() && batch.size() < kLabelBatchSize;
         it = m_pendingLabels.erase(it)) {
        batch.insert(it.key(), std::move(it.value()));
    }
    while (batch.size() < kLabelBatchSize && !m_pendingLiveLabels.isEmpty()) {
        const QPointer<cwTrip> trip = m_pendingLiveLabels.takeLast();
        if (trip) {
            batch.insert(trip->id(), trip->solvedStations());
        }
    }

    const bool isLastBatch = m_pendingLabels.isEmpty() && m_pendingLiveLabels.isEmpty();
    Region->publishSolvedStations(batch, isLastBatch);
    m_labelDrain.setProgressValue(m_labelDrain.future().progressValue() + 1);

    if (isLastBatch) {
        completeLabelDrain();
    } else {
        queueLabelBatch();
    }
}

void cwLinePlotManager::completeLabelDrain()
{
    m_labelDrain.complete();
    // An edit during this solve scheduled the next one, which ends the run
    // instead.
    if (!m_solveScheduled) {
        finishSolving();
    }
}

void cwLinePlotManager::handleMemberViewsChanged()
{
    m_memberViewsMovedSinceSnapshot = true;
    m_pendingLabels.clear();
    m_pendingLiveLabels.clear();
}

void cwLinePlotManager::finishSolving() {
    if(isRunning()) {
        // Working -> Clean (or -> Dirty if a survey edit arrived mid-solve).
        // endRun() finishes the future run() handed out, releasing whoever is
        // waiting on the solve; the signal is the coordinator's cue to re-check
        // its staleness aggregate.
        endRun();
        emit updateStateChanged();
    }
}

/**
  \brief Updates the line plot, and all the station positions for the
  line region
  */
void cwLinePlotManager::publishResults(const cwLinePlotTask::LinePlotResultData& results)
{
    // Single funnel for all manager-side state derived from a pipeline result:
    // cavern log / loop-closure stats / solve-error state (cavernOutputChanged
    // Q_PROPERTYs) and per-chunk error markers. Called from both the async
    // solve callback and the synchronous no-shots / setRegion paths.
    publishCavernOutput(results.CavernLog,
                        results.LoopClosureStats,
                        results.DriverSource,
                        results.hasSolveError()
                            ? std::optional<cwLinePlotTask::SolveError>(results.solveError())
                            : std::nullopt,
                        results.SolveDurationSeconds,
                        results.regionNetwork().stationCount(),
                        results.CavernWarningCount);
    publishPerCaveErrors(results);
    publishFloatingSurveys(results.FloatingSurveys, results.ExternalScopesChecked);
    if (results.ExternalScopesChecked) {
        m_hangingStations = results.Hanging;
    }
    if (results.OriginFixedOwners.has_value()) {
        publishUnfixedFileWarnings(*results.OriginFixedOwners);
    }
    if (results.SolvedEntrances.has_value()) {
        publishSolvedEntrances(*results.SolvedEntrances, results.PlacedEntrances,
                               !results.DriverSource.isEmpty());
    }
    publishUnconnectedStationWarnings();
}

void cwLinePlotManager::publishSolvedEntrances(const QHash<QUuid, QList<cwSolvedEntrance>>& entrances,
                                               const QHash<QUuid, QSet<QString>>& placedEntrances,
                                               bool solved)
{
    if (Region == nullptr) {
        return;
    }
    Region->setSolvedEntrances(entrances);

    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        if (node->errorModel() == nullptr) {
            continue;
        }
        QStringList unplaced;
        if (solved) {
            const QSet<QString> placed = placedEntrances.value(node->id());
            for (const cwEntrance& entrance : node->entrances()->entrances()) {
                const QString station = entrance.stationName().trimmed();
                // A fix on the same station already warns through
                // FixStationReference.
                if (!placed.contains(cwStation::canonicalKey(station)) && !node->fixStations()->isFixed(station)) {
                    unplaced.append(QStringLiteral("\"%1\"").arg(station));
                }
            }
        }
        QString message;
        if (unplaced.size() == 1) {
            message = QStringLiteral("Entrance %1 is on a station the survey lacks.").arg(unplaced.first());
        } else if (unplaced.size() > 1) {
            message = QStringLiteral("Entrances %1 are on stations the survey lacks.")
                          .arg(unplaced.join(QStringLiteral(", ")));
        }
        node->errorModel()->errors()->setTypedWarning(cwErrorTypeId::EntranceUnplaced, message);
    }
}

/**
 * @brief cwLinePlotManager::publishFloatingSurveys
 *
 * Replaces the floating-survey list wholesale — it describes one run, so a
 * survey that stopped floating is a survey the new run simply doesn't mention.
 * Silent when the answer is unchanged, so a solve triggered by unrelated cave
 * data doesn't re-pulse the banner.
 *
 * The exception is \a externalScopesChecked: a run that stopped before the
 * post-solve pass carries no answer for the external half rather than an empty
 * one, so the previous run's external records are carried forward instead of
 * being replaced by silence. Otherwise an unconnected native chunk in one cave
 * — which stops the solve region-wide — would blank a still-true floating
 * record for a different cave's attachment, and the banner would flicker off
 * until the next clean solve.
 */
void cwLinePlotManager::publishFloatingSurveys(QList<cwFindFloatingSurveys::Result> floatingSurveys,
                                               bool externalScopesChecked)
{
    if (!externalScopesChecked) {
        for (const cwFindFloatingSurveys::Result& previous : std::as_const(m_floatingSurveys)) {
            if (previous.trigger == cwFindFloatingSurveys::Result::Trigger::ExternalScope) {
                floatingSurveys.append(previous);
            }
        }
    }

    if (floatingSurveys == m_floatingSurveys) {
        return;
    }
    m_floatingSurveys = std::move(floatingSurveys);
    m_floatingSurveyModel->setResults(m_floatingSurveys);
    emit floatingSurveysChanged();
}

void cwLinePlotManager::publishUnconnectedStationWarnings()
{
    if (Region == nullptr) {
        return;
    }

    QHash<QUuid, QStringList> stationsByOwner;
    for (const auto& hanging : std::as_const(m_hangingStations)) {
        stationsByOwner[hanging.ownerId].append(hanging.stations);
    }

    // A floating record names its stations cave-locally; the trip's own scope
    // comes off so every owner lists its stations in its own namespace.
    QHash<QUuid, const cwTrip*> tripsById;
    for (const cwTrip* trip : Region->rootNode()->allTrips()) {
        tripsById.insert(trip->id(), trip);
    }
    for (const cwFindFloatingSurveys::Result& floating : std::as_const(m_floatingSurveys)) {
        const cwTrip* trip = tripsById.value(floating.tripId);
        if (trip == nullptr) {
            continue;
        }
        const QString scope = cwStation::canonicalKey(trip->scopePrefix());
        QStringList& stations = stationsByOwner[floating.tripId];
        for (const QString& station : floating.stations) {
            stations.append(station.startsWith(scope) ? station.sliced(scope.size()) : station);
        }
    }

    const auto publish = [&stationsByOwner](cwErrorModel* model, const QUuid& ownerId,
                                            const QString& ownerName) {
        if (model == nullptr) {
            return;
        }
        QSet<QString> uniqueStations;
        for (const QString& station : stationsByOwner.value(ownerId)) {
            uniqueStations.insert(cwStation::canonicalKey(station));
        }
        QStringList stations(uniqueStations.cbegin(), uniqueStations.cend());
        std::sort(stations.begin(), stations.end(), cwNameUtils::naturalLess);

        QString message;
        if (stations.size() == 1) {
            message = QStringLiteral("1 station in %1 is not tied to the cave").arg(ownerName);
        } else if (stations.size() > 1) {
            message = QStringLiteral("%1 stations in %2 are not tied to the cave")
                          .arg(QString::number(stations.size()), ownerName);
        }
        model->errors()->setTypedWarning(cwErrorTypeId::UnconnectedStations, message,
                                         stations.join(QStringLiteral(", ")), ownerId);
    };

    const auto displayName = [](const cwExternalCenterline& centerline, const QString& name) {
        return centerline.isEmpty() ? name : QFileInfo(centerline.entryFile()).fileName();
    };

    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        publish(node->errorModel(), node->id(), displayName(node->externalCenterline(), node->name()));
        for (cwTrip* trip : node->trips()) {
            publish(trip->errorModel(), trip->id(), displayName(trip->externalCenterline(), trip->name()));
        }
    }
}

QString cwLinePlotManager::solveErrorStep() const
{
    return m_lastSolveError ? QString::fromLatin1(solveErrorStepName(m_lastSolveError->step))
                            : QString();
}

QString cwLinePlotManager::solveErrorSummary() const
{
    return m_lastSolveError ? cwCavernRunner::firstErrorLine(m_lastSolveError->message)
                            : QString();
}

void cwLinePlotManager::publishCavernOutput(QString cavernLog,
                                            QString loopClosureStats,
                                            QString driverSource,
                                            std::optional<cwLinePlotTask::SolveError> solveError,
                                            double solveDuration,
                                            int stationCount,
                                            int warningCount)
{
    bool changed = false;
    if (cavernLog != m_lastCavernLog
        || loopClosureStats != m_lastLoopClosureStats
        || driverSource != m_lastDriverSource) {
        m_lastCavernLog = std::move(cavernLog);
        m_lastLoopClosureStats = std::move(loopClosureStats);
        m_lastDriverSource = std::move(driverSource);
        changed = true;
    }
    if (solveDuration != m_lastSolveDuration
        || stationCount != m_lastStationCount
        || warningCount != m_lastWarningCount) {
        m_lastSolveDuration = solveDuration;
        m_lastStationCount = stationCount;
        m_lastWarningCount = warningCount;
        changed = true;
    }
    // SolveError has no operator==; compare by presence + message+step+exitCode.
    const bool errorChanged =
        solveError.has_value() != m_lastSolveError.has_value()
        || (solveError.has_value()
            && (solveError->message != m_lastSolveError->message
                || solveError->step != m_lastSolveError->step
                || solveError->exitCode != m_lastSolveError->exitCode));
    if (errorChanged) {
        m_lastSolveError = std::move(solveError);
        changed = true;
    }
    if (changed) {
        emit cavernOutputChanged();
    }
}

void cwLinePlotManager::publishPerCaveErrors(const cwLinePlotTask::LinePlotResultData& results)
{
    if (Region == nullptr) {
        return;
    }
    // Clear stale entries from the previous run before re-publishing the
    // current set; the unconnected-chunk error list is per-pipeline-run.
    clearUnconnectedChunkErrors();
    // Walk the live nodes and resolve each by id() to the worker's UUID-keyed
    // result, skipping any node deleted while the solve was running.
    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        const auto it = results.Caves.constFind(node->id());
        if (it == results.Caves.constEnd()) {
            continue;
        }
        updateUnconnectedChunkErrors(node, it.value());
    }
}

void cwLinePlotManager::publishAttachedFixWarnings(const cwLinePlotTask::ExternalCenterlineInputs& inputs)
{
    if (Region == nullptr) {
        return;
    }

    // Any output system, a local grid's included, places a file whose fixes
    // name no system at the origin.
    const bool placesAtOrigin =
        Region->geoReference()->state() != cwGeoReference::Ungeoreferenced;

    // Names each station the way the node's Fix Stations table lists it: a
    // trip's file stations sit under the trip's scopePrefix().
    const auto addWarning = [&](QStringList& messages, const QUuid& ownerId, const QString& scope,
                                const cwExternalCenterline& centerline) {
        // A file whose own *cs out reads its bare fixes in has a system for
        // them; only a file the driver translates (one in
        // externalBareFixOrigins) is warned about.
        if (!centerline.fixCoordinateSystem().isEmpty()
                || inputs.externalOutputSystems.contains(ownerId)
                || !inputs.externalBareFixOrigins.contains(ownerId)) {
            return;
        }
        const auto stations = inputs.bareFixedStations.constFind(ownerId);
        if (stations == inputs.bareFixedStations.constEnd()) {
            return;
        }
        QStringList scoped;
        scoped.reserve(stations.value().size());
        for (const QString& station : stations.value()) {
            scoped.append(scope + station);
        }
        messages.append(QStringLiteral("%1 fixes %2 without a coordinate system, so it is placed "
                                       "at the project origin until a coordinate system is "
                                       "chosen; choose one on the Fix Stations page or add *cs "
                                       "to the file.")
                            .arg(entryFileName(centerline), scoped.join(QStringLiteral(", "))));
    };

    // The default datum places the file's fixes whether or not the project
    // has a frame, so this one is said either way.
    const auto addDefaultDatumMessages = [&inputs](QStringList& messages, const QUuid& ownerId) {
        for (const auto& defaulted : inputs.defaultedDatums.value(ownerId)) {
            messages.append(QStringLiteral("%1 names UTM zone %2 but no datum; CaveWhere used "
                                           "Compass's default, North American 1927 — add a & "
                                           "line to the file to choose another.")
                                .arg(defaulted.file, defaulted.zone));
        }
    };

    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        QStringList messages;
        if (placesAtOrigin) {
            addWarning(messages, node->id(), QString(), node->externalCenterline());
            for (const cwTrip* trip : node->trips()) {
                addWarning(messages, trip->id(), trip->scopePrefix(), trip->externalCenterline());
            }
        }

        QStringList defaultDatumMessages;
        addDefaultDatumMessages(defaultDatumMessages, node->id());
        for (const cwTrip* trip : node->trips()) {
            addDefaultDatumMessages(defaultDatumMessages, trip->id());
        }

        if (node->errorModel() != nullptr) {
            // cwNodeWarningModel opens the node's Fix Stations page for these
            // types, where the file's own fixes are listed.
            cwErrorListModel* errors = node->errorModel()->errors();
            errors->setTypedWarning(cwErrorTypeId::AttachedFileFixWithoutCS,
                                    messages.join(QLatin1Char('\n')));
            errors->setTypedWarning(cwErrorTypeId::AttachedFileDefaultDatum,
                                    defaultDatumMessages.join(QLatin1Char('\n')));
        }
    }
}

void cwLinePlotManager::publishUnfixedFileWarnings(const QSet<QUuid>& originFixedOwners)
{
    if (Region == nullptr) {
        return;
    }

    const bool georeferenced = Region->geoReference()->hasCoordinateSystem();
    const auto unfixedMessage = [](const cwExternalCenterline& centerline) {
        return QStringLiteral("%1 has no fixed station, so it sits at the origin — fix one of "
                              "its stations.")
            .arg(entryFileName(centerline));
    };

    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        if (node->errorModel() == nullptr) {
            continue;
        }

        QStringList messages;
        if (georeferenced) {
            if (!node->externalCenterline().isEmpty() && originFixedOwners.contains(node->id())) {
                messages.append(unfixedMessage(node->externalCenterline()));
            }
            for (const cwTrip* trip : node->trips()) {
                if (originFixedOwners.contains(trip->id())) {
                    messages.append(unfixedMessage(trip->externalCenterline()));
                }
            }
        }
        node->errorModel()->errors()->setTypedWarning(cwErrorTypeId::AttachedFileUnfixed,
                                                      messages.join(QLatin1Char('\n')));
    }
}

void cwLinePlotManager::updateLinePlot(cwLinePlotTask::LinePlotResultData results) {

    if(Region == nullptr) { return; }

    // (m_lastSolveError, m_lastCavernLog, m_lastLoopClosureStats, and
    // per-chunk error markers are all published by publishResults() before
    // we get here. This function is only responsible for applying the
    // computed geometry / station positions / depth-length to the live caves.)

    // Resolve the worker's UUID-keyed result back to live objects, dropping
    // any cave/trip/scrap deleted before the task finished.
    const ResolvedResults resolved = resolveResultsToLive(Region, results);

    //One lookup and one network for the whole region; every node reads its
    //own scoped view of them.
    if(results.hasRegionStationPositionsChanged()) {
        if(m_memberViewsMovedSinceSnapshot) {
            // The worker's views and labels came from member sets that have
            // since moved: install the positions alone, so every view
            // recomputes from the live member sets, and read each trip's
            // labels from the live tree as its batch drains.
            Region->setStationPositions(results.regionStationPositions());
            m_pendingLabels.clear();
            const QList<cwTrip*> trips = Region->rootNode()->allTrips();
            m_pendingLiveLabels = QList<QPointer<cwTrip>>(trips.cbegin(), trips.cend());
        } else {
            Region->setStationPositions(results.regionStationPositions(), results.NodePositions);
            m_pendingLabels = std::move(results.TripSolvedStations);
            m_pendingLiveLabels.clear();
        }
    }
    if(results.hasRegionNetworkChanged()) {
        Region->setNetwork(results.regionNetwork());
    }

    //Update the length and depth of every node the solve measured
    for(const auto& [node, nodeData] : resolved.nodes.asKeyValueRange()) {
        if(nodeData.hasDepthLengthChanged()) {
            //Update the node's depth and length
            double length = cwUnits::convert(nodeData.length(), cwUnits::Meters, (cwUnits::LengthUnit)node->length()->unit());
            double depth = cwUnits::convert(nodeData.depth(), cwUnits::Meters, (cwUnits::LengthUnit)node->depth()->unit());

            node->length()->setValue(length);
            node->depth()->setValue(depth);
        }
    }

    //Update the 3D plot
    if(m_linePlot != nullptr) {
        m_linePlot->setGeometry(results.stationPositions(),
                                results.tripSplayVertexRanges());
        m_linePlot->setSegmentErrors(results.segmentErrors());
    }

    // Resolve UUIDs to live trips by identity (never by list position).
    QHash<QUuid, cwTrip*> liveByUuid;
    for (cwTrip* trip : Region->rootNode()->allTrips()) {
        liveByUuid.insert(trip->id(), trip);
    }

    const QVector<QUuid> tripUuids = results.tripUuids();
    const QVector<double> tripLengths = results.tripLengths();
    Q_ASSERT(tripLengths.size() == tripUuids.size());
    for (int i = 0; i < tripUuids.size(); ++i) {
        if (cwTrip* trip = liveByUuid.value(tripUuids.at(i), nullptr)) {
            trip->setSolvedLength(tripLengths.at(i));
        }
    }

    // Re-attach per-trip keyword items (centerline + splays) to the new
    // geometry and re-seed each item's visibility by its (shifted) vertex
    // span. setGeometry reset the render object to all-visible, so reconcile
    // pushes the spans that keyword filtering hides.
    reconcileTripKeywordItems(liveByUuid,
                              tripUuids,
                              results.tripVertexRanges(),
                              results.tripSplayVertexRanges());

    // Skip publication when the network hasn't changed so 2D-geometry rules
    // don't rebuild on every line-plot completion triggered by unrelated
    // cave data (labels, calibration, etc.). Note this compares topology only —
    // a positions-only re-solve lands here equal, which is why anything that
    // cares about where a station moved watches the region's lookup instead.
    const cwSurveyNetwork newNetwork = Region->network();
    if (newNetwork != m_lastPublishedNetwork) {
        m_lastPublishedNetwork = newNetwork;
        m_surveyNetworkSource->setSurveyNetwork(
            QtFuture::makeReadyValueFuture(Monad::Result<cwSurveyNetwork>(newNetwork)));
    }

    Region->setStationPositionsStale(false);

    emit stationPositionInCavesChanged(resolved.nodes.keys());
    emit stationPositionInTripsChanged(cw::toList(resolved.trips));
    emit stationPositionInScrapsChanged(cw::toList(resolved.scraps));
}


