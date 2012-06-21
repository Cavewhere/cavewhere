//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwScrapStationView.h"
#include "cwScrapControlPointView.h"
#include "cwSGPolygonNode.h"
#include "cwSGLinesNode.h"

//Qt includes
#include <QGraphicsPolygonItem>
#include <QSGSimpleRectNode>
#include <QPen>
#include <QQmlEngine>

cwScrapItem::cwScrapItem(QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    TransformNodeDirty(false),
    PolygonNode(NULL),
    OutlineNode(NULL),
    StationView(new cwScrapStationView(this)),
    ControlPointView(new cwScrapControlPointView(this))
{
    StationView->setScrapItem(this);

    setFlag(QQuickItem::ItemHasContents, true);

    //Set the declarative context for the station view
    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQmlEngine::setContextForObject(StationView, context);
    QQmlEngine::setContextForObject(ControlPointView, context);

//    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
}

cwScrapItem::cwScrapItem(QQmlContext *context, QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    StationView(new cwScrapStationView(this)),
    ControlPointView(new cwScrapControlPointView(this))
{
    StationView->setScrapItem(this);

    setFlag(QQuickItem::ItemHasContents, true);

    //Set the declarative context for the station view
    QQmlEngine::setContextForObject(this, context);
    QQmlEngine::setContextForObject(StationView, context);
    QQmlEngine::setContextForObject(ControlPointView, context);

//    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
}

cwScrapItem::~cwScrapItem()
{
}

/**
  Sets the scrap that this item will visualize
  */
void cwScrapItem::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {
        if(Scrap != NULL) {
            disconnect(Scrap, NULL, this, NULL);
        }

        Scrap = scrap;
        StationView->setScrap(Scrap);
        ControlPointView->setScrap(Scrap);

        if(Scrap != NULL) {
            connect(Scrap, SIGNAL(insertedPoints(int,int)), SLOT(update()));
            connect(Scrap, SIGNAL(removedPoints(int,int)), SLOT(update()));
            connect(Scrap, SIGNAL(pointChanged(int,int)), SLOT(update()));
            update();
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
    if(TransformUpdater) {
        if(!oldNode) {
            PolygonNode = new cwSGPolygonNode();
            OutlineNode = new cwSGLinesNode();
            OutlineNode->setLineWidth(2.0);
        }

        if(TransformNodeDirty) {
            TransformUpdater->transformNode()->appendChildNode(PolygonNode);
            TransformUpdater->transformNode()->appendChildNode(OutlineNode);
            TransformNodeDirty = false;
        }

        if(PolygonNode) {
            PolygonNode->setPolygon(QPolygonF(Scrap->points()));
            OutlineNode->setLineStrip(Scrap->points());
        }

        return TransformUpdater->transformNode();
    }
    return NULL;

}

/**
Sets the scrap item as the selected scrap
*/
void cwScrapItem::setSelected(bool selected) {
    if(Selected != selected) {
        Selected = selected;

        //Make the pen wider
        QPen pen;
        pen.setCosmetic(true);
        if(Selected) {
            pen.setWidth(2.0);
        } else {
            pen.setWidth(1.0);
        }
        //FIXME: Fix border item's pen
//        BorderItem->setPen(pen);

//        if(!selected) {
//            StationView->clearSelection();
//        }

        emit selectedChanged();
    }
}


/**
Sets transformUpdater
*/
void cwScrapItem::setTransformUpdater(cwTransformUpdater* transformUpdater) {
    if(TransformUpdater != transformUpdater) {

        TransformUpdater = transformUpdater;
        TransformNodeDirty = true;
        StationView->setTransformUpdater(TransformUpdater);
        ControlPointView->setTransformUpdater(TransformUpdater);

        emit transformUpdaterChanged();
        update();
    }
}



