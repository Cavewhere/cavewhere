#ifndef CWRHIITEMRENDERER_H
#define CWRHIITEMRENDERER_H

//Our includes
#include "cwRhiScene.h"
#include "cwRhiFrameRenderer.h"

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

    // Camera/UBO accessors used by render objects and cwRenderBillboards. They read
    // the composed draw engine directly (cwRhiItemRenderer is a friend of cwRhiScene),
    // so they no longer pass through cwRhiScene's own surface.
    QMatrix4x4 viewMatrix() const { return m_sceneRenderer->m_frame.viewMatrix(); }
    QMatrix4x4 projectionMatrix() const { return m_sceneRenderer->m_frame.projectionMatrix(); }
    QMatrix4x4 viewProjectionMatrix() const { return m_sceneRenderer->m_frame.viewProjectionMatrix(); }
    float devicePixelRatio() const { return m_sceneRenderer->m_frame.devicePixelRatio(); }
    QRhiBuffer* globalUniformBuffer() const { return m_sceneRenderer->m_frame.globalUniformBuffer(); }
    quint32 globalUniformBufferStride() const { return m_sceneRenderer->m_frame.globalUniformBufferStride(); }

    // The shared GPU draw engine. Render objects acquire pipelines, read pass
    // routing, and bind the global UBO through this handle instead of reaching
    // cwRhiScene; the offscreen renderer holds the same engine by reference.
    cwRhiFrameRenderer* frameRenderer() const { return &m_sceneRenderer->m_frame; }

    // Request another frame from within the render thread (wraps the protected
    // QQuickRhiItemRenderer::update()). cwRhiScene calls this to spread offscreen
    // renders across frames and to flush pending texture read-backs. Guarded by
    // cwRhiScene so it isn't called unconditionally (that would loop forever).
    void requestUpdate() { update(); }

protected:
    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    cwRhiScene* m_sceneRenderer;
};

#endif // CWRHIITEMRENDERER_H
