/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLayersSceneNode.h"

#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwRenderObject.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

#include <QDebug>
#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcLazSceneNode, "cw.laz.scenenode")

cwLazLayersSceneNode::cwLazLayersSceneNode(QObject* parent) :
    QObject(parent)
{
}

cwLazLayersSceneNode::~cwLazLayersSceneNode()
{
    clear();
}

void cwLazLayersSceneNode::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }
    m_scene = scene;
    rebuild();
}

void cwLazLayersSceneNode::setKeywordItemModel(cwKeywordItemModel* keywordItemModel)
{
    if (m_keywordItemModel == keywordItemModel) {
        return;
    }
    m_keywordItemModel = keywordItemModel;
    rebuild();
}

void cwLazLayersSceneNode::setLazLayerModel(cwLazLayerModel* model)
{
    if (m_model == model) {
        return;
    }
    disconnectModel();
    m_model = model;
    connectModel();
    rebuild();
}

void cwLazLayersSceneNode::setVisibleForAll(bool visible)
{
    for (const auto& renderObject : std::as_const(m_pointClouds)) {
        if (renderObject) {
            renderObject->setVisible(visible);
        }
    }
}

cwRenderPointCloud* cwLazLayersSceneNode::pointCloudForLayer(cwLazLayer* layer) const
{
    if (!layer) {
        return nullptr;
    }
    auto it = m_pointClouds.constFind(layer->id());
    return it == m_pointClouds.constEnd() ? nullptr : it.value();
}

QList<cwLazLayer*> cwLazLayersSceneNode::visibleLayers() const
{
    QList<cwLazLayer*> result;
    if (m_model.isNull()) {
        return result;
    }
    const QList<cwLazLayer*>& all = m_model->layers();
    result.reserve(all.size());
    for (cwLazLayer* layer : all) {
        if (layer == nullptr) {
            continue;
        }
        auto it = m_pointClouds.constFind(layer->id());
        if (it == m_pointClouds.constEnd() || it.value().isNull()) {
            continue;
        }
        if (it.value()->isVisible()) {
            result.append(layer);
        }
    }
    return result;
}

void cwLazLayersSceneNode::connectModel()
{
    if (!m_model) {
        return;
    }
    connect(m_model, &cwLazLayerModel::rowsInserted,
            this, [this](const QModelIndex&, int first, int last) {
                for (int i = first; i <= last; ++i) {
                    addLayer(m_model->layerAt(i));
                }
            });
    connect(m_model, &cwLazLayerModel::rowsAboutToBeRemoved,
            this, [this](const QModelIndex&, int first, int last) {
                for (int i = first; i <= last; ++i) {
                    removeLayer(m_model->layerAt(i));
                }
            });
    connect(m_model, &cwLazLayerModel::modelAboutToBeReset,
            this, [this]() { rebuild(); });
    connect(m_model, &cwLazLayerModel::modelReset,
            this, [this]() { rebuild(); });
}

void cwLazLayersSceneNode::disconnectModel()
{
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
}

void cwLazLayersSceneNode::rebuild()
{
    clear();
    if (!m_model) {
        return;
    }
    for (auto* layer : m_model->layers()) {
        addLayer(layer);
    }
}

void cwLazLayersSceneNode::clear()
{
    for (const auto& item : std::as_const(m_keywordItems)) {
        if (m_keywordItemModel && item) {
            m_keywordItemModel->removeItem(item);
        }
        if (item) {
            item->deleteLater();
        }
    }
    m_keywordItems.clear();

    // QPointer entries auto-null when cwScene tears down first and takes the
    // render objects with it (they are QObject children of the scene). In the
    // steady-state remove path the entry is still live, so unhook from the
    // scene's sync queues before delete.
    for (const auto& renderObject : std::as_const(m_pointClouds)) {
        if (renderObject) {
            renderObject->setScene(nullptr);
            delete renderObject;
        }
    }
    m_pointClouds.clear();
}

void cwLazLayersSceneNode::addLayer(cwLazLayer* layer)
{
    if (!layer || m_pointClouds.contains(layer->id())) {
        return;
    }

    auto* renderObject = new cwRenderPointCloud();
    renderObject->setScene(m_scene);
    m_pointClouds.insert(layer->id(), renderObject);

    if (m_scene.isNull()) {
        qCWarning(lcLazSceneNode) << "addLayer: scene is null — render object has no scene"
                                  << "layer=" << QFileInfo(layer->sourcePath()).fileName();
    }

    // bboxChanged also fires during applyResult, immediately before
    // loadStatusChanged(Loaded). Connecting both would push the geometry
    // twice and rebuild the immutable vertex buffer twice per load.
    connect(layer, &cwLazLayer::loadStatusChanged,
            this, [this, layer]() { syncLayerGeometry(layer); });

    syncLayerGeometry(layer);
    addKeywordItemForLayer(layer);
}

void cwLazLayersSceneNode::removeLayer(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    removeKeywordItemForLayer(layer);
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

void cwLazLayersSceneNode::syncLayerGeometry(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    auto it = m_pointClouds.find(layer->id());
    if (it == m_pointClouds.end()) {
        qCWarning(lcLazSceneNode) << "syncLayerGeometry: no render object for layer"
                                  << QFileInfo(layer->sourcePath()).fileName()
                                  << "— addLayer was never called for this layer.";
        return;
    }
    cwRenderPointCloud* renderObject = it.value();
    if (!renderObject) {
        return;
    }

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

void cwLazLayersSceneNode::addKeywordItemForLayer(cwLazLayer* layer)
{
    if (!m_keywordItemModel || !layer) {
        return;
    }
    if (m_keywordItems.contains(layer->id())) {
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

    m_keywordItems.insert(layer->id(), keywordItem);
}

void cwLazLayersSceneNode::removeKeywordItemForLayer(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    auto it = m_keywordItems.find(layer->id());
    if (it == m_keywordItems.end()) {
        return;
    }
    if (m_keywordItemModel && it.value()) {
        m_keywordItemModel->removeItem(it.value());
    }
    if (it.value()) {
        it.value()->deleteLater();
    }
    m_keywordItems.erase(it);
}
