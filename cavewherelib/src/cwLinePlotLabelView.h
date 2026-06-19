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
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwLabel3dGroup;
class cwKeywordItem;
class cwKeywordItemModel;


class CAVEWHERE_LIB_EXPORT cwLinePlotLabelView : public cwLabel3dView
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinePlotLabelView)

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(cwKeywordItemModel* keywordItemModel READ keywordItemModel WRITE setKeywordItemModel NOTIFY keywordItemModelChanged)


public:
    explicit cwLinePlotLabelView(QQuickItem *parent = 0);
    ~cwLinePlotLabelView();

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

    cwKeywordItemModel* keywordItemModel() const;
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

signals:
    void regionChanged();
    void keywordItemModelChanged();

private:
    cwCavingRegion* Region; //!<
    QPointer<cwKeywordItemModel> m_keywordItemModel;

    // One label group + one keyword item per trip. The group is the keyword
    // item's setVisible() target, so filtering a trip's line plot hides its
    // station labels. The keyword item is registered only while the trip has
    // labels (0<->1), mirroring cwLeadView's per-scrap lifecycle.
    struct TripEntry {
        cwLabel3dGroup* group = nullptr;
        cwKeywordItem* keywordItem = nullptr;
    };
    QHash<cwTrip*, TripEntry> m_tripEntries;

    void connectCave(cwCave* cave);
    void disconnectCave(cwCave* cave);

    void addTrip(cwCave* cave, cwTrip* trip);
    void removeTrip(cwTrip* trip);

    QList<cwLabel3dItem> labels(cwCave* cave, cwTrip* trip) const;

    void addKeywordItem(cwTrip* trip, cwLabel3dGroup* group);
    void removeKeywordItem(TripEntry& entry);
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
    return m_keywordItemModel;
}
#endif // CWLINEPLOTLABELVIEW_H
