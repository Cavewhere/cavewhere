#ifndef CWRHIITEMRENDERER_H
#define CWRHIITEMRENDERER_H

//Our includes
#include "cwRhiScene.h"
#include "cwRhiFrameRenderer.h"

//Qt includes
#include <QMutex>
#include <QQuickRhiItemRenderer>
#include <memory>

class cwRhiItemRenderer;

/**
 * @brief The cwRhiRendererHandle class
 *
 * The GUI thread's handle on a render thread's cwRhiItemRenderer.
 *
 * Qt creates and destroys the renderer on the render thread whenever the scene
 * graph decides to, so a cwRhiViewer can't hold a raw pointer to it. The viewer
 * owns one of these by shared_ptr and hands copies to the render jobs it
 * schedules; the renderer attaches itself at its first synchronize and detaches
 * as it is destroyed, so a job that runs after the renderer is gone does nothing.
 */
class cwRhiRendererHandle
{
public:
    void attach(cwRhiItemRenderer* renderer);
    //! Clears the handle only when @a renderer is still the attached one, so a
    //! replaced renderer's teardown can't unhook its successor
    void detach(const cwRhiItemRenderer* renderer);

    //! Tracks whether the viewer is drawing, so a hide immediately followed by a
    //! show leaves the already scheduled release job with nothing to do
    void setViewVisible(bool visible);

    //! Runs cwRhiItemRenderer::releaseStreamedTextures on the attached renderer,
    //! or nothing when the viewer is visible again or no renderer is attached
    void releaseStreamedTextures();

private:
    QMutex m_mutex;
    cwRhiItemRenderer* m_renderer = nullptr;
    bool m_viewVisible = true;
};

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

    // Release every streamed texture in this renderer's scene from the GPU.
    // Render thread only, from the job the viewer schedules when it is hidden —
    // see cwRhiViewer::itemChange.
    void releaseStreamedTextures() { m_sceneRenderer->releaseStreamedTextures(); }

protected:
    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    cwRhiScene* m_sceneRenderer;
    //! The viewer's handle this renderer attached itself to, kept so the
    //! destructor can detach without knowing the viewer
    std::shared_ptr<cwRhiRendererHandle> m_handle;
};

#endif // CWRHIITEMRENDERER_H
