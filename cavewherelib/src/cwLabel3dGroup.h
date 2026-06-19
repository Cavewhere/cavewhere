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
#include "cwBillboardHandle.h"
class cwLabel3dView;

//Std includes
#include <vector>

class cwLabel3dGroup : public QObject
{
    friend class cwLabel3dView;

    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
public:
    explicit cwLabel3dGroup(cwLabel3dView *parent = 0);
    ~cwLabel3dGroup();

    void setParentView(cwLabel3dView* parent);

    void setLabels(QList<cwLabel3dItem> labels);
    QList<cwLabel3dItem> labels() const { return m_labels; }

    bool isVisible() const { return m_visible; }

    void clear();

signals:
    void visibleChanged();

public slots:
    // The setVisible() target for this group's keyword item. cwKeywordVisibility
    // calls this when keyword filters change; cwLabel3dView skips and releases a
    // hidden group's pooled items and billboards on the next position update.
    void setVisible(bool visible);

private:
    QPointer<cwLabel3dView> m_parentView;
    bool m_visible = true;
    QList<cwLabel3dItem> m_labels;
    QVector<QQuickItem*> m_labelItems;

    // One RAII handle per label, kept in lockstep with m_labelItems. Each owns its
    // label's billboard in the shared layer and removes it when reset or when the
    // group is destroyed (fixing the old leak where a removed group left billboards
    // behind). An empty handle means the label isn't currently billboarded.
    std::vector<cwBillboardHandle> m_billboardHandles;

    //For caching and memory efficientcy
    QVector<QQuickItem*> m_itemPool;

    struct VisibleEntry {
        int index;
        QVector3D position;
    };
    QVector<VisibleEntry> m_visibleEntries;
    
};

#endif // CWLABEL3DGROUP_H
