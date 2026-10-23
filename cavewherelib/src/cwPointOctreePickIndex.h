/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOINTOCTREEPICKINDEX_H
#define CWPOINTOCTREEPICKINDEX_H

//Qt includes
#include <QByteArray>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"

namespace cw::octree {
    //! Axes a quantized point and a quantized box are measured along
    constexpr int kAxisCount = 3;

    //! Points one index leaf covers
    constexpr int kPickLeafPoints = 64;

    //! Leaves one index group covers
    constexpr int kPickGroupLeaves = 8;
}

//! A two level box index over one node's Morton ordered payload.
/*!
    cw::octree::quantizeAll writes a node's points in Morton order, so any run
    of consecutive points is spatially compact. Cutting that run into leaves of
    cw::octree::kPickLeafPoints points and leaves into groups of
    cw::octree::kPickGroupLeaves gives an implicit two level tree whose boxes a
    pick can slab-test, so it scans one leaf instead of the whole node.

    Leaf i covers points [i * kPickLeafPoints, min((i + 1) * kPickLeafPoints,
    count)); group g covers leaves [g * kPickGroupLeaves, min((g + 1) *
    kPickGroupLeaves, leafCount)). Boxes stay in the payload's own quantized
    units, which the reader dequantizes with the node's origin and scale.

    A value type: it is built on the streamer's worker from the bytes that
    worker loaded, travels with them, and is released with them.
*/
class CAVEWHERE_LIB_EXPORT cwPointOctreePickIndex
{
public:
    //! The quantized bounds of one leaf or group, as min and max per axis
    struct QuantizedBox {
        quint16 min[cw::octree::kAxisCount] = {0, 0, 0};
        quint16 max[cw::octree::kAxisCount] = {0, 0, 0};
    };

    //! Indexes @a bytes, a payload of cw::octree::kBytesPerPoint per point.
    //! Empty bytes give an empty index, which a pick scans linearly.
    static cwPointOctreePickIndex build(const QByteArray& bytes);

    //! What build() on a payload of @a payloadBytes will hold, for the
    //! streamer's CPU cap, which has to size a load before it runs.
    static qint64 estimatedBytes(qint64 payloadBytes);

    qint64 byteSize() const;
    bool isEmpty() const { return leaves.isEmpty(); }

    QVector<QuantizedBox> leaves;
    QVector<QuantizedBox> groups;
};

//! One streamed octree node: the bytes the GPU draws and the pick reads, and
//! the index the pick descends before it reads them.
struct cwPointOctreeNodePayload {
    QByteArray bytes;
    cwPointOctreePickIndex index;
};

#endif // CWPOINTOCTREEPICKINDEX_H
