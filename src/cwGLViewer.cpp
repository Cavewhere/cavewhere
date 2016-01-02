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

//Qt includes
#include <QPainter>
#include <QRect>
#include <QDebug>
#include <QGLWidget>

//Std includes
#include "cwMath.h"


cwGLViewer::cwGLViewer(QQuickItem *parent) :
    QQuickPaintedItem(parent)
//    Initialized(false)
{
//    GLWidget = nullptr;
    setFlag(QQuickItem::ItemHasContents, true);

//    GeometryItersecter = new cwGeometryItersecter();
    Camera = new cwCamera(this);

    connect(this, SIGNAL(widthChanged()), SLOT(privateResizeGL()));
    connect(this, SIGNAL(heightChanged()), SLOT(privateResizeGL()));
    connect(Camera, SIGNAL(viewMatrixChanged()), SLOT(updateRenderer()));
    connect(Camera, SIGNAL(projectionChanged()), SLOT(updateRenderer()));

    setRenderTarget(QQuickPaintedItem::InvertedYFramebufferObject);
    setAntialiasing(false);
    setOpaquePainting(false);
}

cwGLViewer::~cwGLViewer() {
//    delete GeometryItersecter;
    Camera->deleteLater();
//    ShaderDebugger->deleteLater();
}

void cwGLViewer::privateResizeGL() {
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

/**
  \brief This paints the cwGLRender and the linePlot labels
  */
void cwGLViewer::paint(QPainter * painter) {
    if(Scene.isNull()) { return; }

    painter->beginNativePainting();

    Scene->setCamera(camera());
    Scene->paint();

    painter->endNativePainting();
}


//QSGNode * cwGLRenderer::updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *data) {
//    if(!Initialized) {
//        initializeGL();
//        Initialized = true;
//    }
//    return QQuickPaintedItem::updatePaintNode(oldNode, data);
//}

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
