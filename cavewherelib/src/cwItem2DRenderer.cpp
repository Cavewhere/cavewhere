#include "cwItem2DRenderer.h"

//Qt private API — the 2D-in-3D inline render path.
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qsgrenderer_p.h>
#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/QSGRendererInterface>

//Qt includes
#include <QQuickWindow>
#include <QPointer>

struct cwItem2DRenderer::Impl
{
    ~Impl()
    {
        delete renderer;
    }

    QPointer<QQuickWindow> window;
    QSGRenderer* renderer = nullptr;
};

cwItem2DRenderer::cwItem2DRenderer(QQuickWindow* window)
    : d(std::make_unique<Impl>())
{
    d->window = window;
}

cwItem2DRenderer::~cwItem2DRenderer() = default;

bool cwItem2DRenderer::prepare(const Frame& frame)
{
    if (!d->window || !frame.rt || !frame.rpd || !frame.cb || frame.rootNodeHandle == 0) {
        return false;
    }

    auto* windowPrivate = QQuickWindowPrivate::get(d->window);
    if (!windowPrivate || !windowPrivate->context) {
        return false;
    }

    auto* rootNode = reinterpret_cast<QSGRootNode*>(frame.rootNodeHandle);

    if (!d->renderer) {
        d->renderer = windowPrivate->context->createRenderer(QSGRendererInterface::RenderMode3D);
        if (!d->renderer) {
            return false;
        }
    }
    if (d->renderer->rootNode() != rootNode) {
        d->renderer->setRootNode(rootNode);
    }

    // The content root node has an identity transform, so the renderer's
    // "projection matrix" carries the full MVP. viewProjection is already
    // clip-space corrected (matching the cave geometry), so depths are comparable.
    const QRect deviceRect(QPoint(0, 0), frame.deviceRect);
    d->renderer->setDevicePixelRatio(frame.devicePixelRatio);
    d->renderer->setProjectionMatrix(frame.mvp);
    d->renderer->setViewportRect(deviceRect);
    d->renderer->setDeviceRect(deviceRect);
    d->renderer->setRenderTarget(QSGRenderTarget(frame.rt, frame.rpd, frame.cb));
    d->renderer->prepareSceneInline();
    return true;
}

void cwItem2DRenderer::render()
{
    if (d->renderer) {
        d->renderer->renderSceneInline();
    }
}
