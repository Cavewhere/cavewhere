/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLABEL3DVIEW_H
#define CWLABEL3DVIEW_H

//Qt includes
#include <QQuickItem>
#include <QQmlComponent>
#include <QMatrix4x4>
#include <QSizeF>
#include <QQmlEngine>

//Our includes
#include "cwLabel3dItem.h"
#include "cwCollisionRectKdTree.h"
#include "cwBillboardOverlayItem.h"
class cwLabel3dGroup;

class cwLabel3dView : public cwBillboardOverlayItem
{
    friend class cwLabel3dGroup;

    Q_OBJECT
    QML_NAMED_ELEMENT(Label3dView)

public:
    explicit cwLabel3dView(QQuickItem *parent = 0);
    ~cwLabel3dView();

    void addGroup(cwLabel3dGroup* group);
    void removeGroup(cwLabel3dGroup* group);

public slots:

protected:
    void repositionBillboards() override;

private:
    QSet<cwLabel3dGroup*> m_labelGroups;

    //For rendering labels
    QQmlComponent* m_component;
    cwCollisionRectKdTree m_labelKdTree;
    QSizeF m_averageLabelSize = QSizeF(80.0, 14.0);

    void updateGroup(cwLabel3dGroup* group);
    void updateGroupPositions(cwLabel3dGroup* group);
    QQuickItem* labelItem(cwLabel3dGroup* group, int labelIndex);
    QQuickItem* acquireLabelItem(cwLabel3dGroup* group, int labelIndex);
    void releaseLabelItem(cwLabel3dGroup* group, int labelIndex);
    void addOrUpdateBillboard(cwLabel3dGroup* group, int labelIndex, QQuickItem* item);
private slots:
    void updatePositions();

};



#endif // CWLABEL3DVIEW_H
