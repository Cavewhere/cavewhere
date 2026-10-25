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

//Our includes
#include "cwLabel3dItem.h"
#include "cwTextBillboardHandle.h"
#include "cwVisibilityProxy.h"
class cwLabel3dView;

//Std includes
#include <vector>

class cwLabel3dGroup : public cwVisibilityProxy
{
    friend class cwLabel3dView;

    Q_OBJECT
public:
    explicit cwLabel3dGroup(cwLabel3dView *parent = 0);
    ~cwLabel3dGroup();

    void setParentView(cwLabel3dView* parent);

    void setLabels(QList<cwLabel3dItem> labels);
    QList<cwLabel3dItem> labels() const { return m_labels; }

    void clear();

protected:
    // The setVisible() target for this group's keyword item. cwKeywordVisibility
    // calls this when keyword filters change; cwLabel3dView skips a hidden group
    // and releases its billboards.
    void applyVisible(bool visible) override;

private:
    QPointer<cwLabel3dView> m_parentView;
    QList<cwLabel3dItem> m_labels;

    // One RAII handle per label, kept in lockstep with m_labels. Each owns its
    // label's text billboard in the shared text layer and removes it when reset
    // or when the group is destroyed. An empty handle means the label isn't
    // placed.
    std::vector<cwTextBillboardHandle> m_billboardHandles;

    struct VisibleEntry {
        int index;
        QVector3D position;
    };
    QVector<VisibleEntry> m_visibleEntries;
    
};

#endif // CWLABEL3DGROUP_H
