/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOMPRESSEDIMAGEITEM_H
#define CWCOMPRESSEDIMAGEITEM_H

//Qt includes
#include <QImage>
#include <QGraphicsItem>

//Our includes
#include "cwGlobals.h"
#include "cwCompressedImage.h"

/**
 * A QGraphicsItem that stores its image zlib-compressed (see
 * cwCompressedImage) and decompresses it only inside paint(). Use this for
 * items painted rarely (export tiles are rendered exactly once, in
 * cwCaptureManager::saveScene); every paint() pays a decompression into a
 * temporary full-size image.
 */
class CAVEWHERE_LIB_EXPORT cwCompressedImageItem : public QGraphicsItem
{
public:
    explicit cwCompressedImageItem(QGraphicsItem* parent = nullptr);

    void setImage(const QImage& image);

    //The stored image, for consumers (the label placer) that share the
    //compressed blob or read its pixel geometry without decompressing here.
    const cwCompressedImage& compressedImage() const { return m_image; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

private:
    cwCompressedImage m_image;
};

#endif // CWCOMPRESSEDIMAGEITEM_H
