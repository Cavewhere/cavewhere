/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBASENOTESTATIONINTERACTION_H
#define CWBASENOTESTATIONINTERACTION_H

//Qt includes
#include <QQuickItem>

//Our includes
#include "cwBasePanZoomInteraction.h"
class cwScrap;
//class cwNoteStationView;
class cwScrapView;
class cwScrapItem;

class cwBaseNoteStationInteraction : public cwInteraction
{
    Q_OBJECT

    Q_PROPERTY(cwScrapView* scrapView READ scrapView WRITE setScrapView NOTIFY scrapViewChanged)

public:
    explicit cwBaseNoteStationInteraction(QQuickItem *parent = 0);

    Q_INVOKABLE void addStation(QPointF notePosition);

    cwScrapView* scrapView() const;
    void setScrapView(cwScrapView* scrapView);
signals:

    void noteStationViewChanged();
    void scrapViewChanged();

public slots:

private:

    cwScrapView* ScrapView; //!< The scrap view, to make sure we add the station to the correct scrap

    cwScrapItem* selectScrapForAdding(QList<cwScrapItem*> scrapItems);
};

/**
 Gets the scrap view for this interaction
*/
inline cwScrapView* cwBaseNoteStationInteraction::scrapView() const {
    return ScrapView;
}


#endif // CWBASENOTESTATIONINTERACTION_H
