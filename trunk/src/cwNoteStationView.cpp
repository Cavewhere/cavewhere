//Our includes
#include "cwNoteStationView.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwDebug.h"

//Qt includes
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QDebug>

cwNoteStationView::cwNoteStationView(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{

}

/**
  Sets the scrap for the station to be generated
  */
void cwNoteStationView::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {
        Scrap = scrap;
        emit scrapChanged();
    }
}

/**
  \brief Sets the selected station.

  This will deselect the previous station.  Send this null to deselect the current
  station
  */
void cwNoteStationView::setSelectedStation(QDeclarativeItem* selectedStation) {
    if(SelectedStation != NULL && SelectedStation->property("selected").toBool() != false) {
        SelectedStation->setProperty("selected", false);
    }

    SelectedStation = selectedStation;

    if(SelectedStation != NULL && SelectedStation->property("selected").toBool() != true) {
        SelectedStation->setProperty("selected", true);
    }
}

/**
  This assumes that scrap has changed and all note stations need to be reloaded.

  There positions and names need to be updated
  */
void cwNoteStationView::updateAllNoteStationItems() {
    //Make sure we have a note component so we can create it
    if(NoteStationComponent == NULL) {
        QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
        if(context == NULL) { return; }
        NoteStationComponent = new QDeclarativeComponent(context->engine(), "qml/NoteStation.qml", this);
        if(NoteStationComponent->isError()) {
            qDebug() << "Notecomponent errors:" << NoteStationComponent->errorString();
        }
    }

    if(NoteStationItems.size() < Scrap->numberOfStations()) {
        int notesToAdd = Scrap->numberOfStations() - NoteStationItems.size();
        //Add more stations to the NoteStations
        for(int i = 0; i < notesToAdd; i++) {
            int noteStationIndex = NoteStationItems.size();

            if(noteStationIndex < 0 || noteStationIndex >= Scrap->stations().size()) {
                qDebug() << "noteStationIndex out of bounds:" << noteStationIndex << NoteStationItems.size();
                continue;
            }

            QDeclarativeItem* stationItem = qobject_cast<QDeclarativeItem*>(NoteStationComponent->create());
            if(stationItem == NULL) {
                qDebug() << "Problem creating new station item ... THIS IS A BUG!";
                continue;
            }

            NoteStationItems.append(stationItem);
            TransformUpdater->addPointItem(stationItem);
        }
    } else if(NoteStationItems.size() > Scrap->numberOfStations()) {
        //Remove stations from NoteStations
        int notesToRemove = NoteStationItems.size() - Scrap->numberOfStations();
        //Add more stations to the NoteStations
        for(int i = 0; i < notesToRemove; i++) {
            QDeclarativeItem* deleteStation = NoteStationItems.last();
            NoteStationItems.removeLast();
            deleteStation->deleteLater();
        }
    }

    for(int i = 0; i < NoteStationItems.size(); i++) {
        QDeclarativeItem* stationItem = NoteStationItems[i];
        stationItem->setProperty("stationId", i);
        stationItem->setProperty("noteViewer", QVariant::fromValue(this));
        stationItem->setProperty("note", QVariant::fromValue(Scrap->parentNote()));
        stationItem->setParentItem(this);
    }

    TransformUpdater->update();
}

/**
  \brief Adds a station to the view

  This will create a new station item
  */
void cwNoteStationView::addNoteStationItem() {

}

/**
  \brief Sets the transform updater

  This will transform all the station in this station view
  */
void cwNoteStationView::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {

        TransformUpdater = updater;

//        updateAllScraps();

        emit transformUpdaterChanged();
    }
}

/**
  This gets the select note station from the view

  If no station is select, this returns an empty note station
  */
cwNoteStation cwNoteStationView::selectNoteStation() {
    cwNoteStation selectedStation;
    if(SelectedStation != NULL &&
            Scrap != NULL) {
        int stationId = SelectedStation->property("stationId").toInt();
        selectedStation = Scrap->station(stationId);
    }
    return selectedStation;
}

/**
  Selects the station at index, if index is out of bounds, this does nothing
  */
void cwNoteStationView::selectNoteStation(int index) {
    if(index < 0 || index >= NoteStationItems.size()) {
        qDebug() << "Can't select index, out of range" << index << LOCATION;
        return;
    }

    NoteStationItems[index]->setProperty("selected", true);
}


