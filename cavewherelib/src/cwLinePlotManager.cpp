/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinePlotManager.h"
#include "cwCavingRegion.h"
#include "cwGeoReference.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineScanner.h"
#include "cwExternalCenterlineSync.h"
#include "cwLinePlotTask.h"
#include "cwRenderLinePlot.h"
#include "cwTripCalibration.h"
#include "cwSurveyNoteModel.h"
#include "cwSaveLoad.h"
#include "cwScrap.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwSurveyChunkSignaler.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwFixStationModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLinePlotTripVisibility.h"
#include "asyncfuture.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFuture>
#include <QSet>

namespace {

// The worker identifies changed caves/trips/scraps by UUID. Resolve those
// UUIDs back to the live objects in `region`, dropping any that were deleted
// while the solve was running. Walking the live hierarchy (rather than a flat
// id->object map) also guarantees a trip/scrap is only kept when its owning
// cave survived.
struct ResolvedResults {
    QHash<cwCave*, cwLinePlotTask::LinePlotCaveData> caves;
    QSet<cwTrip*> trips;
    QSet<cwScrap*> scraps;
};

ResolvedResults resolveResultsToLive(const cwCavingRegion* region,
                                     const cwLinePlotTask::LinePlotResultData& results)
{
    ResolvedResults resolved;
    for (cwCave* cave : region->caves()) {
        const auto caveIt = results.Caves.constFind(cave->id());
        if (caveIt == results.Caves.constEnd()) {
            continue;
        }
        resolved.caves.insert(cave, caveIt.value());

        for (cwTrip* trip : cave->trips()) {
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

} // namespace


cwLinePlotManager::cwLinePlotManager(QObject *parent) :
    QObject(parent),
    m_restarter(this)
{
    Region = nullptr;
    m_linePlot = nullptr;

    m_surveyNetworkArtifact = new cwSurveyNetworkArtifact(this);
    m_surveyNetworkArtifact->setName(QStringLiteral("LinePlotManager Survey Network"));

    // Single watcher for both in-project attachment-dir dependencies and
    // live-link source-side dependencies. fileChanged hands the event off
    // to onWatchedFileChanged which decides between a plain rerun and a
    // reconcile-from-source flow.
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &cwLinePlotManager::onWatchedFileChanged);

    SurveySignaler = new cwSurveyChunkSignaler(this);

    SurveySignaler->addConnectionToCaves(SIGNAL(insertedTrips(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToCaves(SIGNAL(removedTrips(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SLOT(runSurvex()));

    SurveySignaler->addConnectionToTrips(SIGNAL(chunksInserted(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTrips(SIGNAL(chunksRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTripCalibrations(SIGNAL(calibrationsChanged()), this, SLOT(runSurvex()));

    SurveySignaler->addConnectionToChunks(SIGNAL(shotsAdded(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(shotsRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(stationsAdded(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(stationsRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)), this, SLOT(runSurvex()));

    m_restarter.onFutureChanged([this]() {
        if (m_futureManagerToken.isValid()) {
            m_futureManagerToken.addJob({ QFuture<void>(m_restarter.future()), QStringLiteral("Line plot") });
        }
    });
}

cwLinePlotManager::~cwLinePlotManager() {
    m_restarter.future().cancel();
    waitToFinish();

    clearTripKeywordEntries();
}

void cwLinePlotManager::setCaveAttachmentDirs(QHash<QUuid, QString> dirs)
{
    m_caveAttachmentDirs = std::move(dirs);
    recomputeWatchSetAndProbeSources();
}

void cwLinePlotManager::setTripAttachmentDirs(QHash<QUuid, QString> dirs)
{
    m_tripAttachmentDirs = std::move(dirs);
    recomputeWatchSetAndProbeSources();
}

void cwLinePlotManager::setLocalSettings(cwLocalSettings settings)
{
    if (m_localSettings == settings) {
        return;
    }
    m_localSettings = std::move(settings);
    recomputeWatchSetAndProbeSources();
}

void cwLinePlotManager::setSaveLoad(cwSaveLoad* saveLoad)
{
    m_saveLoad = saveLoad;
}

QStringList cwLinePlotManager::watchedFiles() const
{
    return m_watchedFiles;
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
    publishResults(cwLinePlotTask::LinePlotResultData());

    if(Region == nullptr) { return; }

    //Connect all signal from the region
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(runSurvex()));
    connect(Region, SIGNAL(removedCaves(int,int)), SLOT(runSurvex()));

    // globalCoordinateSystem feeds the *cs out / *cs lines on the survex
    // export, so the line plot needs to re-run when the user changes the
    // region's CS.
    connect(Region->geoReference(), &cwGeoReference::globalCoordinateSystemChanged, this, &cwLinePlotManager::runSurvex);

    // worldOrigin is subtracted by cwLinePlotTask::applyWorldOriginOffset, so
    // the line plot must re-solve when it changes (auto-compute or manual recenter).
    connect(Region->geoReference(), &cwGeoReference::worldOriginChanged, this, &cwLinePlotManager::runSurvex);

    SurveySignaler->setRegion(Region);

    // Hook fix-station edits on every existing cave, plus any future caves.
    for (cwCave* cave : Region->caves()) {
        connectFixStations(cave);
    }
    connect(Region, &cwCavingRegion::insertedCaves, this, [this](int begin, int end) {
        for (int i = begin; i <= end && i < Region->caveCount(); ++i) {
            connectFixStations(Region->cave(i));
        }
    });

    //Connect all sub data
    connectCaves(Region);

    // Recompute the external-centerline watch set + missing-source probe.
    // Done after the per-cave/trip connects so any subsequent attach/detach
    // signal recompute lands on a populated region.
    recomputeWatchSetAndProbeSources();

    if(Region->hasCaves()) {
        runSurvex();
    }

    updateLinePlot(cwLinePlotTask::LinePlotResultData());
}

void cwLinePlotManager::connectFixStations(cwCave* cave) {
    if (!cave) { return; }
    auto* model = cave->fixStations();
    if (!model) { return; }
    const auto rerun = [this](){ runSurvex(); };
    connect(model, &cwFixStationModel::dataChanged,  this, rerun);
    connect(model, &cwFixStationModel::rowsInserted, this, rerun);
    connect(model, &cwFixStationModel::rowsRemoved,  this, rerun);
    connect(model, &cwFixStationModel::modelReset,   this, rerun);
}

void cwLinePlotManager::setRenderLinePlot(cwRenderLinePlot* linePlot) {
    m_linePlot = linePlot;
    updateLinePlot(cwLinePlotTask::LinePlotResultData());
}

void cwLinePlotManager::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

void cwLinePlotManager::setKeywordItemModel(cwKeywordItemModel* keywordItemModel)
{
    if (m_keywordItemModel == keywordItemModel) {
        return;
    }

    // Tear down items registered with the old model before switching.
    clearTripKeywordEntries();

    m_keywordItemModel = keywordItemModel;

    // Entries are (re)created on the next updateLinePlot() against the current
    // geometry; nothing to do here.
}

void cwLinePlotManager::reconcileTripKeywordItems(
    const QVector<QUuid>& tripUuids,
    const QVector<cwLinePlotGeometry::VertexRange>& tripVertexRanges)
{
    if (Region == nullptr) {
        return;
    }

    // Built in lockstep in cwLinePlotGeometry::generate (one append each per
    // trip), so the running id indexes both tables identically.
    Q_ASSERT(tripVertexRanges.size() == tripUuids.size());

    // Resolve UUIDs to live trips by identity (never by list position).
    QHash<QUuid, cwTrip*> liveByUuid;
    for (cwCave* cave : Region->caves()) {
        for (cwTrip* trip : cave->trips()) {
            liveByUuid.insert(trip->id(), trip);
        }
    }

    QSet<cwTrip*> present;
    for (int i = 0; i < tripUuids.size(); ++i) {
        cwTrip* trip = liveByUuid.value(tripUuids.at(i), nullptr);
        if (trip == nullptr) {
            continue; // trip deleted mid-solve — skip, no dangling deref
        }
        present.insert(trip);

        cwLinePlotTripVisibility* visibility = nullptr;
        auto entryIt = m_tripKeywordEntries.constFind(trip);
        if (entryIt != m_tripKeywordEntries.constEnd()) {
            visibility = entryIt.value()
                ? qobject_cast<cwLinePlotTripVisibility*>(entryIt.value()->object())
                : nullptr;
        } else if (m_keywordItemModel) {
            auto item = new cwKeywordItem();
            // References the trip-owned line plot keyword model (Type=Line Plot
            // plus the trip's inherited Trip/Year/Date/Cave/Caver keywords), so
            // filtering the Type keyword toggles the whole centerline. The Type
            // lives on that dedicated model, not trip->keywordModel(), so
            // scraps/notes under the trip don't inherit it. The station labels'
            // keyword item references the same model.
            item->keywordModel()->addExtension(trip->linePlotKeywordModel());

            visibility = new cwLinePlotTripVisibility(trip, item);
            item->setObject(visibility);

            // addItem fires resolveVisibility → proxy setVisible, which sets the
            // proxy's state; the seed below pushes it to the render object.
            m_keywordItemModel->addItem(item);

            // Prompt cleanup if the trip is destroyed before the next solve.
            connect(trip, &QObject::destroyed, this, [this, trip]() {
                removeTripKeywordEntry(trip);
            });

            m_tripKeywordEntries.insert(trip, item);
        }

        // Re-bind the proxy to the trip's current vertex span (it shifts each
        // solve) and seed the render object. setGeometry just reset every vertex
        // to visible, so only hidden trips need an explicit push.
        if (visibility) {
            // tripVertexRanges is parallel to tripUuids (both appended once per
            // trip in cwLinePlotGeometry::generate), so index i is always valid.
            const cwLinePlotGeometry::VertexRange range = tripVertexRanges.at(i);
            visibility->setTarget(m_linePlot, range.start, range.count);
            if (m_linePlot && !visibility->isVisible()) {
                m_linePlot->setRangeVisible(range.start, range.count, false);
            }
        }
    }

    // Drop entries for trips that are no longer in the solved geometry.
    const QList<cwTrip*> tracked = m_tripKeywordEntries.keys();
    for (cwTrip* trip : tracked) {
        if (!present.contains(trip)) {
            removeTripKeywordEntry(trip);
        }
    }
}

void cwLinePlotManager::removeTripKeywordEntry(cwTrip* trip)
{
    auto it = m_tripKeywordEntries.find(trip);
    if (it == m_tripKeywordEntries.end()) {
        return;
    }

    if (it.value() && m_keywordItemModel) {
        m_keywordItemModel->removeItem(it.value());
    }
    if (it.value()) {
        it.value()->deleteLater();
    }
    m_tripKeywordEntries.erase(it);

    // Drop the destroyed() connection added when the entry was created;
    // otherwise a trip that leaves and re-enters the solved geometry
    // accumulates a duplicate connection on every cycle. (Lambda connections
    // can't use Qt::UniqueConnection, so disconnect explicitly.)
    disconnect(trip, &QObject::destroyed, this, nullptr);
}

void cwLinePlotManager::clearTripKeywordEntries()
{
    // Synchronous delete (not deleteLater): used for manager destruction and
    // model swaps, where the event loop may not run again to drain deferred
    // deletes. addItem reparents each item to the model, so deleting it here
    // (rather than leaking) is required; the proxy is the item's child, so
    // deleting the item deletes the proxy too.
    for (auto it = m_tripKeywordEntries.begin(); it != m_tripKeywordEntries.end(); ++it) {
        if (it.value() && m_keywordItemModel) {
            m_keywordItemModel->removeItem(it.value());
        }
        delete it.value();
    }
    m_tripKeywordEntries.clear();
}

/**
 * @brief cwLinePlotManager::waitToFinish
 *
 * Will cause the LinePlotManager to block until the underlying task is finished. This is useful
 * for unit testing.
 */
void cwLinePlotManager::waitToFinish()
{
    AsyncFuture::waitForFinished(m_restarter.future());
}

/**
  \brief Connects all the caves in the region to this object
  */
void cwLinePlotManager::connectCaves(cwCavingRegion* region) {
    bool caveIsStale = false;

    for(int i = 0; i < region->caveCount(); i++) {
        cwCave* cave = region->cave(i);

        if(cave->isStationPositionLookupStale()) {
            caveIsStale = true;
        }
    }

    if(caveIsStale) {
        runSurvex();
    }
}

/**
 * @brief cwLinePlotManager::markCaveStationsAsStale
 *
 * This will go through all the caves in the region and mark them as stale
 * This is useful, to make sure that the cave data is upto date. If the user
 * closes cavewhere before the line plot is re-processed.
 */
void cwLinePlotManager::setCaveStationLookupAsStale(bool isStale)
{
    foreach(cwCave* cave, Region->caves()) {
        cave->setStationPositionLookupStale(isStale);
    }
}

/**
 * @brief cwLinePlotManager::updateUnconnectedChunkErrors
 *
 * This will clear all the survey chunk errors and add survey chunk error's that exist. Currently
 * the only errors that is added to the whole survey chunk, are unconnected survey chunk error.
 */
void cwLinePlotManager::updateUnconnectedChunkErrors(cwCave* cave,
                                                     const cwLinePlotTask::LinePlotCaveData& caveData)
{

    //Append unconnected errors
    if(caveData.unconnectedChunkError().size() > 0) {
        foreach(auto errorResult, caveData.unconnectedChunkError()) {
            cwErrorModel* model = cave->trip(errorResult.TripIndex)->chunk(errorResult.SurveyChunkIndex)->errorModel();
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

/**
 * @brief cwLinePlotManager::rerunSurvex
 *
 * Re-runs the survex. This simply just calls runSurvex but is useful for debugging
 * if the re-run isn't working correctly.
 */
void cwLinePlotManager::rerunSurvex()
{
    runSurvex();
}

/**
  \brief Run the line plot task
  */
void cwLinePlotManager::runSurvex() {
    if(!AutomaticUpdate) {
        return;
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
            for (cwCave* cave : Region->caves()) {
                if (!cave->externalCenterline().isEmpty()) {
                    return true;
                }
                for (cwTrip* trip : cave->trips()) {
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
            // No-shots path must also clear the cached cavern output / solve
            // error so CavernOutputPage doesn't keep showing the previous
            // run's text (D-1).
            publishResults(cwLinePlotTask::LinePlotResultData());
            updateLinePlot(cwLinePlotTask::LinePlotResultData());
            return;
        }

        setCaveStationLookupAsStale(true);
        m_restarter.restart([this]() {
            if (Region.isNull()) {
                return QFuture<cwLinePlotTask::LinePlotResultData>();
            }

            auto input = cwLinePlotTask::buildInput(Region.data(),
                                                    m_caveAttachmentDirs,
                                                    m_tripAttachmentDirs);
            auto future = cwLinePlotTask::run(std::move(input));

            // Receive the worker's result by parameter rather than capturing
            // the future and calling future.result() — the parameter form
            // lets AsyncFuture deliver the value directly, no captured-future
            // ping-pong. publishResults updates CavernOutputPage state
            // (log/stats/error + per-chunk markers) on every path; updateLinePlot
            // only runs on the success path so an error doesn't wipe the last
            // good line plot.
            AsyncFuture::observe(future)
                .context(this, [this](cwLinePlotTask::LinePlotResultData result) {
                    publishResults(result);
                    if (!result.hasSolveError()) {
                        updateLinePlot(std::move(result));
                    }
                });

            return future;
        });
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
                        results.hasSolveError()
                            ? std::optional<cwLinePlotTask::SolveError>(results.solveError())
                            : std::nullopt);
    publishPerCaveErrors(results);
}

void cwLinePlotManager::publishCavernOutput(QString cavernLog,
                                            QString loopClosureStats,
                                            std::optional<cwLinePlotTask::SolveError> solveError)
{
    bool changed = false;
    if (cavernLog != m_lastCavernLog || loopClosureStats != m_lastLoopClosureStats) {
        m_lastCavernLog = std::move(cavernLog);
        m_lastLoopClosureStats = std::move(loopClosureStats);
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
    // Walk the live caves and resolve each by id() to the worker's UUID-keyed
    // result, skipping any cave deleted while the solve was running.
    for (cwCave* cave : Region->caves()) {
        const auto it = results.Caves.constFind(cave->id());
        if (it == results.Caves.constEnd()) {
            continue;
        }
        updateUnconnectedChunkErrors(cave, it.value());
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

    //Update all the positions for all the caves that need to be updated
    //Also update the length and depth information
    for(const auto& [cave, caveData] : resolved.caves.asKeyValueRange()) {
        if(caveData.hasStationPositionsChanged()) {
            cave->setStationPositionLookup(caveData.stationPositions());
        }

        if(caveData.hasNetworkChanged()) {
            cave->setSurveyNetwork(caveData.network());
        }

        if(caveData.hasDepthLengthChanged()) {
            //Update the cave's depth and length
            double length = cwUnits::convert(caveData.length(), cwUnits::Meters, (cwUnits::LengthUnit)cave->length()->unit());
            double depth = cwUnits::convert(caveData.depth(), cwUnits::Meters, (cwUnits::LengthUnit)cave->depth()->unit());

            cave->length()->setValue(length);
            cave->depth()->setValue(depth);
        }
    }

    const QVector<QUuid> tripUuids = results.tripUuids();
    const QVector<cwLinePlotGeometry::VertexRange> tripVertexRanges = results.tripVertexRanges();

    //Update the 3D plot
    if(m_linePlot != nullptr) {
        m_linePlot->setGeometry(results.stationPositions());
    }

    // Re-attach per-trip centerline keyword items to the new geometry and
    // re-seed each trip's visibility by its (shifted) vertex span. setGeometry
    // reset the render object to all-visible, so reconcile only pushes the trips
    // that are currently hidden.
    reconcileTripKeywordItems(tripUuids, tripVertexRanges);

    // Skip emission when the network hasn't changed so 2D-geometry rules
    // don't rebuild on every line-plot completion triggered by unrelated
    // cave data (labels, calibration, etc.).
    const cwSurveyNetwork newNetwork = results.regionNetwork();
    if (newNetwork != m_lastPublishedNetwork) {
        m_lastPublishedNetwork = newNetwork;
        m_surveyNetworkArtifact->setSurveyNetwork(
            QtFuture::makeReadyValueFuture(Monad::Result<cwSurveyNetwork>(newNetwork)));
    }

    //Mark all caves as up todate
    setCaveStationLookupAsStale(false);

    emit stationPositionInCavesChanged(resolved.caves.keys());
    emit stationPositionInTripsChanged(cw::toList(resolved.trips));
    emit stationPositionInScrapsChanged(cw::toList(resolved.scraps));

    // First-time auto-compute of worldOrigin: when nobody has explicitly
    // picked one yet and we now have at least one valid fix, recenter the
    // scene. recomputeWorldOrigin() is a no-op when no candidates exist,
    // and the resulting setWorldOrigin() emits worldOriginChanged → re-runs
    // survex so the lookup positions land in offset-relative coords. Sticky
    // after: once anyone has set worldOrigin explicitly (user, load, this
    // recompute, or LAZ auto-adopt), this branch never fires again.
    //
    // Uses hasExplicitWorldOrigin() rather than `worldOrigin() == {}`
    // because an explicit pin to (0,0,0) — e.g. sink-training tests that
    // align render-space with LAZ-source XY — is a valid origin that must
    // not be silently overwritten by the fix-station centroid.
    if (!Region->geoReference()->hasExplicitWorldOrigin()) {
        Region->recomputeWorldOrigin();
    }
}


void cwLinePlotManager::setAutomaticUpdate(bool automaticUpdate) {
    if(AutomaticUpdate != automaticUpdate) {
        AutomaticUpdate = automaticUpdate;
        emit automaticUpdateChanged();
        runSurvex();
    }
}

void cwLinePlotManager::recomputeWatchSetAndProbeSources()
{
    QStringList newWatchedFiles;
    QHash<QString, QUuid> newSourceOwnerForPath;
    QList<QUuid> newMissingSourceOwners;

    if (Region != nullptr) {
        // Per-owner scan: project-side (in-project copy) and source-side
        // (live-link). Both feed the same watch set; the source-side
        // mapping survives separately so onWatchedFileChanged can route
        // to reconcile. Empty entry file means "no attachment" - skipped.
        const auto handleOwner = [&](const QUuid& ownerId,
                                     const QString& attachmentDir,
                                     const QString& entryFile) {
            if (entryFile.isEmpty()) {
                return;
            }
            if (!attachmentDir.isEmpty()) {
                const QString projectEntry = QDir(attachmentDir).filePath(entryFile);
                if (QFileInfo::exists(projectEntry)) {
                    const auto scan = cwExternalCenterlineScanner::scan(projectEntry);
                    if (!scan.hasError()) {
                        for (const QString& dep : scan.value().dependencies) {
                            newWatchedFiles.append(dep);
                        }
                    }
                }
            }
            const QString sourcePath = m_localSettings.sourcePathFor(ownerId);
            if (!sourcePath.isEmpty()) {
                if (!QFileInfo::exists(sourcePath)) {
                    newMissingSourceOwners.append(ownerId);
                } else {
                    const auto scan = cwExternalCenterlineScanner::scan(sourcePath);
                    if (!scan.hasError()) {
                        for (const QString& dep : scan.value().dependencies) {
                            newWatchedFiles.append(dep);
                            // First owner wins; collisions between two
                            // live-link owners pointing at the same source
                            // tree are not a v1 use case.
                            if (!newSourceOwnerForPath.contains(dep)) {
                                newSourceOwnerForPath.insert(dep, ownerId);
                            }
                        }
                    }
                }
            }
        };

        for (cwCave* cave : Region->caves()) {
            if (!cave->externalCenterline().isEmpty()) {
                handleOwner(cave->id(),
                            m_caveAttachmentDirs.value(cave->id()),
                            cave->externalCenterline().entryFile());
            }
            for (cwTrip* trip : cave->trips()) {
                if (!trip->externalCenterline().isEmpty()) {
                    handleOwner(trip->id(),
                                m_tripAttachmentDirs.value(trip->id()),
                                trip->externalCenterline().entryFile());
                }
            }
        }
    }

    // Stable order so equality comparisons against m_watchedFiles are
    // deterministic, and the StringList tests can compare by index.
    const QSet<QString> dedup(newWatchedFiles.cbegin(), newWatchedFiles.cend());
    newWatchedFiles = QStringList(dedup.cbegin(), dedup.cend());
    std::sort(newWatchedFiles.begin(), newWatchedFiles.end());
    std::sort(newMissingSourceOwners.begin(), newMissingSourceOwners.end());

    if (newWatchedFiles != m_watchedFiles) {
        // Wholesale-replace the watcher's table from our intent set. Files
        // that don't exist on disk are kept in m_watchedFiles (so a later
        // recompute can pick them up if they appear) but skipped on the
        // addPaths call - QFileSystemWatcher emits a warning otherwise.
        const QStringList currentlyWatched = m_watcher->files();
        if (!currentlyWatched.isEmpty()) {
            m_watcher->removePaths(currentlyWatched);
        }
        QStringList toAdd;
        toAdd.reserve(newWatchedFiles.size());
        for (const QString& p : newWatchedFiles) {
            if (QFileInfo::exists(p)) {
                toAdd.append(p);
            }
        }
        if (!toAdd.isEmpty()) {
            m_watcher->addPaths(toAdd);
        }
        m_watchedFiles = std::move(newWatchedFiles);
        m_sourceOwnerForPath = std::move(newSourceOwnerForPath);
        emit watchedFilesChanged();
    } else {
        // Union unchanged but per-owner source mapping may have shifted.
        m_sourceOwnerForPath = std::move(newSourceOwnerForPath);
    }

    if (newMissingSourceOwners != m_missingSourceOwners) {
        m_missingSourceOwners = std::move(newMissingSourceOwners);
        emit missingSourceOwnersChanged();
    }
}

void cwLinePlotManager::rearmWatcher(const QString& path)
{
    // macOS atomic-replace (write-to-temp, rename-over) drops the path from
    // the watcher's internal table after the fileChanged event; Linux
    // inotify behaves similarly when the inode is replaced. Re-add so the
    // next edit still notifies us. No-op when the path is already armed.
    if (!m_watcher->files().contains(path) && QFileInfo::exists(path)) {
        m_watcher->addPath(path);
    }
}

void cwLinePlotManager::onWatchedFileChanged(const QString& path)
{
    rearmWatcher(path);

    const auto sourceIt = m_sourceOwnerForPath.constFind(path);
    if (sourceIt != m_sourceOwnerForPath.constEnd() && !m_saveLoad.isNull()) {
        const QUuid ownerId = sourceIt.value();
        const QString sourcePath = m_localSettings.sourcePathFor(ownerId);
        QString attachmentDir = m_caveAttachmentDirs.value(ownerId);
        if (attachmentDir.isEmpty()) {
            attachmentDir = m_tripAttachmentDirs.value(ownerId);
        }
        if (sourcePath.isEmpty()
            || !QFileInfo::exists(sourcePath)
            || attachmentDir.isEmpty()) {
            // Source went missing or we don't know where to reconcile to.
            // Recompute (will surface the missing-source state) and run
            // the solve against the in-project copy as-is.
            recomputeWatchSetAndProbeSources();
            runSurvex();
            return;
        }
        const auto scan = cwExternalCenterlineScanner::scan(sourcePath);
        if (scan.hasError()) {
            runSurvex();
            return;
        }
        auto future = cwExternalCenterlineSync::reconcile(
            m_saveLoad.data(), scan.value(), attachmentDir);
        // Reconcile drains through the cwSaveLoad job queue; the
        // returned future completes after every copy/remove has hit
        // disk, which is the moment runSurvex can see the fresh bytes
        // in the in-project copy. Recompute first so any newly-added
        // *include target is in the watch set before the next solve.
        AsyncFuture::observe(future).context(this,
                [this](Monad::ResultBase) {
            recomputeWatchSetAndProbeSources();
            runSurvex();
        });
        return;
    }

    // Project-side change (or no saveLoad wired): straight rerun. Also
    // recompute the dep set in case the edit added or removed an
    // *include that needs to enter/leave the watch set.
    recomputeWatchSetAndProbeSources();
    runSurvex();
}
