/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPointOctreePickIndex.h"
#include "cwPointOctree.h"

//Qt includes
#include <QtEndian>

//Std includes
#include <algorithm>

namespace {

    using cw::octree::kAxisCount;

    constexpr int kAxisBytes = int(sizeof(quint16));

    //! Leaves or groups covering @a count items at @a perParent items each
    qint64 parentCount(qint64 count, int perParent)
    {
        return (count + perParent - 1) / perParent;
    }

    void growToInclude(cwPointOctreePickIndex::QuantizedBox& box, const quint16 axes[kAxisCount])
    {
        for(int axis = 0; axis < kAxisCount; axis++) {
            box.min[axis] = std::min(box.min[axis], axes[axis]);
            box.max[axis] = std::max(box.max[axis], axes[axis]);
        }
    }

    void growToInclude(cwPointOctreePickIndex::QuantizedBox& box,
                       const cwPointOctreePickIndex::QuantizedBox& child)
    {
        for(int axis = 0; axis < kAxisCount; axis++) {
            box.min[axis] = std::min(box.min[axis], child.min[axis]);
            box.max[axis] = std::max(box.max[axis], child.max[axis]);
        }
    }

    cwPointOctreePickIndex::QuantizedBox emptyBox()
    {
        cwPointOctreePickIndex::QuantizedBox box;
        for(int axis = 0; axis < kAxisCount; axis++) {
            box.min[axis] = cw::octree::kQuantMax;
            box.max[axis] = 0;
        }
        return box;
    }
}

cwPointOctreePickIndex cwPointOctreePickIndex::build(const QByteArray& bytes)
{
    cwPointOctreePickIndex index;

    const qsizetype count = bytes.size() / cw::octree::kBytesPerPoint;
    if(count <= 0) {
        return index;
    }

    const qsizetype leafCount = parentCount(count, cw::octree::kPickLeafPoints);
    index.leaves.reserve(leafCount);
    index.groups.reserve(parentCount(leafCount, cw::octree::kPickGroupLeaves));

    const char* readPoint = bytes.constData();
    QuantizedBox group = emptyBox();

    for(qsizetype leaf = 0; leaf < leafCount; leaf++) {
        const qsizetype first = leaf * cw::octree::kPickLeafPoints;
        const qsizetype last = std::min(first + cw::octree::kPickLeafPoints, count);

        QuantizedBox box = emptyBox();
        for(qsizetype i = first; i < last; i++) {
            //Read through the void* overload: a QByteArray gives no alignment
            //guarantee a quint16* could rest on.
            const quint16 axes[kAxisCount] = {
                qFromLittleEndian<quint16>(readPoint),
                qFromLittleEndian<quint16>(readPoint + kAxisBytes),
                qFromLittleEndian<quint16>(readPoint + 2 * kAxisBytes)
            };
            growToInclude(box, axes);
            readPoint += cw::octree::kBytesPerPoint;
        }

        index.leaves.append(box);
        growToInclude(group, box);

        if((leaf + 1) % cw::octree::kPickGroupLeaves == 0 || leaf + 1 == leafCount) {
            index.groups.append(group);
            group = emptyBox();
        }
    }

    return index;
}

qint64 cwPointOctreePickIndex::estimatedBytes(qint64 payloadBytes)
{
    if(payloadBytes <= 0) {
        return 0;
    }

    const qint64 leafCount =
        parentCount(payloadBytes / cw::octree::kBytesPerPoint, cw::octree::kPickLeafPoints);
    const qint64 groupCount = parentCount(leafCount, cw::octree::kPickGroupLeaves);
    return (leafCount + groupCount) * qint64(sizeof(QuantizedBox));
}

qint64 cwPointOctreePickIndex::byteSize() const
{
    return (leaves.size() + groups.size()) * qint64(sizeof(QuantizedBox));
}
