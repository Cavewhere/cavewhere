/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinePlotManager.h"
#include "cwRestarterTracking.h"
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
#include "cwEquateModel.h"
#include "cwFixStationModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLinePlotTripVisibility.h"
#include "cwNameUtils.h"
#include "cwStation.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterUtils.h"
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

using ExternalInputs = cwLinePlotTask::ExternalCenterlineInputs;

QSet<QString> nativeStationKeys(const cwSurveyNode* node)
{
    QSet<QString> keys;
    for (const cwTrip* trip : node->trips()) {
        for (const cwSurveyChunk* chunk : trip->chunks()) {
            for (int i = 0; i < chunk->stationCount(); ++i) {
                const cwStation station = chunk->station(i);
                if (station.isValid()) {
                    keys.insert(cwStation::canonicalKey(station.name()));
                }
            }
        }
    }
    return keys;
}

//! An attachment the solve reads that the driver fixes at the origin when
//! nothing else anchors it.
bool isUnfixedAttachment(const QUuid& ownerId, const cwExternalCenterline& centerline,
                         const QStringList& stations, const ExternalInputs& inputs)
{
    return !inputs.excludedExternalOwners.contains(ownerId)
           && !cwSurvexExporterCaveTask::originStation(
                   centerline, stations, inputs.externalFixedStations.value(ownerId))
                   .isEmpty();
}

//! Whether the driver keeps one of \a node's own fixes, by the rules of
//! cwSurvexExporterCaveTask::writeFixStations: the fix validates against the
//! stations the node's block knows, and names none an attached file fixes.
bool keepsAFix(const cwSurveyNode* node, const QSet<QString>& nativeKeys,
               const ExternalInputs& inputs)
{
    const QList<cwFixStation>& fixes = node->fixStations()->fixStations();
    if (fixes.isEmpty()) {
        return false;
    }

    QSet<QString> knownKeys = nativeKeys;
    QSet<QString> fileFixedKeys;
    const auto addFile = [&](const QUuid& ownerId, const QString& scope, const QStringList& stations) {
        knownKeys.unite(cwSurvexExporterUtils::scopedStationKeys(scope, stations));
        fileFixedKeys.unite(cwSurvexExporterUtils::scopedStationKeys(
            scope, inputs.externalFixedStations.value(ownerId)));
    };
    for (const cwTrip* trip : node->trips()) {
        if (!trip->externalCenterline().isEmpty()
            && !inputs.excludedExternalOwners.contains(trip->id())) {
            addFile(trip->id(), trip->scopePrefix(), trip->externalStations());
        }
    }
    addFile(node->id(), QString(), node->externalStations());

    QStringList errors;
    const QList<cwFixStation> valid =
        cwSurvexExporterUtils::validateFixStations(fixes, knownKeys, errors);
    return std::any_of(valid.cbegin(), valid.cend(), [&fileFixedKeys](const cwFixStation& fix) {
        return !fileFixedKeys.contains(cwStation::canonicalKey(fix.stationName().trimmed()));
    });
}

QString entryFileName(const cwExternalCenterline& centerline)
{
    return QFileInfo(centerline.entryFile()).fileName();
}

//! Per node, the attached files the driver fixes at the origin because they
//! fix nothing themselves: a sourced root, or a trip's file in a block, that
//! no enclosing block, node fix, or native station anchors
//! (cwSurvexExporterCaveTask::writeNodeBlock).
void collectFilesAtOrigin(const cwSurveyNode* node, bool anchoredAbove,
                          const ExternalInputs& inputs,
                          QHash<const cwSurveyNode*, QStringList>& files)
{
    const QSet<QString> nativeKeys = nativeStationKeys(node);
    const bool keepsOwnFix = keepsAFix(node, nativeKeys, inputs);
    if (!node->externalCenterline().isEmpty()) {
        if (!anchoredAbove && !keepsOwnFix
            && isUnfixedAttachment(node->id(), node->externalCenterline(),
                                   node->externalStations(), inputs)) {
            files[node].append(entryFileName(node->externalCenterline()));
        }
        return;
    }

    const bool anchored = anchoredAbove || keepsOwnFix || !nativeKeys.isEmpty();
    if (!anchored) {
        for (const cwTrip* trip : node->trips()) {
            if (isUnfixedAttachment(trip->id(), trip->externalCenterline(),
                                    trip->externalStations(), inputs)) {
                files[node].append(entryFileName(trip->externalCenterline()));
            }
        }
    }
    for (const cwSurveyNode* child : node->childNodes()) {
        collectFilesAtOrigin(child, anchored, inputs, files);
    }
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
    //half-destroyed manager. This line finishes the outer future; the cancel the
    //worker itself sees comes from ~Restarter, which cancels the inner future
    //synchronously as m_restarter is destroyed.
    m_restarter.future().cancel();

    // m_keywordRegistry's destructor tears down the keyword items
    // synchronously.
}

/**
  \brief Sets the region that this manager will listen to
  */
void cwLinePlotManager::setRegion(cwCavingRegion* region) {
    Region = region;

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

    if(Region == nullptr) {
        SurveySignaler->setRegion(nullptr);
        m_externalCenterlineManager->setRegion(nullptr);
        return;
    }

    // A trip or node added or removed at any depth changes what cavern reads,
    // and a node added anywhere brings fix stations to hook.
    connect(Region->rootNode(), &cwSurveyNode::subtreeChanged, this, [this]() {
        connectNodeInputs();
        runSurvex();
    });

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

    rerunIfAnyNodeIsStale(Region);

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
}

void cwLinePlotManager::connectEquates(cwEquateModel* equates) {
    if (!equates) { return; }
    connect(equates, &cwEquateModel::dataChanged,  this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(equates, &cwEquateModel::rowsInserted, this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(equates, &cwEquateModel::rowsRemoved,  this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
    connect(equates, &cwEquateModel::modelReset,   this, &cwLinePlotManager::runSurvex, Qt::UniqueConnection);
}

void cwLinePlotManager::connectNodeInputs() {
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

    // Resolve UUIDs to live trips by identity (never by list position).
    QHash<QUuid, cwTrip*> liveByUuid;
    for (cwTrip* trip : Region->rootNode()->allTrips()) {
        liveByUuid.insert(trip->id(), trip);
    }

    // Re-binds an item's proxy to its current vertex span (they shift each
    // solve) and re-hides a keyword-hidden span. setGeometry just reset every
    // vertex to visible, so a visible proxy has nothing to publish.
    const auto retarget = [this](cwKeywordItem* item,
                                 cwLinePlotGeometry::VertexRange range) {
        if (item == nullptr) {
            return;
        }
        auto* visibility = qobject_cast<cwLinePlotTripVisibility*>(item->object());
        if (visibility) {
            visibility->setTarget(m_linePlot, range);
            if (!visibility->isVisible()) {
                visibility->pushToTarget();
            }
        }
    };

    QSet<cwTrip*> present;
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

        cwKeywordItem* centerline = m_keywordRegistry.ensure(
            {trip, TripKeywordKind::Centerline}, [this, trip]() {
                return makeTripKeywordItem(trip, trip->linePlotKeywordModel());
            });

        // The splays item exists only while the trip has splay geometry, so
        // an empty filter panel stays free of a "Splays" type that matches
        // nothing.
        cwKeywordItem* splays = nullptr;
        if (splayRange.count > 0) {
            splays = m_keywordRegistry.ensure(
                {trip, TripKeywordKind::Splays}, [this, trip]() {
                    return makeTripKeywordItem(trip, trip->splaysKeywordModel());
                });
        } else {
            m_keywordRegistry.drop({trip, TripKeywordKind::Splays});
        }

        retarget(centerline, centerlineRange);
        retarget(splays, splayRange);
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
    AsyncFuture::waitForFinished(m_restarter.future());
}

/**
  \brief Re-solves when any node in the region holds a stale station lookup
  */
void cwLinePlotManager::rerunIfAnyNodeIsStale(cwCavingRegion* region) {
    const QList<cwSurveyNode*> nodes = region->rootNode()->allNodes();
    const bool anyNodeIsStale = std::any_of(nodes.cbegin(), nodes.cend(), [](const cwSurveyNode* node) {
        return node->isStationPositionLookupStale();
    });

    if(anyNodeIsStale) {
        runSurvex();
    }
}

/**
 * @brief cwLinePlotManager::markCaveStationsAsStale
 *
 * This will go through every node in the region and mark it as stale
 * This is useful, to make sure that the node data is up to date. If the user
 * closes cavewhere before the line plot is re-processed.
 */
void cwLinePlotManager::setCaveStationLookupAsStale(bool isStale)
{
    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        node->setStationPositionLookupStale(isStale);
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
    // Enter Working and drop the pending-dirty marker in one step: a solve now
    // covers the current data, so the pipeline is Working (not Dirty) until it
    // completes. Reporting Working — not the synchronously-cleared Dirty — is
    // what keeps a caller waiting on the returned future from mistaking the
    // pipeline for "finished" while the solve is still in flight.
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

        setCaveStationLookupAsStale(true);
        m_restarter.restart([this]() {
            if (Region.isNull()) {
                finishSolving();
                return QFuture<cwLinePlotTask::LinePlotResultData>();
            }

            const auto externalInputs = m_externalCenterlineManager->solveInputs();
            publishAttachedFixWarnings(externalInputs);
            auto input = cwLinePlotTask::buildInput(Region.data(), externalInputs);
            auto future = cwLinePlotTask::run(std::move(input));

            // Receive the worker's result by parameter rather than capturing
            // the future and calling future.result() — the parameter form
            // lets AsyncFuture deliver the value directly, no captured-future
            // ping-pong. publishResults updates CavernOutputPage state
            // (log/stats/error + per-chunk markers) on every path; updateLinePlot
            // only runs on the success path so an error doesn't wipe the last
            // good line plot. Station positions and cave length/depth therefore
            // keep their last good values while a solve error stands — the
            // long-standing contract for native trips, which external owners
            // share.
            AsyncFuture::observe(future)
                .context(this, [this](cwLinePlotTask::LinePlotResultData result) {
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
                    finishSolving();
                });

            return future;
        });
    } else {
        // No region: nothing to solve.
        finishSolving();
    }

    return solve;
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
    publishUnconnectedStationWarnings();
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

    const bool georeferenced = Region->geoReference()->hasCoordinateSystem();

    // Names each station the way the node's Fix Stations table lists it: a
    // trip's file stations sit under the trip's scopePrefix().
    const auto addWarning = [&](QStringList& messages, const QUuid& ownerId, const QString& scope,
                                const cwExternalCenterline& centerline) {
        const auto stations = inputs.bareFixedStations.constFind(ownerId);
        if (stations == inputs.bareFixedStations.constEnd()) {
            return;
        }
        QStringList scoped;
        scoped.reserve(stations.value().size());
        for (const QString& station : stations.value()) {
            scoped.append(scope + station);
        }
        messages.append(QStringLiteral("%1 fixes %2 without a coordinate system; add one to the "
                                       "file or remove that fix.")
                            .arg(entryFileName(centerline), scoped.join(QStringLiteral(", "))));
    };

    QHash<const cwSurveyNode*, QStringList> filesAtOrigin;
    if (georeferenced) {
        for (const cwSurveyNode* cave : Region->rootNode()->childNodes()) {
            collectFilesAtOrigin(cave, false, inputs, filesAtOrigin);
        }
    }

    for (cwSurveyNode* node : Region->rootNode()->allNodes()) {
        QStringList messages;
        if (georeferenced) {
            addWarning(messages, node->id(), QString(), node->externalCenterline());
            for (const cwTrip* trip : node->trips()) {
                addWarning(messages, trip->id(), trip->scopePrefix(), trip->externalCenterline());
            }
        }

        QStringList unfixedMessages;
        for (const QString& file : filesAtOrigin.value(node)) {
            unfixedMessages.append(QStringLiteral("%1 has no fixed station, so it sits at the "
                                                  "origin — fix one of its stations.")
                                       .arg(file));
        }

        if (node->errorModel() != nullptr) {
            // cwNodeWarningModel opens the node's Fix Stations page for both
            // types, where the file's own fixes are listed.
            cwErrorListModel* errors = node->errorModel()->errors();
            errors->setTypedWarning(cwErrorTypeId::AttachedFixWithoutCS,
                                    messages.join(QLatin1Char('\n')));
            errors->setTypedWarning(cwErrorTypeId::AttachedFileUnfixed,
                                    unfixedMessages.join(QLatin1Char('\n')));
        }
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

    //Update all the positions for all the nodes that need to be updated
    //Also update the length and depth information
    for(const auto& [node, nodeData] : resolved.nodes.asKeyValueRange()) {
        if(nodeData.hasStationPositionsChanged()) {
            node->setStationPositionLookup(nodeData.stationPositions());
        }

        if(nodeData.hasNetworkChanged()) {
            node->setSurveyNetwork(nodeData.network());
        }

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
    }

    // Re-attach per-trip keyword items (centerline + splays) to the new
    // geometry and re-seed each item's visibility by its (shifted) vertex
    // span. setGeometry reset the render object to all-visible, so reconcile
    // pushes the spans that keyword filtering hides.
    reconcileTripKeywordItems(results.tripUuids(),
                              results.tripVertexRanges(),
                              results.tripSplayVertexRanges());

    // Skip publication when the network hasn't changed so 2D-geometry rules
    // don't rebuild on every line-plot completion triggered by unrelated
    // cave data (labels, calibration, etc.). Note this compares topology only —
    // a positions-only re-solve lands here equal, which is why anything that
    // cares about where a station moved watches the cave's lookup instead.
    const cwSurveyNetwork newNetwork = results.regionNetwork();
    if (newNetwork != m_lastPublishedNetwork) {
        m_lastPublishedNetwork = newNetwork;
        m_surveyNetworkSource->setSurveyNetwork(
            QtFuture::makeReadyValueFuture(Monad::Result<cwSurveyNetwork>(newNetwork)));
    }

    //Mark all nodes as up todate
    setCaveStationLookupAsStale(false);

    emit stationPositionInCavesChanged(resolved.nodes.keys());
    emit stationPositionInTripsChanged(cw::toList(resolved.trips));
    emit stationPositionInScrapsChanged(cw::toList(resolved.scraps));
}


