#ifndef CWSCALELENGTHITEM_H
#define CWSCALELENGTHITEM_H


//Our includes
#include "cwAbstract2PointItem.h"

class cwScaleLengthItem : public cwAbstract2PointItem
{
    Q_OBJECT

public:
    explicit cwScaleLengthItem(QDeclarativeItem *parent = 0);
    
public slots:

private:
    QGraphicsObject* LengthHandler;
    QGraphicsPathItem* LengthLine;

    void connectTransformer();
    void disconnectTransformer();

    void p1Updated();
    void p2Updated();

private slots:
    void updateScaleLengthPath();
    
};

/**
  This is called when the p1 is updated
  */
inline void cwScaleLengthItem::p1Updated() {
    updateScaleLengthPath();
}

/**
  This is called when p2 is updated
  */
inline void cwScaleLengthItem::p2Updated() {
    updateScaleLengthPath();
}

#endif // CWSCALELENGTHITEM_H
