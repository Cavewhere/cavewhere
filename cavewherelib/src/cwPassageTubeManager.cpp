/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeManager.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwConcurrent.h"
#include "cwKeyword.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLinePlotManager.h"
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeSettings.h"
#include "cwRenderPassageTube.h"
#include "cwTrip.h"

//Qt includes
#include <QHash>
#include <QSettings>

namespace {
QString visibleSettingsKey() { return QStringLiteral("view/passageTubesVisible"); }
QString passageKeywordType() { return QStringLiteral("Passage"); }
constexpr bool kDefaultVisible = true;
}

void cwPassageTubeTripVisibility::setTarget(cwRenderPassageTube* tube,
                                            cwPassageTubeMesh::VertexRange range)
{
    m_tube = tube;
    m_range = range;
    if (!isVisible()) {
        applyVisible(false);
    }
}

void cwPassageTubeTripVisibility::applyVisible(bool visible)
{
    if (m_tube) {
        m_tube->setRangeVisible(m_range.first, m_range.count, visible);
    }
}

cwPassageTubeManager::cwPassageTubeManager(QObject* parent) :
    QObject(parent),
    m_restarter(this)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kRebuildDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &cwPassageTubeManager::startBuild);

    QSettings settings;
    m_visible = settings.value(visibleSettingsKey(), kDefaultVisible).toBool();
}

cwPassageTubeManager::~cwPassageTubeManager()
{
    //See cwUpdatable::beginTeardown().
    beginTeardown();

    //Cancel without waiting: the worker builds from value snapshots, and the
    //delivery is bound with context(this), so it is dropped here.
    m_debounce.stop();
    m_restarter.future().cancel();
}

void cwPassageTubeManager::setRegion(cwCavingRegion* region)
{
    if (m_region == region) {
        return;
    }
    m_region = region;
    rebuildWhenRegionHasCaves();
}

void cwPassageTubeManager::setLinePlotManager(cwLinePlotManager* linePlotManager)
{
    if (m_linePlotManager == linePlotManager) {
        return;
    }
    if (m_linePlotManager) {
        disconnect(m_linePlotManager, nullptr, this, nullptr);
    }
    m_linePlotManager = linePlotManager;
    if (m_linePlotManager) {
        connect(m_linePlotManager, &cwLinePlotManager::stationPositionInCavesChanged,
                this, &cwPassageTubeManager::onStationPositionsChanged);
    }
}

void cwPassageTubeManager::setSettings(cwPassageTubeSettings* settings)
{
    if (m_settings == settings) {
        return;
    }
    if (m_settings) {
        disconnect(m_settings, nullptr, this, nullptr);
    }
    m_settings = settings;
    if (m_settings) {
        connect(m_settings, &cwPassageTubeSettings::sampleSpacingMetersChanged,
                this, &cwPassageTubeManager::requestRebuild);
        connect(m_settings, &cwPassageTubeSettings::ringResolutionChanged,
                this, &cwPassageTubeManager::requestRebuild);
    }
    rebuildWhenRegionHasCaves();
}

void cwPassageTubeManager::setRenderPassageTube(cwRenderPassageTube* tube)
{
    if (m_tube == tube) {
        return;
    }
    m_tube = tube;
    if (m_tube) {
        m_tube->setVisible(m_visible);
    }
    rebuildWhenRegionHasCaves();
}

void cwPassageTubeManager::setKeywordItemModel(cwKeywordItemModel* keywordItemModel)
{
    if (m_keywordRegistry.model() == keywordItemModel) {
        return;
    }

    //Items are recreated against the new model on the next build.
    m_keywordRegistry.setModel(keywordItemModel);
    for (cwTrip* trip : std::as_const(m_trackedTrips)) {
        disconnect(trip, &QObject::destroyed, this, nullptr);
    }
    m_trackedTrips.clear();
    rebuildWhenRegionHasCaves();
}

void cwPassageTubeManager::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    emit visibleChanged();

    QSettings settings;
    settings.setValue(visibleSettingsKey(), visible);
    if (m_tube) {
        m_tube->setVisible(visible);
    }
}

void cwPassageTubeManager::requestRebuild()
{
    if (!m_needsUpdate) {
        m_needsUpdate = true;
        emit updateStateChanged();
    }
    runIfStandalone();
}

//A solve that touched no cave rebuilds only to clear a mesh left over from
//caves that are gone.
void cwPassageTubeManager::onStationPositionsChanged(const QList<cwCave*>& caves)
{
    if (caves.isEmpty() && m_meshEmpty) {
        return;
    }
    requestRebuild();
}

//Wiring up an empty region leaves the empty mesh as it is, so startup stays
//idle until the first line-plot solve.
void cwPassageTubeManager::rebuildWhenRegionHasCaves()
{
    if (m_region && m_region->caveCount() > 0) {
        requestRebuild();
    }
}

cwUpdatable::State cwPassageTubeManager::doUpdateState() const
{
    if (m_needsUpdate) { return cwUpdatable::State::Dirty; }
    if (isRunning())   { return cwUpdatable::State::Working; }
    return cwUpdatable::State::Clean;
}

QFuture<void> cwPassageTubeManager::doRun()
{
    //The debounce is part of the run: a request that lands while it is pending
    //restarts the timer and joins the same run.
    const cwUpdatable::State previousState = updateState();
    m_needsUpdate = false;
    const QFuture<void> run = beginRun();
    if (updateState() != previousState) {
        emit updateStateChanged();
    }
    m_debounce.start();
    return run;
}

void cwPassageTubeManager::startBuild()
{
    m_restarter.restart([this]() {
        if (m_region.isNull()) {
            deliverMesh(cwPassageTubeMesh());
            finishBuilding();
            return QFuture<cwPassageTubeMesh>();
        }

        QHash<QUuid, cwStationPositionLookup> positionsByCave;
        QHash<QUuid, cwSplayTipsByStation> splaysByCave;
        for (const cwCave* cave : m_region->caves()) {
            positionsByCave.insert(cave->id(), cave->stationPositionLookup());
            splaysByCave.insert(cave->id(), cave->splayTips());
        }

        cwPassageTubeBuilder::Options options;
        if (m_settings) {
            options.sampleSpacingMeters = m_settings->sampleSpacingMeters();
            options.ringResolution = m_settings->ringResolution();
        }

        auto future = cwConcurrent::run([region = m_region->data(),
                                         positionsByCave = std::move(positionsByCave),
                                         splaysByCave = std::move(splaysByCave),
                                         options]() {
            return cwPassageTubeBuilder::build(region, positionsByCave, splaysByCave, options);
        });

        AsyncFuture::observe(future).context(this, [this](cwPassageTubeMesh mesh) {
            deliverMesh(mesh);
            //A request that arrived mid-build restarted the debounce; that run
            //is still owed.
            if (!m_debounce.isActive()) {
                finishBuilding();
            }
        });

        return future;
    });
}

void cwPassageTubeManager::deliverMesh(const cwPassageTubeMesh& mesh)
{
    QStringList warnings;
    warnings.reserve(mesh.warnings.size());
    for (const cwPassageTubeMesh::Warning& warning : mesh.warnings) {
        warnings.append(warning.message);
    }
    if (m_warnings != warnings) {
        m_warnings = warnings;
        emit warningsChanged();
    }

    m_meshEmpty = mesh.isEmpty();

    if (m_tube) {
        m_tube->setMesh(mesh);
    }

    reconcileTripKeywordItems(mesh.tripUuids, mesh.tripIndexRanges);

    emit meshChanged();
}

void cwPassageTubeManager::finishBuilding()
{
    if (isRunning()) {
        endRun();
        emit updateStateChanged();
    }
}

void cwPassageTubeManager::reconcileTripKeywordItems(
    const QVector<QUuid>& tripUuids,
    const QVector<cwPassageTubeMesh::VertexRange>& tripIndexRanges)
{
    if (m_region.isNull() || m_keywordRegistry.model() == nullptr) {
        return;
    }

    Q_ASSERT(tripUuids.size() == tripIndexRanges.size());

    QHash<QUuid, cwTrip*> liveByUuid;
    for (cwCave* cave : m_region->caves()) {
        for (cwTrip* trip : cave->trips()) {
            liveByUuid.insert(trip->id(), trip);
        }
    }

    QSet<cwTrip*> present;
    for (int i = 0; i < tripUuids.size(); ++i) {
        cwTrip* trip = liveByUuid.value(tripUuids.at(i), nullptr);
        if (trip == nullptr) {
            continue; //deleted while the build ran
        }
        present.insert(trip);

        if (!m_trackedTrips.contains(trip)) {
            m_trackedTrips.insert(trip);
            connect(trip, &QObject::destroyed, this, [this, trip]() {
                removeTripKeywordItem(trip);
            });
        }

        const cwKeywordItem* item = m_keywordRegistry.ensure(trip, [this, trip]() {
            return makeTripKeywordItem(trip);
        });
        if (auto* visibility = qobject_cast<cwPassageTubeTripVisibility*>(item->object())) {
            visibility->setTarget(m_tube, tripIndexRanges.at(i));
        }
    }

    const QSet<cwTrip*> tracked = m_trackedTrips;
    for (cwTrip* trip : tracked) {
        if (!present.contains(trip)) {
            removeTripKeywordItem(trip);
        }
    }
}

cwKeywordItem* cwPassageTubeManager::makeTripKeywordItem(cwTrip* trip)
{
    auto item = new cwKeywordItem();
    item->keywordModel()->add(cwKeyword(cwKeywordModel::TypeKey, passageKeywordType()));
    item->keywordModel()->addExtension(trip->keywordModel());
    item->setObject(new cwPassageTubeTripVisibility(item));
    return item;
}

void cwPassageTubeManager::removeTripKeywordItem(cwTrip* trip)
{
    if (!m_trackedTrips.remove(trip)) {
        return;
    }
    m_keywordRegistry.drop(trip);
    disconnect(trip, &QObject::destroyed, this, nullptr);
}

void cwPassageTubeManager::waitToFinish()
{
    if (updateState() == cwUpdatable::State::Dirty) {
        run();
    }
    if (m_debounce.isActive()) {
        m_debounce.stop();
        startBuild();
    }
    AsyncFuture::waitForFinished(currentRun());
}
