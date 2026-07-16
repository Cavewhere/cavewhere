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

    // The global camera UBO and its per-slot stride. Render objects bind the UBO and
    // select a camera slot via the stride; the per-job camera itself reaches CPU-side
    // draws (cwRenderBillboards) through cwRHIObject::RenderData, not the renderer —
    // the live frame renderer deliberately no longer exposes its on-screen camera, so
    // an offscreen render can't pick up the wrong camera.
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
