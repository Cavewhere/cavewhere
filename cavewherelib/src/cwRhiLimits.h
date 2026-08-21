/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRHILIMITS_H
#define CWRHILIMITS_H

// Qt includes
#include <QtGlobal>

// Std includes
#include <limits>

namespace cw {

// QRhiBuffer sizes are quint32, so a buffer can hold at most 4 GiB - 1 byte.
constexpr qint64 kMaxRhiBufferBytes = qint64(std::numeric_limits<quint32>::max());

// Largest byte size at or below @a byteSize that fits a QRhiBuffer and holds a
// whole number of @a stride-byte vertices. Returns @a byteSize unchanged when it
// already fits. A stride of zero or less has no whole-vertex meaning, so the
// size is clamped to the raw buffer limit.
constexpr qint64 clampedVertexBytes(qint64 byteSize, int stride)
{
    if (byteSize <= kMaxRhiBufferBytes) {
        return byteSize;
    }

    if (stride <= 0) {
        return kMaxRhiBufferBytes;
    }

    return (kMaxRhiBufferBytes / stride) * stride;
}

// Number of whole @a stride-byte vertices that fit in the clamped byte range.
constexpr qint64 clampedVertexCount(qint64 byteSize, int stride)
{
    if (stride <= 0) {
        return 0;
    }

    return clampedVertexBytes(byteSize, stride) / stride;
}

}

#endif // CWRHILIMITS_H
