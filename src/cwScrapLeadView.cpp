/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScrapLeadView.h"
#include "cwScrap.h"

cwScrapLeadView::cwScrapLeadView(QQuickItem *parent) :
    cwScrapPointView(parent)
{

}

cwScrapLeadView::~cwScrapLeadView()
{

}

void cwScrapLeadView::setScrap(cwScrap *scrap)
{
    if(Scrap != scrap) {
        if(Scrap != nullptr) {
            disconnect(Scrap, nullptr, this, nullptr);
        }

        Scrap = scrap;

        if(Scrap != nullptr) {
            connect(Scrap, &cwScrap::leadsInserted, this, &cwScrapLeadView::pointsInserted);
            connect(Scrap, &cwScrap::leadsRemoved, this, &cwScrapLeadView::pointsRemoved);
            connect(Scrap, &cwScrap::leadsDataChanged, this, &cwScrapLeadView::updateViewWithData);
            resizeNumberOfItems(Scrap->leads().size());
        } else {
            resizeNumberOfItems(0);
        }

        cwScrapPointView::setScrap(scrap);
    }
}

void cwScrapLeadView::updateItemPosition(QQuickItem *item, int index)
{
    QPointF point = Scrap->leadData(cwScrap::LeadPosition, index).value<QPointF>();
    item->setProperty("position3D", QVector3D(point));
}

void cwScrapLeadView::updateViewWithData(int begin, int end, QList<int> roles)
{
    foreach(int role, roles) {
        if(role == cwScrap::LeadPosition) {
            updateItemsPositions(begin, end);
        }
    }
}

//QSGNode *cwScrapLeadView::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
//{
//    Q_UNUSED(oldNode);
//    return nullptr;
//}

