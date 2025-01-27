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

public:
    explicit cwQuickSceneView(QQuickItem *parent = 0);

    QGraphicsScene* scene() const;
    void setScene(QGraphicsScene* scene);

    // QPointF toView(QPointF paperPoint) const;
    Q_INVOKABLE QRectF toView(QRectF paperRect) const;
    Q_INVOKABLE QPointF toPaper(QPointF pointPixels) const;

    double viewScale() const;
    double paperScale() const { return 1.0 / viewScale(); }

    virtual void paint(QPainter * painter);

signals:
    void sceneChanged();

public slots:

private:
    QPointer<QGraphicsScene> Scene; //!<
    QImage m_image;

    QTransform toViewTransform() const;

};




#endif // CWQUICKSCENEVIEW_H
