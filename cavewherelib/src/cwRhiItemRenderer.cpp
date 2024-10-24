#include "cwRhiItemRenderer.h"
#include "cwGLViewer.h"

cwRhiItemRenderer::cwRhiItemRenderer() :
    m_sceneRenderer(new cwSceneRenderer())
{

}

void cwRhiItemRenderer::initialize(QRhiCommandBuffer *cb) {
    m_sceneRenderer->initialize(cb, this);
}

void cwRhiItemRenderer::synchronize(QQuickRhiItem *item){
    //Call synchronize for all elements
    auto viewerItem = static_cast<cwGLViewer*>(item);
    Q_ASSERT(dynamic_cast<cwGLViewer*>(item) != nullptr);

    m_sceneRenderer->synchroize(viewerItem->scene(), this);
}

void cwRhiItemRenderer::render(QRhiCommandBuffer *cb) {
    //Go through all the rendering objects
    m_sceneRenderer->render(cb, this);

    //This cause the item to redraw ever frame
    update();

}
