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

#include <algorithm>

Q_LOGGING_CATEGORY(lcLazSceneNode, "cw.laz.scenenode")

namespace {
    // World-space sprite radius bounds in meters. Lower bound covers
    // sub-decimeter sprites for high-density scans; upper bound prevents
    // a runaway multiplicative P+wheel from turning the render into one
    // fat blob. sink_repatcher --point-radius clamps to the same range.
    constexpr float kMinWorldRadius = 0.01f;
    constexpr float kMaxWorldRadius = 50.0f;

QString shortId(const cwLazLayer* layer) {
    return layer == nullptr
        ? QStringLiteral("(null)")
        : layer->id().toString(QUuid::WithoutBraces).left(8);
}
QString fileName(const cwLazLayer* layer) {
    return layer == nullptr
        ? QStringLiteral("(null)")
        : QFileInfo(layer->sourcePath()).fileName();
}
constexpr float kMinGapFudge = 1.0f;
constexpr float kMaxGapFudge = 10.0f;
} // namespace

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
    if (m_keywordRegistry.model() == keywordItemModel) {
        return;
    }
    m_keywordRegistry.setModel(keywordItemModel);
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
    // modelAboutToBeReset gets the lightweight teardown only — at that point
    // the model still holds the about-to-be-qDeleteAll'd layers, so calling
    // rebuild() (which iterates m_model->layers()) would materialize render
    // objects for layers the model is about to destroy, only to tear them
    // down again on the trailing modelReset. clear() drops current state
    // without touching the layer list.
    connect(m_model, &cwLazLayerModel::modelAboutToBeReset,
            this, [this]() { clear(); });
    connect(m_model, &cwLazLayerModel::modelReset,
            this, [this]() { rebuild(); });
}

void cwLazLayersSceneNode::disconnectModel()
{
    if (!m_model) {
        return;
    }
    // Sever per-layer connections BEFORE the model swap so a subsequent
    // setEnabled() on a layer that belonged to the previous model cannot
    // reach onEnabledChanged() and materialize an orphan render object into
    // m_pointClouds. clear()/rebuild() don't walk layers, so the layer-side
    // connect installed by addLayer() would otherwise outlive the swap.
    for (cwLazLayer* layer : m_model->layers()) {
        if (layer) {
            disconnect(layer, nullptr, this, nullptr);
        }
    }
    disconnect(m_model, nullptr, this, nullptr);
}

void cwLazLayersSceneNode::rebuild()
{
    qCDebug(lcLazSceneNode) << "rebuild: BEGIN model=" << (m_model.isNull() ? "(null)" : "set")
                            << "currentPointClouds=" << m_pointClouds.size();
    clear();
    if (!m_model) {
        qCDebug(lcLazSceneNode) << "rebuild: END (no model)";
        return;
    }
    for (auto* layer : m_model->layers()) {
        addLayer(layer);
    }
    qCDebug(lcLazSceneNode) << "rebuild: END pointClouds=" << m_pointClouds.size();
}

void cwLazLayersSceneNode::clear()
{
    qCDebug(lcLazSceneNode) << "clear: pointClouds=" << m_pointClouds.size();
    // Drop the keyword items before the point clouds they target via setObject().
    m_keywordRegistry.clear();

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
    if (!layer) {
        qCDebug(lcLazSceneNode) << "addLayer: skip (null layer)";
        return;
    }
    if (m_pointClouds.contains(layer->id())) {
        qCDebug(lcLazSceneNode) << "addLayer: skip (already present)"
                                << shortId(layer) << fileName(layer);
        return;
    }
    qCDebug(lcLazSceneNode) << "addLayer:" << shortId(layer) << fileName(layer)
                            << "enabled=" << layer->enabled();

    // The enabled hook-up lives on the layer regardless of materialization
    // state so that flipping enabled later can dispatch through this node.
    // Member-function connect (not a lambda) so Qt::UniqueConnection works —
    // a re-addLayer() on the same layer is then idempotent rather than
    // accumulating duplicate slots.
    connect(layer, &cwLazLayer::enabledChanged,
            this, &cwLazLayersSceneNode::onEnabledChanged,
            Qt::UniqueConnection);

    if (layer->enabled()) {
        materialize(layer);
    }
}

void cwLazLayersSceneNode::removeLayer(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    qCDebug(lcLazSceneNode) << "removeLayer:" << shortId(layer) << fileName(layer);
    if (m_pointClouds.contains(layer->id())) {
        dematerialize(layer);
    }
    disconnect(layer, nullptr, this, nullptr);
}

void cwLazLayersSceneNode::materialize(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    if (m_pointClouds.contains(layer->id())) {
        qCDebug(lcLazSceneNode) << "materialize: skip (already present)"
                                << shortId(layer) << fileName(layer);
        return;
    }
    qCDebug(lcLazSceneNode) << "materialize:" << shortId(layer) << fileName(layer);

    auto* renderObject = new cwRenderPointCloud();
    renderObject->setWorldRadius(m_worldRadius);
    renderObject->setScene(m_scene);
    m_pointClouds.insert(layer->id(), renderObject);

    if (m_scene.isNull()) {
        qCWarning(lcLazSceneNode) << "materialize: scene is null — render object has no scene"
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

void cwLazLayersSceneNode::dematerialize(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    qCDebug(lcLazSceneNode) << "dematerialize:" << shortId(layer) << fileName(layer);
    removeKeywordItemForLayer(layer);
    auto it = m_pointClouds.find(layer->id());
    if (it == m_pointClouds.end()) {
        return;
    }
    // Drop the loadStatusChanged hook-up before tearing down the render object
    // so a late-arriving signal can't look up a destroyed pointer. The
    // enabledChanged hook-up must survive — disconnecting per-signal preserves
    // it while clearing the geometry sync.
    disconnect(layer, &cwLazLayer::loadStatusChanged, this, nullptr);
    cwRenderPointCloud* renderObject = it.value();
    renderObject->setScene(nullptr);
    delete renderObject;
    m_pointClouds.erase(it);
}

void cwLazLayersSceneNode::onEnabledChanged()
{
    auto* layer = qobject_cast<cwLazLayer*>(sender());
    if (!layer) {
        return;
    }
    qCDebug(lcLazSceneNode) << "onEnabledChanged:" << shortId(layer) << fileName(layer)
                            << "enabled=" << layer->enabled();
    if (layer->enabled()) {
        materialize(layer);
    } else {
        dematerialize(layer);
    }
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
    if (m_keywordRegistry.model() == nullptr || !layer) {
        qCDebug(lcLazSceneNode) << "addKeywordItemForLayer: skip"
                                << "model=" << (m_keywordRegistry.model() == nullptr ? "(null)" : "set")
                                << "layer=" << shortId(layer);
        return;
    }
    auto pointCloudIt = m_pointClouds.constFind(layer->id());
    if (pointCloudIt == m_pointClouds.constEnd()) {
        qCDebug(lcLazSceneNode) << "addKeywordItemForLayer: skip (no point cloud)"
                                << shortId(layer);
        return;
    }

    cwRenderPointCloud* pointCloud = pointCloudIt.value();
    auto* keywordItem = m_keywordRegistry.ensure(layer->id(), [layer, pointCloud]() {
        auto* item = new cwKeywordItem();
        item->keywordModel()->addExtension(layer->keywordModel());
        item->setObject(pointCloud);
        return item;
    });
    qCDebug(lcLazSceneNode) << "addKeywordItemForLayer:" << shortId(layer)
                            << "item=" << static_cast<void*>(keywordItem)
                            << "object=" << static_cast<void*>(pointCloud);
}

void cwLazLayersSceneNode::removeKeywordItemForLayer(cwLazLayer* layer)
{
    if (!layer) {
        return;
    }
    qCDebug(lcLazSceneNode) << "removeKeywordItemForLayer:" << shortId(layer);
    m_keywordRegistry.drop(layer->id());
}

void cwLazLayersSceneNode::setWorldRadius(float worldRadius)
{
    const float clamped = std::clamp(worldRadius, kMinWorldRadius, kMaxWorldRadius);
    if (qFuzzyCompare(m_worldRadius, clamped)) {
        return;
    }
    m_worldRadius = clamped;

    for (const auto& renderObject : std::as_const(m_pointClouds)) {
        if (renderObject) {
            renderObject->setWorldRadius(clamped);
        }
    }

    emit worldRadiusChanged(clamped);
}
