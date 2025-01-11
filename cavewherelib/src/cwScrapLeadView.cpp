/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScrapLeadView.h"
#include "cwScrap.h"
#include "cwScrapView.h"

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
    auto notePoint = Scrap->leadData(cwScrap::LeadPositionOnNote, index).value<QPointF>();
    QPointF imagePoint = cwScrapView::toImage(Scrap->parentNote()).map(notePoint);
    item->setPosition(imagePoint);

}

void cwScrapLeadView::updateViewWithData(int begin, int end, QList<int> roles)
{
    if(roles.contains(cwScrap::LeadPositionOnNote)) {
        updateItemsPositions(begin, end);
    }
}

/**
 * @brief cwScrapStationView::qmlSource
 * @return The qml source of the point that'll be rendered
 */
QString cwScrapLeadView::qmlSource() const
{
    return QStringLiteral("qrc:/cavewherelib/cavewherelib/NoteLead.qml");
}
