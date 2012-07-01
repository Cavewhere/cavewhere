//Our includes
#include "cwBaseScrapInteraction.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwNote.h"
#include "cwImageItem.h"
#include "cwScrapOutlinePointView.h"

cwBaseScrapInteraction::cwBaseScrapInteraction(QQuickItem *parent) :
    cwNoteInteraction(parent),
    Scrap(NULL),
    ImageItem(NULL),
    OutlinePointView(NULL)
{
    connect(this, SIGNAL(noteChanged()), SLOT(startNewScrap()));
    connect(this, SIGNAL(visibleChanged()), SLOT(deactivating()));
}

/**
    This adds a new scrap to the note and also make the current scrap equal to the newly add
    scrap
  */
void cwBaseScrapInteraction::addScrap() {
    setScrap(new cwScrap());
    note()->addScrap(Scrap);
}

/**
 * @brief cwBaseScrapInteraction::closeCurrentScrap
 *
 * Closes the current scrap geometry
 */
void cwBaseScrapInteraction::closeCurrentScrap() {
    if(Scrap != NULL) {
        Scrap->close();
    }
}

cwBaseScrapInteraction::cwClosestPoint cwBaseScrapInteraction::calcClosestPoint(QPoint qtViewportPosition) const
{
    if(note() == NULL) {
        return cwClosestPoint();
    }

    if(imageItem() == NULL) {
        return cwClosestPoint();
    }

    if(Scrap == NULL) {
        return cwClosestPoint();
    }

    QPointF noteCoordinate = imageItem()->mapQtViewportToNote(qtViewportPosition);
    double buffer = 5.0;

//    foreach(cwScrap* scrap, note()->scraps()) {

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

//            qDebug() << "Line:" << (-360.0 + angle) << line << transLine << "rotatedNoteCoord" << rotatedNoteCoord;

            //Converted the rotated stuff back into pixels
            p1 = imageItem()->mapNoteToQtViewport(transLine.p1());
            p2 = imageItem()->mapNoteToQtViewport(transLine.p2());
            rotatedNoteCoord = imageItem()->mapNoteToQtViewport(rotatedNoteCoord);

            p1 += QPointF(0.0, -buffer);
            p2 += QPointF(0.0, buffer);

            QRectF rect(p1, p2);
            if(rect.contains(rotatedNoteCoord)) {
                QPointF closest(rotatedNoteCoord.x(), rect.center().y());
                QPointF closestNoteCoord = ImageItem->mapQtViewportToNote(closest.toPoint());
                QTransform inverse = transform.inverted();
                closestNoteCoord = inverse.map(closestNoteCoord);
                return cwClosestPoint(closestNoteCoord, i+1);
            }
        }
//    }

        return cwClosestPoint();
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
    Add a point to the end of the current scrap.

    If no scrap is currently selected, this will create a new scrap

    QPointF viewportCoordinate - The point in viewportCoordinate coordinates
  */
void cwBaseScrapInteraction::addPoint(QPoint viewportCoordinate) {
    if(note() == NULL) {
        qDebug() << "This is a bug! Can't add point because Note is null" << LOCATION;
        return;
    }

    if(Scrap == NULL) {
        addScrap();
    }

    if(imageItem() == NULL) {
        qDebug() << "This is a bug! Can't add point because the image item is null" << LOCATION;
        return;
    }

    if(outlinePointView() == NULL) {
        qDebug() << "This is a bug! Can't add point because controlPointView is null" << LOCATION;
        return;
    }

    if(Scrap == NULL) {
        qDebug() << "This is a bug! scrap is null" << LOCATION;
        return;
    }



    cwClosestPoint closestPoint = calcClosestPoint(viewportCoordinate);
    if(closestPoint.IsValid) {
        Scrap->insertPoint(closestPoint.InsertIndex, closestPoint.ClosestPoint);
    } else {
        //Add the last point to the scrap
        QPointF noteCoordinate = imageItem()->mapQtViewportToNote(viewportCoordinate);
        Scrap->addPoint(noteCoordinate);
    }
}

/**
  This starts a new scrap instead of
  */
void cwBaseScrapInteraction::startNewScrap() {
    closeCurrentScrap();
    setScrap(NULL);
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

    if(Scrap != NULL) {
        disconnect(Scrap, &cwScrap::closeChanged, this, &cwBaseScrapInteraction::startNewScrap);
    }

    Scrap = scrap;

    if(Scrap != NULL) {
        connect(Scrap, &cwScrap::closeChanged, this, &cwBaseScrapInteraction::startNewScrap);
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
        if(Scrap == NULL) {
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


