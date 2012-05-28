//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwScrapStationView.h"

//Qt includes
#include <QGraphicsPolygonItem>
#include <QSGSimpleRectNode>
#include <QPen>
#include <QQmlEngine>

cwScrapItem::cwScrapItem(QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    BorderItemHandler(NULL),
    //FIXME: Port BorderItem to qt5
//    BorderItem(new QGraphicsPolygonItem(BorderItemHandler)),
    StationView(NULL) //new cwScrapStationView(this))
{
    //Set the declarative context for the station view
//    QQmlContext* context = QQmlEngine::contextForObject(this);
//    QQmlEngine::setContextForObject(StationView, context);

    setFlag(QQuickItem::ItemHasContents, true);
    setWidth(100);
    setHeight(100);
    setPos(QPointF(200, 200));

//    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
}

cwScrapItem::cwScrapItem(QQmlContext *context, QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    BorderItemHandler(NULL), //new QQuickItem(this)),
        //FIXME: Port BorderItem to qt5
//    BorderItem(new QGraphicsPolygonItem(BorderItemHandler)),
    StationView(NULL) //new cwScrapStationView(this))
{
//    StationView->setScrapItem(this);

    //Set the declarative context for the station view
//    QQmlEngine::setContextForObject(this, context);
//    QQmlEngine::setContextForObject(StationView, context);

//    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
}

cwScrapItem::~cwScrapItem()
{
//    if(TransformUpdater != NULL) {
//        TransformUpdater->removeTransformItem(BorderItemHandler);
//    }
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

        qDebug() << "Scrap:" << Scrap;

//        StationView->setScrap(Scrap);

        if(Scrap != NULL) {
            connect(Scrap, SIGNAL(insertedPoints(int,int)), SLOT(updateScrapGeometry()));
            connect(Scrap, SIGNAL(removedPoints(int,int)), SLOT(updateScrapGeometry()));
            updateScrapGeometry();
        }

        emit scrapChanged();
    }
}

/**
    This will update the polygon item's geometry
  */
void cwScrapItem::updateScrapGeometry() {
    //FIXME: Add set polygon for border item
    //    BorderItem->setPolygon(QPolygonF(Scrap->points()));
    QPolygonF(Scrap->points());
    qDebug() << "Update scrap geometry " << Scrap->points().size() << isVisible();
    update();
}

/**
 * @brief cwScrapItem::updatePaintNode
 * @param oldNode
 * @return See qt documentation
 */
QSGNode *cwScrapItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *) {
    qDebug() << "OldNode:" << oldNode;

    QSGSimpleRectNode *n = static_cast<QSGSimpleRectNode *>(oldNode);
    if (!n) {
        n = new QSGSimpleRectNode();
        n->setColor(Qt::red);
    }
    n->setRect(boundingRect());
    return n;

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
//    if(TransformUpdater != transformUpdater) {
//        if(TransformUpdater != NULL) {
//        //    TransformUpdater->removeTransformItem(BorderItemHandler);
//        }

//        TransformUpdater = transformUpdater;
////        StationView->setTransformUpdater(TransformUpdater);

//        if(TransformUpdater != NULL) {
//          //r  TransformUpdater->addTransformItem(BorderItemHandler);
//        }

//        emit transformUpdaterChanged();
//    }
}

