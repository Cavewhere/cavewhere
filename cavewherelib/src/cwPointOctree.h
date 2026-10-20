// cwPointOctree.h
#pragma once

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

//Qt includes
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"
#include "qbox3d.h"

struct cwPointOctreeNode;

/**
 * Format constants and pure helpers shared by the point octree builder, the
 * loader, and the renderer. Nothing here touches state, so every function is
 * safe to call from a cwConcurrent worker.
 */
namespace cw::octree {

    //Bumping this invalidates every cached manifest and node at once.
    //Generation 4 adds each node's measured spacing and kMinSplitPoints;
    //generation 5 wrote a local spacing ratio into each point's reserved word,
    //generation 6 wrote the point's absolute 3D native spacing code there,
    //generation 7 wrote its horizontal native spacing under the
    //data-relative cap, and generation 8 measures it over the pass B chunk's
    //whole xy column and a halo of its neighbors, under a per-column cap.
    constexpr int kFormatGeneration = 8;

    //Points per node edge; spacing = nodeSize / kSampleGridResolution
    constexpr int kSampleGridResolution = 128;

    //! The grid spacing of a level in a root cube @a rootSize on a side
    inline double levelGridSpacing(double rootSize, int level)
    {
        return std::ldexp(rootSize / double(kSampleGridResolution), -level);
    }

    //A leaf holds at most this many points, so a node is at most 160 KB
    constexpr int kLeafMaxPoints = 20000;

    /**
     * A node that sampled its own points first keeps whatever its grid left
     * behind, as a leaf, while those leftovers number fewer than this, rather
     * than splitting them into children. Without it every leftover splits down
     * to kMaxLevel: on the BC tile 86 % of the 749 k node files sat at level 9
     * and deeper holding 8 % of the points. The value comes from that tile's
     * node histogram: at 256 the node count falls to about 117 k, levels 0..7
     * barely move, and the tree ends near level 11. A kept leaf holds at most
     * what sampled first, so it stays under kLeafMaxPoints.
     */
    constexpr int kMinSplitPoints = 256;
    static_assert(kMinSplitPoints <= kLeafMaxPoints,
                  "A kept leaf holds what sampled first, which is at most kLeafMaxPoints");

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

    //A cell packs into one key: level in the high bits, then x, y, z
    constexpr int kCellKeyLevelShift = 60;
    constexpr int kCellKeyXShift = 40;
    constexpr int kCellKeyYShift = 20;
    constexpr quint64 kCellKeyCoordinateMask = (quint64(1) << kCellKeyYShift) - 1;

    //! The key of the cell at @a level, @a x, @a y, @a z, which names one node
    constexpr quint64 cellKey(int level, quint32 x, quint32 y, quint32 z)
    {
        return (quint64(level) << kCellKeyLevelShift)
               | (quint64(x) << kCellKeyXShift)
               | (quint64(y) << kCellKeyYShift)
               | quint64(z);
    }

    //! cellKey() of the node's own cell
    CAVEWHERE_LIB_EXPORT quint64 cellKey(const cwPointOctreeNode& node);

    struct QuantizedPoint {
        quint16 x = 0;
        quint16 y = 0;
        quint16 z = 0;
        //! The point's native spacing code (encodeLocalSpacing), 0 where
        //! nothing was measured
        quint16 reserved = 0;
    };

    //! The code of a point nothing measured, which sizes by its grid spacing
    constexpr quint16 kUnmeasuredLocalSpacingCode = 0;

    /**
     * Per-point native spacing (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section
     * 14.5, WP13 and WP15). Every point carries the spacing of the raw data
     * around it: the distance r_k, measured along kNativeSpacingAxes, to its
     * kLocalSpacingNeighbors-th nearest neighbor at a distinct position there
     * among every point of the cloud near it, as a surface spacing
     * s = r_k * kLocalSpacingSurfaceFactor. The spacing is held to
     * kLoneSpacingMedianRatio times the median of its pass B chunk, clamped
     * to [kMinLocalSpacingUnits, kMaxLocalSpacingUnits], and stored as a
     * 16-bit log2 code over that absolute range
     * (cw::octree::measureNativeSpacing).
     *
     * The range is in the octree's own units, which are the frame CS's
     * meters whenever the layer has a frame CS and the source's native units
     * otherwise: a cloud in US survey feet with no frame CS codes over 0.005
     * to 64 feet (1.5 mm to 19.5 m), which still spans every spacing a lidar
     * tile carries, and the median cap usually binds first. The builder knows
     * nothing of the layer's horizontalMetersPerUnit, and a per-cloud range
     * would need a per-cloud decode in the shader, so the range stays fixed.
     */
    constexpr int kLocalSpacingNeighbors = 8;

    /**
     * The axes a native spacing is measured along: x and y, the horizontal
     * plane, with z ignored. This assumes nadir airborne lidar, which lays its
     * pulses down at a density per ground area: horizontal spacing is what is
     * uniform there, so a canopy return spaces by the pulses around it rather
     * than by the other returns meters above or below it, a slope reads its
     * pulse spacing rather than a secant longer, and returns stacked at one
     * xy count as one neighbor. A vertical or terrestrial scan (a cave wall, a
     * cliff) needs a per-cloud choice of axes later.
     */
    constexpr std::array<int, 2> kNativeSpacingAxes {0, 1};

    //! sqrt(pi / k): on a surface, the k nearest neighbors fill a disc of
    //! radius r_k, so pi r_k^2 = k s^2
    inline const double kLocalSpacingSurfaceFactor =
        std::sqrt(std::numbers::pi / double(kLocalSpacingNeighbors));

    //! The absolute range a native spacing codes over, in the octree's units
    constexpr double kMinLocalSpacingUnits = 0.005;
    constexpr double kMaxLocalSpacingUnits = 64.0;

    //! The octaves the codes from 1 to kQuantMax split evenly
    inline const double kLocalSpacingCodeOctaves =
        std::log2(kMaxLocalSpacingUnits / kMinLocalSpacingUnits);

    //! The cap, in median native spacings of the points of the point's own
    //! pass B xy column that found kLocalSpacingNeighbors neighbors, that
    //! keeps a lone return from drawing a giant disc
    //! (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section 14.5, WP15 and WP16).
    //! Pass C carries each top point's column spacing, so a top point is
    //! capped by the column it was measured in.
    constexpr double kLoneSpacingMedianRatio = 4.0;

    /**
     * How far past its column's xy faces pass B's neighbor index reaches into
     * the neighboring columns, in the octree's units
     * (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section 14.5, WP16 (B)). A
     * point's k-th neighbor lies its spacing over kLocalSpacingSurfaceFactor
     * away, so the BC tile's widest chunk ceiling (0.93 m) puts the farthest
     * neighbor an uncapped point there needs 1.48 m out: 2 units holds every
     * one of them with a margin, where the search's own widest reach (102
     * units) would pull in whole columns. Neighbors stop only at the halo's
     * outer edge, so a point whose k-th neighbor lies within this of it
     * measures exactly as a search over the whole cloud would wherever it
     * sits in its column, and a point farther than this from every face of
     * its column does for every k-th neighbor out to twice it. Like the
     * other *Units constants the bound assumes meter-scale units: a cloud in
     * US survey feet with the same ground spacing has ceilings near 3 units
     * and k-th neighbors out to about 4.8, so a point within 2 to 4.8 units
     * of a face there measures against a halo that stops short of some of
     * its neighbors, up to sqrt(2) wider at a face and 2 times at a corner.
     * When an overlapping column sits at a different level (one z cell of an
     * xy footprint split, its sibling kept), its whole overlap lies inside
     * the halo box and is lent, so that halo is the overlap plus the face
     * band rather than the band alone.
     */
    constexpr double kLocalSpacingHaloUnits = 2.0;

    //! The 16-bit code of a native @a spacing: its log2 over
    //! [kMinLocalSpacingUnits, kMaxLocalSpacingUnits] after the clamp, from 1
    //! at the floor to kQuantMax at the cap, so a measured point never codes
    //! to kUnmeasuredLocalSpacingCode
    CAVEWHERE_LIB_EXPORT quint16 encodeLocalSpacing(double spacing);

    //! The native spacing @a code stands for, whatever the grid; @a gridSpacing
    //! itself for kUnmeasuredLocalSpacingCode
    CAVEWHERE_LIB_EXPORT double decodeLocalSpacing(quint16 code, double gridSpacing);

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
     * quantizeAll() with @a localSpacingCodes, one per point in @a points'
     * order, written into each point's reserved word. A code travels with its
     * point through the Morton sort.
     */
    CAVEWHERE_LIB_EXPORT QByteArray quantizeAll(const QVector<QVector3D>& points,
                                                const QVector<quint16>& localSpacingCodes,
                                                const QBox3D& nodeBounds);

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
     * The pack file of the LAZ file at @a lazPath:
     * <cacheRootPath>/.cw_cache/<LAZ dir mirrored the way cwDiskCacher mirrors
     * a key path>/<laz file name>-octree<kFormatGeneration>.cwpack
     */
    CAVEWHERE_LIB_EXPORT QString packPath(const QString& cacheRootPath, const QString& lazPath);

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

    /**
     * The per-cloud part of every cache key: the LAZ file name and format
     * generation the ids start with, the LAZ directory the keys live in, and
     * the fingerprint they carry. Deriving it runs QFileInfo on the LAZ path,
     * which on iOS calls into the security-scoped file engine, so the renderer
     * derives it once per source and keys nodes by string concatenation.
     */
    struct CAVEWHERE_LIB_EXPORT NodeKeyBase {
        QString idPrefix;
        QString dir;
        QString fingerprint;
    };

    CAVEWHERE_LIB_EXPORT NodeKeyBase nodeKeyBase(const QString& lazPath,
                                                 const QString& fingerprint);

    //Same key as manifestKey(lazPath, fingerprint) for the base's cloud
    CAVEWHERE_LIB_EXPORT cwDiskCacher::Key manifestKey(const NodeKeyBase& base);

    //Same key as nodeKey(lazPath, fingerprint, nodeName) for the base's cloud,
    //without touching the file system
    CAVEWHERE_LIB_EXPORT cwDiskCacher::Key nodeKey(const NodeKeyBase& base,
                                                   const QString& nodeName);
}
