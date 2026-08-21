/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCompressedImageItem.h"

//Qt includes
#include <QPainter>

cwCompressedImageItem::cwCompressedImageItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
}

void cwCompressedImageItem::setImage(const QImage& image)
{
    prepareGeometryChange();
    m_image = cwCompressedImage(image);
}

QRectF cwCompressedImageItem::boundingRect() const
{
    return QRectF(QPointF(), m_image.size());
}

void cwCompressedImageItem::paint(QPainter* painter,
                                  const QStyleOptionGraphicsItem* option,
                                  QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    //decompress() warns and returns null on a corrupt payload — drawing
    //nothing beats drawing garbage.
    const QImage image = m_image.decompress();
    if(image.isNull()) {
        return;
    }
    painter->drawImage(QPointF(), image);
}
