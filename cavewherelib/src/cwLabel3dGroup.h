/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLABEL3DGROUP_H
#define CWLABEL3DGROUP_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QVector>
class QQuickItem;

//Our includes
#include "cwLabel3dItem.h"
class cwLabel3dView;

class cwLabel3dGroup : public QObject
{
    friend class cwLabel3dView;

    Q_OBJECT
public:
    explicit cwLabel3dGroup(cwLabel3dView *parent = 0);
    ~cwLabel3dGroup();

    void setParentView(cwLabel3dView* parent);

    void setLabels(QList<cwLabel3dItem> labels);
    QList<cwLabel3dItem> labels() const;

    void clear();

signals:
    
public slots:

private:
    QPointer<cwLabel3dView> m_parentView;
    QList<cwLabel3dItem> m_labels;
    QVector<QQuickItem*> m_labelItems;

    //For caching and memory efficientcy
    QVector<QQuickItem*> m_itemPool;

    struct VisibleEntry {
        int index;
        QVector3D position;
    };
    QVector<VisibleEntry> m_visibleEntries;
    
};

#endif // CWLABEL3DGROUP_H
