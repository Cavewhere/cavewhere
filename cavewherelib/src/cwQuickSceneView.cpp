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
            disconnect(Scene.data(), nullptr, this, nullptr);
        }

        Scene = scene;

        if(!Scene.isNull()) {
            connect(Scene.data(), &QGraphicsScene::changed, this, [this](const QList<QRectF>& region) {
                Q_UNUSED(region);
                auto size = boundingRect().size();
                // qDebug() << "size:" << size << boundingRect() << Scene->sceneRect();
                if(m_image.size() != size) {
                    m_image = QImage(size.toSize(), QImage::Format_ARGB32);
                }

                m_image.fill(Qt::transparent);

                QPainter painter(&m_image);
                Scene->render(&painter, boundingRect(), Scene->sceneRect(), Qt::KeepAspectRatio);
                update();

            });

            emit sceneChanged();
        }
    }
}

QRectF cwQuickSceneView::toView(QRectF paperRect) const
{
    QTransform transform = toViewTransform();
    return transform.mapRect(paperRect);
}

QPointF cwQuickSceneView::toPaper(QPointF pointPixel) const
{
    QTransform transform = toViewTransform().inverted();
    return transform.map(pointPixel);
}

double cwQuickSceneView::viewScale() const
{
    if(Scene) {
        auto boundingRect = this->boundingRect();
        auto sceneRect = Scene->sceneRect();

        // Scale
        qreal xratio = boundingRect.width()  / sceneRect.width();
        qreal yratio = boundingRect.height() / sceneRect.height();
        return qMin(xratio, yratio);
    } else {
        return 1.0;
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
    qDebug() << "Render!";
    //This should probably be drawn to a texture instead of copied to another QImage in the backend
    //Using the Scene here directly doesn't work because of threading and qtimer issue and will cause
    //high CPU usage in a QTimer that's internal to the Scene. Qt doc warns against this
    painter->drawImage(QPoint(0,0), m_image);
}

QTransform cwQuickSceneView::toViewTransform() const
{
    QTransform transform;
    // Shift target origin
    auto boundingRect = this->boundingRect();
    auto sceneRect = Scene->sceneRect();
    auto topLeft = boundingRect.topLeft();
    transform.translate(topLeft.x(), topLeft.y());

    // qDebug() << "Scene:" << Scene->sceneRect();
    auto scale = viewScale();
    transform.scale(scale, scale);

    // Shift so (0,0) in sceneRect is at origin
    auto sceneTopLeft = sceneRect.topLeft();
    transform.translate(sceneTopLeft.x(), sceneTopLeft.y());

    return transform;
}
