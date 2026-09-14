/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZLAYERSSCENENODE_H
#define CWLAZLAYERSSCENENODE_H

//Qt includes
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUuid>

//Our includes
#include "cwGlobals.h"
#include "cwKeywordItemRegistry.h"
#include "cwRenderPointCloud.h"

class cwKeywordItemModel;
class cwLazLayer;
class cwLazLayerModel;
class cwScene;

/**
 * Per-window bridge between cwLazLayerModel (shared data) and a cwScene
 * (per-window). Owns the per-layer cwRenderPointCloud render objects and the
 * cwKeywordItem entries that drive filter-chip visibility.
 *
 * Belongs to cwRegionSceneManager. Data model + scene + keyword sink are
 * wired in explicitly; tearing down a model wire-up drops all render objects
 * and keyword items.
 */
class CAVEWHERE_LIB_EXPORT cwLazLayersSceneNode : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LazLayersSceneNode)
    QML_UNCREATABLE("Access via RegionSceneManager.lazLayersSceneNode")

    Q_PROPERTY(float worldRadius READ worldRadius WRITE setWorldRadius NOTIFY worldRadiusChanged)
    Q_PROPERTY(float spacingCoverage READ spacingCoverage WRITE setSpacingCoverage NOTIFY spacingCoverageChanged)

public:
    explicit cwLazLayersSceneNode(QObject* parent = nullptr);
    ~cwLazLayersSceneNode() override;

    void setScene(cwScene* scene);
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);
    void setLazLayerModel(cwLazLayerModel* model);

    /// Toggles `visible` on every owned cwRenderPointCloud. Used by capture
    /// mode in cwRegionSceneManager to hide overlays before a screenshot.
    void setVisibleForAll(bool visible);

    /// Test accessor: render object backing @a layer, or nullptr.
    cwRenderPointCloud* pointCloudForLayer(cwLazLayer* layer) const;

    float worldRadius() const { return m_worldRadius; }
    float spacingCoverage() const { return m_spacingCoverage; }

    /// Subset of the bound model's layers whose render object is currently
    /// visible (keyword-filter pipeline gates this). Returns layers in the
    /// same order they appear in the model.
    QList<cwLazLayer*> visibleLayers() const;

public slots:
    void setWorldRadius(float worldRadius);
    void setSpacingCoverage(float spacingCoverage);

signals:
    void worldRadiusChanged(float worldRadius);
    void spacingCoverageChanged(float spacingCoverage);

private slots:
    /// Resolves the originating cwLazLayer through QObject::sender() so the
    /// connect site can use the 4-arg member-function form. That form supports
    /// Qt::UniqueConnection (lambdas do not), which makes a re-addLayer() on
    /// an already-tracked layer idempotent.
    void onEnabledChanged();

private:
    void connectModel();
    void disconnectModel();
    void rebuild();
    void clear();
    void addLayer(cwLazLayer* layer);
    void removeLayer(cwLazLayer* layer);
    void materialize(cwLazLayer* layer);
    void dematerialize(cwLazLayer* layer);
    void syncLayerOctree(cwLazLayer* layer);
    void addKeywordItemForLayer(cwLazLayer* layer);
    void removeKeywordItemForLayer(cwLazLayer* layer);

    QPointer<cwScene> m_scene;
    QPointer<cwLazLayerModel> m_model;

    QHash<QUuid, QPointer<cwRenderPointCloud>> m_pointClouds;
    // Declared after m_pointClouds so it destructs first: keyword items target
    // the point clouds via setObject(), so they must die before the clouds do.
    cwKeywordItemRegistry<QUuid> m_keywordRegistry;

    // Mirrors cwRenderPointCloud::RenderState::worldRadius default.
    // setWorldRadius fans out to every owned cwRenderPointCloud, is bound to
    // the P+wheel gesture in the 3D view, and is the entry point used by
    // sink_repatcher --point-radius. Kept here (rather than only on
    // cwRenderPointCloud) so the value survives layers added later in the
    // session.
    float m_worldRadius = cw::pointcloud::kDefaultWorldRadius;

    // Fanned out to every owned cwRenderPointCloud exactly as m_worldRadius is,
    // so it too survives layers added later in the session.
    float m_spacingCoverage = cw::pointcloud::kDefaultSpacingCoverage;
};

#endif // CWLAZLAYERSSCENENODE_H
