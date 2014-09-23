/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwCaptureItem.h"

cwCaptureItem::cwCaptureItem(QObject *parent) :
    QObject(parent)
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
