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

//Qt includes
#include <QGraphicsPolygonItem>
#include <QSGSimpleRectNode>
#include <QPen>
#include <QQmlEngine>
#include <QQmlContext>

cwScrapItem::cwScrapItem(QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(nullptr),
    TransformUpdater(nullptr),
    TransformNodeDirty(false),
    PolygonNode(nullptr),
    OutlineNode(nullptr),
    StationView(new cwScrapStationView(this)),
    LeadView(new cwScrapLeadView(this)),
    OutlinePointView(new cwScrapOutlinePointView(this)),
    SelectionManager(nullptr),
    Selected(false)
{
    StationView->setScrapItem(this);
    LeadView->setScrapItem(this);
    OutlinePointView->setScrapItem(this);

    setFlag(QQuickItem::ItemHasContents, true);

    setAntialiasing(true);

    //Set the declarative context for the station view
    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQmlEngine::setContextForObject(StationView, context);
    QQmlEngine::setContextForObject(LeadView, context);
    QQmlEngine::setContextForObject(OutlinePointView, context);
}

cwScrapItem::cwScrapItem(QQmlContext *context, QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(nullptr),
    TransformUpdater(nullptr),
    TransformNodeDirty(false),
    PolygonNode(nullptr),
    OutlineNode(nullptr),
    StationView(new cwScrapStationView(this)),
    LeadView(new cwScrapLeadView(this)),
    OutlinePointView(new cwScrapOutlinePointView(this)),
    Selected(false)
{
    StationView->setScrapItem(this);
    LeadView->setScrapItem(this);
    OutlinePointView->setScrapItem(this);

    setFlag(QQuickItem::ItemHasContents, true);

    //Set the declarative context for the station view
    QQmlEngine::setContextForObject(this, context);
    QQmlEngine::setContextForObject(StationView, context);
    QQmlEngine::setContextForObject(LeadView, context);
    QQmlEngine::setContextForObject(OutlinePointView, context);

    // setWidth(1000);
    // setHeight(1000);
}

cwScrapItem::~cwScrapItem()
{
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
    //    if(ScrapPoints.isEmpty()) { return oldNode; }

    qDebug() << "Draw scrap!" << this << this->isVisible() << this->parentItem()->isVisible();

    // QSGSimpleRectNode *n = static_cast<QSGSimpleRectNode *>(oldNode);
    // if (!n) {
    //     n = new QSGSimpleRectNode();
    //     n->setColor(Qt::green);
    // }
    // n->setRect(boundingRect());
    // return n;


    if(!oldNode) {
        oldNode = new QSGTransformNode();
        PolygonNode = new cwSGPolygonNode();
        OutlineNode = new cwSGLinesNode();

        oldNode->appendChildNode(PolygonNode);
        oldNode->appendChildNode(OutlineNode);

        PolygonNode->setPolygon(QPolygonF(ScrapPoints));
        OutlineNode->setLineStrip(ScrapPoints);
    }

    if(transformUpdater()) {
        QSGTransformNode* transformNode = static_cast<QSGTransformNode*>(oldNode);
        QMatrix4x4 scale;
        scale.scale(10000, 10000, 1.0);
        // transformNode->setMatrix(transformUpdater()->matrix());
        transformNode->setMatrix(scale);
    }

    if(Selected) {
        //Selecet, red color
        PolygonNode->setColor(QColor::fromRgbF(1.0, 1.0, 0.0, 0.15));
        OutlineNode->setLineWidth(2.0);
    } else {
        //Not selected, blue color
        PolygonNode->setColor(QColor::fromRgbF(0.0, 0.0, 1.0, 0.10));
        OutlineNode->setLineWidth(0.001); //1.0);
    }

    PolygonNode->setPolygon(ScrapPoints);
    OutlineNode->setLineStrip(ScrapPoints);


    qDebug() << "Bounding rectangle:" << boundingRect();

    return oldNode;
}



/**
 * @brief cwScrapItem::updatePoints
 */
void cwScrapItem::updatePoints()
{
    if(Scrap != nullptr) {
        ScrapPoints = Scrap->points();
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


/**
Sets transformUpdater
*/
void cwScrapItem::setTransformUpdater(cwTransformUpdater* transformUpdater) {
    if(TransformUpdater != transformUpdater) {

        if(TransformUpdater != nullptr) {
            disconnect(TransformUpdater, &cwTransformUpdater::matrixChanged, this, &cwScrapItem::update);
        }

        TransformUpdater = transformUpdater;

        if(TransformUpdater != nullptr) {
            connect(TransformUpdater, &cwTransformUpdater::matrixChanged, this, &cwScrapItem::update);
        }

        StationView->setTransformUpdater(TransformUpdater);
        LeadView->setTransformUpdater(TransformUpdater);
        OutlinePointView->setTransformUpdater(TransformUpdater);

        emit transformUpdaterChanged();
        update();
    }
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
