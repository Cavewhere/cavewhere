/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBaseScrapInteraction.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwNote.h"
#include "cwImageItem.h"
#include "cwScrapOutlinePointView.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"

cwBaseScrapInteraction::cwBaseScrapInteraction(QQuickItem *parent) :
    cwNoteInteraction(parent),
    Scrap(nullptr),
    ImageItem(nullptr),
    OutlinePointView(nullptr)
{
    connect(this, SIGNAL(noteChanged()), SLOT(startNewScrap()));
    connect(this, SIGNAL(visibleChanged()), SLOT(deactivating()));
}

/**
    This adds a new scrap to the note and also make the current scrap equal to the newly add
    scrap
  */
void cwBaseScrapInteraction::addScrap() {
    //Get the last transform
    cwNoteTranformation* transform = lastNoteTransformation();

    setScrap(new cwScrap());

    if(transform != nullptr) {
        //Used the previous note transform
        scrap()->setCalculateNoteTransform(false);
        *(scrap()->noteTransformation()) = *transform; //Copy the note transform data to the scrap
    }

    note()->addScrap(scrap());
}

/**
 * @brief cwBaseScrapInteraction::closeCurrentScrap
 *
 * Closes the current scrap geometry
 */
void cwBaseScrapInteraction::closeCurrentScrap() {
    if(Scrap != nullptr) {
        Scrap->close();
    }
}

cwBaseScrapInteraction::cwClosestPoint cwBaseScrapInteraction::calcClosestPoint(QPoint qtViewportPosition) const
{
    if(note() == nullptr) {
        return cwClosestPoint();
    }

    if(imageItem() == nullptr) {
        return cwClosestPoint();
    }

    if(Scrap == nullptr) {
        return cwClosestPoint();
    }

    QPointF noteCoordinate = imageItem()->mapQtViewportToNote(qtViewportPosition);
    double buffer = 7.5; //In pixels

    //Go through each scrap polygon's lines
    for(int i = 0; i < Scrap->numberOfPoints() - 1; i++) {
        QPointF p1 = Scrap->points().at(i);
        QPointF p2 = Scrap->points().at(i+1);
        QLineF line(p1, p2);
        float angle = line.angle();

        QTransform transform;
        transform.rotate(-360.0 + angle);
        transform.translate(-p1.x(), -p1.y());

        QLineF transLine = transform.map(line);
        QPointF rotatedNoteCoord = transform.map(noteCoordinate);

        QPointF mappedBuffer = imageItem()->mapQtViewportToNote(QPoint(0, buffer));
        QPointF origin = imageItem()->mapQtViewportToNote(QPoint(0, 0));
        double bufferNoteCoords = QLineF(origin, mappedBuffer).length();

        p1 = transLine.p1();
        p2 = transLine.p2();

        p1 += QPointF(0.0, -bufferNoteCoords);
        p2 += QPointF(0.0, bufferNoteCoords);

        QRectF rect(p1, p2);
        if(rect.contains(rotatedNoteCoord)) {
            QPointF closestNoteCoord(rotatedNoteCoord.x(), rect.center().y());
            QTransform inverse = transform.inverted();
            closestNoteCoord = inverse.map(closestNoteCoord);
            return cwClosestPoint(closestNoteCoord, i+1);
        }
    }

    return cwClosestPoint();
}

/**
 * @brief cwBaseScrapInteraction::lastNoteTransformation
 * @return Will return the last note tranformation.  This will return nullptr
 * if the wasn't a previous note transformation from this trip
 */
cwNoteTranformation *cwBaseScrapInteraction::lastNoteTransformation() const
{
    cwNote* lastScrapNote = nullptr;
    if(note()->hasScraps()) {
        lastScrapNote = note();
    } else {
        cwSurveyNoteModel* model = note()->parentTrip()->notes();
        int index = model->notes().indexOf(note());
        int previousNote = index - 1;
        while(previousNote >= 0) {
            lastScrapNote = model->notes().at(previousNote);
            if(lastScrapNote->hasScraps()) {
                break;
            }
            previousNote--;
        }
    }

    if(lastScrapNote != nullptr) {
        if(lastScrapNote->hasScraps()) {
            cwScrap* lastScrap = lastScrapNote->scraps().last();
            if(!lastScrap->calculateNoteTransform()) {
                return lastScrap->noteTransformation();
            }
        }
    }

    return nullptr;
}

/**
 * @brief cwBaseScrapInteraction::deactivating
 *
 * Called when the interaction is not visible anymore
 */
void cwBaseScrapInteraction::deactivating() {
    if(!isVisible()) {
        startNewScrap();
    }
}

/**
  This starts a new scrap instead of
  */
void cwBaseScrapInteraction::startNewScrap() {
    closeCurrentScrap();
}

/**
 Sets imageItem
 */
void cwBaseScrapInteraction::setImageItem(cwImageItem* imageItem) {
    if(ImageItem != imageItem) {
        ImageItem = imageItem;
        emit imageItemChanged();
    }
}

/**
Sets controlPointView
*/
void cwBaseScrapInteraction::setOutlinePointView(cwScrapOutlinePointView* controlPointView) {
    if(OutlinePointView != controlPointView) {
        OutlinePointView = controlPointView;
        emit outlinePointViewChanged();
    }
}

/**
  * @brief cwBaseScrapInteraction::setScrap
  * @param scrap - Sets the current scrap
  */
void cwBaseScrapInteraction::setScrap(cwScrap *scrap)
{
    if(Scrap == scrap) { return; }

    if(Scrap != nullptr) {
        disconnect(Scrap, &cwScrap::destroyed, this, &cwBaseScrapInteraction::scrapDeleted);
    }

    Scrap = scrap;

    if(Scrap != nullptr) {
        connect(Scrap, &cwScrap::destroyed, this, &cwBaseScrapInteraction::scrapDeleted);
    }

    emit scrapChanged();
}

/**
  * @brief cwBaseScrapInteraction::mouseOver
  * @param position - The mouse is over position
  *
  * This returns a variant map.
  *
  * The map, has 2 keys in it, if it's valid.  It'll have no key's if it's qtViewportPosition
  * isn't valid.
  *
  * The keys are:
  * IsSnapped - True if the point is snapped to the scrap's
  * NoteCoordsPoint - Where the point should be placed on the line (In note coordinates)
  * QtViewportPoint - Where the point should be placed on the line (In note coordinates)
  * InsertIndex - Where the point should be inserted into the scrap
  *
  */
QVariantMap cwBaseScrapInteraction::snapToScrapLine(QPoint qtViewportPosition) const
{
    cwClosestPoint point = calcClosestPoint(qtViewportPosition);
    QVariantMap map;

    if(point.IsValid) {
        //Point is snapped
        map.insert("IsSnapped", true);
        map.insert("NoteCoordsPoint", point.ClosestPoint);
        map.insert("QtViewportPoint", imageItem()->mapNoteToQtViewport(point.ClosestPoint));
        map.insert("InsertIndex", point.InsertIndex);
    } else {
        //Point isn't snapped to the line
        map.insert("IsSnapped", false);
        map.insert("NoteCoordsPoint", imageItem()->mapQtViewportToNote(qtViewportPosition));
        map.insert("qtViewportPoint", qtViewportPosition);
        if(Scrap == nullptr) {
            map.insert("InsertIndex", 0);
        } else {
            map.insert("InsertIndex", Scrap->numberOfPoints());
        }
    }

    return map;
}

///**
//  * @brief cwBaseScrapInteraction::mouseOver
//  * @param position - The mouse is over position
//  */
//bool cwBaseScrapInteraction::mouseOver(QPoint position) const
//{
//    cwClosestPoint point = calcClosestPoint(position);
//    return point.IsValid;
//}


