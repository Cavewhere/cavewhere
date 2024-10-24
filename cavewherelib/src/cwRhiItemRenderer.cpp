#include "cwRhiItemRenderer.h"
#include "cwRhiViewer.h"

//Qt include
#include <QThread>
#include <QDebug>

cwRhiItemRenderer::cwRhiItemRenderer() :
    m_sceneRenderer(new cwRhiScene())
{
}

cwRhiItemRenderer::~cwRhiItemRenderer() {
    delete m_sceneRenderer;
}

void cwRhiItemRenderer::initialize(QRhiCommandBuffer *cb) {
    m_sceneRenderer->initialize(cb, this);
}

void cwRhiItemRenderer::synchronize(QQuickRhiItem *item){
    //Call synchronize for all elements
    auto viewerItem = static_cast<cwRhiViewer*>(item);
    Q_ASSERT(dynamic_cast<cwRhiViewer*>(item) != nullptr);

    m_sceneRenderer->synchroize(viewerItem->scene(), this);
}

void cwRhiItemRenderer::render(QRhiCommandBuffer *cb) {
    //Go through all the rendering objects
    m_sceneRenderer->render(cb, this);

    //This cause the item to redraw ever frame
    update();

}
