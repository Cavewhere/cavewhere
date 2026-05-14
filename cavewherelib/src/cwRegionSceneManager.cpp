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
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
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
    for (const auto& item : std::as_const(m_lazKeywordItems)) {
        if (m_keywordItemModel && item) {
            m_keywordItemModel->removeItem(item);
        }
        if (item) {
            item->deleteLater();
        }
    }
    m_lazKeywordItems.clear();

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
    addKeywordItemForLazLayer(layer);
}

void cwRegionSceneManager::removeLazLayer(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    removeKeywordItemForLazLayer(layer);
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
        renderObject->setGeometry({
            .geometry = layer->geometry(),
            .bboxMin = layer->bboxMin(),
            .bboxMax = layer->bboxMax(),
            .meanSpacingXY = layer->meanSpacingXY(),
        });
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

void cwRegionSceneManager::setKeywordItemModel(cwKeywordItemModel* keywordItemModel)
{
    m_keywordItemModel = keywordItemModel;
}

void cwRegionSceneManager::addKeywordItemForLazLayer(cwLazLayer* layer)
{
    if (!m_keywordItemModel || !layer) {
        return;
    }

    if (m_lazKeywordItems.contains(layer->id())) {
        return;
    }

    auto pointCloudIt = m_pointClouds.constFind(layer->id());
    if (pointCloudIt == m_pointClouds.constEnd()) {
        return;
    }

    auto* keywordItem = new cwKeywordItem();
    keywordItem->keywordModel()->addExtension(layer->keywordModel());
    keywordItem->setObject(pointCloudIt.value());
    m_keywordItemModel->addItem(keywordItem);

    m_lazKeywordItems.insert(layer->id(), keywordItem);
}

void cwRegionSceneManager::removeKeywordItemForLazLayer(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    auto it = m_lazKeywordItems.find(layer->id());
    if (it == m_lazKeywordItems.end()) {
        return;
    }

    if (m_keywordItemModel && it.value()) {
        m_keywordItemModel->removeItem(it.value());
    }
    if (it.value()) {
        it.value()->deleteLater();
    }

    m_lazKeywordItems.erase(it);
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



