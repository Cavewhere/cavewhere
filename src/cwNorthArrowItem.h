#ifndef CWNORTHAREA_H
#define CWNORTHAREA_H

//Our includes
#include "cwAbstract2PointItem.h"
class cwTransformUpdater;
class cwPositioner3D;

class cwNorthArrowItem : public cwAbstract2PointItem
{
    Q_OBJECT

public:
    explicit cwNorthArrowItem(QQuickItem *parent = 0);

public slots:

private:
    QGraphicsObject* NorthArrowLineHandler;
    QGraphicsPathItem* NorthArrowLine;

    cwPositioner3D* NorthTextHandler;
    QGraphicsTextItem* NorthText;

    void disconnectTransformer();
    void connectTransformer();

    void p1Updated();
    void p2Updated();

private slots:
    void updateNorthArrowPath();

};

inline void cwNorthArrowItem::p2Updated() {
    updateNorthArrowPath();
}


#endif // CWNORTHAREA_H
