#ifndef CWNORTHAREA_H
#define CWNORTHAREA_H

//Our includes
#include "cwAbstract2PointItem.h"
class cwTransformUpdater;
class cwPositioner3D;
class cwSGLinesNode;

class cwNorthArrowItem : public cwAbstract2PointItem
{
    Q_OBJECT

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
