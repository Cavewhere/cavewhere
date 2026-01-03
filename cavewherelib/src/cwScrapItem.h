/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAPITEM_H
#define CWSCRAPITEM_H

//Qt includes
#include <QQuickPaintedItem>
#include <QQuickTransform>
#include <QQmlEngine>
#include <QPropertyNotifier>

//Our includes
#include "cwScrap.h"
#include "cwScrapStationView.h"
#include "cwScrapLeadView.h"
#include "cwScrapOutlinePointView.h"
#include "cwSelectionManager.h"
class cwSGPolygonNode;

/**
  \brief This draws a scrap
  */
class cwScrapItem : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScrapItem)

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected NOTIFY selectedChanged)
    // Q_PROPERTY(cwTransformItemUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrapStationView* stationView READ stationView CONSTANT)
    Q_PROPERTY(cwScrapLeadView* leadView READ leadView CONSTANT)

    Q_PROPERTY(cwScrapOutlinePointView* outlinePointView READ outlinePointView CONSTANT)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager WRITE setSelectionManager NOTIFY selectionManagerChanged)

    Q_PROPERTY(QQuickItem* targetItem READ targetItem WRITE setTargetItem NOTIFY targetItemChanged FINAL)

public:
    explicit cwScrapItem(QQuickItem *parent = 0);
    explicit cwScrapItem(QQmlContext* context, QQuickItem *parent = 0);
    ~cwScrapItem();

    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

    bool isSelected() const;
    void setSelected(bool selected);

    void setZoom(double zoom);

    // cwTransformItemUpdater* transformUpdater() const;
    // void setTransformUpdater(cwTransformItemUpdater* transformUpdater);

    // void setTransformNode(QSGTransformNode* node);
    // QSGTransformNode* transformNode() const;

    cwScrapStationView* stationView() const;
    cwScrapLeadView* leadView() const;
    cwScrapOutlinePointView* outlinePointView() const;

    cwSelectionManager* selectionManager() const;
    void setSelectionManager(cwSelectionManager* selectionManager);

    // QRectF boundingRect() const { return QRectF(0, 0, 1000, 1000); }

    Q_INVOKABLE QPointF toNoteCoordinates(QPointF imageCoordinates) const;

    QQuickItem *targetItem() const;
    void setTargetItem(QQuickItem *newTargetItem);

signals:
    void scrapChanged();
    void selectedChanged();
    void transformUpdaterChanged();
    void selectionManagerChanged();

    void targetItemChanged();

public slots:

private:
    //Data class
    cwScrap* Scrap;

    //For keeping the 2D object aligned
    double m_zoom = 1.0;
    QPropertyNotifier m_matrixChanged;
    QMatrix4x4 m_transformMatrix;


    //Visual elements
    bool TransformNodeDirty;
    cwSGPolygonNode* PolygonNode; //!< Draws the polygon
    cwSGLinesNode* OutlineNode; //!< Drawing the outline of the polygon
    cwScrapStationView* StationView; //!< All the stations in the scrap
    cwScrapLeadView* LeadView; //!< All the leads in the scrap
    cwScrapOutlinePointView* OutlinePointView; //!< All the control points around the scrap
    QVector<QPointF> ScrapPoints;

    //For showing all the control points around the scrap
    // QQmlComponent* OutlineControlPoints;
    // QList<QQuickItem*> OutlineStation;


    cwSelectionManager* SelectionManager; //!< For selection for control items (this is passed to child class)
    bool Selected; //!< True if the scrap is select and false if it isn't

    void initilize(QQmlContext* context);

    QQuickItem *m_targetItem = nullptr;

protected:
    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);

protected slots:
    void updatePoints();

};

Q_DECLARE_METATYPE(cwScrapItem*)

/**
  Gets the scrap this scrap will scrap item will draw
  */
inline cwScrap* cwScrapItem::scrap() const {
    return Scrap;
}


/**
  Returns true if the scrap item is select and false if it isn't
  */
inline bool cwScrapItem::isSelected() const {
    return Selected;
}

// /**
// Gets transformUpdater
// */
// inline cwTransformItemUpdater* cwScrapItem::transformUpdater() const {
//     return TransformUpdater;
// }

/**
Gets stationView, this stores all the note station item
*/
inline cwScrapStationView* cwScrapItem::stationView() const {
    return StationView;
}

/**
Gets controlPointView
*/
inline cwScrapOutlinePointView* cwScrapItem::outlinePointView() const {
    return OutlinePointView;
}

/**
Gets selectionManager
*/
inline cwSelectionManager* cwScrapItem::selectionManager() const {
    return SelectionManager;
}

/**
* @brief cwScrapItem::leadView
* @return
*/
inline cwScrapLeadView* cwScrapItem::leadView() const {
    return LeadView;
}


#endif // CWSCRAPITEM_H
