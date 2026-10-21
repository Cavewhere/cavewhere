#include "cwRhiItemRenderer.h"
#include "cwRhiViewer.h"

//Qt include
#include <QMutexLocker>
#include <QThread>
#include <QDebug>

void cwRhiRendererHandle::attach(cwRhiItemRenderer *renderer)
{
    QMutexLocker locker(&m_mutex);
    m_renderer = renderer;
}

void cwRhiRendererHandle::detach(const cwRhiItemRenderer *renderer)
{
    QMutexLocker locker(&m_mutex);
    if(m_renderer == renderer) {
        m_renderer = nullptr;
    }
}

void cwRhiRendererHandle::setViewVisible(bool visible)
{
    QMutexLocker locker(&m_mutex);
    m_viewVisible = visible;
}

void cwRhiRendererHandle::releaseStreamedResources()
{
    QMutexLocker locker(&m_mutex);
    if(m_viewVisible || m_renderer == nullptr) {
        return;
    }
    m_renderer->releaseStreamedResources();
}

cwRhiItemRenderer::cwRhiItemRenderer() :
    m_sceneRenderer(new cwRhiScene())
{
}

cwRhiItemRenderer::~cwRhiItemRenderer() {
    if(m_handle) {
        m_handle->detach(this);
    }
    delete m_sceneRenderer;
}

void cwRhiItemRenderer::initialize(QRhiCommandBuffer *cb) {
    m_sceneRenderer->initialize(cb, this);
}

void cwRhiItemRenderer::synchronize(QQuickRhiItem *item){
    //Call synchronize for all elements
    auto viewerItem = static_cast<cwRhiViewer*>(item);
    Q_ASSERT(dynamic_cast<cwRhiViewer*>(item) != nullptr);

    // The sync barrier is the one moment both threads are lined up, so it is
    // where this renderer publishes itself to the viewer's handle.
    if(!m_handle) {
        m_handle = viewerItem->rendererHandle();
        m_handle->attach(this);
    }

    m_sceneRenderer->synchroize(viewerItem->scene(), this);
}

void cwRhiItemRenderer::render(QRhiCommandBuffer *cb) {
    //Go through all the rendering objects
    m_sceneRenderer->render(cb, this);

    //This cause the item to redraw ever frame
    // update();

}
