#ifndef CWABSTRACT2POINTITEM_H
#define CWABSTRACT2POINTITEM_H

//Qt includes
#include <QDeclarativeItem>
#include <QPen>

//Our includes
class cwTransformUpdater;

class cwAbstract2PointItem : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(QPointF p1 READ p1 WRITE setP1 NOTIFY p1Changed)
    Q_PROPERTY(QPointF p2 READ p2 WRITE setP2 NOTIFY p2Changed)

public:
    cwAbstract2PointItem(QDeclarativeItem *parent);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* transformUpdater);

    QPointF p1() const;
    void setP1(QPointF p1);

    QPointF p2() const;
    void setP2(QPointF p2);

protected:
    cwTransformUpdater* TransformUpdater; //!< This updates the graphics scene objects on the 2d screen
    QPointF P1; //!< The first point in the north arrow
    QPointF P2; //!< The second point in the north arrow
    QPen LinePen; //!< The default pen for lines

    virtual void connectTransformer() = 0;
    virtual void disconnectTransformer() = 0;

    virtual void p1Updated() = 0;
    virtual void p2Updated() = 0;

private slots:
    void updateTransformUpdater();

signals:
    void transformUpdaterChanged();
    void p1Changed();
    void p2Changed();

};

/**
Gets transformUpdater
*/
inline cwTransformUpdater* cwAbstract2PointItem::transformUpdater() const {
    return TransformUpdater;
}

/**
  Gets p1
  */
inline QPointF cwAbstract2PointItem::p1() const {
    return P1;
}

/**
  Gets p2
  */
inline QPointF cwAbstract2PointItem::p2() const {
    return P2;
}

#endif // CWABSTRACT2POINTITEM_H
