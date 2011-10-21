//Our includes
#include "cwScrapStationView.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwDebug.h"

//Qt includes
#include <QDeclarativeComponent>
#include <QDeclarativeItem>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>

cwScrapStationView::cwScrapStationView(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    TransformUpdater(NULL),
    Scrap(NULL),
    StationItemComponent(NULL)
{
}

/**
  \brief Sets the transform updater

  This will transform all the station in this station view
  */
void cwScrapStationView::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {
        if(TransformUpdater != NULL) {
            //Remove all previous stations
            foreach(QDeclarativeItem* stationItem, StationItems) {
                TransformUpdater->removePointItem(stationItem);
            }
        }

        TransformUpdater = updater;

        if(TransformUpdater != NULL) {
            //Add station to the new transformUpdater
            foreach(QDeclarativeItem* stationItem, StationItems) {
                TransformUpdater->addPointItem(stationItem);
            }
        }

        emit transformUpdaterChanged();
    }
}

/**
Sets scrap that this class renders all the stations of
*/
void cwScrapStationView::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {

        if(Scrap != NULL) {
            //disconnect
            disconnect(Scrap, NULL, this, NULL);
        }

        Scrap = scrap;

        if(Scrap != NULL) {
            connect(Scrap, SIGNAL(stationAdded()), SLOT(stationAdded()));
            connect(Scrap, SIGNAL(stationRemoved(int)), SLOT(stationRemoved(int)));
            connect(Scrap, SIGNAL(stationPositionChanged(int)), SLOT(udpateStationPosition(int)));
            updateAllStations();
        }

        emit scrapChanged();
    }
}

/**
  This creates the station component used generate the station symobols
  */
void cwScrapStationView::createStationComponent() {
    //Make sure we have a note component so we can create it
    if(StationItemComponent == NULL) {
        QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
        if(context == NULL) { return; }
        StationItemComponent = new QDeclarativeComponent(context->engine(), "qml/NoteStation.qml", this);
        if(StationItemComponent->isError()) {
            qDebug() << "Notecomponent errors:" << StationItemComponent->errorString();
        }
    }
}

/**
  This is a private method, that adds a new station the end of the station list
  */
void cwScrapStationView::addNewStationItem() {
    if(StationItemComponent == NULL) {
        qDebug() << "StationItemComponent isn't ready, ... THIS IS A BUG" << LOCATION;
    }

    QDeclarativeItem* stationItem = qobject_cast<QDeclarativeItem*>(StationItemComponent->create());
    if(stationItem == NULL) {
        qDebug() << "Problem creating new station item ... THIS IS A BUG!" << LOCATION;
        return;
    }

    StationItems.append(stationItem);

    //Add the point to the transform updater
    if(TransformUpdater != NULL) {
        TransformUpdater->addPointItem(stationItem);
    }
}

/**
  Updates the station item's data
  */
void cwScrapStationView::updateStationItemData(int index){
    QDeclarativeItem* stationItem = StationItems[index];
    stationItem->setProperty("stationId", index);
    stationItem->setProperty("stationView", QVariant::fromValue(this));
    stationItem->setProperty("scrap", QVariant::fromValue(Scrap));
    stationItem->setParentItem(this);
}

/**
  Called when a station has been added to the scrap
  */
void cwScrapStationView::stationAdded() {
    createStationComponent();

    QDeclarativeItem* stationItem = qobject_cast<QDeclarativeItem*>(StationItemComponent->create());
    if(stationItem == NULL) {
        qDebug() << "Problem creating new station item ... THIS IS A BUG!" << LOCATION;
        return;
    }

    //Create a new station item
    addNewStationItem();

    //Update the station
    int lastIndex = StationItems.size() - 1;
    updateStationItemData(lastIndex);
}

/**
  This is called when the scrap removes a station
  */
void cwScrapStationView::stationRemoved(int stationIndex) {
    //Unselect the item that's going to be deleted
    if(stationIndex >= 0 || stationIndex < StationItems.size()) {
//        if(SelectedNoteStation == StationItems[stationIndex]) {
//            SelectedNoteStation = NULL;
//        }

        StationItems[stationIndex]->deleteLater();
        StationItems.removeAt(stationIndex);
//        regenerateStationVertices();
    }
}

/**
  \brief Updates the station's position
  */
void cwScrapStationView::udpateStationPosition(int stationIndex) {
    if(Scrap == NULL) { return; }
    if(stationIndex >= 0 || stationIndex < StationItems.size()) {
        QPointF point = Scrap->stationData(cwScrap::StationPosition, stationIndex).value<QPointF>();
        StationItems[stationIndex]->setProperty("position3D", QVector3D(point));
    }
}

/**
    \brief This is called when all the stations need to be reset

    Stations will be reused, extra stations will be delete.
  */
void cwScrapStationView::updateAllStations() {
    if(Scrap == NULL) { return; }

    //Make sure we have a note component so we can create it
    createStationComponent();

    if(StationItems.size() < Scrap->numberOfStations()) {
        int notesToAdd = Scrap->numberOfStations() - StationItems.size();
        //Add more stations to the NoteStations
        for(int i = 0; i < notesToAdd; i++) {
            int noteStationIndex = StationItems.size();

            if(noteStationIndex < 0 || noteStationIndex >= Scrap->stations().size()) {
                qDebug() << "noteStationIndex out of bounds:" << noteStationIndex << StationItems.size();
                continue;
            }

            addNewStationItem();
        }
    } else if(StationItems.size() > Scrap->numberOfStations()) {
        //Remove stations from NoteStations
        int notesToRemove = StationItems.size() - Scrap->numberOfStations();
        //Add more stations to the NoteStations
        for(int i = 0; i < notesToRemove; i++) {
            QDeclarativeItem* deleteStation = StationItems.last();
            StationItems.removeLast();
            deleteStation->deleteLater();
        }
    }

    //Update all the station data
    for(int i = 0; i < StationItems.size(); i++) {
        updateStationItemData(i);
    }

//    if(TransformUpdater != NULL) {
//        TransformUpdater->update();
//    }
}


