/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwGLViewer.h"
#include "cwCamera.h"
#include "cwShaderDebugger.h"
#include "cwScene.h"
#include "cwRhiItemRenderer.h"

//Qt includes
#include <QPainter>
#include <QRect>
#include <QDebug>
#include <QTimer>
#include <QSGRenderNode>
#include <QQuickWindow>

//Std includes
#include "cwMath.h"

// class cwRender : public QQuickRhiItemRenderer {
// public:


//     // QQuickRhiItemRenderer interface
// protected:
//     void initialize(QRhiCommandBuffer *cb) {

//     }

//     void synchronize(QQuickRhiItem *item){
//         //Call synchronize for all elements
//         auto viewerItem = static_cast<cwGLViewer*>(item);

//         m_sceneRenderer->synchroize(viewerItem->scene());
//     }

//     void render(QRhiCommandBuffer *cb) {
//         //Go through all the rendering objects
//         m_sceneRenderer->render(cb);

//     }

// private:
//     cwSceneRenderer* m_sceneRenderer = nullptr;

// };


cwGLViewer::cwGLViewer(QQuickItem *parent)
{
//    GLWidget = nullptr;
    setFlag(QQuickItem::ItemHasContents, true);

//    GeometryItersecter = new cwGeometryItersecter();
    Camera = new cwCamera(this);

    connect(this, SIGNAL(widthChanged()), SLOT(privateResize()));
    connect(this, SIGNAL(heightChanged()), SLOT(privateResize()));
    connect(Camera, SIGNAL(viewMatrixChanged()), SLOT(updateRenderer()));
    connect(Camera, SIGNAL(projectionChanged()), SLOT(updateRenderer()));

    setSampleCount(4);

    // setRenderTarget(QQuickPaintedItem::InvertedYFramebufferObject);
    // setAntialiasing(false);
    // setOpaquePainting(false);
}

cwGLViewer::~cwGLViewer() {
}

void cwGLViewer::privateResize() {
//    if(GLWidget == nullptr) { return; }
    if(width() == 0.0 || height() == 0.0) { return; }
    QSize framebufferSize(width(), height());

    //Update the viewport
    QRect viewportRect(QPoint(0, 0), framebufferSize);
    Camera->setViewport(viewportRect);

    //Update the subclass
    resizeGL();

    update();
}

QQuickRhiItemRenderer *cwGLViewer::createRenderer()
{
    return new cwRhiItemRenderer();
}

/**
  \brief This paints the cwGLRender and the linePlot labels
  */
// void cwGLViewer::paint(QPainter * painter) {
//     if(Scene.isNull()) { return; }

//     painter->beginNativePainting();

//     Scene->setCamera(camera());
//     Scene->paint();

//     painter->endNativePainting();
// }

// void cwGLViewer::releaseResources()
// {
//     Scene->releaseResources();
//     QQuickPaintedItem::releaseResources();
// }

// QSGNode *cwGLViewer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
// {

//     if(!oldNode) {

//     }
// }


/**
 * @brief cwGLViewer::setScene
 * @param scene
 */
void cwGLViewer::setScene(cwScene* scene) {
    if(Scene != scene) {

        if(Scene != nullptr) {
            disconnect(Scene.data(), &cwScene::needsRendering, this, &cwGLViewer::updateRenderer);
        }

        Scene = scene;

        if(Scene != nullptr) {
            Scene->setCamera(Camera);
            connect(Scene.data(), &cwScene::needsRendering, this, &cwGLViewer::updateRenderer);
        }

        emit sceneChanged();
    }
}

/**
 * @brief cwGLViewer::scene
 * @return
 */
cwScene* cwGLViewer::scene() const {
    return Scene;
}
