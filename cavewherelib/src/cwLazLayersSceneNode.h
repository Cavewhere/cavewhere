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
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUuid>

//Our includes
#include "cwGlobals.h"

class cwKeywordItem;
class cwKeywordItemModel;
class cwLazLayer;
class cwLazLayerModel;
class cwRenderPointCloud;
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

public slots:
    void setWorldRadius(float worldRadius);

signals:
    void worldRadiusChanged(float worldRadius);

private:
    void connectModel();
    void disconnectModel();
    void rebuild();
    void clear();
    void addLayer(cwLazLayer* layer);
    void removeLayer(cwLazLayer* layer);
    void syncLayerGeometry(cwLazLayer* layer);
    void addKeywordItemForLayer(cwLazLayer* layer);
    void removeKeywordItemForLayer(cwLazLayer* layer);

    QPointer<cwScene> m_scene;
    QPointer<cwKeywordItemModel> m_keywordItemModel;
    QPointer<cwLazLayerModel> m_model;

    QHash<QUuid, QPointer<cwRenderPointCloud>> m_pointClouds;
    QHash<QUuid, QPointer<cwKeywordItem>> m_keywordItems;

    // Mirrors cwRenderPointCloud::RenderState::worldRadius default.
    // setWorldRadius fans out to every owned cwRenderPointCloud, is bound to
    // the P+wheel gesture in the 3D view, and is the entry point used by
    // sink_repatcher --point-radius. Kept here (rather than only on
    // cwRenderPointCloud) so the value survives layers added later in the
    // session.
    float m_worldRadius = 1.29f;
};

#endif // CWLAZLAYERSSCENENODE_H
