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

//Our includes
#include "cwScrap.h"
#include "cwTransformUpdater.h"
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
    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrapStationView* stationView READ stationView CONSTANT)
    Q_PROPERTY(cwScrapLeadView* leadView READ leadView CONSTANT)

    Q_PROPERTY(cwScrapOutlinePointView* outlinePointView READ outlinePointView CONSTANT)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager WRITE setSelectionManager NOTIFY selectionManagerChanged)

public:
    explicit cwScrapItem(QQuickItem *parent = 0);
    explicit cwScrapItem(QQmlContext* context, QQuickItem *parent = 0);
    ~cwScrapItem();

    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

    bool isSelected() const;
    void setSelected(bool selected);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* transformUpdater);

    void setTransformNode(QSGTransformNode* node);
    QSGTransformNode* transformNode() const;

    cwScrapStationView* stationView() const;
    cwScrapLeadView* leadView() const;
    cwScrapOutlinePointView* outlinePointView() const;

    cwSelectionManager* selectionManager() const;
    void setSelectionManager(cwSelectionManager* selectionManager);

signals:
    void scrapChanged();
    void selectedChanged();
    void transformUpdaterChanged();
    void selectionManagerChanged();

public slots:

private:
    //Data class
    cwScrap* Scrap;

    //For keeping the 2D object aligned
    cwTransformUpdater* TransformUpdater; //!<

    //Visual elements
    bool TransformNodeDirty;
    cwSGPolygonNode* PolygonNode; //!< Draws the polygon
    cwSGLinesNode* OutlineNode; //!< Drawing the outline of the polygon
    cwScrapStationView* StationView; //!< All the stations in the scrap
    cwScrapLeadView* LeadView; //!< All the leads in the scrap
    cwScrapOutlinePointView* OutlinePointView; //!< All the control points around the scrap
    QVector<QPointF> ScrapPoints;

    //For showing all the control points around the scrap
    QQmlComponent* OutlineControlPoints;
    QList<QQuickItem*> OutlineStation;


    cwSelectionManager* SelectionManager; //!< For selection for control items (this is passed to child class)
    bool Selected; //!< True if the scrap is select and false if it isn't

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

/**
Gets transformUpdater
*/
inline cwTransformUpdater* cwScrapItem::transformUpdater() const {
    return TransformUpdater;
}

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
