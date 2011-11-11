#ifndef CWNORTHAREA_H
#define CWNORTHAREA_H

//Qt includes
#include <QDeclarativeItem>

//Our includes
class cwTransformUpdater;
class cwPositioner3D;

class cwNorthArrowItem : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(QPointF p1 READ p1 WRITE setP1 NOTIFY p1Changed)
    Q_PROPERTY(QPointF p2 READ p2 WRITE setP2 NOTIFY p2Changed)


public:
    explicit cwNorthArrowItem(QDeclarativeItem *parent = 0);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* transformUpdater);

    QPointF p1() const;
    void setP1(QPointF p1);

    QPointF p2() const;
    void setP2(QPointF p2);

signals:
    void transformUpdaterChanged();
    void p1Changed();
    void p2Changed();

public slots:

private:
    QGraphicsObject* NorthArrowLineHandler;
    QGraphicsPathItem* NorthArrowLine;

    cwPositioner3D* NorthTextHandler;
    QGraphicsTextItem* NorthText;

    cwTransformUpdater* TransformUpdater; //!< This updates the graphics scene objects on the 2d screen
    QPointF P1; //!< The first point in the north arrow
    QPointF P2; //!< The second point in the north arrow

    void disconnectFromTransformer();

private slots:
    void updateTransformUpdater();
    void updateNorthArrowPath();

};

/**
Gets transformUpdater
*/
inline cwTransformUpdater* cwNorthArrowItem::transformUpdater() const {
    return TransformUpdater;
}

/**
  Gets p1
  */
inline QPointF cwNorthArrowItem::p1() const {
    return P1;
}

/**
  Gets p2
  */
inline QPointF cwNorthArrowItem::p2() const {
    return P2;
}
#endif // CWNORTHAREA_H
