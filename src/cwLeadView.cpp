/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwLeadView.h"
#include "cwCavingRegion.h"

cwLeadView::cwLeadView()
{

}

cwLeadView::~cwLeadView()
{

}

/**
* @brief cwLeadView::region
* @return
*/
cwCavingRegion* cwLeadView::region() const {
    return Region;
}

/**
* @brief cwLeadView::setRegion
* @param region
*/
void cwLeadView::setRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;
        emit regionChanged();
    }
}

QUrl cwLeadView::qmlSource() const
{
    return QUrl();
}

void cwLeadView::updateItemData(QQuickItem *item, int pointIndex)
{
    Q_UNUSED(item);
    Q_UNUSED(pointIndex)
}

void cwLeadView::updateItemPosition(QQuickItem *item, int pointIndex)
{
    Q_UNUSED(item);
    Q_UNUSED(pointIndex);
}

