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
class QGraphicsScene;

/**
 * @brief The cwQuickSceneView class
 *
 * This class renders a QGraphisScene to a QQuickItem
 */
class cwQuickSceneView : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(QGraphicsScene* scene READ scene WRITE setScene NOTIFY sceneChanged)

public:
    explicit cwQuickSceneView(QQuickItem *parent = 0);

    QGraphicsScene* scene() const;
    void setScene(QGraphicsScene* scene);

    virtual void paint(QPainter * painter);

signals:
    void sceneChanged();

public slots:

private:
    QPointer<QGraphicsScene> Scene; //!<

};




#endif // CWQUICKSCENEVIEW_H
