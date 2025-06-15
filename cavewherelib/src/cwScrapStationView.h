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

    void setScrap(cwScrap* scrap);
    void setScrapItem(cwScrapItem* scrapItem);
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

    virtual void updateItemPosition(QQuickItem* item, int index);
    virtual QString qmlSource() const;


private slots:
    void updateShotLinesWithAnimation();
    void updateShotLines();

protected:
    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);

    QMatrix4x4 runningProfileDirection() const;


};

// Q_DECLARE_METATYPE(cwScrapStationView*)

/**
 * @brief cwScrapStationView::qmlSource
 * @return The qml source of the point that'll be rendered
 */
inline QString cwScrapStationView::qmlSource() const
{
    return QStringLiteral("qrc:/qt/qml/cavewherelib/qml/NoteStation.qml");
}




#endif // CWSCRAPSTATIONVIEW_H
