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
    cwAbstractPointManager(parent),
    Scrap(NULL),
    ShotLineScale(1.0),
    ScrapItem(NULL)
{
    setFlag(QQuickItem::ItemHasContents, true);

    connect(this, SIGNAL(selectedItemIndexChanged()), this, SLOT(updateShotLines()));
}

cwScrapStationView::~cwScrapStationView() {
}


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
            connect(Scrap, SIGNAL(stationAdded()), SLOT(pointAdded()));
            connect(Scrap, SIGNAL(stationRemoved(int)), SLOT(pointRemoved(int)));
            connect(Scrap, SIGNAL(stationPositionChanged(int)), SLOT(updateItemPosition(int)));
            connect(Scrap, SIGNAL(stationNameChanged(int)), SLOT(updateShotLines()));
            connect(Scrap->noteTransformation(), SIGNAL(scaleChanged()), SLOT(updateShotLines()));
            connect(Scrap->noteTransformation(), SIGNAL(northUpChanged()), SLOT(updateShotLines()));
            resizeNumberOfItems(Scrap->numberOfStations());
        } else {
            resizeNumberOfItems(0); //No items, remove all old ones
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

/**
  Updates the station item's data
  */
void cwScrapStationView::updateItemData(QQuickItem* item, int pointIndex){
    QQuickItem* stationItem = item;
    stationItem->setProperty("scrap", QVariant::fromValue(scrap()));
    stationItem->setProperty("scrapItem", QVariant::fromValue(scrapItem()));
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

    cwNoteStation noteStation = scrap()->station(selectedItemIndex());
    //Get the current trip
    cwNote* note = scrap()->parentNote();
    cwTrip* trip = note->parentTrip();
    cwCave* cave = trip->parentCave();
    cwStationPositionLookup stationPositionLookup = cave->stationPositionLookup();

    if(noteStation.isValid() && stationPositionLookup.hasPosition(noteStation.name())) {
        QString stationName = noteStation.name();
        QSet<cwStation> neighboringStations = trip->neighboringStations(stationName);

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
    if(transformUpdater()) {
        if(!oldNode) {
            ShotLinesNode = new cwSGLinesNode();
            transformUpdater()->transformNode()->appendChildNode(ShotLinesNode);
        }

        ShotLinesNode->setLines(ShotLines);
        return transformUpdater()->transformNode();
    }
    return NULL;
}

/**
  \brief Updates the station's position
  */
void cwScrapStationView::updateItemPosition(QQuickItem* item, int index) {

    QPointF point = Scrap->stationData(cwScrap::StationPosition, index).value<QPointF>();
    item->setProperty("position3D", QVector3D(point));

    if(index == selectedItemIndex()) {
        updateShotLines();
    }

}

/**
  \brief This gets the note station for the currently select station

If no station is select, this returns an empty note station
  */
cwNoteStation cwScrapStationView::selectedNoteStation() const {
    if(selectedItem() != NULL) {
        int stationIndex = selectedItemIndex();
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
        updateAllItemData();
        emit scrapItemChanged();
    }
}


