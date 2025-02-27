/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwScrapStationView.h"
#include "cwScrapOutlinePointView.h"
#include "cwSGPolygonNode.h"
#include "cwSGLinesNode.h"
#include "cwScrapLeadView.h"
#include "cwScrapView.h"
#include "cwNote.h"

//Qt includes
#include <QGraphicsPolygonItem>
#include <QSGSimpleRectNode>
#include <QPen>
#include <QQmlEngine>
#include <QQmlContext>

cwScrapItem::cwScrapItem(QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(nullptr),
    TransformNodeDirty(false),
    PolygonNode(nullptr),
    OutlineNode(nullptr),
    StationView(new cwScrapStationView(this)),
    LeadView(new cwScrapLeadView(this)),
    OutlinePointView(new cwScrapOutlinePointView(this)),
    SelectionManager(nullptr),
    Selected(false)
{
    initilize(QQmlEngine::contextForObject(this));
}

cwScrapItem::cwScrapItem(QQmlContext *context, QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(nullptr),
    TransformNodeDirty(false),
    PolygonNode(nullptr),
    OutlineNode(nullptr),
    StationView(new cwScrapStationView(this)),
    LeadView(new cwScrapLeadView(this)),
    OutlinePointView(new cwScrapOutlinePointView(this)),
    Selected(false)
{
    initilize(context);
}

cwScrapItem::~cwScrapItem()
{
}

void cwScrapItem::initilize(QQmlContext *context)
{
    StationView->setScrapItem(this);
    LeadView->setScrapItem(this);
    OutlinePointView->setScrapItem(this);

    setFlag(QQuickItem::ItemHasContents, true);

    setAntialiasing(true);

    //Set the declarative context for the station view
    QQmlEngine::setContextForObject(StationView, context);
    QQmlEngine::setContextForObject(LeadView, context);
    QQmlEngine::setContextForObject(OutlinePointView, context);

    StationView->setParentItem(m_targetItem);
    LeadView->setParentItem(m_targetItem);
    OutlinePointView->setParentItem(m_targetItem);
}

/**
  Sets the scrap that this item will visualize
  */
void cwScrapItem::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {
        if(Scrap != nullptr) {
            disconnect(Scrap, nullptr, this, nullptr);
        }

        Scrap = scrap;
        StationView->setScrap(Scrap);
        LeadView->setScrap(Scrap);
        OutlinePointView->setScrap(Scrap);

        if(Scrap != nullptr) {
            connect(Scrap, SIGNAL(insertedPoints(int,int)), SLOT(updatePoints()));
            connect(Scrap, SIGNAL(removedPoints(int,int)), SLOT(updatePoints()));
            connect(Scrap, SIGNAL(pointChanged(int,int)), SLOT(updatePoints()));
            updatePoints();
        }

        emit scrapChanged();
    }
}

/**
 * @brief cwScrapItem::updatePaintNode
 * @param oldNode
 * @return See qt documentation
 */
QSGNode *cwScrapItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *) {
    if(!oldNode) {
        oldNode = new QSGTransformNode();
        PolygonNode = new cwSGPolygonNode();
        OutlineNode = new cwSGLinesNode();

        oldNode->appendChildNode(PolygonNode);
        oldNode->appendChildNode(OutlineNode);

        PolygonNode->setPolygon(QPolygonF(ScrapPoints));
        OutlineNode->setLineStrip(ScrapPoints);
    }

    auto lineWidthFromZoom = [this](double lineWidth) {
        return cwSGLinesNode::lineWidthFromZoom(m_zoom, lineWidth);
    };

    if(Selected) {
        //Selecet, red color
        PolygonNode->setColor(QColor::fromRgbF(1.0, 1.0, 0.0, 0.15));

        //This is pretty slow be becauce it has to update the geometry
        OutlineNode->setLineWidth(lineWidthFromZoom(2.0));
    } else {
        //Not selected, blue color
        PolygonNode->setColor(QColor::fromRgbF(0.0, 0.0, 1.0, 0.10));
        OutlineNode->setLineWidth(lineWidthFromZoom(1.0));
    }

    PolygonNode->setPolygon(ScrapPoints);
    OutlineNode->setLineStrip(ScrapPoints);

    return oldNode;
}



/**
 * @brief cwScrapItem::updatePoints
 */
void cwScrapItem::updatePoints()
{
    if(Scrap != nullptr) {
        ScrapPoints.resize(Scrap->points().size());

        //Translate scrap points
        QTransform transform = cwScrapView::toImage(Scrap->parentNote());

        const auto points = Scrap->points();
        for(int i = 0; i < points.size(); ++i) {
            ScrapPoints[i] = transform.map(points.at(i));
        }

        update();
    }
}

/**
Sets the scrap item as the selected scrap
*/
void cwScrapItem::setSelected(bool selected) {
    if(Selected != selected) {
        Selected = selected;
        emit selectedChanged();
        update();
    }
}

void cwScrapItem::setZoom(double zoom)
{
    m_zoom = zoom;
    StationView->setZoom(zoom);
    update();
}

/**
Sets selectionManager
*/
void cwScrapItem::setSelectionManager(cwSelectionManager* selectionManager) {
    if(SelectionManager != selectionManager) {
        SelectionManager = selectionManager;

        stationView()->setSelectionManager(SelectionManager);
        outlinePointView()->setSelectionManager(SelectionManager);
        leadView()->setSelectionManager(SelectionManager);

        emit selectionManagerChanged();
    }
}

QPointF cwScrapItem::toNoteCoordinates(QPointF imageCoordinates) const
{
    return cwScrapView::toNormalized(Scrap->parentNote()).map(imageCoordinates);
}



QQuickItem *cwScrapItem::targetItem() const
{
    return m_targetItem;
}

void cwScrapItem::setTargetItem(QQuickItem *newTargetItem)
{
    if (m_targetItem == newTargetItem)
        return;
    m_targetItem = newTargetItem;

    StationView->setParentItem(m_targetItem);
    LeadView->setParentItem(m_targetItem);
    OutlinePointView->setParentItem(m_targetItem);

    emit targetItemChanged();
}
