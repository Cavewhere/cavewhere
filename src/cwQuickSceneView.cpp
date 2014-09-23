/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QGraphicsScene>
#include <QPainter>

//Our includes
#include "cwQuickSceneView.h"

cwQuickSceneView::cwQuickSceneView(QQuickItem *parent) :
    QQuickPaintedItem(parent)
{
    setRenderTarget(FramebufferObject);
}

/**
* @brief cwQuickSceneView::setScene
* @param scene
*/
void cwQuickSceneView::setScene(QGraphicsScene* scene) {
    if(Scene != scene) {
        if(!Scene.isNull()) {
            disconnect(Scene.data(), SIGNAL(changed(const QList<QRectF>)), this, SLOT(update()));
        }

        Scene = scene;

        if(!Scene.isNull()) {
            connect(Scene.data(), SIGNAL(changed(const QList<QRectF>)), this, SLOT(update()));
        }
        emit sceneChanged();
    }
}

/**
* @brief cwQuickSceneView::scene
* @return
*/
QGraphicsScene* cwQuickSceneView::scene() const {
    return Scene;
}

/**
 * @brief cwQuickSceneView::paint
 * @param painter
 */
void cwQuickSceneView::paint(QPainter *painter)
{
    if(Scene.isNull()) { return; }
    Scene->render(painter);
}
