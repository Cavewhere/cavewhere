#ifndef CWRHIITEMRENDERER_H
#define CWRHIITEMRENDERER_H

#include "cwScene.h"
#include <QQuickRhiItemRenderer>

class cwRhiItemRenderer : public QQuickRhiItemRenderer
{
public:
    cwRhiItemRenderer();
    ~cwRhiItemRenderer() override = default;


    // Public pass-through functions
    QRhiTexture *colorTexture() const {
        return QQuickRhiItemRenderer::colorTexture();
    }

    QRhiRenderBuffer *depthStencilBuffer() const {
        return QQuickRhiItemRenderer::depthStencilBuffer();
    }

    QRhiRenderBuffer *msaaColorBuffer() const {
        return QQuickRhiItemRenderer::msaaColorBuffer();
    }

    QRhiRenderTarget *renderTarget() const {
        return QQuickRhiItemRenderer::renderTarget();
    }

    QRhiTexture *resolveTexture() const {
        return QQuickRhiItemRenderer::resolveTexture();
    }

    QRhi *rhi() const {
        return QQuickRhiItemRenderer::rhi();
    }

protected:
    void initialize(QRhiCommandBuffer *cb);
    void synchronize(QQuickRhiItem *item);
    void render(QRhiCommandBuffer *cb);

private:
    cwSceneRenderer* m_sceneRenderer;
};

#endif // CWRHIITEMRENDERER_H
