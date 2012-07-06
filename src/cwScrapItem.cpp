//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwScrapStationView.h"
#include "cwScrapOutlinePointView.h"
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
    OutlinePointView(new cwScrapOutlinePointView(this)),
    Selected(false)
{
    StationView->setScrapItem(this);
    OutlinePointView->setScrapItem(this);

    setFlag(QQuickItem::ItemHasContents, true);

    //Set the declarative context for the station view
    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQmlEngine::setContextForObject(StationView, context);
    QQmlEngine::setContextForObject(OutlinePointView, context);
}

cwScrapItem::cwScrapItem(QQmlContext *context, QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    TransformNodeDirty(false),
    PolygonNode(NULL),
    OutlineNode(NULL),
    StationView(new cwScrapStationView(this)),
    OutlinePointView(new cwScrapOutlinePointView(this)),
    Selected(false)
{
    StationView->setScrapItem(this);
    OutlinePointView->setScrapItem(this);

    setFlag(QQuickItem::ItemHasContents, true);

    //Set the declarative context for the station view
    QQmlEngine::setContextForObject(this, context);
    QQmlEngine::setContextForObject(StationView, context);
    QQmlEngine::setContextForObject(OutlinePointView, context);
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
        OutlinePointView->setScrap(Scrap);

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
    if(!oldNode) {
        oldNode = new QSGTransformNode();
        PolygonNode = new cwSGPolygonNode();
        OutlineNode = new cwSGLinesNode();

        oldNode->appendChildNode(PolygonNode);
        oldNode->appendChildNode(OutlineNode);

        PolygonNode->setPolygon(QPolygonF(Scrap->points()));
        OutlineNode->setLineStrip(Scrap->points());
    }

    if(transformUpdater()) {
        QSGTransformNode* transformNode = static_cast<QSGTransformNode*>(oldNode);
        transformNode->setMatrix(transformUpdater()->matrix());
    }

    if(Selected) {
        //Selecet, red color
        PolygonNode->setColor(QColor::fromRgbF(1.0, 1.0, 0.0, 0.15));
        OutlineNode->setLineWidth(2.0);
    } else {
        //Not selected, blue color
        PolygonNode->setColor(QColor::fromRgbF(0.0, 0.0, 1.0, 0.10));
        OutlineNode->setLineWidth(1.0);
    }

    PolygonNode->setPolygon(QPolygonF(Scrap->points()));
    OutlineNode->setLineStrip(Scrap->points());

    return oldNode;
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

        if(TransformUpdater != NULL) {
            disconnect(TransformUpdater, &cwTransformUpdater::matrixChanged, this, &cwScrapItem::update);
        }

        TransformUpdater = transformUpdater;

        if(TransformUpdater != NULL) {
            connect(TransformUpdater, &cwTransformUpdater::matrixChanged, this, &cwScrapItem::update);
        }

        StationView->setTransformUpdater(TransformUpdater);
        OutlinePointView->setTransformUpdater(TransformUpdater);

        emit transformUpdaterChanged();
        update();
    }
}
