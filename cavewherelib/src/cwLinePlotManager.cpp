/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinePlotManager.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
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
#include "cwSurveyNetworkArtifact.h"
#include "cwFixStationModel.h"
#include "asyncfuture.h"

#include <QFuture>


cwLinePlotManager::cwLinePlotManager(QObject *parent) :
    QObject(parent),
    m_restarter(this)
{
    Region = nullptr;
    m_linePlot = nullptr;

    m_surveyNetworkArtifact = new cwSurveyNetworkArtifact(this);
    m_surveyNetworkArtifact->setName(QStringLiteral("LinePlotManager Survey Network"));

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
    connect(Region, &cwCavingRegion::globalCoordinateSystemChanged, this, &cwLinePlotManager::runSurvex);

    // worldOrigin is subtracted by cwLinePlotTask::applyWorldOriginOffset, so
    // the line plot must re-solve when it changes (auto-compute or manual recenter).
    connect(Region, &cwCavingRegion::worldOriginChanged, this, &cwLinePlotManager::runSurvex);

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
 * @brief cwLinePlotManager::validateResultsData
 * @param results
 *
 * This goes through the results and removes
 */
void cwLinePlotManager::validateResultsData(cwLinePlotTask::LinePlotResultData &results)
{
    QMap<cwCave*, cwLinePlotTask::LinePlotCaveData> validCaves;
    QSet<cwTrip*> validTrips;
    QSet<cwScrap*> validScraps;

    //Update all the positions for all the caves
    foreach(cwCave* cave, Region->caves()) {
        if(results.caveData().contains(cave)) {

            //Add this cave to the valid, needs to be updated list
            validCaves.insert(cave, results.caveData().value(cave));

            foreach(cwTrip* trip, cave->trips()) {
                if(results.trips().contains(trip)) {

                    //Add this trip to the valid trips
                    validTrips.insert(trip);

                    foreach(cwNote* note, trip->notes()->notes()) {
                        foreach(cwScrap* scrap, note->scraps()) {
                            if(results.scraps().contains(scrap)) {

                                //Add this scrap to the valid scrap
                                validScraps.insert(scrap);
                            }
                        }
                    }
                }
            }
        }
    }

    results.setCaveData(validCaves);
    results.setTrip(validTrips);
    results.setScraps(validScraps);
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
        // Skip the pipeline when no shots exist — cavern warns about empty surveys
        // and there is nothing useful to compute.
        bool hasShots = false;
        for(cwCave* cave : Region->caves()) {
            for(cwTrip* trip : cave->trips()) {
                for(cwSurveyChunk* chunk : trip->chunks()) {
                    if(chunk->shotCount() > 0) {
                        hasShots = true;
                        break;
                    }
                }
                if(hasShots) break;
            }
            if(hasShots) break;
        }
        if(!hasShots) {
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

            auto input = cwLinePlotTask::buildInput(Region.data());
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
    // Snapshot the live cave list once: cwCavingRegion::caves() returns by
    // value, so calling it inside the loop would deep-copy per iteration.
    const QList<cwCave*> liveCaves = Region->caves();
    for (auto it = results.Caves.constBegin(); it != results.Caves.constEnd(); ++it) {
        cwCave* cave = it.key();
        if (!liveCaves.contains(cave)) {
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

    //Validate all the objects in resultData, remove any that were delete before the task was over
    validateResultsData(results); //Modifies resultData inplace

    //Update all the positions for all the caves that need to be updated
    //Also update the length and depth information
    QMapIterator<cwCave*, cwLinePlotTask::LinePlotCaveData> iter(results.caveData());
    while(iter.hasNext()) {
        iter.next();

        cwCave* cave = iter.key();
        cwLinePlotTask::LinePlotCaveData caveData = iter.value();

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

    //Update the 3D plot
    if(m_linePlot != nullptr) {
        m_linePlot->setGeometry(results.stationPositions(), results.linePlotIndexData());
    }

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

    emit stationPositionInCavesChanged(results.caveData().keys());
    emit stationPositionInTripsChanged(cw::toList(results.trips()));
    emit stationPositionInScrapsChanged(cw::toList(results.scraps()));

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
    if (!Region->hasExplicitWorldOrigin()) {
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
