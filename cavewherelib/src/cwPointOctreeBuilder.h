// cwPointOctreeBuilder.h
#pragma once

//Qt includes
#include <QFuture>
#include <QString>

//Std includes
#include <optional>

//Our includes
#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"

namespace cw::octree {

    //The points per cell the chunk depth aims at, averaged over every cell of
    //the depth. A flat tile fills only the cells its terrain crosses, so an
    //occupied chunk holds several times this, at 12 bytes a point in pass B.
    constexpr qint64 kChunkTargetPoints = 2000000;

    //At most 8^5 chunk cells
    constexpr int kMaxChunkDepth = 5;

    //The multiple of the chunk target a cell may hold before it is split, so
    //the cap follows whatever target the request asks for
    constexpr qint64 kChunkMaxPointsMultiple = 4;

    //Absorbs reprojection curvature before the measured-bounds retry
    constexpr double kRootPaddingFraction = 0.01;

    //The working set pass B's chunks may hold together before a chunk waits
    //for a running one to finish
    constexpr qint64 kDefaultBuildMemoryBudgetBytes = 1536LL * 1024 * 1024;
}

/**
 * Builds the point octree of a LAZ file without ever holding the cloud: the
 * file is read once, chunked into temporary files, turned into subtrees by a
 * fleet of workers, and written into the project's .cw_cache as one manifest
 * entry plus one entry per node.
 *
 * Threading: build() runs entirely on cwConcurrent workers. The request is
 * captured by value; the only shared state is cwDiskCacher, which mutexes per
 * file. Cancellation is polled per chunk of points, per chunk, and per node.
 */
class CAVEWHERE_LIB_EXPORT cwPointOctreeBuilder
{
public:
    struct Request {
        QString path;              //!< absolute filesystem path to a .laz / .las file
        QString sourceCSOverride;  //!< empty → use the LAZ embedded CS (or identity)
        QString frameCS;           //!< destination CS: the project's local projection
        QString cacheRootPath;     //!< the cwDiskCacher root, the project root

        //!< Points per chunk file, which sets the chunk depth
        qint64 chunkTargetPoints = cw::octree::kChunkTargetPoints;

        //!< Points a chunk cell may hold before it is split a level deeper.
        //!< 0 or less asks for kChunkMaxPointsMultiple x chunkTargetPoints. A
        //!< cell over the cap is split until it is under it or it sits at
        //!< kMaxChunkDepth, so an uneven cloud's chunks end up at mixed levels.
        qint64 chunkMaxPoints = 0;

        //!< Workers pass A decodes with, 0 = one per kMinPointsPerDecodeWorker
        //!< points. Either way the count is held to the pool, and a file whose
        //!< seek decodes from the start decodes on one worker whatever this asks.
        int decodeWorkerCount = 0;

        //!< The working set pass B's chunks may hold together, 0 = the
        //!< default. A chunk bigger than the whole budget runs on its own.
        qint64 memoryBudgetBytes = cw::octree::kDefaultBuildMemoryBudgetBytes;
    };

    //Stable codes so a test asserts the failure kind, not the message wording
    enum ErrorCode : int {
        OpenFailed = Monad::ResultBase::CustomError,
        ReadFailed,
        CacheWriteFailed,
        TempDirFailed,
        Cancelled,
    };

    using Result = Monad::Result<cwPointOctreeManifest>;

    /**
     * The depth whose cells hold about chunkTargetPoints each:
     * clamp(ceil(log8(npoints / chunkTargetPoints)), 0, kMaxChunkDepth).
     */
    static int chunkDepthFor(qint64 npoints,
                             qint64 chunkTargetPoints = cw::octree::kChunkTargetPoints);

    /**
     * The content fingerprint the build writes its entries under. Empty when
     * the file can't be read.
     */
    static QString fingerprintFor(const Request& request);

    /**
     * The already-built octree of request, or nullopt when it has to be built:
     * the manifest entry is missing or stale, or one of its nodes is gone.
     */
    static std::optional<cwPointOctreeManifest> cachedManifest(const Request& request);

    //Builds the octree and writes it into the cache. Never reads the cache.
    static QFuture<Result> build(const Request& request);
};
