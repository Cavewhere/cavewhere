/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWNORTHAREA_H
#define CWNORTHAREA_H

//Our includes
#include "cwAbstract2PointItem.h"
class cwTransformUpdater;
class cwSGLinesNode;

//Our includes
#include <QQmlEngine>

class cwNorthArrowItem : public cwAbstract2PointItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NorthArrowItem)

public:
    explicit cwNorthArrowItem(QQuickItem *parent = 0);

public slots:

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData *);

    void p1Updated();
    void p2Updated();

private:
    void disconnectTransformer();
    void connectTransformer();


private slots:
    void updateNorthArrowPath();

private:
    QVector<QPointF> NorthArrowLineStrip;
    cwSGLinesNode* NorthArrowLinesNode;

    QVector<QPointF> createNorthArrowLineStrip() const;

};

inline void cwNorthArrowItem::p2Updated() {
    updateNorthArrowPath();
}


#endif // CWNORTHAREA_H
