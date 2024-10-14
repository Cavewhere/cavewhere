/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTLABELVIEW_H
#define CWLINEPLOTLABELVIEW_H

//Our includes
#include "cwLabel3dView.h"
class cwCavingRegion;
class cwCave;
class cwLabel3dGroup;

//Qt includes
#include <QQmlEngine>


class cwLinePlotLabelView : public cwLabel3dView
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinePlotLabelView)

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)


public:
    explicit cwLinePlotLabelView(QQuickItem *parent = 0);

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

signals:
    void regionChanged();

private:
    cwCavingRegion* Region; //!<

    QList<cwLabel3dGroup*> CaveLabelGroups;

    void connectCave(cwCave* cave);
    void disconnectCave(cwCave* cave);

    QList<cwLabel3dItem> labels(cwCave* cave) const;

    void clear();
    void updateCaveStations(cwCave* cave);

public slots:
    
private slots:
    void addCaves(int begin, int end);
    void removeCaves(int begion, int end);
    void updateStations();



};

/**
Gets region
*/
inline cwCavingRegion* cwLinePlotLabelView::region() const {
    return Region;
}
#endif // CWLINEPLOTLABELVIEW_H
