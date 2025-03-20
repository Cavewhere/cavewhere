/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWQUICKSCENEVIEW_H
#define CWQUICKSCENEVIEW_H

//Qt includes
#include <QQuickPaintedItem>
#include <QPointer>
#include <QQmlEngine>
#include <QImage>
#include <QObjectBindableProperty>
#include <QProperty>
class QGraphicsScene;


/**
 * @brief The cwQuickSceneView class
 *
 * This class renders a QGraphisScene to a QQuickItem
 */
class cwQuickSceneView : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(QuickSceneView)

    Q_PROPERTY(QGraphicsScene* scene READ scene WRITE setScene NOTIFY sceneChanged)

    Q_PROPERTY(QTransform viewTransform READ viewTransform NOTIFY viewTransformChanged BINDABLE bindableViewTransform)
    Q_PROPERTY(double viewScale READ viewScale  NOTIFY viewScaleChanged BINDABLE bindableViewScale)

public:
    explicit cwQuickSceneView(QQuickItem *parent = 0);

    QGraphicsScene* scene() const;
    void setScene(QGraphicsScene* scene);

    // QPointF toView(QPointF paperPoint) const;
    QTransform viewTransform() const { return m_viewTransform.value(); }
    QBindable<QTransform> bindableViewTransform() { return &m_viewTransform; }

    //Multipling paper values by this scale give you pixels
    double viewScale() const { return m_viewScale.value(); }
    QBindable<double> bindableViewScale() { return &m_viewScale; }

    double paperScale() const { return 1.0 / m_viewScale.value(); }

    Q_INVOKABLE QRectF toView(QRectF paperRect) const;
    Q_INVOKABLE QPointF toView(QPointF paperPoint) const;
    Q_INVOKABLE QPointF toPaper(QPointF pointPixels) const;

    virtual void paint(QPainter * painter);

signals:
    void sceneChanged();
    void viewTransformChanged();
    void viewScaleChanged();
    void paperScaleChanged();

public slots:

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwQuickSceneView, QTransform, m_viewTransform, QTransform(), &cwQuickSceneView::viewTransformChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwQuickSceneView, double, m_viewScale, 1.0, &cwQuickSceneView::viewScaleChanged);

    QProperty<QRectF> m_viewRect;
    QProperty<QRectF> m_sceneRect;

    QPointer<QGraphicsScene> Scene; //!<
    QImage m_image;

    QTransform toViewTransform() const;


};




#endif // CWQUICKSCENEVIEW_H
