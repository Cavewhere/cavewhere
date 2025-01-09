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
    cwScrapPointView(parent),
    PositionRole(cwScrap::LeadPositionOnNote)
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
    QVector3D point;
    switch (positionRole()) {
    case cwScrap::LeadPositionOnNote: {
        auto notePoint = Scrap->leadData(positionRole(), index).value<QPointF>();
        QPointF imagePoint = cwScrapView::toImage(Scrap->parentNote()).map(notePoint);
        item->setPosition(imagePoint);
        break;
    }
    case cwScrap::LeadPosition:
        point = Scrap->leadData(positionRole(), index).value<QVector3D>();
        //Note sure how we handle 3d view yet
        break;
    default:
        break;
    }
    // item->setProperty("position3D", point);
}

void cwScrapLeadView::updateViewWithData(int begin, int end, QList<int> roles)
{
    foreach(int role, roles) {
        if(positionRole() == role) {
            updateItemsPositions(begin, end);
            break;
        }
    }
}

//QSGNode *cwScrapLeadView::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
//{
//    Q_UNUSED(oldNode);
//    return nullptr;
//}

/**
 * @brief cwScrapLeadView::setPositionRole
 * @param role
 */
void cwScrapLeadView::setPositionRole(cwScrap::LeadDataRole role)
{
    Q_ASSERT(role == cwScrap::LeadPosition || role == cwScrap::LeadPositionOnNote);
    if(role != PositionRole) {
        PositionRole = role;
        updateAllItemData();;
    }
}

/**
 * @brief cwScrapStationView::qmlSource
 * @return The qml source of the point that'll be rendered
 */
QString cwScrapLeadView::qmlSource() const
{
    switch(positionRole()) {
    case cwScrap::LeadPosition:
        return QStringLiteral("qrc:/cavewherelib/cavewherelib/LeadPoint.qml");
    case cwScrap::LeadPositionOnNote:
        return QStringLiteral("qrc:/cavewherelib/cavewherelib/NoteLead.qml");
    default:
        break;
    }
    return QString();

}
