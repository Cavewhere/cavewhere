/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwRegionSceneManager.h"
#include "cwScene.h"
#include "cwRenderGridPlane.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPointCloud.h"
#include "cwRenderRadialGradient.h"
#include "cwRenderTexturedItems.h"


#include "cwCavingRegion.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"

cwRegionSceneManager::cwRegionSceneManager(QObject *parent) :
    QObject(parent),
    Scene(new cwScene(this)),
    Region(nullptr)
{

    //Renders the background, should be drawn first
    m_background = new cwRenderRadialGradient();
    m_linePlot = new cwRenderLinePlot();
    m_plane = new cwRenderGridPlane();
    m_items = new cwRenderTexturedItems();

    //    Terrain->setScene(scene());

    m_background->setScene(scene());
    m_linePlot->setScene(scene());
    m_items->setScene(scene());

    //Render the plane last
    m_plane->setScene(scene());


}

/**
  \brief Sets the caving region for the renderer
  */
void cwRegionSceneManager::setCavingRegion(cwCavingRegion* region) {
    if(Region != region) {
        disconnectLazLayers();
        Region = region;
        connectLazLayers();
        rebuildLazRenderObjects();
        emit cavingRegionChanged();
    }
}

void cwRegionSceneManager::connectLazLayers()
{
    if (!Region) {
        return;
    }
    auto* model = Region->lazLayers();
    connect(model, &cwLazLayerModel::rowsInserted,
            this, [this, model](const QModelIndex&, int first, int last) {
                for (int i = first; i <= last; ++i) {
                    addLazLayer(model->layerAt(i));
                }
            });
    connect(model, &cwLazLayerModel::rowsAboutToBeRemoved,
            this, [this, model](const QModelIndex&, int first, int last) {
                for (int i = first; i <= last; ++i) {
                    removeLazLayer(model->layerAt(i));
                }
            });
    connect(model, &cwLazLayerModel::modelAboutToBeReset,
            this, [this]() {
                rebuildLazRenderObjects();
            });
    connect(model, &cwLazLayerModel::modelReset,
            this, [this]() {
                rebuildLazRenderObjects();
            });
}

void cwRegionSceneManager::disconnectLazLayers()
{
    if (Region) {
        disconnect(Region->lazLayers(), nullptr, this, nullptr);
    }
    clearPointClouds();
}

void cwRegionSceneManager::rebuildLazRenderObjects()
{
    clearPointClouds();

    if (!Region) {
        return;
    }
    const auto& layers = Region->lazLayers()->layers();
    for (auto* layer : layers) {
        addLazLayer(layer);
    }
}

void cwRegionSceneManager::clearPointClouds()
{
    // setScene(nullptr) unhooks each render object from cwScene before delete;
    // ~cwRenderObject doesn't do this itself, so deleting directly would leave
    // dangling pointers in cwScene::m_newRenderObjects and crash on next sync.
    for (auto* renderObject : std::as_const(m_pointClouds)) {
        renderObject->setScene(nullptr);
        delete renderObject;
    }
    m_pointClouds.clear();
}

void cwRegionSceneManager::addLazLayer(cwLazLayer* layer)
{
    if (!layer || m_pointClouds.contains(layer->id())) {
        return;
    }

    auto* renderObject = new cwRenderPointCloud();
    renderObject->setScene(scene());
    m_pointClouds.insert(layer->id(), renderObject);

    // bboxChanged also fires during applyResult, immediately before
    // loadStatusChanged(Loaded). Connecting both would push the geometry
    // twice and rebuild the immutable vertex buffer twice per load.
    connect(layer, &cwLazLayer::loadStatusChanged,
            this, [this, layer]() { syncLazLayerGeometry(layer); });

    syncLazLayerGeometry(layer);
}

void cwRegionSceneManager::removeLazLayer(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    auto it = m_pointClouds.find(layer->id());
    if (it == m_pointClouds.end()) {
        return;
    }
    cwRenderPointCloud* renderObject = it.value();
    renderObject->setScene(nullptr);
    delete renderObject;
    m_pointClouds.erase(it);
    disconnect(layer, nullptr, this, nullptr);
}

void cwRegionSceneManager::syncLazLayerGeometry(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    auto it = m_pointClouds.find(layer->id());
    if (it == m_pointClouds.end()) {
        return;
    }
    cwRenderPointCloud* renderObject = it.value();

    if (layer->loadStatus() == cwLazLayer::LoadStatus::Loaded) {
        renderObject->setGeometry(layer->geometry(),
                                  layer->bboxMin(),
                                  layer->bboxMax());
    } else {
        renderObject->clear();
    }
}

/**
  \brief Returns the caving region that's owned by the renderer
  */
cwCavingRegion* cwRegionSceneManager::cavingRegion() const {
    return Region;
}

void cwRegionSceneManager::setCapturing(bool newCapturing)
{
    if (m_capturing == newCapturing) {
        return;
    }
    m_capturing = newCapturing;
    m_background->setVisible(!m_capturing);
    m_linePlot->setVisible(!m_capturing);
    m_plane->setVisible(!m_capturing);
    for (auto* renderObject : std::as_const(m_pointClouds)) {
        renderObject->setVisible(!m_capturing);
    }
    emit capturingChanged();
}



