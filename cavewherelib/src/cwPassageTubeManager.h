/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEMANAGER_H
#define CWPASSAGETUBEMANAGER_H

//Our includes
#include "cwGlobals.h"
#include "cwKeywordItemRegistry.h"
#include "cwPassageTubeMesh.h"
#include "cwUpdatable.h"
#include "cwVisibilityProxy.h"
class cwCave;
class cwCavingRegion;
class cwKeywordItem;
class cwKeywordItemModel;
class cwLinePlotManager;
class cwPassageTubeSettings;
class cwRenderPassageTube;
class cwTrip;

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVector>

//Async includes
#include <asyncfuture.h>

/**
 * The setVisible() target for a trip's Type="Passage" keyword item: shows or
 * hides that trip's index range in the tube. The manager re-points it after
 * every build, because the range moves each time the mesh is rebuilt.
 *
 * The union gives each triangle to the trip whose tube it came from, so
 * hiding a trip leaves an opening where its passage met a visible one; the
 * visible trip's boundary disc there closes it (cwPassageTubeMesh).
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeTripVisibility : public cwVisibilityProxy
{
    Q_OBJECT

public:
    using cwVisibilityProxy::cwVisibilityProxy;

    // A new mesh shows every range, so a hidden trip hides its new range here.
    void setTarget(cwRenderPassageTube* tube, cwPassageTubeMesh::VertexRange range);

protected:
    void applyVisible(bool visible) override;

private:
    QPointer<cwRenderPassageTube> m_tube;
    cwPassageTubeMesh::VertexRange m_range;
};

/**
 * Rebuilds the passage tube on a worker thread whenever the line plot solves or
 * the tube settings change, and hands the mesh to cwRenderPassageTube.
 *
 * Rebuild requests are debounced: a run waits kRebuildDebounceMs for the burst
 * to settle before it snapshots the region and starts cwPassageTubeBuilder.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeManager : public QObject, public cwUpdatableBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PassageTubeManager)
    QML_UNCREATABLE("PassageTubeManager is owned by RootData")

    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY warningsChanged)

public:
    static constexpr int kRebuildDebounceMs = 150;

    explicit cwPassageTubeManager(QObject* parent = nullptr);
    ~cwPassageTubeManager() override;

    void setRegion(cwCavingRegion* region);
    void setLinePlotManager(cwLinePlotManager* linePlotManager);
    void setSettings(cwPassageTubeSettings* settings);
    void setRenderPassageTube(cwRenderPassageTube* tube);
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);

    QStringList warnings() const { return m_warnings; }

    // Test-only: starts any pending rebuild now and blocks until it is delivered.
    void waitToFinish();

signals:
    void updateStateChanged();
    void meshChanged();
    void visibleChanged();
    void warningsChanged();

private:
    cwUpdatable::State doUpdateState() const override;
    QFuture<void> doRun() override;

    void requestRebuild();
    void rebuildWhenRegionHasCaves();
    void onStationPositionsChanged(const QList<cwCave*>& caves);
    void startBuild();
    void deliverMesh(const cwPassageTubeMesh& mesh);
    void finishBuilding();

    void reconcileTripKeywordItems(const QVector<QUuid>& tripUuids,
                                   const QVector<cwPassageTubeMesh::VertexRange>& tripIndexRanges);
    void removeTripKeywordItem(cwTrip* trip);
    cwKeywordItem* makeTripKeywordItem(cwTrip* trip);

    QPointer<cwCavingRegion> m_region;
    QPointer<cwLinePlotManager> m_linePlotManager;
    QPointer<cwPassageTubeSettings> m_settings;
    QPointer<cwRenderPassageTube> m_tube;

    QTimer m_debounce;
    AsyncFuture::Restarter<cwPassageTubeMesh> m_restarter;

    QStringList m_warnings;

    cwKeywordItemRegistry<cwTrip*> m_keywordRegistry;
    QSet<cwTrip*> m_trackedTrips;

    bool m_needsUpdate = false;
    bool m_visible = true;
    bool m_meshEmpty = true;
};

#endif // CWPASSAGETUBEMANAGER_H
