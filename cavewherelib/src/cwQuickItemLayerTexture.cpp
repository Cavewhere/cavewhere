/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwQuickItemLayerTexture.h"
#include "cwRenderObject.h"

//Qt private API: the QSGLayer behind `layer.enabled`, created by the window's
//scene-graph context.
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuick/private/qsgcontext_p.h>

//Qt includes
#include <QQuickWindow>
#include <QRectF>
#include <QSGNode>
#include <QtMath>

struct cwQuickItemLayerTexture::Impl
{
    ~Impl()
    {
        delete layer;
    }

    QSGLayer* layer = nullptr;
    bool hasItem = false;
    bool hasSize = false;
};

cwQuickItemLayerTexture::cwQuickItemLayerTexture(QQuickWindow* window, cwRenderObject* updateReceiver)
    : d(std::make_unique<Impl>())
{
    if (window == nullptr) {
        return;
    }

    auto* windowPrivate = QQuickWindowPrivate::get(window);
    QSGRenderContext* renderContext = windowPrivate ? windowPrivate->context : nullptr;
    if (renderContext == nullptr || !renderContext->isValid()) {
        return;
    }

    d->layer = renderContext->sceneGraphContext()->createLayer(renderContext);
    if (d->layer == nullptr) {
        return;
    }

    // Live: the layer re-renders itself when the subtree changes. RGBA8 is the
    // premultiplied format the scene graph's materials write. Vertical mirroring
    // stays on, which is what gives the texture an Image's orientation (row 0 is
    // the item's top), so a (0, 0) uv samples the item's top-left corner.
    d->layer->setLive(true);
    d->layer->setRecursive(false);
    d->layer->setFormat(QSGLayer::RGBA8);
    d->layer->setHasMipmaps(false);
    d->layer->setMirrorHorizontal(false);
    d->layer->setMirrorVertical(true);

    if (updateReceiver != nullptr) {
        // The receiver lives on the GUI thread, so Qt queues the call and drops
        // the connection when the receiver is destroyed.
        QObject::connect(d->layer, &QSGLayer::updateRequested,
                         updateReceiver, [updateReceiver]() { updateReceiver->update(); });
    }
}

cwQuickItemLayerTexture::~cwQuickItemLayerTexture() = default;

bool cwQuickItemLayerTexture::isValid() const
{
    return d->layer != nullptr;
}

void cwQuickItemLayerTexture::setRootNodeHandle(quintptr rootNodeHandle)
{
    if (d->layer == nullptr) {
        return;
    }
    d->layer->setItem(reinterpret_cast<QSGNode*>(rootNodeHandle));
    d->hasItem = rootNodeHandle != 0;
}

void cwQuickItemLayerTexture::setContentSize(const QSizeF& logicalSize, float devicePixelRatio)
{
    if (d->layer == nullptr) {
        return;
    }
    const QSize pixelSize(qCeil(logicalSize.width() * devicePixelRatio),
                          qCeil(logicalSize.height() * devicePixelRatio));
    d->layer->setRect(QRectF(QPointF(0.0, 0.0), logicalSize));
    d->layer->setSize(pixelSize);
    d->layer->setDevicePixelRatio(devicePixelRatio);
    d->hasSize = !pixelSize.isEmpty();
}

void cwQuickItemLayerTexture::updateTexture()
{
    if (d->layer == nullptr || !d->hasItem || !d->hasSize) {
        return;
    }
    // A no-op while the layer is clean; otherwise one render pass of the subtree
    // on the current frame's command buffer.
    d->layer->updateTexture();
}

QRhiTexture* cwQuickItemLayerTexture::rhiTexture() const
{
    if (d->layer == nullptr) {
        return nullptr;
    }
    return d->layer->rhiTexture();
}
