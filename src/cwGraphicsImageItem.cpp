/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGraphicsImageItem.h"

//Qt includes
#include <QPainter>

cwGraphicsImageItem::cwGraphicsImageItem(QGraphicsItem *parent) :
    QGraphicsItem(parent)
{
}

void cwGraphicsImageItem::setImage(QImage image)
{
    Image = image;
}

QImage cwGraphicsImageItem::image() const
{
    return Image;
}

/**
 * @brief cwGraphicsImageItem::boundingRect
 * @return Return's the bounding box of this graphics item
 */
QRectF cwGraphicsImageItem::boundingRect() const
{
    return QRectF(QPointF(), Image.size());
}

/**
 * @brief cwGraphicsImageItem::paint
 * @param painter
 * @param option
 * @param widget
 */
void cwGraphicsImageItem::paint(QPainter *painter,
                                const QStyleOptionGraphicsItem *option,
                                QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->drawImage(QPointF(), Image);
}
