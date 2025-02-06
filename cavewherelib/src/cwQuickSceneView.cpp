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

    QBindable<double> widthBinding(this, "width");
    QBindable<double> heightBinding(this, "height");
    m_viewRect.setBinding([widthBinding, heightBinding]() {
        return QRectF(0.0, 0.0, widthBinding.value(), heightBinding.value());
    });

    m_viewScale.setBinding([this]()->double {
        // Scale
        const auto viewRect = m_viewRect.value();
        const auto sceneRect = m_sceneRect.value();

        double xRatio = viewRect.width() / sceneRect.width();
        double yRatio = viewRect.height() / sceneRect.height();

        return std::min(xRatio, yRatio);
    });

    m_viewTransform.setBinding([this, widthBinding, heightBinding]() {
        QTransform transform;

        // Shift target origin
        auto topLeft = m_viewRect.value().topLeft();
        transform.translate(topLeft.x(), topLeft.y());

        // qDebug() << "Scene:" << Scene->sceneRect();
        double scale = m_viewScale;
        transform.scale(scale, scale);

        // Shift so (0,0) in sceneRect is at origin
        auto sceneTopLeft = m_sceneRect.value().topLeft();
        transform.translate(sceneTopLeft.x(), sceneTopLeft.y());

        return transform;
    });
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

            connect(Scene.data(), &QGraphicsScene::sceneRectChanged, this, [this](const QRectF& sceneRect) {
                m_sceneRect = sceneRect;
            });

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
        } else {
            m_sceneRect = QRectF();
        }
    }
}

QRectF cwQuickSceneView::toView(QRectF paperRect) const
{
    QTransform transform = toViewTransform();
    return transform.mapRect(paperRect);
}

QPointF cwQuickSceneView::toView(QPointF paperPoint) const
{
    QTransform transform = toViewTransform();
    return transform.map(paperPoint);
}

QPointF cwQuickSceneView::toPaper(QPointF pointPixel) const
{
    QTransform transform = toViewTransform().inverted();
    return transform.map(pointPixel);
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
    //This should probably be drawn to a texture instead of copied to another QImage in the backend
    //Using the Scene here directly doesn't work because of threading and qtimer issue and will cause
    //high CPU usage in a QTimer that's internal to the Scene. Qt doc warns against this
    painter->drawImage(QPoint(0,0), m_image);
}

QTransform cwQuickSceneView::toViewTransform() const
{
    return m_viewTransform;
}
