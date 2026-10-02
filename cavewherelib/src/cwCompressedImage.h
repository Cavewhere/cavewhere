/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOMPRESSEDIMAGE_H
#define CWCOMPRESSEDIMAGE_H

//Qt includes
#include <QByteArray>
#include <QImage>
#include <QSize>

//Our includes
#include "cwGlobals.h"

/**
 * A QImage held zlib-compressed, as a cheap-to-copy value type (every member
 * is implicitly shared, so copies are handle copies). Export tile grabs are
 * transparent everywhere except cave ink — long runs of zero bytes — so
 * holding them raw is what ran whole-page exports out of application memory,
 * while compressed they shrink by orders of magnitude.
 *
 * Construct from a QImage to compress; decompress() returns a QImage that
 * owns its decompressed buffer without an extra pixel copy, or a null image
 * (with a warning) on a corrupt payload. The geometry accessors work without
 * decompressing, for consumers (the label placer) that reject by bounds
 * before ever touching pixels. Note decompress() drops the source image's
 * devicePixelRatio — consumers that care scale externally
 * (cwCompressedImageItem pairs it with an item-level setScale(1/dpr)).
 */
class CAVEWHERE_LIB_EXPORT cwCompressedImage
{
public:
    cwCompressedImage() = default;
    explicit cwCompressedImage(const QImage& image);

    bool isNull() const { return m_compressed.isEmpty(); }

    QSize size() const { return m_size; }
    qsizetype bytesPerLine() const { return m_bytesPerLine; }
    QImage::Format format() const { return m_format; }
    qint64 rawSizeInBytes() const { return m_rawSizeInBytes; }
    QByteArray compressedData() const { return m_compressed; }

    QImage decompress() const;

private:
    QByteArray     m_compressed;
    QSize          m_size;
    qsizetype      m_bytesPerLine = 0;
    QImage::Format m_format = QImage::Format_Invalid;
    qint64         m_rawSizeInBytes = 0;
};

#endif // CWCOMPRESSEDIMAGE_H
