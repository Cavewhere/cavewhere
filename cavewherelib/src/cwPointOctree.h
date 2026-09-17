// cwPointOctree.h
#pragma once

//Qt includes
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"
#include "qbox3d.h"

/**
 * Format constants and pure helpers shared by the point octree builder, the
 * loader, and the renderer. Nothing here touches state, so every function is
 * safe to call from a cwConcurrent worker.
 */
namespace cw::octree {

    //Bumping this invalidates every cached manifest and node at once
    constexpr int kFormatGeneration = 3;

    //Points per node edge; spacing = nodeSize / kSampleGridResolution
    constexpr int kSampleGridResolution = 128;

    //A leaf holds at most this many points, so a node is at most 160 KB
    constexpr int kLeafMaxPoints = 20000;

    //Recursion guard for duplicate-heavy input
    constexpr int kMaxLevel = 20;

    constexpr int kChildCount = 8;

    constexpr quint16 kQuantMax = 65535;

    //uint16 x, y, z, reserved
    constexpr int kBytesPerPoint = 8;

    //Every quantized bit of every axis interleaves into a 48 bit Morton key
    constexpr int kMortonBitsPerAxis = 16;

    //Head and tail of the source file sampled by sourceFingerprint()
    constexpr qint64 kFingerprintWindowBytes = 4 * 1024 * 1024;

    struct QuantizedPoint {
        quint16 x = 0;
        quint16 y = 0;
        quint16 z = 0;
        quint16 reserved = 0;
    };

    /**
     * Quantizes point to the node's cell. Each axis is rounded to the nearest
     * of kQuantMax + 1 steps and clamped, so a point outside nodeBounds lands
     * on the closest face.
     */
    CAVEWHERE_LIB_EXPORT QuantizedPoint quantize(const QVector3D& point, const QBox3D& nodeBounds);

    //The inverse of quantize(), accurate to nodeSize / kQuantMax
    CAVEWHERE_LIB_EXPORT QVector3D dequantize(const QuantizedPoint& point, const QBox3D& nodeBounds);

    /**
     * The Morton (Z-order) key of a quantized point: bit i of x lands at bit
     * 3i, y at 3i + 1, and z at 3i + 2, so the key spans the low 48 bits.
     * Sorting by it lays points out so any run of consecutive points is
     * spatially compact, which is what cwPointOctreePickIndex indexes.
     */
    CAVEWHERE_LIB_EXPORT quint64 mortonKey(const QuantizedPoint& point);

    /**
     * Quantizes every point into the on-disk node payload: little-endian
     * uint16 x, y, z, reserved per point, so size() == points.size() * kBytesPerPoint.
     * The points are written in Morton order (see mortonKey), stably, so the
     * payload is deterministic and a pick can index it by spatial locality.
     */
    CAVEWHERE_LIB_EXPORT QByteArray quantizeAll(const QVector<QVector3D>& points, const QBox3D& nodeBounds);

    /**
     * The Potree style name of a node, where each digit is an octant index:
     * an empty path is "r", {3} is "r3", {3, 5} is "r35".
     */
    CAVEWHERE_LIB_EXPORT QString nodeName(const QVector<int>& octantPath);

    /**
     * A content fingerprint of the LAZ file at lazPath as it will be stored:
     * the file size, its first and last kFingerprintWindowBytes, the resolved
     * source CRS, and the frame CRS. Returns an empty string when the file
     * can't be read.
     */
    CAVEWHERE_LIB_EXPORT QString sourceFingerprint(const QString& lazPath,
                                                   const QString& sourceCS,
                                                   const QString& frameCS);

    /**
     * The cache key of the manifest for lazPath. The key's path is the LAZ
     * file's own directory, which the caller's cacher resolves against its
     * root.
     */
    CAVEWHERE_LIB_EXPORT cwDiskCacher::Key manifestKey(const QString& lazPath,
                                                       const QString& fingerprint);

    //The cache key of one node's payload, named by nodeName()
    CAVEWHERE_LIB_EXPORT cwDiskCacher::Key nodeKey(const QString& lazPath,
                                                   const QString& fingerprint,
                                                   const QString& nodeName);
}
