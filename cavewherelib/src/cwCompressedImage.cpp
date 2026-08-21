/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCompressedImage.h"
#include "cwDebug.h"

//Qt includes
#include <QDebug>

namespace {
// zlib's fastest level. Export tiles are dominated by runs of transparent
// (zero) bytes, which every level collapses almost equally well — higher
// levels only add per-tile CPU on the GUI thread during the grab loop.
constexpr int CompressionLevel = 1;
}

cwCompressedImage::cwCompressedImage(const QImage& image)
{
    if(image.isNull()) {
        return;
    }
    m_size = image.size();
    m_bytesPerLine = image.bytesPerLine();
    m_format = image.format();
    m_rawSizeInBytes = image.sizeInBytes();
    m_compressed = qCompress(image.constBits(), image.sizeInBytes(), CompressionLevel);
}

QImage cwCompressedImage::decompress() const
{
    if(m_compressed.isEmpty()) {
        return QImage();
    }

    QByteArray pixels = qUncompress(m_compressed);
    if(pixels.size() != qsizetype(m_bytesPerLine) * m_size.height()) {
        //Corrupt payload; handing garbage pixels downstream would be worse
        //than nothing — but say so, or the image silently vanishes.
        qWarning() << "Compressed image payload doesn't match its geometry —"
                   << "dropping" << m_size << LOCATION;
        return QImage();
    }

    //Wrap the buffer without copying: the returned image owns it through the
    //cleanup function, so it outlives this cwCompressedImage.
    auto* holder = new QByteArray(std::move(pixels));
    return QImage(reinterpret_cast<const uchar*>(holder->constData()),
                  m_size.width(), m_size.height(), m_bytesPerLine, m_format,
                  [](void* info) { delete static_cast<QByteArray*>(info); },
                  holder);
}
