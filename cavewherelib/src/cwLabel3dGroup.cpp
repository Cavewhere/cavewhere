/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLabel3dGroup.h"
#include "cwLabel3dView.h"

cwLabel3dGroup::cwLabel3dGroup(cwLabel3dView *parent) :
    QObject(parent),
    ParentView(nullptr)
{
    setParentView(parent);
}

cwLabel3dGroup::~cwLabel3dGroup()
{
    setParentView(nullptr);
}

/**
 * @brief cwLabel3dGroup::setParentView
 * @param parent - The parent view that owns this object
 */
void cwLabel3dGroup::setParentView(cwLabel3dView *parent) {
    if(ParentView != parent) {
        if(ParentView != nullptr) {
            ParentView->removeGroup(this);
        }

        ParentView = parent;

        if(ParentView != nullptr) {
            ParentView->addGroup(this);
        }

        setParent(ParentView);
    }
}

void cwLabel3dGroup::setLabels(QList<cwLabel3dItem> labels) {
    Labels = labels;

    if(ParentView != nullptr) {
        ParentView->updateGroup(this);
    }
}
