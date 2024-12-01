/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
#include "cwAbstractScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"

//Qt includes
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QPen>
#include <QSGTransformNode>
#include <QSGFlatColorMaterial>

cwScrapStationView::cwScrapStationView(QQuickItem *parent) :
    cwScrapPointView(parent),
    LineDataDirty(false),
    ScaleAnimation(new QVariantAnimation(this))
    // OldTransformUpdater(nullptr)
{
    setFlag(QQuickItem::ItemHasContents, true);

    ScaleAnimation->setStartValue(QPointF(0.0, 0.0));
    ScaleAnimation->setEndValue(QPointF(1.0, 1.0));
    ScaleAnimation->setDuration(200);
    connect(ScaleAnimation, SIGNAL(valueChanged(QVariant)), this, SLOT(update()));

    connect(this, &cwScrapStationView::selectedItemIndexChanged, this, &cwScrapStationView::updateShotLinesWithAnimation);
    // connect(this, &cwScrapStationView::transformUpdaterChanged, this, &cwScrapStationView::updateTransformUpdate);
}

cwScrapStationView::~cwScrapStationView() {

}


/**
Sets scrap that this class renders all the stations of
*/
void cwScrapStationView::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {

        if(Scrap != nullptr) {
            //disconnect
            disconnect(Scrap, nullptr, this, nullptr);
            disconnect(Scrap->noteTransformation(), nullptr, this, nullptr);
        }

        Scrap = scrap;

        if(Scrap != nullptr) {
            connect(Scrap, SIGNAL(stationAdded()), SLOT(pointAdded()));
            connect(Scrap, SIGNAL(stationRemoved(int)), SLOT(pointRemoved(int)));
            connect(Scrap, SIGNAL(stationPositionChanged(int, int)), SLOT(updateItemsPositions(int, int)));
            connect(Scrap, SIGNAL(stationNameChanged(int)), SLOT(updateShotLines()));
            connect(Scrap->noteTransformation(), SIGNAL(scaleChanged()), SLOT(updateShotLines()));
            connect(Scrap->noteTransformation(), SIGNAL(northUpChanged()), SLOT(updateShotLines()));
            resizeNumberOfItems(Scrap->numberOfStations());
        } else {
            resizeNumberOfItems(0); //No items, remove all old ones
        }

        cwScrapPointView::setScrap(scrap);
    }
}

/**
 * @brief cwScrapStationView::updateShotLinesWithAnimation
 *
 * This update the line data, as well as start a scaling animation
 */
void cwScrapStationView::updateShotLinesWithAnimation()
{
    updateShotLines();
    ScaleAnimation->start();
}

/**
  \brief When a station has been selected, this updates the shot lines

  This shows the shot lines from the selected station.  If no station is
  currently selected, this will hide the lines
  */
void cwScrapStationView::updateShotLines() {
    if(scrap() == nullptr) { return; }
    if(scrap()->parentNote() == nullptr) { return; }
    if(scrap()->parentNote()->parentTrip() == nullptr) { return; }
    // if(transformUpdater() == nullptr) { return; }

    cwNoteStation noteStation = scrap()->station(selectedItemIndex());
    //Get the current trip
    cwNote* note = scrap()->parentNote();
    cwTrip* trip = note->parentTrip();
    cwCave* cave = trip->parentCave();
    cwStationPositionLookup stationPositionLookup = cave->stationPositionLookup();

    //Clear all the lines
    ShotLines.clear();

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
        dotPerMeter.scale(note->dotPerMeter(), note->dotPerMeter(), 1.0);

        QMatrix4x4 noteStationOffset;
        noteStationOffset.translate(QVector3D(noteStation.positionOnNote()));

        QMatrix4x4 toNormalizedNote = noteStationOffset *
                dotPerMeter *
                notePageAspect *
                noteTransformMatrix *
                scrap()->viewMatrix()->matrix() *
                offsetMatrix;

        //Only used if the scrap is in running profile
        QMatrix4x4 profileDirection = runningProfileDirection();

        //Go through all the neighboring stations and add the position to the line
        foreach(cwStation station, neighboringStations) {

            QVector3D currentPos = stationPositionLookup.position(station.name());

            if(scrap()->type() == cwScrap::RunningProfile) {
                bool foundStation = false;
                foreach(cwNoteStation currentNoteStation, scrap()->stations()) {
                    if(currentNoteStation.name().toLower() == station.name().toLower()) {
                        foundStation = true;
                    }
                }

                if(foundStation) { continue; }

                //Caluclate the running profile rotation based on the stations
                QMatrix4x4 toProfileRotation = cwRunningProfileScrapViewMatrix::Data(selectedStationPos, currentPos).matrix();

                toNormalizedNote = noteStationOffset *
                        dotPerMeter *
                        notePageAspect *
                        noteTransformMatrix *
                        profileDirection *
                        toProfileRotation *
                        offsetMatrix;
            }

            QVector3D normalizeNotePos = toNormalizedNote.map(currentPos);

            ShotLines.append(QLineF(noteStation.positionOnNote(), normalizeNotePos.toPointF()));
        }
    }

    LineDataDirty = true;

    //Update the node geometry
    update();
}

/**
 * @brief cwScrapStationView::updateTransformUpdate
 *
 * Called when the transform update has changed
 */
void cwScrapStationView::updateTransformUpdate() {
   //Called when the transform updater has changed

    // if(OldTransformUpdater != transformUpdater()) {

    //     if(OldTransformUpdater != nullptr) {
    //         disconnect(OldTransformUpdater, &cwTransformItemUpdater::matrixChanged, this, &cwScrapStationView::update);
    //     }

    //     OldTransformUpdater = transformUpdater();

    //     if(OldTransformUpdater != nullptr) {
    //         connect(OldTransformUpdater, &cwTransformItemUpdater::matrixChanged, this, &cwScrapStationView::update);
    //     }

    //     update();
    // }
}

/**
 * @brief cwScrapItem::updatePaintNode
 * @param oldNode
 * @return See qt documentation
 */
QSGNode *cwScrapStationView::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if(!oldNode) {
        oldNode = new QSGTransformNode();

        QSGFlatColorMaterial* forgroundColor = new QSGFlatColorMaterial();
        forgroundColor->setColor(QColor(0x19,0x19,0x19));

        QSGFlatColorMaterial* backgroundColor = new QSGFlatColorMaterial();
        backgroundColor->setColor(QColor(0xb5,0xe8,0xe0));

        ShotLinesNodeBackground = new cwSGLinesNode();
        ShotLinesNodeForground = new cwSGLinesNode();

        ShotLinesNodeBackground->setLineWidth(3.0);
        ShotLinesNodeForground->setLineWidth(1.0);

        ShotLinesNodeBackground->setMaterial(backgroundColor);
        ShotLinesNodeForground->setMaterial(forgroundColor);

        oldNode->appendChildNode(ShotLinesNodeBackground);
        oldNode->appendChildNode(ShotLinesNodeForground);
    }

    // if(transformUpdater()) {
    //     QPointF scale = ScaleAnimation->currentValue().toPointF();
    //     QMatrix4x4 scaleMatrix;
    //     scaleMatrix.scale(scale.x(), scale.y(), 1.0);

    //     cwNoteStation noteStation = scrap()->station(selectedItemIndex());
    //     QPointF position = noteStation.positionOnNote();

    //     QMatrix4x4 toOrigin;
    //     toOrigin.translate(QVector3D(-position.x(), -position.y(), 0.0));

    //     QMatrix4x4 fromOrigin;
    //     fromOrigin.translate(QVector3D(position.x(), position.y(), 0.0));

    //     QMatrix4x4 matrix = transformUpdater()->matrix();

    //     QSGTransformNode* transformNode = static_cast<QSGTransformNode*>(oldNode);
    //     transformNode->setMatrix(matrix * fromOrigin * scaleMatrix * toOrigin);
    // }

    if(LineDataDirty) {
        ShotLinesNodeBackground->setLines(ShotLines);
        ShotLinesNodeForground->setLines(ShotLines);
        LineDataDirty = false;
    }

    return oldNode;
}

/**
  This takes all the scrap note points in order and figure out if the user has
  entered them in from left to right, or right to left. Then return's a transformation matrix
  that flip along the y-axis. Left to right return's an idenity and right to left returns
  matrix that scaled (-1, 1, 1)
 */
QMatrix4x4 cwScrapStationView::runningProfileDirection() const
{
    QMatrix4x4 profileDirection;

    if(scrap()->type() == cwScrap::RunningProfile) {
        QMatrix4x4 noteMatrix = scrap()->noteTransformation()->matrix();

        QMatrix4x4 pageOffset;
        pageOffset.translate(-QVector3D(scrap()->stations().first().positionOnNote()));

        //Move to first station, rotate so up is along the y-axis
        QMatrix4x4 normalizePage = noteMatrix * pageOffset;

        int numLeft = 0;
        int numRight = 0;
        for(int i = 1; i < scrap()->stations().size(); i++) {
            const cwNoteStation& station = scrap()->stations().at(i);
            QVector3D rotatedNoteStation = normalizePage * QVector3D(station.positionOnNote());
            if(rotatedNoteStation.x() > 0) {
                ++numRight;
            } else {
                ++numLeft;
            }
        }

        if(numRight < numLeft) {
            //Make a matrix that flips along the y-axis
            //Going from right to left
            profileDirection.scale(-1.0, 1.0, 1.0);
        }
    }
    return profileDirection;
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
    if(selectedItem() != nullptr) {
        int stationIndex = selectedItemIndex();
        return scrap()->station(stationIndex);
    }
    return cwNoteStation();
}
