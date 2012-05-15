//Our includes
#include "cwLabel3dGroup.h"
#include "cwLabel3dView.h"

cwLabel3dGroup::cwLabel3dGroup(cwLabel3dView *parent) :
    QObject(parent)
{
    setParentView(parent);
}

cwLabel3dGroup::~cwLabel3dGroup()
{
    foreach(QQuickItem* item, LabelItems) {
        item->deleteLater();
    }

    setParentView(NULL);
}

/**
 * @brief cwLabel3dGroup::setParentView
 * @param parent - The parent view that owns this object
 */
void cwLabel3dGroup::setParentView(cwLabel3dView *parent) {
    if(ParentView != parent) {
        if(ParentView != NULL) {
            ParentView->removeGroup(this);
        }

        ParentView = parent;

        if(ParentView != NULL) {
            ParentView->addGroup(this);
        }

        setParent(ParentView);
    }
}

void cwLabel3dGroup::setLabels(QList<cwLabel3dItem> labels) {
    Labels = labels;

    if(ParentView != NULL) {
        ParentView->updateGroup(this);
    }
}
