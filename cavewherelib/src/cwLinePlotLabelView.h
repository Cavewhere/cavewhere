/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTLABELVIEW_H
#define CWLINEPLOTLABELVIEW_H

//Qt includes
#include <QHash>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "cwLabel3dView.h"
#include "cwGlobals.h"
#include "cwKeywordItemRegistry.h"
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwLabel3dGroup;
class cwKeywordItemModel;


class CAVEWHERE_LIB_EXPORT cwLinePlotLabelView : public cwLabel3dView
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinePlotLabelView)

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(cwKeywordItemModel* keywordItemModel READ keywordItemModel WRITE setKeywordItemModel NOTIFY keywordItemModelChanged)


public:
    explicit cwLinePlotLabelView(QQuickItem *parent = 0);

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

    cwKeywordItemModel* keywordItemModel() const;
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

signals:
    void regionChanged();
    void keywordItemModelChanged();

private:
    cwCavingRegion* Region; //!<

    // One label group per trip. The group is the trip's keyword item's
    // setVisible() target (see updateKeywordItem), so filtering a trip's line plot
    // hides its station labels. The keyword item itself is owned by the registry,
    // registered only while the trip has labels (0<->1).
    QHash<cwTrip*, cwLabel3dGroup*> m_groups;
    cwKeywordItemRegistry<cwTrip*> m_keywordRegistry;

    void connectCave(cwCave* cave);
    void disconnectCave(cwCave* cave);

    void addTrip(cwCave* cave, cwTrip* trip);
    void removeTrip(cwTrip* trip);

    QList<cwLabel3dItem> labels(cwCave* cave, cwTrip* trip) const;

    void updateKeywordItem(cwTrip* trip);

    void clear();

private slots:
    void addCaves(int begin, int end);
    void removeCaves(int begin, int end);
    void caveTripsInserted(int begin, int end);
    void caveTripsRemoved(int begin, int end);
    void updateStations();

};

/**
Gets region
*/
inline cwCavingRegion* cwLinePlotLabelView::region() const {
    return Region;
}

inline cwKeywordItemModel* cwLinePlotLabelView::keywordItemModel() const {
    return m_keywordRegistry.model();
}
#endif // CWLINEPLOTLABELVIEW_H
