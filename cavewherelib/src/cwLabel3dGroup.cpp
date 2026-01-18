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
    m_parentView(nullptr)
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
    if(m_parentView != parent) {
        if(m_parentView != nullptr) {
            m_parentView->removeGroup(this);
        }

        m_parentView = parent;

        if(m_parentView != nullptr) {
            m_parentView->addGroup(this);
        }

        setParent(m_parentView);
    }
}

void cwLabel3dGroup::setLabels(QList<cwLabel3dItem> labels) {
    m_labels = labels;

    if(m_parentView != nullptr) {
        m_parentView->updateGroup(this);
    }
}
