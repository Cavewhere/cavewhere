#ifndef CWRHIITEMRENDERER_H
#define CWRHIITEMRENDERER_H

//Our includes
#include "cwRhiScene.h"

//Qt includes
#include <QQuickRhiItemRenderer>

class cwRhiItemRenderer : public QQuickRhiItemRenderer
{
public:
    cwRhiItemRenderer();
    ~cwRhiItemRenderer() override;


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

    QMatrix4x4 viewMatrix() const { return m_sceneRenderer->viewMatrix(); }
    QMatrix4x4 projectionMatrix() const { return m_sceneRenderer->projectionMatrix(); }
    QMatrix4x4 viewProjectionMatrix() const { return m_sceneRenderer->viewProjectionMatrix(); }
    float devicePixelRatio() const { return m_sceneRenderer->devicePixelRatio(); }
    QRhiBuffer* globalUniformBuffer() const { return m_sceneRenderer->globalUniformBuffer(); }

protected:
    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    cwRhiScene* m_sceneRenderer;
};

#endif // CWRHIITEMRENDERER_H
