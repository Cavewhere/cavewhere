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

//    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
//    Q_PROPERTY(cwNoteStationView* noteStationView READ noteStationView WRITE setNoteStationView NOTIFY noteStationViewChanged)
    Q_PROPERTY(cwScrapView* scrapView READ scrapView WRITE setScrapView NOTIFY scrapViewChanged)

public:
    explicit cwBaseNoteStationInteraction(QQuickItem *parent = 0);

    Q_INVOKABLE void addStation(QPointF notePosition);

//    void setScrap(cwScrap* scrap);
//    cwScrap* scrap() const;

//    cwNoteStationView* noteStationView() const;
//    void setNoteStationView(cwNoteStationView* stationView);

    cwScrapView* scrapView() const;
    void setScrapView(cwScrapView* scrapView);
signals:
//    void scrapChanged();
    void noteStationViewChanged();
    void scrapViewChanged();

public slots:

private:
//    cwScrap* Scrap;
//    cwNoteStationView* NoteStationView;
    cwScrapView* ScrapView; //!< The scrap view, to make sure we add the station to the correct scrap

    cwScrapItem* selectScrapForAdding(QList<cwScrapItem*> scrapItems);
};

//inline cwScrap* cwBaseNoteStationInteraction::scrap() const {
//    return Scrap;
//}

///**
//  Gets the note station view for this interaction
//  */
//inline cwNoteStationView* cwBaseNoteStationInteraction::noteStationView() const {
//    return NoteStationView;
//}

/**
 Gets the scrap view for this interaction
*/
inline cwScrapView* cwBaseNoteStationInteraction::scrapView() const {
    return ScrapView;
}


#endif // CWBASENOTESTATIONINTERACTION_H
