//Our includes
#include "cwScrapStationView.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwDebug.h"
#include "cwScrapItem.h"
#include "cwNote.h"
#include "cwTrip.h"
#include "cwGlobalDirectory.h"
#include "cwCave.h"
#include "cwSGLinesNode.h"

//Qt includes
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QPen>
#include <QSGTransformNode>

cwScrapStationView::cwScrapStationView(QQuickItem *parent) :
    QQuickItem(parent),
//    TransformUpdater(NULL),
//    TransformNodeDirty(false),
    Scrap(NULL),
//    StationItemComponent(NULL),
    ShotLineScale(1.0),
    ScrapItem(NULL)
//    SelectedStationIndex(-1)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

cwScrapStationView::~cwScrapStationView() {
}

///**
//  \brief Sets the transform updater

//  This will transform all the station in this station view
//  */
//void cwScrapStationView::setTransformUpdater(cwTransformUpdater* updater) {
//    if(TransformUpdater != updater) {
//        if(TransformUpdater != NULL) {
//            //Remove all previous stations
//            foreach(QQuickItem* stationItem, StationItems) {
//                TransformUpdater->removePointItem(stationItem);
//            }
//        }

//        TransformUpdater = updater;
//        TransformNodeDirty = true;

//        if(TransformUpdater != NULL) {
//            //Add station to the new transformUpdater
//            foreach(QQuickItem* stationItem, StationItems) {
//                TransformUpdater->addPointItem(stationItem);
//            }
//        }

//        emit transformUpdaterChanged();
//    }
//}

/**
Sets scrap that this class renders all the stations of
*/
void cwScrapStationView::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {

        if(Scrap != NULL) {
            //disconnect
            disconnect(Scrap, NULL, this, NULL);
            disconnect(Scrap->noteTransformation(), NULL, this, NULL);
        }

        Scrap = scrap;

        if(Scrap != NULL) {
            connect(Scrap, SIGNAL(stationAdded()), SLOT(stationAdded()));
            connect(Scrap, SIGNAL(stationRemoved(int)), SLOT(stationRemoved(int)));
            connect(Scrap, SIGNAL(stationPositionChanged(int)), SLOT(udpateStationPosition(int)));
            connect(Scrap, SIGNAL(stationNameChanged(int)), SLOT(updateShotLines()));
            connect(Scrap->noteTransformation(), SIGNAL(scaleChanged()), SLOT(updateShotLines()));
            connect(Scrap->noteTransformation(), SIGNAL(northUpChanged()), SLOT(updateShotLines()));
            updateAllStations();
        }

        emit scrapChanged();
    }
}

/**
 * @brief cwScrapStationView::setShotLineScale
 * @param scale
 */
void cwScrapStationView::setShotLineScale(float scale) {
    if(ShotLineScale != scale) {
        scale = ShotLineScale;
        emit shotLineScaleChanged();
        update();
    }
}

///**
//  This creates the station component used generate the station symobols
//  */
//void cwScrapStationView::createStationComponent() {
//    //Make sure we have a note component so we can create it
//    if(StationItemComponent == NULL) {
//        QQmlContext* context = QQmlEngine::contextForObject(this);
//        if(context == NULL) { return; }
//        StationItemComponent = new QQmlComponent(context->engine(), cwGlobalDirectory::baseDirectory() + "qml/NoteStation.qml", this);
//        if(StationItemComponent->isError()) {
//            qDebug() << "Notecomponent errors:" << StationItemComponent->errorString();
//        }
//    }
//}

///**
//  This is a private method, that adds a new station the end of the station list
//  */
//void cwScrapStationView::addNewStationItem() {
//    if(StationItemComponent == NULL) {
//        qDebug() << "StationItemComponent isn't ready, ... THIS IS A BUG" << LOCATION;
//    }

//    QQmlContext* context = QQmlEngine::contextForObject(this);
//    QQuickItem* stationItem = qobject_cast<QQuickItem*>(StationItemComponent->create(context));
//    if(stationItem == NULL) {
//        qDebug() << "Problem creating new station item ... THIS IS A BUG!" << LOCATION;
//        return;
//    }

//    StationItems.append(stationItem);

//    //Add the point to the transform updater
//    if(TransformUpdater != NULL) {
//        TransformUpdater->addPointItem(stationItem);
//    }
//}

/**
  Updates the station item's data
  */
void cwScrapStationView::updatePoint(QQuickItem* item, int pointIndex){
    QQuickItem* stationItem = item;
    stationItem->setProperty("stationId", pointIndex);
    stationItem->setProperty("stationView", QVariant::fromValue(this));
    stationItem->setProperty("scrap", QVariant::fromValue(scrap()));
    stationItem->setProperty("scrapItem", QVariant::fromValue(scrapItem()));
    stationItem->setParentItem(this);
}

/**
  \brief When a station has been selected, this updates the shot lines

  This shows the shot lines from the selected station.  If no station is
  currently selected, this will hide the lines
  */
void cwScrapStationView::updateShotLines() {
    if(scrap() == NULL) { return; }
    if(scrap()->parentNote() == NULL) { return; }
    if(scrap()->parentNote()->parentTrip() == NULL) { return; }
    if(transformUpdater() == NULL) { return; }

    cwNoteStation noteStation = scrap()->station(selectedStationIndex());
    //Get the current trip
    cwNote* note = scrap()->parentNote();
    cwTrip* trip = note->parentTrip();
    cwCave* cave = trip->parentCave();
    cwStationPositionLookup stationPositionLookup = cave->stationPositionLookup();

    if(noteStation.isValid() && stationPositionLookup.hasPosition(noteStation.name())) {
        QString stationName = noteStation.name();
        QSet<cwStation> neighboringStations = trip->neighboringStations(stationName);

        //The lines we are creating
        QPainterPath shotLines; //In normalized note coordinates

        //The position of the selected station
        QVector3D selectedStationPos = stationPositionLookup.position(noteStation.name());

        //Create the matrix to covert global position into note position
        QMatrix4x4 noteTransformMatrix = scrap()->noteTransformation()->matrix(); //Matrix from page coordinates to cave coordinates
        noteTransformMatrix = noteTransformMatrix.inverted(); //From cave coordinates to page coordinates

        QMatrix4x4 notePageAspect = note->scaleMatrix().inverted(); //The note's aspect ratio

        QMatrix4x4 offsetMatrix;
        offsetMatrix.translate(-selectedStationPos);

        QMatrix4x4 dotPerMeter;
        cwImage noteImage = note->image();
        dotPerMeter.scale(noteImage.originalDotsPerMeter(), noteImage.originalDotsPerMeter(), 1.0);

        QMatrix4x4 noteStationOffset;
        noteStationOffset.translate(QVector3D(noteStation.positionOnNote()));

        QMatrix4x4 toNormalizedNote = noteStationOffset *
                dotPerMeter *
                notePageAspect *
                noteTransformMatrix *
                offsetMatrix;

        //Clear all the lines
        ShotLines.clear();

        //Go through all the neighboring stations and add the position to the line
        foreach(cwStation station, neighboringStations) {

            QVector3D currentPos = stationPositionLookup.position(station.name());
            QVector3D normalizeNotePos = toNormalizedNote.map(currentPos);

            ShotLines.append(QLineF(noteStation.positionOnNote(), normalizeNotePos.toPointF()));
        }

        //Update the node geometry
        update();
    }
}

/**
 * @brief cwScrapItem::updatePaintNode
 * @param oldNode
 * @return See qt documentation
 */
QSGNode *cwScrapStationView::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if(TransformUpdater) {
        if(!oldNode) {
            ShotLinesNode = new cwSGLinesNode();
        }

        if(TransformNodeDirty) {
            TransformUpdater->transformNode()->appendChildNode(ShotLinesNode);
            TransformNodeDirty = false;
        }

        if(ShotLinesNode) {
            ShotLinesNode->setLines(ShotLines);
        }

        return TransformUpdater->transformNode();
    }
    return NULL;
}

///**
//  Called when a station has been added to the scrap
//  */
//void cwScrapStationView::stationAdded() {
//    createStationComponent();

//    if(StationItemComponent == NULL) {
//        qDebug() << "StationItemComponent bad" << LOCATION;
//        return;
//    }

//    //Create a new station item
//    addNewStationItem();

//    //Update the station
//    int lastIndex = StationItems.size() - 1;
//    updateStationItemData(lastIndex);
//}

///**
//  This is called when the scrap removes a station
//  */
//void cwScrapStationView::stationRemoved(int stationIndex) {
//    //Unselect the item that's going to be deleted
//    if(stationIndex >= 0 || stationIndex < StationItems.size()) {
//        if(selectedStationIndex() == stationIndex) {
//            clearSelection();
//        }

//        StationItems[stationIndex]->deleteLater();
//        StationItems.removeAt(stationIndex);

//        updateAllStationData();
//    }
//}

///**
//  \brief Updates the station's position
//  */
//void cwScrapStationView::udpateStationPosition(int stationIndex) {
//    if(Scrap == NULL) { return; }
//    if(stationIndex >= 0 || stationIndex < StationItems.size()) {
//        QPointF point = Scrap->stationData(cwScrap::StationPosition, stationIndex).value<QPointF>();
//        StationItems[stationIndex]->setProperty("position3D", QVector3D(point));

//        if(stationIndex == selectedStationIndex()) {
//            updateShotLines();
//        }
//    }
//}

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
            QQuickItem* deleteStation = StationItems.last();
            StationItems.removeLast();
            deleteStation->deleteLater();
        }
    }

    updateAllStationData();
}

/**
  \brief Goes through all the stations and updates there data
  */
void cwScrapStationView::updateAllStationData()
{
    //Update all the station data
    for(int i = 0; i < StationItems.size(); i++) {
        updateStationItemData(i);
    }
}

/**
Sets selectedStationIndex
*/
void cwScrapStationView::setSelectedStationIndex(int selectedStationIndex) {
    if(SelectedStationIndex != selectedStationIndex) {

       // scrapItem()->selectScrapStationView(this);

        if(selectedStationIndex >= StationItems.size()) {
            qDebug() << "Selected station index invalid" << selectedStationIndex << LOCATION;
            return;
        }

        //Deselect the old index
        QQuickItem* oldStationItem = selectedStationItem();
        if(oldStationItem != NULL) {
            oldStationItem->setProperty("selected", false);
        }

        //Select the new station item
        if(selectedStationIndex >= 0) {
            QQuickItem* newStationItem = StationItems[selectedStationIndex];
            if(!newStationItem->property("selected").toBool()) {
                newStationItem->setProperty("selected", true);
            }
        }

        SelectedStationIndex = selectedStationIndex;
        updateShotLines();
        emit selectedStationIndexChanged();
    }
}

/**
  Gets the currently select station item

  If there's no select station item, this will return null
  */
QQuickItem* cwScrapStationView::selectedStationItem() const {
    if(SelectedStationIndex >= 0 && SelectedStationIndex < StationItems.size()) {
        return StationItems[SelectedStationIndex];
    }
    return NULL;
}

/**
  \brief This gets the note station for the currently select station

If no station is select, this returns an empty note station
  */
cwNoteStation cwScrapStationView::selectedNoteStation() const {
    if(selectedStationItem() != NULL) {
        int stationIndex = selectedStationIndex();
        return scrap()->station(stationIndex);
    }
    return cwNoteStation();
}

/**
Sets scrapItem
*/
void cwScrapStationView::setScrapItem(cwScrapItem* scrapItem) {
    if(ScrapItem != scrapItem) {
        ScrapItem = scrapItem;
        updateAllStationData();
        emit scrapItemChanged();
    }
}
