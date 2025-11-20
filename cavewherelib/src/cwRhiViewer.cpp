/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwRhiViewer.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwRhiItemRenderer.h"

//Qt includes
#include <QPainter>
#include <QRect>
#include <QDebug>
#include <QTimer>
#include <QSGRenderNode>
#include <QQuickWindow>


cwRhiViewer::cwRhiViewer(QQuickItem *parent)
{
    Camera = new cwCamera(this);

    connect(this, SIGNAL(widthChanged()), SLOT(privateResize()));
    connect(this, SIGNAL(heightChanged()), SLOT(privateResize()));
    connect(Camera, SIGNAL(viewMatrixChanged()), SLOT(updateRenderer()));
    connect(Camera, SIGNAL(projectionChanged()), SLOT(updateRenderer()));
}

cwRhiViewer::~cwRhiViewer() {
}

void cwRhiViewer::privateResize() {
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

QQuickRhiItemRenderer *cwRhiViewer::createRenderer()
{
    return new cwRhiItemRenderer();
}


/**
 * @brief cwRhiViewer::setScene
 * @param scene
 */
void cwRhiViewer::setScene(cwScene* scene) {
    if(Scene != scene) {

        if(Scene != nullptr) {
            disconnect(Scene.data(), &cwScene::needsRendering, this, &cwRhiViewer::updateRenderer);
        }

        Scene = scene;

        if(Scene != nullptr) {
            Scene->setCamera(Camera);
            connect(Scene.data(), &cwScene::needsRendering, this, &cwRhiViewer::updateRenderer);
        }

        emit sceneChanged();
    }
}

/**
 * @brief cwRhiViewer::scene
 * @return
 */
cwScene* cwRhiViewer::scene() const {
    return Scene;
}
