#ifndef CWBASENOTESTATIONINTERACTION_H
#define CWBASENOTESTATIONINTERACTION_H

//Qt includes
#include <QDeclarativeItem>

//Our includes
#include "cwBasePanZoomInteraction.h"
class cwScrap;
class cwNoteStationView;

class cwBaseNoteStationInteraction : public cwBasePanZoomInteraction
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(cwNoteStationView* noteStationView READ noteStationView WRITE setNoteStationView NOTIFY noteStationViewChanged)

public:
    explicit cwBaseNoteStationInteraction(QDeclarativeItem *parent = 0);

    Q_INVOKABLE void addStation(QPointF notePosition);

    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

    cwNoteStationView* noteStationView() const;
    void setNoteStationView(cwNoteStationView* stationView);

signals:
    void scrapChanged();
    void noteStationViewChanged();

public slots:

private:
    cwScrap* Scrap;
    cwNoteStationView* NoteStationView;

};

inline cwScrap* cwBaseNoteStationInteraction::scrap() const {
    return Scrap;
}

/**
  Gets the note station view for this interaction
  */
inline cwNoteStationView* cwBaseNoteStationInteraction::noteStationView() const {
    return NoteStationView;
}

#endif // CWBASENOTESTATIONINTERACTION_H
