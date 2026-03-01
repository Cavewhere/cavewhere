/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAPSTATIONVIEW_H
#define CWSCRAPSTATIONVIEW_H

//Our includes
#include "cwScrapPointView.h"
#include "cwGlobalDirectory.h"
class cwTransformUpdater;
class cwScrap;
class cwImageItem;
class cwScrapItem;
class cwSGLinesNode;
#include "cwNoteStation.h"

//Qt includes
#include <QVariantAnimation>
#include <QQmlEngine>

/**
  This class manages a list of station items that visualize all the stations in a scrap.
  */
class cwScrapStationView : public cwScrapPointView
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScrapStationView)

public:
    explicit cwScrapStationView(QQuickItem *parent = 0);
    ~cwScrapStationView();

    void setScrap(cwScrap* scrap) override;
    void setScrapItem(cwScrapItem* scrapItem) override;
    void setZoom(double zoom);

    cwNoteStation selectedNoteStation() const;

signals:
    void scrapChanged();
    void scrapItemChanged();
    void shotLineScaleChanged();

public slots:

private:
    //Geometry for the shot lines
    bool LineDataDirty;
    cwSGLinesNode* ShotLinesNodeBackground;
    cwSGLinesNode* ShotLinesNodeForground;
    QVector<QLineF> ShotLines;
    double m_zoom = 1.0;

    QVariantAnimation* ScaleAnimation;

    // cwTransformItemUpdater* OldTransformUpdater;

    void updateItemData(QQuickItem* item, int index) override;
    void updateItemPosition(QQuickItem* item, int index) override;

private slots:
    void syncStationsFromScrap();
    void updateShotLinesWithAnimation();
    void updateShotLines();

protected:
    QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *) override;

    QMatrix4x4 runningProfileDirection() const;


};

// Q_DECLARE_METATYPE(cwScrapStationView*)





#endif // CWSCRAPSTATIONVIEW_H
