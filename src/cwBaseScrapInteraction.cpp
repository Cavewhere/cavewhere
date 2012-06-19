//Our includes
#include "cwBaseScrapInteraction.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwNote.h"
#include "cwImageItem.h"
#include "cwScrapControlPointView.h"

cwBaseScrapInteraction::cwBaseScrapInteraction(QQuickItem *parent) :
    cwNoteInteraction(parent),
    CurrentScrapIndex(-1),
    ImageItem(NULL),
    ControlPointView(NULL)
{
    connect(this, SIGNAL(noteChanged()), SLOT(startNewScrap()));
    connect(this, SIGNAL(visibleChanged()), SLOT(deactivating()));
}

/**
    This adds a new scrap to the note and also make the current scrap equal to the newly add
    scrap
  */
void cwBaseScrapInteraction::addScrap() {
    cwScrap* scrap = new cwScrap();
    note()->addScrap(scrap);
    CurrentScrapIndex = note()->scraps().size() - 1;
}

/**
 * @brief cwBaseScrapInteraction::closeCurrentScrap
 *
 * Closes the current scrap geometry
 */
void cwBaseScrapInteraction::closeCurrentScrap() {
    cwScrap* currentScrap = note()->scrap(CurrentScrapIndex);
    if(currentScrap != NULL) {
        if(!currentScrap->polygon().isClosed() &&
                currentScrap->polygon().size() > 1) {
            //Connect the last point to the first point
            currentScrap->addPoint(currentScrap->polygon().first());
        }
    }
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

    if(CurrentScrapIndex == -1) {
        addScrap();
    }

    if(imageItem() == NULL) {
        qDebug() << "This is a bug! Can't add point because the image item is null" << LOCATION;
        return;
    }

    if(controlPointView() == NULL) {
        qDebug() << "This is a bug! Can't add point because controlPointView is null" << LOCATION;
        return;
    }

    cwScrap* scrap = note()->scrap(CurrentScrapIndex);

    if(scrap == NULL) {
        qDebug() << "This is a bug! scrap is null" << LOCATION;
        return;
    }

    if(scrap->numberOfPoints() > 0) {
        QPointF firstPoint = scrap->points().first();
        QLineF(firstPoint, viewportCoordinate);
    }

    if(!controlPointView()->items().isEmpty()) {
        //Get the first qml object
        QQuickItem* scrapPoint = controlPointView()->items().first();
        QPointF point = imageItem()->mapToItem(scrapPoint, viewportCoordinate); //Point in scrap point coordinates

        //Ask the qml object if the point's contained in it
        QVariant pointInScrapPoint;
        QMetaObject::invokeMethod(scrapPoint, "contains", Q_RETURN_ARG(QVariant, pointInScrapPoint), Q_ARG(QVariant, point));

        if(pointInScrapPoint.toBool()) {
            //User has clicked on the first point
            startNewScrap();
            return;
        }
    }

    //Add the last point to the scrap
    QPointF noteCoordinate = imageItem()->mapQtViewportToNote(viewportCoordinate);
    scrap->addPoint(noteCoordinate);
}

/**
  This starts a new scrap instead of
  */
 void cwBaseScrapInteraction::startNewScrap() {
     closeCurrentScrap();
     CurrentScrapIndex = -1;
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
 void cwBaseScrapInteraction::setControlPointView(cwScrapControlPointView* controlPointView) {
     if(ControlPointView != controlPointView) {
         ControlPointView = controlPointView;
         emit controlPointViewChanged();
     }
 }

