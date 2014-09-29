/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwCaptureItem.h"

cwCaptureItem::cwCaptureItem(QObject *parent) :
    QObject(parent),
    Rotation(0.0)
{
}

/**
* @brief cwLayerCapture::setName
* @param name
*/
void cwCaptureItem::setName(QString name) {
    if(Name != name) {
        Name = name;
        emit nameChanged();
    }
}

/**
 * @brief cwCaptureItem::setPositionOnPaper
 * @param positionOnPaper
 *
 * Sets the position of the item in paper units
 */
void cwCaptureItem::setPositionOnPaper(QPointF positionOnPaper)
{
    if(PositionOnPaper != positionOnPaper) {
        PositionOnPaper = positionOnPaper;
        emit positionOnPaperChanged();
    }
}

/**
 * @brief cwCaptureItem::setPaperSizeOfItem
 * @param paperSize
 *
 * Sets the size of the item in paper units
 */
void cwCaptureItem::setPaperSizeOfItem(QSizeF paperSize)
{
    if(PaperSizeOfItem != paperSize) {
        PaperSizeOfItem = paperSize;
        emit paperSizeOfItemChanged();
    }
}

/**
 * @brief cwCaptureItem::setBoundingBox
 * @param boundingbox
 *
 * This sets the new bounding box for the capture item. This bounding box is
 * the local coordinate system in paper units.
 *
 * This is useful for displaying annotation, and interactions ontop of the item
 * in qml.
 */
void cwCaptureItem::setBoundingBox(QRectF boundingbox)
{
    if(boundingbox != BoundingBox) {
        BoundingBox = boundingbox;
        emit boundingBoxChanged();
    }
}

/**
* @brief cwCaptureItem::setRotation
* @param rotation
*/
void cwCaptureItem::setRotation(double rotation) {
    if(Rotation != rotation) {
        Rotation = rotation;
        emit rotationChanged();
    }
}
