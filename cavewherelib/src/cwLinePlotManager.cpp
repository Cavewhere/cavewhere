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
#include "cwConcurrent.h"
#include "asyncfuture.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFuture>
#include <QSet>

//Std includes
#include <algorithm>
#include <tuple>

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

// Deterministic presentation order: cave display name, then trip
// display name (cave-level owners sort ahead of their trips via the
// empty trip key), with ownerId as a stable tiebreak for duplicates.
void sortAttachedRows(QVector<cwAttachedCenterlinesModel::Row>& rows)
{
    std::stable_sort(rows.begin(), rows.end(),
                     [](const cwAttachedCenterlinesModel::Row& left,
                        const cwAttachedCenterlinesModel::Row& right) {
        const QString leftTripKey = left.ownerKind == QStringLiteral("Cave")
            ? QString() : left.ownerName;
        const QString rightTripKey = right.ownerKind == QStringLiteral("Cave")
            ? QString() : right.ownerName;
        return std::tie(left.caveName, leftTripKey, left.ownerId)
             < std::tie(right.caveName, rightTripKey, right.ownerId);
    });
}

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
    m_restarter(this),
    m_scanRestarter(this)
{
    Region = nullptr;
    m_linePlot = nullptr;

    m_surveyNetworkArtifact = new cwSurveyNetworkArtifact(this);
    m_surveyNetworkArtifact->setName(QStringLiteral("LinePlotManager Survey Network"));

    m_attachedCenterlinesModel = new cwAttachedCenterlinesModel(this);

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

    // Renames re-sort the attached-centerlines rows immediately from
    // cached scan counts — no disk I/O, no waiting for a full recompute.
    SurveySignaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));
    SurveySignaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));
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

    // Same idiom as the solve: registering the scan with the future
    // manager lets tests drain it via futureManagerModel()->waitForFinished().
    m_scanRestarter.onFutureChanged([this]() {
        if (m_futureManagerToken.isValid()) {
            m_futureManagerToken.addJob({ QFuture<void>(m_scanRestarter.future()), QStringLiteral("External centerline scan") });
        }
    });
}

cwLinePlotManager::~cwLinePlotManager() {
    m_scanRestarter.future().cancel();
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

void cwLinePlotManager::setExternalSourceSettings(cwExternalSourceSettings* settings)
{
    if (m_externalSourceSettings == settings) {
        return;
    }
    if (!m_externalSourceSettings.isNull()) {
        disconnect(m_externalSourceSettings.data(), &cwExternalSourceSettings::externalCenterlineSourcesChanged,
                   this, &cwLinePlotManager::recomputeWatchSetAndProbeSources);
    }
    m_externalSourceSettings = settings;
    if (!m_externalSourceSettings.isNull()) {
        connect(m_externalSourceSettings.data(), &cwExternalSourceSettings::externalCenterlineSourcesChanged,
                this, &cwLinePlotManager::recomputeWatchSetAndProbeSources);
    }
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

    if(Region == nullptr) {
        // Supersede any scan still in flight for the previous region —
        // its apply would otherwise reinstall that region's watch set and
        // model rows. The null-region snapshot produces an empty result,
        // clearing both.
        recomputeWatchSetAndProbeSources();
        return;
    }

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
    // signal recompute lands on a populated region. The initial solve
    // chains behind the scan's apply — buildInput reads
    // m_fileOwnsDeclination, so it must not race the worker.
    if (Region->hasCaves()) {
        m_solveOnScanApply = true;
    }
    recomputeWatchSetAndProbeSources();

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
    // The scan's apply continuation can chain into a solve (and a solve
    // never chains back into a scan), so draining scan-then-solve settles
    // both pipelines. waitForFinished pumps queued continuations before
    // returning, which is what lets the apply's runSurvex land in between.
    AsyncFuture::waitForFinished(m_scanRestarter.future());
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
                                                    m_tripAttachmentDirs,
                                                    m_fileOwnsDeclination);
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
                        m_attachedCenterlinesModel->markSolved(QDateTime::currentDateTime());
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
                        results.DriverSource,
                        results.hasSolveError()
                            ? std::optional<cwLinePlotTask::SolveError>(results.solveError())
                            : std::nullopt);
    publishPerCaveErrors(results);
}

void cwLinePlotManager::publishCavernOutput(QString cavernLog,
                                            QString loopClosureStats,
                                            QString driverSource,
                                            std::optional<cwLinePlotTask::SolveError> solveError)
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
        emit regionNetworkChanged();
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
    m_scanRestarter.restart([this]() {
        // Stage 1 (main thread, no I/O): snapshot the per-owner value
        // inputs. Clearing the since-snapshot set here — not at restart
        // time — matters: the restarter defers this lambda through the
        // event loop, and "since the snapshot" means since this instant.
        m_staleRaisedSinceSnapshot.clear();
        const QVector<OwnerScanInput> owners = collectOwnerSnapshots();

        // Stage 2 (worker thread): every scan and freshness probe. The
        // scanner and planner are stateless; `owners` crosses by value.
        auto future = cwConcurrent::run([owners]() {
            return scanOwners(owners);
        });

        // Stage 3 (main thread): install the result wholesale. A result
        // superseded by a newer restart never lands (the restarter
        // cancels this future), and a result outliving the manager is
        // dropped by the context auto-disconnect.
        AsyncFuture::observe(future).context(this, [this](ExternalScanResult result) {
            applyScanResult(std::move(result));
        });

        return future;
    });
}

QVector<cwLinePlotManager::OwnerScanInput> cwLinePlotManager::collectOwnerSnapshots() const
{
    QVector<OwnerScanInput> owners;
    if (Region.isNull()) {
        return owners;
    }

    const auto appendOwner = [&](const QUuid& ownerId,
                                 const QString& attachmentDir,
                                 const QString& entryFile,
                                 const QString& caveName,
                                 const QString& ownerName,
                                 const QString& ownerKind) {
        if (entryFile.isEmpty()) {
            return;
        }
        owners.append(OwnerScanInput { ownerId, caveName, ownerName, ownerKind,
                                       entryFile, attachmentDir,
                                       sourcePathForOwner(ownerId) });
    };

    for (cwCave* cave : Region->caves()) {
        if (!cave->externalCenterline().isEmpty()) {
            appendOwner(cave->id(),
                        m_caveAttachmentDirs.value(cave->id()),
                        cave->externalCenterline().entryFile(),
                        cave->name(),
                        cave->name(),
                        QStringLiteral("Cave"));
        }
        for (cwTrip* trip : cave->trips()) {
            if (!trip->externalCenterline().isEmpty()) {
                appendOwner(trip->id(),
                            m_tripAttachmentDirs.value(trip->id()),
                            trip->externalCenterline().entryFile(),
                            cave->name(),
                            trip->name(),
                            QStringLiteral("Trip"));
            }
        }
    }
    return owners;
}

cwAttachedCenterlinesModel::Row cwLinePlotManager::rowFromOwner(const OwnerScanInput& owner)
{
    cwAttachedCenterlinesModel::Row row;
    row.ownerId = owner.ownerId;
    row.caveName = owner.caveName;
    row.ownerName = owner.ownerName;
    row.ownerKind = owner.ownerKind;
    row.entryFile = owner.entryFile;
    return row;
}

cwLinePlotManager::ExternalScanResult cwLinePlotManager::scanOwners(
    const QVector<OwnerScanInput>& owners)
{
    ExternalScanResult result;

    // Per-owner scan: project-side (in-project copy) and source-side
    // (live-link). Both feed the same watch set; the source-side
    // mapping survives separately so onWatchedFileChanged can route
    // to the stale flag. Each owner also contributes one row to the
    // attached-centerlines model, counted from the project-side scan
    // when it resolves (the solve *includes the in-project bytes) and
    // the source-side scan otherwise.
    for (const OwnerScanInput& owner : owners) {
        cwAttachedCenterlinesModel::Row row = rowFromOwner(owner);
        bool rowCounted = false;

        if (!owner.attachmentDir.isEmpty()) {
            const QString projectEntry = QDir(owner.attachmentDir).filePath(owner.entryFile);
            if (QFileInfo::exists(projectEntry)) {
                const auto scan = cwExternalCenterlineScanner::scan(projectEntry);
                if (!scan.hasError()) {
                    for (const QString& dep : scan.value().dependencies) {
                        result.watchedFiles.append(dep);
                    }
                    result.fileOwnsDeclination.insert(
                        owner.ownerId, scan.value().seededMetadata.fileOwnsDeclination());
                    row.depCount = scan.value().dependencies.size();
                    row.warningCount = scan.value().warnings.size();
                    rowCounted = true;
                }
            }
        }
        if (!owner.sourcePath.isEmpty()) {
            if (!QFileInfo::exists(owner.sourcePath)) {
                result.missingSourceOwners.append(owner.ownerId);
            } else {
                const auto scan = cwExternalCenterlineScanner::scan(owner.sourcePath);
                if (!scan.hasError()) {
                    for (const QString& dep : scan.value().dependencies) {
                        result.watchedFiles.append(dep);
                        // First owner wins; collisions between two
                        // live-link owners pointing at the same source
                        // tree are not a v1 use case.
                        if (!result.sourceOwnerForPath.contains(dep)) {
                            result.sourceOwnerForPath.insert(dep, owner.ownerId);
                        }
                    }
                    // The project-side flag wins when both scans resolve:
                    // the solve *includes the in-project copy, so its scan
                    // describes the bytes actually exported. The source
                    // scan only fills in when no project entry exists yet;
                    // once reconcile copies the source over, the next
                    // recompute converges the flag to the new bytes.
                    if (!result.fileOwnsDeclination.contains(owner.ownerId)) {
                        result.fileOwnsDeclination.insert(
                            owner.ownerId, scan.value().seededMetadata.fileOwnsDeclination());
                    }
                    if (!rowCounted) {
                        row.depCount = scan.value().dependencies.size();
                        row.warningCount = scan.value().warnings.size();
                    }
                    // Open-time / recompute freshness probe: a pending
                    // copy in the reconcile plan means the source has
                    // drifted from the in-project bytes. computePlan
                    // only reads the filesystem, so the probe is free
                    // of side effects (Phase 2 direction change).
                    if (!owner.attachmentDir.isEmpty()
                        && !cwExternalCenterlineSync::computePlan(
                                scan.value(), owner.attachmentDir).copies.isEmpty()) {
                        result.staleSourceOwners.append(owner.ownerId);
                    }
                }
            }
        }
        result.rows.append(row);
    }

    // Stable order so equality comparisons against m_watchedFiles are
    // deterministic, and the StringList tests can compare by index.
    const QSet<QString> dedup(result.watchedFiles.cbegin(), result.watchedFiles.cend());
    result.watchedFiles = QStringList(dedup.cbegin(), dedup.cend());
    std::sort(result.watchedFiles.begin(), result.watchedFiles.end());
    result.existingWatchedFiles.reserve(result.watchedFiles.size());
    for (const QString& path : std::as_const(result.watchedFiles)) {
        if (QFileInfo::exists(path)) {
            result.existingWatchedFiles.append(path);
        }
    }
    std::sort(result.missingSourceOwners.begin(), result.missingSourceOwners.end());
    std::sort(result.staleSourceOwners.begin(), result.staleSourceOwners.end());
    sortAttachedRows(result.rows);

    return result;
}

void cwLinePlotManager::applyScanResult(ExternalScanResult result)
{
    // Commit-8 merge policy: a watcher event that fired after the
    // snapshot raised a flag the probe was computed without — union it
    // back in rather than letting the wholesale rebuild wipe it.
    for (const QUuid& ownerId : std::as_const(m_staleRaisedSinceSnapshot)) {
        if (!result.staleSourceOwners.contains(ownerId)) {
            result.staleSourceOwners.append(ownerId);
        }
    }
    std::sort(result.staleSourceOwners.begin(), result.staleSourceOwners.end());

    if (result.watchedFiles != m_watchedFiles) {
        // Wholesale-replace the watcher's table from our intent set. Files
        // that don't exist on disk are kept in m_watchedFiles (so a later
        // recompute can pick them up if they appear) but skipped on the
        // addPaths call - QFileSystemWatcher emits a warning otherwise.
        // The existence filter ran on the worker so the apply stays free
        // of file I/O.
        const QStringList currentlyWatched = m_watcher->files();
        if (!currentlyWatched.isEmpty()) {
            m_watcher->removePaths(currentlyWatched);
        }
        if (!result.existingWatchedFiles.isEmpty()) {
            m_watcher->addPaths(result.existingWatchedFiles);
        }
        m_watchedFiles = std::move(result.watchedFiles);
        m_sourceOwnerForPath = std::move(result.sourceOwnerForPath);
        emit watchedFilesChanged();
    } else {
        // Union unchanged but per-owner source mapping may have shifted.
        m_sourceOwnerForPath = std::move(result.sourceOwnerForPath);
    }

    if (result.missingSourceOwners != m_missingSourceOwners) {
        m_missingSourceOwners = std::move(result.missingSourceOwners);
        emit missingSourceOwnersChanged();
    }

    if (result.staleSourceOwners != m_staleSourceOwners) {
        m_staleSourceOwners = std::move(result.staleSourceOwners);
        emit staleSourceOwnersChanged();
    }

    // Chain the solve behind the flag swap so buildInput never reads
    // half-applied declination state; an owner entering or leaving the
    // map is a change too (its *include just appeared or vanished).
    const bool solveNow = m_solveOnScanApply
        || result.fileOwnsDeclination != m_fileOwnsDeclination;
    m_solveOnScanApply = false;
    m_fileOwnsDeclination = std::move(result.fileOwnsDeclination);

    m_lastScanRows = result.rows;
    m_attachedCenterlinesModel->setRows(std::move(result.rows));

    if (solveNow) {
        runSurvex();
    }
}

void cwLinePlotManager::rebuildAttachedRowsFromNames()
{
    QVector<cwAttachedCenterlinesModel::Row> rows;
    const QVector<OwnerScanInput> owners = collectOwnerSnapshots();
    rows.reserve(owners.size());
    for (const OwnerScanInput& owner : owners) {
        cwAttachedCenterlinesModel::Row row = rowFromOwner(owner);
        // Counts come from the last scan; an owner attached since then
        // shows zeros until its scan applies. lastSolved is carried by
        // the model itself.
        const auto cached = std::find_if(m_lastScanRows.cbegin(), m_lastScanRows.cend(),
                                         [&owner](const cwAttachedCenterlinesModel::Row& candidate) {
            return candidate.ownerId == owner.ownerId;
        });
        if (cached != m_lastScanRows.cend()) {
            row.depCount = cached->depCount;
            row.warningCount = cached->warningCount;
        }
        rows.append(row);
    }
    sortAttachedRows(rows);
    m_lastScanRows = rows;
    m_attachedCenterlinesModel->setRows(std::move(rows));
}

QString cwLinePlotManager::sourcePathForOwner(const QUuid& ownerId) const
{
    return m_externalSourceSettings.isNull() ? QString() : m_externalSourceSettings->sourcePathFor(ownerId);
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
    if (sourceIt != m_sourceOwnerForPath.constEnd()) {
        if (!QFileInfo::exists(path)) {
            // The source-side file vanished (deleted or renamed away) —
            // "stale" would offer an Update for a file that is gone.
            // Reclassify through the full probe so the owner lands in
            // missingSourceOwners instead. (An editor's atomic-save
            // replace has already re-created the path by the time the
            // event is delivered, so it takes the stale branch below.)
            recomputeWatchSetAndProbeSources();
            return;
        }
        // Direction change (see the header comment on
        // setExternalSourceSettings): a source-side edit only flags the
        // owner stale. It never touches the filesystem or re-solves —
        // the user triggers the reconcile via updateFromSource. Also
        // record it for the apply-time union: a scan in flight probed
        // the disk before this edit and must not clear the flag.
        const QUuid ownerId = sourceIt.value();
        m_staleRaisedSinceSnapshot.insert(ownerId);
        if (!m_staleSourceOwners.contains(ownerId)) {
            m_staleSourceOwners.append(ownerId);
            std::sort(m_staleSourceOwners.begin(), m_staleSourceOwners.end());
            emit staleSourceOwnersChanged();
        }
        return;
    }

    // Project-side change: recompute the dep set in case the edit added
    // or removed an *include that needs to enter/leave the watch set,
    // with the rerun chained behind the apply so the solve sees the
    // fresh declination flags.
    m_solveOnScanApply = true;
    recomputeWatchSetAndProbeSources();
}

void cwLinePlotManager::updateFromSource(const QUuid& ownerId)
{
    if (m_updateInFlightOwners.contains(ownerId) || m_saveLoad.isNull()) {
        return;
    }
    const QString sourcePath = sourcePathForOwner(ownerId);
    QString attachmentDir = m_caveAttachmentDirs.value(ownerId);
    if (attachmentDir.isEmpty()) {
        attachmentDir = m_tripAttachmentDirs.value(ownerId);
    }
    if (sourcePath.isEmpty()
        || !QFileInfo::exists(sourcePath)
        || attachmentDir.isEmpty()) {
        // Source went missing or we don't know where to reconcile to.
        // Recompute so missingSourceOwners reflects the current disk state.
        recomputeWatchSetAndProbeSources();
        return;
    }
    const auto scan = cwExternalCenterlineScanner::scan(sourcePath);
    if (scan.hasError()) {
        // Unreadable source: nothing sensible to reconcile. The stale
        // flag is probe-owned — the next recompute re-derives it.
        return;
    }
    m_updateInFlightOwners.insert(ownerId);
    auto future = cwExternalCenterlineSync::reconcile(
        m_saveLoad.data(), scan.value(), attachmentDir);
    // Reconcile drains through the cwSaveLoad job queue; the returned
    // future completes after every copy/remove has hit disk, which is
    // the moment runSurvex can see the fresh bytes in the in-project
    // copy. The recompute re-probes freshness (clearing the stale flag)
    // and installs any newly-added *include target in the watch set
    // before the next solve. The canceled path (project retired
    // mid-drain) must still release the per-owner token or the Update
    // affordance stays stuck for the rest of the session.
    AsyncFuture::observe(future).context(this,
            [this, ownerId](Monad::ResultBase) {
        m_updateInFlightOwners.remove(ownerId);
        m_solveOnScanApply = true;
        recomputeWatchSetAndProbeSources();
    },
            [this, ownerId]() {
        m_updateInFlightOwners.remove(ownerId);
    });
}
