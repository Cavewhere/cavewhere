/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwDebug.h"
#include "cwGeometryItersecter.h"
#include "cwPickingLog.h"
#include "cwRenderObject.h"
#include "cwConcurrent.h"
#include "cwTask.h"

//Std limits
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <math.h>

//Qt includes
#include <QtNumeric>
#include <QPlane3D>
#include <QPromise>
#include <QThreadPool>
#include <QVarLengthArray>

Q_LOGGING_CATEGORY(lcPick, "cw.picking", QtWarningMsg)

// Logging category lcPick is defined at top-of-file and declared in
// cwPickingLog.h so cwRenderTexturedItems.cpp and cwScrapManager.cpp
// can share the same name ("cw.picking"). Enable with:
//   QT_LOGGING_RULES="cw.picking.debug=true"

namespace {
    QString formatKey(const cwGeometryItersecter::Object& object)
    {
        const cwRenderObject* parent = object.parent();
        const char* parentClass = parent != nullptr
            ? parent->metaObject()->className()
            : "(null)";
        return QStringLiteral("{%1@%2, id=%3}")
            .arg(QLatin1String(parentClass))
            .arg(reinterpret_cast<quintptr>(parent), 0, 16)
            .arg(object.id());
    }
}

// Per-Object acceleration structure, model space. Cached across rebuilds
// in m_subBvhs and republished into BvhData. The primitives' nodeIndex
// references back to the BvhData::nodesSnapshot slot the Object lived in
// at build time, so testPrimitive can look up geometry and modelMatrix
// the same way as the old flat-BVH path.
struct cwGeometryItersecter::SubBvh {
    QVector<BvhNode> bvhNodes;          // model space
    QVector<Primitive> primitives;
    QBox3D modelRootBox;                // bvhNodes[0].bbox, cached

    // Owning copy of the Object (cheap — cwGeometry is implicitly shared).
    // Holding it here decouples sub-BVH primitives from BvhData::
    // nodesSnapshot ordering: across rebuilds, the same Object may move
    // to a different snapshot index, so traversal cannot key on
    // prim.nodeIndex anymore. Note: object.modelMatrix() may be stale
    // relative to the current BvhData snapshot; the caller passes the
    // fresh worldFromModel/modelToWorld in from BvhData::modelMatrices.
    // pickRadius / geometry / parent / id are stable across modelMatrix
    // changes (addObject re-replacing geometry evicts the cache entirely).
    Object object;
};

// Closest sphere-miss seen during a traversal; tryPromoteNearMiss snaps
// best to it when no true hit exists.
struct cwGeometryItersecter::NearMissResult {
    bool valid = false;
    double dSq = (std::numeric_limits<double>::max)();
    double tCenterModel = 0.0;
    Primitive prim;
    float radius = 0.0f;
    // Pointer into the owning SubBvh::object — valid for the duration
    // of intersectsDetailed (SubBvh is held alive by m_bvh->subBvhs).
    // SubBvh::object's modelMatrix may be stale (sub-BVHs survive
    // modelMatrix changes), so worldFromModel below carries the fresh
    // matrix from BvhData::modelMatrices.
    const Object* object = nullptr;
    QMatrix4x4 worldFromModel;
};

// Per-pick rejection counters. Populated by intersectsDetailed and
// testPrimitive when the cw.picking category has debug enabled; nullptr
// otherwise so the hot path pays nothing. Defined here (not in the anon
// namespace) because cwGeometryItersecter::PickStats is forward-declared
// in the header for the testPrimitive signature.
struct cwGeometryItersecter::PickStats {
    int nodesVisited = 0;
    int nodesBoxMiss = 0;          // ray missed the BVH-node AABB
    int nodesPrunedByBest = 0;     // AABB hit but farther than current best
    int leavesVisited = 0;
    int primsTested = 0;
    int primsNotPickable = 0;      // parent cwRenderObject not visible
    int primsPointRadiusZero = 0;  // Point primitive with pickRadius == 0
    int primsSphereMiss = 0;       // ray missed the point's sphere
    int primsRayBehind = 0;        // tNear/tWorld <= 0 (origin past prim)
    int primsTriMiss = 0;          // ray-triangle test rejected
    int primsLineMiss = 0;         // ray outside the line's screen-space tube
    int primsFartherThanBest = 0;  // hit but farther than current best
    int primsAccepted = 0;         // became the new best

    // Hidden friend — found by ADL when streaming a PickStats, and the
    // friendship lets the operator name the (private) PickStats type
    // from outside cwGeometryItersecter.
    friend QDebug operator<<(QDebug d, const PickStats& s)
    {
        QDebugStateSaver saver(d);
        d.nospace()
            << "traversal: nodesVisited=" << s.nodesVisited
            << " boxMiss=" << s.nodesBoxMiss
            << " prunedByBest=" << s.nodesPrunedByBest
            << " leaves=" << s.leavesVisited
            << " | prims tested=" << s.primsTested
            << " accepted=" << s.primsAccepted
            << " notPickable=" << s.primsNotPickable
            << " ptRadius0=" << s.primsPointRadiusZero
            << " sphereMiss=" << s.primsSphereMiss
            << " rayBehind=" << s.primsRayBehind
            << " triMiss=" << s.primsTriMiss
            << " lineMiss=" << s.primsLineMiss
            << " fartherThanBest=" << s.primsFartherThanBest;
        return d;
    }
};

namespace {
    // Multiplier applied to a point's pickRadius when expanding the cloud's
    // broad-phase AABB so that rays passing tangentially through the
    // outermost spheres aren't rejected by the box test.
    constexpr float kPointAabbPadScale = 1.0f;

    // Tube-pick fallback radius as a multiplier of pickRadius. Lets the
    // user clicking near a point in a sub-pixel gap still pivot on it.
    constexpr float kTubeFactor = 5.0f;

    // Leaf threshold — the largest count of primitives we'll let stop
    // subdivision. Bigger leaves trade a slightly longer per-leaf linear
    // scan for far fewer BVH nodes. 16 keeps BVH overhead at ~N/8 nodes,
    // which is the difference between a 100M-point LAZ fitting in RAM and
    // not.
    constexpr int kBvhLeafSize = 16;

    // Each Phase A worker fills approximately this many BuildPrim slots.
    // 64K per chunk balances task overhead against load balance across
    // cores — typical LAZ clouds (10M-100M points) yield 150-1500 chunks.
    constexpr qsizetype kEnumChunkSize = 64 * 1024;

    // Phase B-1 splits the root range serially down this many levels
    // (2^4 = up to 16 SubRanges fed to the parallel Phase B-2 builder).
    constexpr int kParallelFanoutDepth = 4;

    // Inline stack capacity for BVH traversal. Realistic BVH depth is
    // log2(prims) — under 30 even for 100M-point clouds — so 64 covers
    // both the top-level and the per-Object sub-BVH traversals without
    // ever spilling to the heap.
    constexpr int kBvhTraversalStackInline = 64;

    // Job name surfaced in the cwFutureManagerModel UI while the async
    // BVH build is running.
    constexpr auto kAcceleratingPickingJobName = QLatin1StringView("Accelerating picking");

    // An axis-aligned box has 8 corners; bit i of the corner index selects
    // the maximum along axis i.
    constexpr int kBoxCornerCount = 8;
    constexpr int kCornerMaskX = 1;
    constexpr int kCornerMaskY = 2;
    constexpr int kCornerMaskZ = 4;

    // Fixed progress resolution: per-mille (0..1000). Decouples the
    // setProgressValue range (int) from absolute primitive counts so
    // multi-billion-point clouds can still report monotonic progress
    // without saturating at INT_MAX.
    constexpr int kProgressResolution = 1000;

    // Per-phase progress reporter. `base` and `total` are constant for
    // the lifetime of a phase; only `done` is the running count passed
    // in at each call. Scales (base + done) into [0, kProgressResolution]
    // and pushes to the promise.
    struct ProgressScaler {
        QPromise<void>& promise;
        qsizetype base;
        qsizetype total;

        void report(qsizetype done) const
        {
            Q_ASSERT(total > 0);
            promise.setProgressValue(static_cast<int>(
                ((base + done) * kProgressResolution) / total));
        }
    };

    // Visibility guard shared by every traversal entry point. A null parent
    // means the object isn't owned by a cwRenderObject (test fixtures), so
    // treat it as pickable.
    bool isPickable(const cwGeometryItersecter::Object& object) {
        const cwRenderObject* parent = object.parent();
        return parent == nullptr || parent->isVisible();
    }

    // Map a geometry type to the pick-query flag it satisfies. Returns 0 for
    // types that never enter the BVH (they contribute no primitives), so the
    // kind filter rejects them.
    cwPickQuery::Kinds pickKindOf(cwGeometry::Type type) {
        switch (type) {
        case cwGeometry::Type::Triangles: return cwPickQuery::Kind::Triangles;
        case cwGeometry::Type::Lines:     return cwPickQuery::Kind::Lines;
        case cwGeometry::Type::Points:    return cwPickQuery::Kind::Points;
        default:                          return {};
        }
    }

    // Stride-aware reader for a geometry's position attribute. Hoists the
    // buffer base pointer and stride once, then fetches positions by vertex
    // index — the raw-buffer access shared by the point-cloud paths (AABB
    // union, BVH point enumeration, the sphere pick, per-point boxes). index
    // is qsizetype so the byte offset can't overflow on clouds past ~200M
    // points.
    struct PointPositionReader {
        const char* base = nullptr;
        int stride = 0;

        PointPositionReader(const cwGeometry& geometry,
                            const cwGeometry::VertexAttribute* positionAttribute) :
            base(geometry.vertexBuffer(positionAttribute->bufferIndex)->constData()
                 + positionAttribute->byteOffsetInBuffer),
            stride(positionAttribute->bufferStride)
        {}

        QVector3D at(qsizetype index) const {
            const float* p = reinterpret_cast<const float*>(base + index * stride);
            return QVector3D(p[0], p[1], p[2]);
        }
    };

    struct RaySphereHit {
        bool hit;
        double tNear;    // sphere-entry depth (valid only when hit)
        double tCenter;  // perpendicular-projection depth of the sphere center
        double dSq;
    };

    // QSphere3D::intersection is float32; at world-magnitude coordinates
    // (~10^4) (V·D)^2 - V·V cancels into r^2 noise and returns garbage.
    // Build the perpendicular vector by subtraction in double instead,
    // so the small (~r) result keeps full precision.
    RaySphereHit raySphereIntersectDouble(const QRay3D& ray,
                                          const QVector3D& center,
                                          float radius)
    {
        const double ox = ray.origin().x();
        const double oy = ray.origin().y();
        const double oz = ray.origin().z();
        const double dx = ray.direction().x();
        const double dy = ray.direction().y();
        const double dz = ray.direction().z();
        const double cx = center.x();
        const double cy = center.y();
        const double cz = center.z();

        const double dDotD = dx*dx + dy*dy + dz*dz;
        // Reject zero-length, negative (impossible for sum-of-squares
        // but cheap), and NaN-direction rays before they poison
        // tNear/dSq with inf/NaN.
        if (!(dDotD > 0.0)) {
            return {false, 0.0, 0.0, 0.0};
        }
        const double invDDotD = 1.0 / dDotD;
        const double tCenter =
            ((cx - ox)*dx + (cy - oy)*dy + (cz - oz)*dz) * invDDotD;
        const double perpX = cx - (ox + tCenter * dx);
        const double perpY = cy - (oy + tCenter * dy);
        const double perpZ = cz - (oz + tCenter * dz);
        const double dSq = perpX*perpX + perpY*perpY + perpZ*perpZ;
        const double rSq = double(radius) * double(radius);

        if (dSq > rSq) {
            return {false, 0.0, tCenter, dSq};
        }
        return {true, tCenter - std::sqrt((rSq - dSq) * invDDotD), tCenter, dSq};
    }

    struct RaySegmentHit {
        double dSq;          // squared closest distance between ray and segment
        QVector3D segPoint;  // closest point on the segment (becomes pointWorld)
    };

    // Closest approach between a semi-infinite ray (origin + s*dir, s >= 0)
    // and the finite segment AB. The standard segment/segment solver
    // (Ericson, Real-Time Collision Detection) with the ray parameter
    // clamped only at its lower bound and the segment parameter to [0,1].
    // Double precision throughout: at world-magnitude coordinates (~10^4)
    // the float cancellation that bit the sphere test bites here too.
    RaySegmentHit raySegmentClosest(const QRay3D& ray,
                                    const QVector3D& A,
                                    const QVector3D& B)
    {
        const double ox = ray.origin().x(),    oy = ray.origin().y(),    oz = ray.origin().z();
        const double dx = ray.direction().x(), dy = ray.direction().y(), dz = ray.direction().z();
        const double ax = A.x(), ay = A.y(), az = A.z();
        const double ex = double(B.x()) - ax, ey = double(B.y()) - ay, ez = double(B.z()) - az;

        const double a = dx*dx + dy*dy + dz*dz;          // |dir|^2
        const double e = ex*ex + ey*ey + ez*ez;          // |B-A|^2
        const double rx = ox - ax, ry = oy - ay, rz = oz - az;
        const double f = ex*rx + ey*ry + ez*rz;          // (B-A)·(origin-A)
        const double c = dx*rx + dy*ry + dz*rz;          // dir·(origin-A)
        const double b = dx*ex + dy*ey + dz*ez;          // dir·(B-A)

        constexpr double kEps = 1e-12;

        double s = 0.0; // ray parameter, clamped to [0, inf)
        double t = 0.0; // segment parameter, clamped to [0, 1]

        if (e <= kEps) {
            // (1) Zero-length segment (a from==to shot): treat as the point A.
            t = 0.0;
            s = (a > kEps) ? std::max(0.0, -c / a) : 0.0;
        } else {
            const double denom = a * e - b * b;
            // (2) Ray parallel to the segment (denom ~ 0): the determinant
            // solve is unstable, so pin the ray parameter to 0 and let the
            // segment clamp pick the nearest endpoint.
            s = (denom > kEps) ? std::max(0.0, (b * f - c * e) / denom) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = (a > kEps) ? std::max(0.0, -c / a) : 0.0;
            } else if (t > 1.0) {
                t = 1.0;
                s = (a > kEps) ? std::max(0.0, (b - c) / a) : 0.0;
            }
        }

        const double spx = ax + t * ex, spy = ay + t * ey, spz = az + t * ez;
        const double rpx = ox + s * dx, rpy = oy + s * dy, rpz = oz + s * dz;
        const double diffX = rpx - spx, diffY = rpy - spy, diffZ = rpz - spz;

        return {diffX*diffX + diffY*diffY + diffZ*diffZ,
                QVector3D(float(spx), float(spy), float(spz))};
    }

    // Ray parameter at which `box` is entered, clamped to 0 when the origin is
    // already inside it. NaN when the box is missed, is entirely behind the
    // origin, or is not finite.
    //
    // QBox3D::intersection(ray) cannot serve here: it returns the ENTRY
    // parameter only while that is positive, and silently switches to the EXIT
    // parameter once the origin is inside the box. A depth prune needs a lower
    // bound on the candidates inside the node, and the exit parameter is an
    // upper one — using it skips nodes that hold the nearest candidate.
    double boxEntryDistance(const QBox3D& box, const QRay3D& ray)
    {
        float minimumT = 0.0f;
        float maximumT = 0.0f;
        if (!box.intersection(ray, &minimumT, &maximumT) || maximumT < 0.0f) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return std::max(0.0f, minimumT);
    }

    // A screen-space accept radius grows with depth, so no single scalar pad
    // fits every AABB test. The radius at `box`'s farthest depth bounds every
    // candidate inside it: projectedDistance is affine, so its maximum over
    // the box is at a corner, and radiusAt() is non-decreasing (for the
    // slope >= 0 every cwCamera::pickQuery produces). The fine test still
    // applies the exact radiusAt(candidate depth). Zero when no tolerance is
    // supplied, collapsing the pads to their tolerance-free values.
    //
    // Pass the tightest box the caller can justify. Handed the BVH root it
    // yields one scene-global pad, which is conservative but prunes almost
    // nothing in a deep scene; handed each node's own box it prunes properly.
    float conservativeTolerancePad(const QRay3D& ray,
                                   const QBox3D& box,
                                   const cwPickTolerance& tolerance)
    {
        if (!tolerance.enabled()) {
            return 0.0f;
        }

        const QVector3D mn = box.minimum();
        const QVector3D mx = box.maximum();
        double farDepth = 0.0;
        for (int corner = 0; corner < kBoxCornerCount; ++corner) {
            const QVector3D c((corner & kCornerMaskX) ? mx.x() : mn.x(),
                              (corner & kCornerMaskY) ? mx.y() : mn.y(),
                              (corner & kCornerMaskZ) ? mx.z() : mn.z());
            farDepth = std::max(farDepth, double(ray.projectedDistance(c)));
        }
        return float(tolerance.radiusAt(farDepth));
    }
}

// Per-primitive work item carried only during the build. Holds just the
// centroid (for median split) and the final primitive handle (for emission
// into m_primitives). The per-primitive AABB is *not* stored — it's
// re-derived from geometry at leaf creation. At 100M points this is the
// difference between a ~5 GB and a ~2.5 GB build-time temporary.
struct cwGeometryItersecter::BuildPrim {
    QVector3D centroid;
    cwGeometryItersecter::Primitive prim;
};

// Per-call progress accounting for the serial Phase B-1 split. Phase B-1
// runs single-threaded so `done` is a plain counter, not atomic.
struct cwGeometryItersecter::SplitProgress {
    ProgressScaler scaler;
    qsizetype done = 0;
};

// Bundle of references the recursive Phase B helpers all need. Saves
// passing six parameters through every recursive call.
struct cwGeometryItersecter::BuildContext {
    const Object& object;
    QVector<BuildPrim>& prims;
    QVector<BvhNode>& outNodes;
    std::atomic<uint32_t>& nextNode;
    // Only set during the parallel Phase B pass; serialSplitToFanout
    // leaves it null because the serial split doesn't create leaves.
    std::atomic<qsizetype>* leafPrimCounter = nullptr;
    // Non-null only during Phase B-1 (see launchBuildJob).
    SplitProgress* splitProgress = nullptr;
};

namespace {
    // Pick the longest axis of a centroid AABB. Returns -1 when every
    // extent is zero (collinear / duplicate centroids).
    int dominantSplitAxis(const QVector3D& extent)
    {
        int axis = 0;
        if (extent.y() > extent.x()) {
            axis = 1;
        }
        if (extent.z() > extent[axis]) {
            axis = 2;
        }
        return extent[axis] > 0.0f ? axis : -1;
    }
}

cwGeometryItersecter::MedianSplitResult
cwGeometryItersecter::medianSplit(QVector<BuildPrim>& prims,
                                  qsizetype begin,
                                  qsizetype end)
{
    QBox3D centroidBox;
    for (qsizetype i = begin; i < end; ++i) {
        centroidBox.unite(prims[i].centroid);
    }
    const QVector3D extent = centroidBox.maximum() - centroidBox.minimum();
    const int axis = dominantSplitAxis(extent);
    if (axis < 0) {
        return {0, -1};
    }
    const qsizetype mid = (begin + end) / 2;
    std::nth_element(prims.begin() + begin,
                     prims.begin() + mid,
                     prims.begin() + end,
                     [axis](const BuildPrim& a, const BuildPrim& b) {
                         return a.centroid[axis] < b.centroid[axis];
                     });
    return {mid, axis};
}

qsizetype cwGeometryItersecter::countNodePrimitives(const Object& object)
{
    const cwGeometry& geometry = object.geometry();
    switch (geometry.type()) {
    case cwGeometry::Type::Triangles:
        return geometry.indices().size() / 3;
    case cwGeometry::Type::Lines:
        return geometry.indices().size() / 2;
    case cwGeometry::Type::Points:
        return object.pickRadius() > 0.0f ? geometry.vertexCount() : 0;
    default:
        return 0;
    }
}

template <typename Future>
void cwGeometryItersecter::waitOnPool(Future& future)
{
    QThreadPool* pool = cwTask::threadPool();
    pool->releaseThread();
    future.waitForFinished();
    pool->reserveThread();
}

cwGeometryItersecter::cwGeometryItersecter(QObject* parent) :
    QObject(parent),
    m_bvhRestarter(this)
{
    m_bvhRestarter.onFutureChanged([this]() {
        if (m_futureManagerToken.isValid()) {
            m_futureManagerToken.addJob({m_bvhRestarter.future(), kAcceleratingPickingJobName});
        }
    });
}

void cwGeometryItersecter::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

cwGeometryItersecter::DebugStatistics cwGeometryItersecter::debugStatistics() const
{
    DebugStatistics stats;
    const QList<Node>& source = (m_bvh && !m_bvh->nodesSnapshot.isEmpty())
                                ? m_bvh->nodesSnapshot
                                : Nodes;
    stats.hasBvh = static_cast<bool>(m_bvh);
    stats.sourceNodeCount = source.size();
    for (const Node& n : source) {
        stats.totalPrimitives += countNodePrimitives(n.Object);
        switch (n.Object.geometry().type()) {
        case cwGeometry::Type::Triangles: ++stats.triangleSourceNodes; break;
        case cwGeometry::Type::Lines:     ++stats.lineSourceNodes;     break;
        case cwGeometry::Type::Points:    ++stats.pointSourceNodes;    break;
        default:                                                       break;
        }
    }
    if (m_bvh) {
        // Total BVH node count = top-level + sum of all sub-BVH sizes.
        // Useful for the test panel; reflects the full two-level structure.
        stats.bvhNodeCount = m_bvh->topLevel.size();
        for (const auto& sb : m_bvh->subBvhs) {
            if (sb) {
                stats.bvhNodeCount += sb->bvhNodes.size();
            }
        }
    }
    return stats;
}

void cwGeometryItersecter::waitForFinish()
{
    AsyncFuture::waitForFinished(m_bvhRestarter.future());
}

/**
 * @brief cwGeometryItersecter::addTriangles
 * @param object
 *
 * Add the object to the itersector
 */
void cwGeometryItersecter::addObject(const cwGeometryItersecter::Object &object)
{
    switch(object.geometry().type()) {
    case cwGeometry::Type::Triangles:
        addTriangles(object);
        break;
    case cwGeometry::Type::Lines:
        addLines(object);
        break;
    case cwGeometry::Type::Points:
        addPoints(object);
        break;
    default:
        // The renderer doesn't require geometry().type() but the picker does;
        // a triangulation task that forgets setType() drops out of picking
        // silently. Warn so the regression isn't invisible — but only when
        // there's actual geometry to drop, since a default-constructed empty
        // cwGeometry has Type::None legitimately.
        if (object.geometry().vertexCount() > 0
            || !object.geometry().indices().isEmpty()) {
            qCWarning(lcPick).nospace()
                << "addObject DROPPED " << formatKey(object)
                << " geometry.type()=" << cwGeometry::typeName(object.geometry().type())
                << " vertexCount=" << object.geometry().vertexCount()
                << " indexCount=" << object.geometry().indices().size()
                << " — upstream forgot to set Type::Triangles/Lines/Points";
        }
        break;
    }
}

bool cwGeometryItersecter::eraseNodeIfPresent(const Key& key)
{
    auto iter = findNode(key);
    if (iter == Nodes.end()) {
        return false;
    }
    Nodes.erase(iter);
    m_subBvhs.remove(key);
    m_dirtyKeys.remove(key);
    invalidatePublishedSlot(key);
    return true;
}

/**
 * @brief cwGeometryItersecter::clearObjects
 * @param parentObject
 *
 * This removes all objects that have parent object of parentObject
 *
 * If parentObject is null, this will clear objects
 */
void cwGeometryItersecter::clear(cwRenderObject *parentObject)
{
    if(parentObject == nullptr) {
        qCDebug(lcPick).nospace()
            << "clear(all) — dropping " << Nodes.size() << " Nodes, "
            << m_subBvhs.size() << " cached sub-BVHs";
        Nodes.clear();
        m_subBvhs.clear();
        m_dirtyKeys.clear();
        // Total wipe: drop the published BVH entirely so every pick goes
        // no-hit immediately. No retention benefit here — there's no Key
        // left we'd want picks to still hit.
        m_bvh.reset();
        scheduleTopLevelRebuild();
        return;
    }

    int erased = 0;
    QList<Node>::iterator iter = Nodes.begin();
    while(iter != Nodes.end()) {
        Node& currentNode = *iter;
        if(currentNode.Object.parent() == parentObject) {
            const Key key{currentNode.Object.parent(), currentNode.Object.id()};
            m_subBvhs.remove(key);
            m_dirtyKeys.remove(key);
            invalidatePublishedSlot(key);
            iter = Nodes.erase(iter);
            ++erased;
        } else {
            iter++;
        }
    }
    qCDebug(lcPick).nospace()
        << "clear(parent=" << parentObject << ") — erased " << erased
        << " Nodes; remaining=" << Nodes.size();
    scheduleTopLevelRebuild();
}

/**
 * @brief cwGeometryItersecter::removeObject
 * @param parentObject
 * @param id
 *
 * Removes the geometry that makes up parentObject and id
 */
void cwGeometryItersecter::removeObject(cwRenderObject *parentObject, uint64_t id)
{
    removeObject(Key(parentObject, id));
}

void cwGeometryItersecter::removeObject(const Key &objectKey)
{
    if (eraseNodeIfPresent(objectKey)) {
        qCDebug(lcPick).nospace()
            << "removeObject {parent=" << objectKey.parentObject
            << ", id=" << objectKey.id << "}"
            << " Nodes.size after=" << Nodes.size();
        scheduleTopLevelRebuild();
    }
}

void cwGeometryItersecter::setModelMatrix(cwRenderObject *parentObject, uint64_t id, const QMatrix4x4 &modelMatrix)
{
    setModelMatrix(Key(parentObject, id), modelMatrix);
}

void cwGeometryItersecter::setModelMatrix(const Key &objectKey, const QMatrix4x4& modelMatrix)
{
    auto iter = findNode(objectKey);
    if (iter == Nodes.end()) {
        qCDebug(lcPick).nospace()
            << "setModelMatrix {parent=" << objectKey.parentObject
            << ", id=" << objectKey.id << "} — Key not in Nodes; no-op";
        return;
    }
    qCDebug(lcPick).nospace()
        << "setModelMatrix {parent=" << objectKey.parentObject
        << ", id=" << objectKey.id << "} — top-level rebuild only"
        << " (sub-BVH cache preserved)";
    iter->Object.setModelMatrix(modelMatrix);
    // No sub-BVH invalidation — sub-BVHs are model-space and unaffected
    // by modelMatrix changes. Only the top-level needs refreshing.
    scheduleTopLevelRebuild();
}

QBox3D cwGeometryItersecter::boundingBox(const Key &objectKey) const
{
    auto iter = findNode(objectKey);
    if (iter != Nodes.end()) {
        return iter->BoundingBox.transformed(iter->Object.modelMatrix());
    } else {
        return QBox3D();
    }
}

QBox3D cwGeometryItersecter::boundingBox(const QList<Key>& keys) const
{
    QBox3D box;
    for (const Key& key : keys) {
        box.unite(boundingBox(key));
    }
    return box;
}

QBox3D cwGeometryItersecter::boundingBox() const
{
    QBox3D box;
    for (const Node& node : Nodes) {
        box.unite(node.BoundingBox.transformed(node.Object.modelMatrix()));
    }
    return box;
}

bool cwGeometryItersecter::isPickableEmpty() const
{
    QBox3D box;
    for (const Node& node : Nodes) {
        if (!isPickable(node.Object)) {
            continue;
        }
        box.unite(node.BoundingBox.transformed(node.Object.modelMatrix()));
    }
    return box.isNull() || !box.isFinite();
}

/**
 * @brief cwGeometryItersecter::intersects
 * @param ray
 * @return Closest intersection along the ray, or NaN when the BVH (with
 *         the tube-pick fallback in intersectsDetailed) finds nothing.
 *         Callers can then fall back to a grid plane / projected pivot.
 */
double cwGeometryItersecter::intersects(const QRay3D &ray, const cwPickQuery& query) const
{
    const cwRayHit hit = intersectsDetailed(ray, query);
    if (hit.hit()) {
        return hit.tWorld();
    }
    return qSNaN();
}

/**
 * @brief cwGeometryItersecter::addTriangles
 * @param object
 *
 * Adds the object as a triangle, if the object already exist is the
 * interecter, the object will be replaced.
 */
void cwGeometryItersecter::addTriangles(const cwGeometryItersecter::Object &object)
{
    //Make sure the object has the right number of indices
    if(object.geometry().indices().size() % 3 != 0) {
        qDebug() << "Can't add object" << object.parent() << object.id() << "because it has an invalid indexes" << LOCATION;
        return;
    }

    const Key key{object.parent(), object.id()};
    eraseNodeIfPresent(key);

    auto positionAttribute = object.geometry().attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);

    QBox3D box;
    for(int i = 0; i < object.geometry().vertexCount(); i++) {
        box.unite(object.geometry().value<QVector3D>(positionAttribute, i));
    }

    Nodes.append(Node(box, object));
    qCDebug(lcPick).nospace()
        << "addTriangles " << formatKey(object)
        << " tris=" << (object.geometry().indices().size() / 3)
        << " box=[" << box.minimum() << " .. " << box.maximum() << "]"
        << " Nodes.size=" << Nodes.size();
    scheduleObjectRebuild(key);
}

/**
 * @brief cwGeometryItersecter::addLines
 * @param object
 *
 * Adds the object as a lines.
 *
 * If the object already exists in the intersecter the object will be replace with
 * the new data.
 */
void cwGeometryItersecter::addLines(const cwGeometryItersecter::Object &object)
{
    const cwGeometry& geometry = object.geometry();

    //Make sure the object has the right number of indices
    if(geometry.indices().size() % 2 != 0) {
        qDebug() << "Can't add object" << object.parent() << object.id() << "because it has an invalid indexes" << LOCATION;
        return;
    }

    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    if (positionAttribute == nullptr) {
        qDebug() << "Can't add line object" << object.parent() << object.id() << "— missing Position attribute" << LOCATION;
        return;
    }

    const Key key{object.parent(), object.id()};
    eraseNodeIfPresent(key);

#ifndef QT_NO_DEBUG
    // Line picking compares a model-space ray-segment distance against a
    // world-space tolerance radius (testPrimitive's Line branch). That holds
    // only when the model->world transform is an isometry (identity, rotation,
    // translation) — a scale would silently mis-size the pick radius. The
    // centerline registers with an identity matrix; assert the invariant so a
    // future scaled Lines object trips here, not as a hard-to-trace misfire.
    {
        constexpr float kIsometryEps = 1e-3f;
        const QMatrix4x4& m = object.modelMatrix();
        const float lenX = mapDirection(m, QVector3D(1.0f, 0.0f, 0.0f)).length();
        const float lenY = mapDirection(m, QVector3D(0.0f, 1.0f, 0.0f)).length();
        const float lenZ = mapDirection(m, QVector3D(0.0f, 0.0f, 1.0f)).length();
        Q_ASSERT_X(qAbs(lenX - 1.0f) < kIsometryEps &&
                   qAbs(lenY - 1.0f) < kIsometryEps &&
                   qAbs(lenZ - 1.0f) < kIsometryEps,
                   "cwGeometryItersecter::addLines",
                   "Lines pick assumes an isometric model matrix; a scaled "
                   "transform would mis-size the screen-space pick radius.");
    }
#endif

    // One whole-object Node with a tight combined AABB (mirrors addPoints /
    // addTriangles). Each segment becomes a BVH primitive at build time; the
    // per-pick screen-space tolerance — not a baked pad — decides hits, so
    // the leaf box stays tight and the picking-disabled cost is unchanged.
    QBox3D box;
    for(int i = 0; i < geometry.vertexCount(); i++) {
        box.unite(geometry.value<QVector3D>(positionAttribute, i));
    }

    Nodes.append(Node(box, object));
    qCDebug(lcPick).nospace()
        << "addLines " << formatKey(object)
        << " segments=" << (geometry.indices().size() / 2)
        << " box=[" << box.minimum() << " .. " << box.maximum() << "]"
        << " Nodes.size=" << Nodes.size();
    scheduleObjectRebuild(key);
}

/**
 * @brief cwGeometryItersecter::addPoints
 * @param object
 *
 * Adds the object as a point cloud. One Node per cloud with a combined AABB
 * expanded by Object::pickRadius() so that broad-phase ray rejection doesn't
 * miss rays that graze the outermost sphere of a point.
 */
void cwGeometryItersecter::addPoints(const cwGeometryItersecter::Object &object)
{
    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    if (positionAttribute == nullptr) {
        qDebug() << "Can't add point object" << object.parent() << object.id() << "— missing Position attribute" << LOCATION;
        return;
    }

    const qsizetype vertexCount = geometry.vertexCount();
    if (vertexCount <= 0) {
        return;
    }

    const Key key{object.parent(), object.id()};
    eraseNodeIfPresent(key);

    const PointPositionReader reader(geometry, positionAttribute);
    QBox3D box;
    for (qsizetype i = 0; i < vertexCount; ++i) {
        box.unite(reader.at(i));
    }

    const float pad = object.pickRadius() * kPointAabbPadScale;
    const QVector3D padVec(pad, pad, pad);

    Nodes.append(Node(QBox3D(box.minimum() - padVec, box.maximum() + padVec), object));
    qCDebug(lcPick).nospace()
        << "addPoints " << formatKey(object)
        << " vertices=" << vertexCount
        << " pickRadius=" << object.pickRadius()
        << " box=[" << box.minimum() << " .. " << box.maximum() << "]"
        << " Nodes.size=" << Nodes.size()
        << (object.pickRadius() <= 0.0f
            ? " WARNING: pickRadius<=0 so 0 prims will be enumerated"
            : "");
    scheduleObjectRebuild(key);
}


cwRayHit cwGeometryItersecter::intersectsDetailed(const QRay3D &ray, const cwPickQuery& query) const
{
    cwRayHit best;

    const bool debug = lcPick().isDebugEnabled();
    PickStats stats;
    PickStats* statsPtr = debug ? &stats : nullptr;

    if (!m_bvh || m_bvh->topLevel.isEmpty()) {
        if (debug) {
            qCDebug(lcPick).nospace()
                << "intersectsDetailed: no BVH yet (m_bvh="
                << (m_bvh ? "built but empty" : "null")
                << ", Nodes.size=" << Nodes.size()
                << ", ray.origin=" << ray.origin()
                << ", ray.dir=" << ray.direction() << ")";
        }
        return best;
    }

    const QVector<BvhNode>& topLevel = m_bvh->topLevel;
    const QVector<std::shared_ptr<const SubBvh>>& subBvhs = m_bvh->subBvhs;

    // Top-level tube pad uses the global max across all Objects: a single
    // value keeps the AABB-test fast path identical to the pre-tube cost
    // when m_tubePickEnabled is off.
    const float topTubeDist = m_tubePickEnabled
                              ? m_bvh->maxPickRadius * kTubeFactor
                              : 0.0f;

    // Line picking grows its accept radius with depth (screen-space), so a
    // fixed baked AABB pad can't bound it. Derive one conservative scalar —
    // the radius at the farthest scene depth — and fold it into the box-test
    // pads; the fine test still uses the exact radiusAt(hitDepth). Zero when
    // no tolerance is supplied, so the box pads collapse to the pre-line tube
    // values and the AABB fast path is byte-for-byte the old one (line sub-BVHs
    // are skipped entirely in that case — see the kind-filter block below).
    const float linePad =
        conservativeTolerancePad(ray, topLevel.at(0).bbox, query.tolerance);

    const float topPad = std::max(topTubeDist, linePad);
    const QVector3D topTubePad(topPad, topPad, topPad);

    NearMissResult nearMiss;
    NearMissResult* nearMissPtr = m_tubePickEnabled ? &nearMiss : nullptr;

    if (debug) {
        qCDebug(lcPick).nospace()
            << "intersectsDetailed: topLevel=" << topLevel.size()
            << ", subBvhs=" << subBvhs.size()
            << ", source nodes=" << m_bvh->nodesSnapshot.size()
            << ", maxPickRadius=" << m_bvh->maxPickRadius
            << ", topTubeDist=" << topTubeDist
            << ", ray.origin=" << ray.origin()
            << ", ray.dir=" << ray.direction();
    }

    // Top-level traversal in world space — depth bounded by log2(N_objects),
    // which is well under 64 for any realistic project.
    QVarLengthArray<uint32_t, kBvhTraversalStackInline> topStack;
    topStack.append(0);

    while (!topStack.isEmpty()) {
        const uint32_t idx = topStack.last();
        topStack.removeLast();

        const BvhNode& bn = topLevel[idx];
        if (debug) {
            ++stats.nodesVisited;
        }

        const double tBox = (topPad == 0.0f)
            ? bn.bbox.intersection(ray)
            : QBox3D(bn.bbox.minimum() - topTubePad,
                     bn.bbox.maximum() + topTubePad).intersection(ray);
        if (qIsNaN(tBox)) {
            if (debug) {
                ++stats.nodesBoxMiss;
            }
            continue;
        }
        if (best.hit() && tBox >= best.tWorld()) {
            if (debug) {
                ++stats.nodesPrunedByBest;
            }
            continue;
        }

        if (!bn.isLeaf) {
            uint32_t nearChild = bn.left;
            uint32_t farChild = bn.right;
            if (ray.direction()[bn.splitAxis] < 0.0f) {
                std::swap(nearChild, farChild);
            }
            topStack.append(farChild);
            topStack.append(nearChild);
            continue;
        }

        // Top-level leaf — descend the Object's sub-BVH in model space.
        const uint32_t slot = bn.left;
        const std::shared_ptr<const SubBvh>& subPtr = subBvhs.at(slot);
        // Slot nulled mid-rebuild — see invalidatePublishedSlot.
        if (!subPtr) {
            continue;
        }
        const SubBvh& sub = *subPtr;

        // Hoist visibility check: every primitive in the sub-BVH belongs
        // to the same Object, so a single isPickable() guard short-
        // circuits the entire descent when the Object is hidden.
        if (!isPickable(sub.object)) {
            continue;
        }

        // Runtime kind filter (§5): priority lives in the caller as a cheap
        // skip beside isPickable(), never as a depth bias inside traversal.
        // Every primitive in this sub-BVH shares the Object's geometry type,
        // so one test short-circuits the whole descent.
        const cwGeometry::Type objType = sub.object.geometry().type();
        if (!(query.kinds & pickKindOf(objType))) {
            continue;
        }
        const bool isLineObject = (objType == cwGeometry::Type::Lines);

        // Lines only ever produce a hit when a screen-space tolerance is
        // supplied (testPrimitive's Line branch bails otherwise). Skip the
        // whole line sub-BVH descent up front when tolerance is disabled, so
        // a default pick over a scene containing the centerline pays nothing
        // for it — the pre-line traversal cost is recovered exactly.
        if (isLineObject && !query.tolerance.enabled()) {
            continue;
        }

        const QMatrix4x4& worldFromModel = m_bvh->modelMatrices.at(slot);
        const QMatrix4x4& modelToWorld = m_bvh->inverseModelMatrices.at(slot);
        const QRay3D rayModel = transformRayWithInverse(modelToWorld, ray);

        // Per-Object tube pad applies to the model-space boxes within
        // this sub-BVH. Triangle-only Objects have pickRadius() == 0,
        // which collapses the pad to zero and matches the pre-tube cost.
        // The line pick tolerance pads boxes the same way; for a line
        // object the conservative linePad lets a grazing ray reach the
        // leaf where the exact distance test decides the hit. (Model-space
        // pad against a world-space radius — exact for the identity model
        // matrix the line plot uses, like the point/sphere path.)
        const float subTubeDist = m_tubePickEnabled
                                  ? sub.object.pickRadius() * kTubeFactor
                                  : 0.0f;
        const float subPad = isLineObject
                             ? std::max(subTubeDist, linePad)
                             : subTubeDist;
        const QVector3D subTubePad(subPad, subPad, subPad);

        // tModel and tWorld parameterize the same point — transformRayToModel
        // applies the inverse model matrix uniformly to direction so the
        // parameter scalar is preserved (see math derivation in design doc).
        // That means best.tWorld() is a valid pruning threshold against
        // model-space tSubBox.
        QVarLengthArray<uint32_t, kBvhTraversalStackInline> subStack;
        subStack.append(0);

        while (!subStack.isEmpty()) {
            const uint32_t sidx = subStack.last();
            subStack.removeLast();

            const BvhNode& sbn = sub.bvhNodes[sidx];
            if (debug) {
                ++stats.nodesVisited;
            }

            const double tSubBox = (subPad == 0.0f)
                ? sbn.bbox.intersection(rayModel)
                : QBox3D(sbn.bbox.minimum() - subTubePad,
                         sbn.bbox.maximum() + subTubePad).intersection(rayModel);
            if (qIsNaN(tSubBox)) {
                if (debug) {
                    ++stats.nodesBoxMiss;
                }
                continue;
            }
            if (best.hit() && tSubBox >= best.tWorld()) {
                if (debug) {
                    ++stats.nodesPrunedByBest;
                }
                continue;
            }

            if (sbn.isLeaf) {
                if (debug) {
                    ++stats.leavesVisited;
                }
                const uint32_t first = sbn.left;
                const uint32_t primCount = sbn.right;
                if (debug) {
                    qCDebug(lcPick).nospace()
                        << "leaf top=" << idx << " sub=" << sidx << ": " << primCount
                        << " prims, bbox=[" << sbn.bbox.minimum() << " .. "
                        << sbn.bbox.maximum() << "]"
                        << " bestSoFar="
                        << (best.hit() ? QString("tWorld=%1").arg(best.tWorld())
                                       : QStringLiteral("none"));
                }
                for (uint32_t p = first; p < first + primCount; ++p) {
                    if (debug) {
                        dumpLeafPrimitive(sub.object, sub.primitives.at(p), ray, sidx, p - first);
                    }
                    testPrimitive(sub.object, sub.primitives.at(p),
                                  ray, rayModel, worldFromModel, modelToWorld,
                                  query.tolerance, best, nearMissPtr, statsPtr);
                }
            } else {
                uint32_t nearChild = sbn.left;
                uint32_t farChild = sbn.right;
                if (rayModel.direction()[sbn.splitAxis] < 0.0f) {
                    std::swap(nearChild, farChild);
                }
                subStack.append(farChild);
                subStack.append(nearChild);
            }
        }
    }

    // Without the tube fallback, a sub-pixel cursor gap in a dense LAZ
    // surface returns NaN from intersects() and the camera-pivot path in
    // cwBaseTurnTableInteraction snaps to the grid plane at the wrong
    // depth, jerking the view.
    if (m_tubePickEnabled) {
        tryPromoteNearMiss(best, nearMiss, ray, debug);
    }

    if (debug) {
        if (best.hit()) {
            qCDebug(lcPick).nospace()
                << "intersectsDetailed HIT tWorld=" << best.tWorld()
                << " pWorld=" << best.pointWorld()
                << " object=" << best.object()
                << " objectId=" << best.objectId()
                << " firstIndex=" << best.firstIndex()
                << " | " << stats;
        } else {
            qCDebug(lcPick).nospace() << "intersectsDetailed MISS | " << stats;
            // Per-Object summary across the published sub-BVHs.
            qCDebug(lcPick).nospace()
                << "  live BVH sources: " << subBvhs.size() << " sub-BVHs";
            for (qsizetype i = 0; i < subBvhs.size(); ++i) {
                const std::shared_ptr<const SubBvh>& subPtr = subBvhs.at(i);
                if (!subPtr) {
                    // Slot nulled mid-rebuild — see invalidatePublishedSlot.
                    qCDebug(lcPick).nospace() << "    [" << i << "] <rebuilding>";
                    continue;
                }
                const SubBvh& sb = *subPtr;
                const cwRenderObject* parent = sb.object.parent();
                const char* parentClass = parent != nullptr
                    ? parent->metaObject()->className()
                    : "(null)";
                qCDebug(lcPick).nospace()
                    << "    [" << i << "] " << QLatin1String(parentClass)
                    << "@" << parent
                    << " type=" << cwGeometry::typeName(sb.object.geometry().type())
                    << " prims=" << sb.primitives.size()
                    << " visible=" << (parent != nullptr ? parent->isVisible() : true);
            }
        }
    }

    return best;
}

std::optional<QVector3D> cwGeometryItersecter::nearestGeometryPoint(const QRay3D& ray,
                                                                    const cwPickQuery& query) const
{
    if (!m_bvh || m_bvh->topLevel.isEmpty() || !query.tolerance.enabled()) {
        return std::nullopt;
    }

    const QVector<BvhNode>& topLevel = m_bvh->topLevel;
    const QVector<std::shared_ptr<const SubBvh>>& subBvhs = m_bvh->subBvhs;

    // The anchor is a real point ON geometry: the closest point on a
    // primitive to the ray — a cloud point, a spot on a shot segment, a spot
    // on a scrap triangle (its exact hit, else its nearest edge). Candidates
    // must lie within the screen-space tolerance radius at their depth; the
    // front-most accepted candidate wins, matching every other pick's
    // nearest-by-depth rule. Distances are measured in model space against
    // the world-space radius — exact for the identity model matrices the
    // line plot and point clouds use (the same assumption as testPrimitive).
    bool found = false;
    double bestT = (std::numeric_limits<double>::max)();
    QVector3D bestPoint;

    // Each box test derives its own pad from that node's far corner, rather
    // than one scene-global radiusAt(root far depth). Same conservative
    // guarantee — radiusAt() is non-decreasing, so the radius at a node's far
    // corner bounds every candidate inside it — but a node near the camera is
    // no longer inflated by the radius belonging to the far end of the scene,
    // which is what collapsed this traversal into a linear scan.

    QVarLengthArray<uint32_t, kBvhTraversalStackInline> topStack;
    topStack.append(0);

    while (!topStack.isEmpty()) {
        const BvhNode& bn = topLevel[topStack.last()];
        topStack.removeLast();

        const float nodePad = conservativeTolerancePad(ray, bn.bbox, query.tolerance);
        const QVector3D nodePadVec(nodePad, nodePad, nodePad);
        const double tBox = boxEntryDistance(QBox3D(bn.bbox.minimum() - nodePadVec,
                                                    bn.bbox.maximum() + nodePadVec), ray);
        if (qIsNaN(tBox)) {
            continue;
        }
        if (found && tBox >= bestT) {
            continue;
        }

        if (!bn.isLeaf) {
            uint32_t nearChild = bn.left;
            uint32_t farChild = bn.right;
            if (ray.direction()[bn.splitAxis] < 0.0f) {
                std::swap(nearChild, farChild);
            }
            topStack.append(farChild);
            topStack.append(nearChild);
            continue;
        }

        // Top-level leaf — one Object. Descend its model-space sub-BVH.
        const uint32_t slot = bn.left;
        const std::shared_ptr<const SubBvh>& subPtr = subBvhs.at(slot);
        if (!subPtr) {
            continue;  // slot nulled mid-rebuild — see invalidatePublishedSlot.
        }
        const SubBvh& sub = *subPtr;
        if (!isPickable(sub.object)) {
            continue;  // hidden objects never anchor the pivot.
        }
        const cwGeometry& geometry = sub.object.geometry();
        if (!(query.kinds & pickKindOf(geometry.type()))) {
            continue;
        }
        auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
        Q_ASSERT(positionAttribute);

        const QMatrix4x4& worldFromModel = m_bvh->modelMatrices.at(slot);
        const QRay3D rayModel =
            transformRayWithInverse(m_bvh->inverseModelMatrices.at(slot), ray);

        const auto considerCandidate = [&](const QVector3D& pointModel, double dSq) {
            const QVector3D pWorld = mapPoint(worldFromModel, pointModel);
            const double tWorld = ray.projectedDistance(pWorld);
            if (tWorld <= 0.0) {
                return;  // behind the camera.
            }
            if (found && tWorld >= bestT) {
                return;
            }
            const double radius = query.tolerance.radiusAt(tWorld);
            if (dSq > radius * radius) {
                return;
            }
            found = true;
            bestT = tWorld;
            bestPoint = pWorld;
        };

        QVarLengthArray<uint32_t, kBvhTraversalStackInline> subStack;
        subStack.append(0);

        while (!subStack.isEmpty()) {
            const BvhNode& sbn = sub.bvhNodes[subStack.last()];
            subStack.removeLast();

            const float subPad =
                conservativeTolerancePad(rayModel, sbn.bbox, query.tolerance);
            const QVector3D subPadVec(subPad, subPad, subPad);
            const double tSubBox = boxEntryDistance(QBox3D(sbn.bbox.minimum() - subPadVec,
                                                          sbn.bbox.maximum() + subPadVec),
                                                   rayModel);
            if (qIsNaN(tSubBox)) {
                continue;
            }
            if (found && tSubBox >= bestT) {
                continue;
            }

            if (!sbn.isLeaf) {
                uint32_t nearChild = sbn.left;
                uint32_t farChild = sbn.right;
                if (rayModel.direction()[sbn.splitAxis] < 0.0f) {
                    std::swap(nearChild, farChild);
                }
                subStack.append(farChild);
                subStack.append(nearChild);
                continue;
            }

            const uint32_t first = sbn.left;
            const uint32_t primCount = sbn.right;
            for (uint32_t p = first; p < first + primCount; ++p) {
                const Primitive& prim = sub.primitives.at(p);

                if (prim.kind == Primitive::Kind::Triangle) {
                    const QVector<uint32_t>& indices = geometry.indices();
                    const int i = static_cast<int>(prim.primitiveIndex);
                    const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(i + 0));
                    const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(i + 1));
                    const QVector3D c = geometry.value<QVector3D>(positionAttribute, indices.at(i + 2));

                    // Anchoring is orientation-agnostic (never cull): a pivot
                    // on the back side of a scrap is still on the scrap.
                    const cwRayHit local = rayTriangleMT(rayModel, a, b, c, false);
                    if (local.hit()) {
                        considerCandidate(local.pointModel(), 0.0);
                    } else {
                        // A ray that misses a triangle's interior is closest
                        // to its boundary: take the nearest of the 3 edges.
                        RaySegmentHit edge = raySegmentClosest(rayModel, a, b);
                        const RaySegmentHit bc = raySegmentClosest(rayModel, b, c);
                        if (bc.dSq < edge.dSq) {
                            edge = bc;
                        }
                        const RaySegmentHit ca = raySegmentClosest(rayModel, c, a);
                        if (ca.dSq < edge.dSq) {
                            edge = ca;
                        }
                        considerCandidate(edge.segPoint, edge.dSq);
                    }
                } else if (prim.kind == Primitive::Kind::Line) {
                    const QVector<uint32_t>& indices = geometry.indices();
                    const int i = static_cast<int>(prim.primitiveIndex);
                    const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(i + 0));
                    const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(i + 1));

                    const RaySegmentHit seg = raySegmentClosest(rayModel, a, b);
                    considerCandidate(seg.segPoint, seg.dSq);
                } else {
                    const PointPositionReader reader(geometry, positionAttribute);
                    const QVector3D center = reader.at(prim.primitiveIndex);

                    // Radius 0 never reports a hit, but dSq (double-precision
                    // perpendicular distance to the ray) is always filled.
                    const RaySphereHit sphere =
                        raySphereIntersectDouble(rayModel, center, 0.0f);
                    considerCandidate(center, sphere.dSq);
                }
            }
        }
    }

    if (!found) {
        return std::nullopt;
    }
    return bestPoint;
}

void cwGeometryItersecter::testPrimitive(const Object& object,
                                         const Primitive& prim,
                                         const QRay3D& ray,
                                         const QRay3D& rayModel,
                                         const QMatrix4x4& worldFromModel,
                                         const QMatrix4x4& modelToWorld,
                                         const cwPickTolerance& tolerance,
                                         cwRayHit& best,
                                         NearMissResult* nearMiss,
                                         PickStats* stats)
{
    // Visibility is guaranteed by the caller (intersectsDetailed hoists
    // isPickable() to once per top-level leaf, before this descent).
    if (stats != nullptr) {
        ++stats->primsTested;
    }

    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);

    if (prim.kind == Primitive::Kind::Triangle) {
        const QVector<uint32_t>& indices = geometry.indices();
        const int i = static_cast<int>(prim.primitiveIndex);
        const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(i + 0));
        const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(i + 1));
        const QVector3D c = geometry.value<QVector3D>(positionAttribute, indices.at(i + 2));

        cwRayHit local = rayTriangleMT(rayModel, a, b, c, geometry.cullBackfaces());
        if (!local.hit()) {
            if (stats != nullptr) {
                ++stats->primsTriMiss;
            }
            return;
        }

        const QVector3D pWorld = mapPoint(worldFromModel, local.pointModel());
        const QVector3D nWorld = transformNormalWithInverse(modelToWorld, local.normalModel());
        const double tWorld = ray.projectedDistance(pWorld);

        if (tWorld <= 0.0) {
            if (stats != nullptr) {
                ++stats->primsRayBehind;
            }
            return;
        }
        if (best.hit() && tWorld >= best.tWorld()) {
            if (stats != nullptr) {
                ++stats->primsFartherThanBest;
            }
            return;
        }

        best = local;
        best.m_hit = true;
        best.m_pointWorld = pWorld;
        best.m_normalWorld = nWorld;
        best.m_tWorld = tWorld;
        best.m_object = object.parent();
        best.m_objectId = object.id();
        best.m_firstIndex = i;
        if (stats != nullptr) {
            ++stats->primsAccepted;
        }
        return;
    }

    if (prim.kind == Primitive::Kind::Line) {
        // Lines pick only when the caller supplies a screen-space tolerance
        // (default {} leaves line picking off, so every pre-existing caller
        // returns here and the BVH behaves exactly as it did before).
        if (!tolerance.enabled()) {
            return;
        }

        const QVector<uint32_t>& indices = geometry.indices();
        const int i = static_cast<int>(prim.primitiveIndex);
        const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(i + 0));
        const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(i + 1));

        const RaySegmentHit seg = raySegmentClosest(rayModel, a, b);
        const QVector3D pWorld = mapPoint(worldFromModel, seg.segPoint);
        const double tWorld = ray.projectedDistance(pWorld);
        if (tWorld <= 0.0) {
            if (stats != nullptr) {
                ++stats->primsRayBehind;
            }
            return;
        }

        // Accept when the ray–segment distance is within the screen-space
        // radius at this depth. seg.dSq is model space; for the identity
        // model matrix the line plot uses, model == world, so comparing it
        // to a world-space radius is exact (same assumption as the sphere
        // path).
        const double radius = tolerance.radiusAt(tWorld);
        if (seg.dSq > radius * radius) {
            if (stats != nullptr) {
                ++stats->primsLineMiss;
            }
            return;
        }
        if (best.hit() && tWorld >= best.tWorld()) {
            if (stats != nullptr) {
                ++stats->primsFartherThanBest;
            }
            return;
        }

        fillLineHit(best, object, prim, ray, rayModel, seg.segPoint, pWorld, tWorld);
        if (stats != nullptr) {
            ++stats->primsAccepted;
        }
        return;
    }

    // Point primitive — ray-vs-sphere using the Object's pickRadius.
    const float radius = object.pickRadius();
    if (radius <= 0.0f) {
        if (stats != nullptr) {
            ++stats->primsPointRadiusZero;
        }
        return;
    }

    const PointPositionReader reader(geometry, positionAttribute);
    const QVector3D center = reader.at(prim.primitiveIndex);

    const RaySphereHit sphere = raySphereIntersectDouble(rayModel, center, radius);
    if (!sphere.hit) {
        if (stats != nullptr) {
            ++stats->primsSphereMiss;
        }
        // tCenter > 0 filters primitives behind the camera; the dSq
        // threshold is applied once in tryPromoteNearMiss. Skipped
        // entirely when nearMiss is null (tube-pick disabled), so the
        // sphere-miss hot path matches the pre-tube cost.
        if (nearMiss != nullptr
            && sphere.dSq < nearMiss->dSq
            && sphere.tCenter > 0.0) {
            nearMiss->valid = true;
            nearMiss->dSq = sphere.dSq;
            nearMiss->tCenterModel = sphere.tCenter;
            nearMiss->prim = prim;
            nearMiss->radius = radius;
            nearMiss->object = &object;
            nearMiss->worldFromModel = worldFromModel;
        }
        return;
    }
    if (sphere.tNear <= 0.0) {
        if (stats != nullptr) {
            ++stats->primsRayBehind;
        }
        return;
    }

    const QVector3D pWorld = mapPoint(worldFromModel, center);

    // Rank by sphere-entry depth in world space, not by the depth of
    // the projected center. Center depth ignores how head-on a hit is,
    // so a near-grazing point at shallow depth would beat a head-on
    // point at slightly greater depth — even though the head-on point's
    // sphere surface (what the user sees as the front of the splat)
    // is closer to the camera. tNear naturally blends depth and
    // perpendicular distance via tNear = tCenter - sqrt(r^2 - d^2).
    // pointWorld remains the center so coordinate readouts snap to the
    // data point rather than to the sphere surface.
    //
    // Compute the entry point component-wise so the tNear*direction
    // multiplication stays in double; otherwise narrowing tNear to
    // float first would discard ~half of the precision the double
    // sphere math just paid for at world-magnitude coords.
    const QVector3D entryModel(
        float(rayModel.origin().x() + sphere.tNear * rayModel.direction().x()),
        float(rayModel.origin().y() + sphere.tNear * rayModel.direction().y()),
        float(rayModel.origin().z() + sphere.tNear * rayModel.direction().z()));
    const QVector3D entryWorld = mapPoint(worldFromModel, entryModel);
    const double tWorld = ray.projectedDistance(entryWorld);
    if (tWorld <= 0.0) {
        if (stats != nullptr) {
            ++stats->primsRayBehind;
        }
        return;
    }
    if (best.hit() && tWorld >= best.tWorld()) {
        if (stats != nullptr) {
            ++stats->primsFartherThanBest;
        }
        return;
    }

    fillPointHit(best, object, prim, ray, rayModel,
                 center, pWorld, double(sphere.tNear), tWorld);
    if (stats != nullptr) {
        ++stats->primsAccepted;
    }
}

void cwGeometryItersecter::fillPointHit(cwRayHit& best,
                                        const Object& object,
                                        const Primitive& prim,
                                        const QRay3D& ray,
                                        const QRay3D& rayModel,
                                        const QVector3D& centerModel,
                                        const QVector3D& centerWorld,
                                        double tModel,
                                        double tWorld)
{
    best.m_hit = true;
    best.m_tModel = float(tModel);
    best.m_u = std::numeric_limits<float>::quiet_NaN();
    best.m_v = std::numeric_limits<float>::quiet_NaN();
    best.m_pointModel = centerModel;
    best.m_normalModel = -rayModel.direction().normalized();
    best.m_pointWorld = centerWorld;
    best.m_normalWorld = -ray.direction().normalized();
    best.m_tWorld = tWorld;
    best.m_object = object.parent();
    best.m_objectId = object.id();
    best.m_firstIndex = static_cast<int>(prim.primitiveIndex);
}

void cwGeometryItersecter::fillLineHit(cwRayHit& best,
                                       const Object& object,
                                       const Primitive& prim,
                                       const QRay3D& ray,
                                       const QRay3D& rayModel,
                                       const QVector3D& segPointModel,
                                       const QVector3D& segPointWorld,
                                       double tWorld)
{
    best.m_hit = true;
    best.m_tModel = float(rayModel.projectedDistance(segPointModel));
    // A line has no parametric surface coordinate; u/v stay NaN exactly
    // like the point path. Consumers that need the station derive it from
    // firstIndex + the segment endpoints (Commit 2), not from u/v.
    best.m_u = std::numeric_limits<float>::quiet_NaN();
    best.m_v = std::numeric_limits<float>::quiet_NaN();
    best.m_pointModel = segPointModel;
    // Camera-facing normal — a defined value, not garbage; a line surface
    // normal is undefined.
    best.m_normalModel = -rayModel.direction().normalized();
    best.m_pointWorld = segPointWorld;
    best.m_normalWorld = -ray.direction().normalized();
    best.m_tWorld = tWorld;
    best.m_object = object.parent();
    best.m_objectId = object.id();
    // First index of the hit segment. Under the iota index list the line
    // plot registers, this equals the from-vertex index (Commit 2's
    // segmentEndpoints leans on that identity).
    best.m_firstIndex = static_cast<int>(prim.primitiveIndex);
}

void cwGeometryItersecter::tryPromoteNearMiss(cwRayHit& best,
                                              const NearMissResult& nearMiss,
                                              const QRay3D& ray,
                                              bool debug)
{
    if (best.hit() || !nearMiss.valid || nearMiss.object == nullptr) {
        return;
    }
    const double tubeLimit = double(nearMiss.radius) * double(kTubeFactor);
    if (nearMiss.dSq > tubeLimit * tubeLimit) {
        if (debug) {
            qCDebug(lcPick).nospace()
                << "tube-pick rejected: best near-miss d=" << std::sqrt(nearMiss.dSq)
                << " > tube limit " << tubeLimit;
        }
        return;
    }

    const Object& object = *nearMiss.object;
    const cwGeometry& geometry = object.geometry();
    const auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);

    // Use the snapshotted modelMatrix (the SubBvh-stored Object's matrix
    // may be stale across modelMatrix changes — see SubBvh::object).
    const QMatrix4x4& worldFromModel = nearMiss.worldFromModel;
    const QVector3D centerModel = geometry.value<QVector3D>(
        positionAttribute, nearMiss.prim.primitiveIndex);
    const QVector3D centerWorld = mapPoint(worldFromModel, centerModel);
    const double tWorld = ray.projectedDistance(centerWorld);
    if (tWorld <= 0.0) {
        if (debug) {
            qCDebug(lcPick).nospace()
                << "tube-pick rejected: tWorld=" << tWorld
                << " <= 0 after reprojection to world space"
                << " (model matrix may mirror or skew)";
        }
        return;
    }

    const QRay3D rayModel = transformRayToModel(worldFromModel, ray);
    fillPointHit(best, object, nearMiss.prim, ray, rayModel,
                 centerModel, centerWorld, nearMiss.tCenterModel, tWorld);
    if (debug) {
        qCDebug(lcPick).nospace()
            << "tube-pick promoted: d=" << std::sqrt(nearMiss.dSq)
            << " (<= " << tubeLimit << ")"
            << " tWorld=" << tWorld
            << " pWorld=" << centerWorld
            << " vert=" << nearMiss.prim.primitiveIndex;
    }
}

void cwGeometryItersecter::dumpLeafPrimitive(const Object& object,
                                             const Primitive& prim,
                                             const QRay3D& ray,
                                             uint32_t leafIdx,
                                             uint32_t localIdx)
{
    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    if (positionAttribute == nullptr) {
        return;
    }
    const QMatrix4x4& worldFromModel = object.modelMatrix();
    const QRay3D rayModel = transformRayToModel(worldFromModel, ray);

    const cwRenderObject* parent = object.parent();
    const char* parentClass = parent != nullptr
        ? parent->metaObject()->className()
        : "(null)";

    if (prim.kind == Primitive::Kind::Point) {
        const QVector3D centerModel =
            PointPositionReader(geometry, positionAttribute).at(prim.primitiveIndex);
        const QVector3D centerWorld = mapPoint(worldFromModel, centerModel);

        const float radius = object.pickRadius();
        const double tCenter = ray.projectedDistance(centerWorld);

        // dPerp is in model space; equals world-space d only for
        // identity model matrices (which is all current callers).
        const RaySphereHit sphere = raySphereIntersectDouble(rayModel, centerModel, radius);
        const double dPerp = std::sqrt(sphere.dSq);
        QString tNearStr = QStringLiteral("MISS");
        if (sphere.hit) {
            const QVector3D entryWorld = mapPoint(worldFromModel,
                rayModel.origin() + float(sphere.tNear) * rayModel.direction());
            tNearStr = QString::number(ray.projectedDistance(entryWorld));
        }

        qCDebug(lcPick).nospace()
            << "  leaf " << leafIdx << " [" << localIdx << "] POINT"
            << " " << QLatin1String(parentClass) << "@" << parent
            << " vert=" << prim.primitiveIndex
            << " center=" << centerWorld
            << " d=" << dPerp
            << " tCenter=" << tCenter
            << " tNear=" << tNearStr
            << " radius=" << radius;
        return;
    }

    if (prim.kind == Primitive::Kind::Line) {
        // A line segment is two indices (indices[primitiveIndex] and +1), the
        // same layout primitiveModelBox reads. Do NOT fall through to the
        // triangle path below: that reads a third index and overruns the
        // buffer for the last segment (issue: crash in dumpLeafPrimitive).
        const QVector<uint32_t>& indices = geometry.indices();
        const int i = static_cast<int>(prim.primitiveIndex);
        const QVector3D a = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(i + 0)));
        const QVector3D b = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(i + 1)));
        qCDebug(lcPick).nospace()
            << "  leaf " << leafIdx << " [" << localIdx << "] LINE"
            << " " << QLatin1String(parentClass) << "@" << parent
            << " idx0=" << i
            << " a=" << a << " b=" << b;
        return;
    }

    // Triangle
    const QVector<uint32_t>& indices = geometry.indices();
    const int i = static_cast<int>(prim.primitiveIndex);
    const QVector3D a = mapPoint(worldFromModel,
        geometry.value<QVector3D>(positionAttribute, indices.at(i + 0)));
    const QVector3D b = mapPoint(worldFromModel,
        geometry.value<QVector3D>(positionAttribute, indices.at(i + 1)));
    const QVector3D c = mapPoint(worldFromModel,
        geometry.value<QVector3D>(positionAttribute, indices.at(i + 2)));
    qCDebug(lcPick).nospace()
        << "  leaf " << leafIdx << " [" << localIdx << "] TRI"
        << " " << QLatin1String(parentClass) << "@" << parent
        << " idx0=" << i
        << " a=" << a << " b=" << b << " c=" << c;
}

QBox3D cwGeometryItersecter::primitiveModelBox(const Object& object,
                                               const Primitive& prim)
{
    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);

    if (prim.kind == Primitive::Kind::Triangle) {
        const QVector<uint32_t>& indices = geometry.indices();
        const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 0));
        const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 1));
        const QVector3D c = geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 2));
        return Node::triangleToBoundingBox(a, b, c);
    }

    if (prim.kind == Primitive::Kind::Line) {
        // Tight endpoint AABB (no pad) — the per-pick linePad inflates the
        // box test instead, so picking-disabled cost stays unchanged.
        const QVector<uint32_t>& indices = geometry.indices();
        const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 0));
        const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 1));
        QBox3D box;
        box.unite(a);
        box.unite(b);
        return box;
    }

    // Point primitive — pad the vertex by pickRadius on each axis (model space).
    const QVector3D centerModel =
        PointPositionReader(geometry, positionAttribute).at(prim.primitiveIndex);
    const float radius = object.pickRadius();
    const QVector3D padVec(radius, radius, radius);
    return QBox3D(centerModel - padVec, centerModel + padVec);
}

uint32_t cwGeometryItersecter::serialSplitToFanout(BuildContext& ctx,
                                                   qsizetype begin,
                                                   qsizetype end,
                                                   int depthLeft,
                                                   QVector<SubRange>& outSubRanges,
                                                   QVector<uint32_t>& outUpperInnerNodes)
{
    const uint32_t selfIndex = ctx.nextNode.fetch_add(1);
    const qsizetype count = end - begin;

    auto recordSubrange = [&]() {
        // Slot reserved here; the parallel subtree builder fills it.
        outSubRanges.append(SubRange{selfIndex, begin, end});
    };

    if (depthLeft == 0 || count <= kBvhLeafSize) {
        recordSubrange();
        return selfIndex;
    }

    const MedianSplitResult split = medianSplit(ctx.prims, begin, end);
    if (split.axis < 0) {
        // Degenerate centroid bounds — let the parallel builder make a leaf.
        recordSubrange();
        return selfIndex;
    }

    // medianSplit's nth_element on `count` items dominates this recursion
    // level, so attribute `count` units after it returns. Sum across all
    // calls equals the phaseB1Budget reserved in launchBuildJob().
    if (ctx.splitProgress != nullptr) {
        ctx.splitProgress->done += count;
        ctx.splitProgress->scaler.report(ctx.splitProgress->done);
    }

    // Track this as an upper inner node so the bottom-up bbox pass picks
    // it up. Recording before recursion gives us pre-order; iterating in
    // reverse later yields children-before-parents.
    outUpperInnerNodes.append(selfIndex);

    const uint32_t leftIndex = serialSplitToFanout(ctx, begin, split.mid,
                                                   depthLeft - 1,
                                                   outSubRanges, outUpperInnerNodes);
    const uint32_t rightIndex = serialSplitToFanout(ctx, split.mid, end,
                                                    depthLeft - 1,
                                                    outSubRanges, outUpperInnerNodes);

    BvhNode& self = ctx.outNodes[selfIndex];
    self.left = leftIndex;
    self.right = rightIndex;
    self.splitAxis = static_cast<uint8_t>(split.axis);
    self.isLeaf = false;
    // bbox stays default; filled in the bottom-up pass after subtrees finish.
    return selfIndex;
}

void cwGeometryItersecter::buildBvhSubtree(BuildContext& ctx,
                                           qsizetype begin,
                                           qsizetype end,
                                           uint32_t selfIndex)
{
    const qsizetype count = end - begin;

    auto makeLeaf = [&]() {
        QBox3D box;
        for (qsizetype i = begin; i < end; ++i) {
            box.unite(primitiveModelBox(ctx.object, ctx.prims[i].prim));
        }
        BvhNode& self = ctx.outNodes[selfIndex];
        self.bbox = box;
        self.left = static_cast<uint32_t>(begin);
        self.right = static_cast<uint32_t>(count);
        self.isLeaf = true;
        if (ctx.leafPrimCounter != nullptr) {
            ctx.leafPrimCounter->fetch_add(count, std::memory_order_relaxed);
        }
    };

    if (count <= kBvhLeafSize) {
        makeLeaf();
        return;
    }

    const MedianSplitResult split = medianSplit(ctx.prims, begin, end);
    if (split.axis < 0) {
        makeLeaf();
        return;
    }

    const uint32_t leftIndex = ctx.nextNode.fetch_add(1);
    const uint32_t rightIndex = ctx.nextNode.fetch_add(1);

    buildBvhSubtree(ctx, begin, split.mid, leftIndex);
    buildBvhSubtree(ctx, split.mid, end, rightIndex);

    QBox3D box = ctx.outNodes[leftIndex].bbox;
    box.unite(ctx.outNodes[rightIndex].bbox);

    BvhNode& self = ctx.outNodes[selfIndex];
    self.bbox = box;
    self.left = leftIndex;
    self.right = rightIndex;
    self.splitAxis = static_cast<uint8_t>(split.axis);
    self.isLeaf = false;
}

void cwGeometryItersecter::enumerateTrianglesChunk(const Object& object,
                                                   const EnumChunk& chunk,
                                                   QVector<BuildPrim>& prims)
{
    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const QVector<uint32_t>& indices = geometry.indices();

    for (uint32_t i = 0; i < chunk.count; ++i) {
        const uint32_t triIdx = chunk.inputBegin + i;
        const uint32_t indexBase = triIdx * 3;
        // Centroids and AABBs live in **model space** — the sub-BVH this
        // chunk feeds is cached per-Object and reused across modelMatrix
        // changes, so applying worldFromModel here would force a rebuild
        // for every drag.
        const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 0));
        const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 1));
        const QVector3D c = geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 2));

        BuildPrim& bp = prims[chunk.outBegin + i];
        bp.prim.kind = Primitive::Kind::Triangle;
        bp.prim.primitiveIndex = indexBase;
        bp.centroid = (a + b + c) * (1.0f / 3.0f);
    }
}

void cwGeometryItersecter::enumeratePointsChunk(const Object& object,
                                                const EnumChunk& chunk,
                                                QVector<BuildPrim>& prims)
{
    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const PointPositionReader reader(geometry, positionAttribute);

    for (uint32_t i = 0; i < chunk.count; ++i) {
        const uint32_t vertIdx = chunk.inputBegin + i;
        // Model space — see comment in enumerateTrianglesChunk.
        const QVector3D centerModel = reader.at(vertIdx);

        BuildPrim& bp = prims[chunk.outBegin + i];
        bp.prim.kind = Primitive::Kind::Point;
        bp.prim.primitiveIndex = vertIdx;
        bp.centroid = centerModel;
    }
}

void cwGeometryItersecter::enumerateLinesChunk(const Object& object,
                                               const EnumChunk& chunk,
                                               QVector<BuildPrim>& prims)
{
    const cwGeometry& geometry = object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const QVector<uint32_t>& indices = geometry.indices();

    for (uint32_t i = 0; i < chunk.count; ++i) {
        const uint32_t segIdx = chunk.inputBegin + i;
        const uint32_t indexBase = segIdx * 2;
        // Model space — see comment in enumerateTrianglesChunk.
        const QVector3D a = geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 0));
        const QVector3D b = geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 1));

        BuildPrim& bp = prims[chunk.outBegin + i];
        bp.prim.kind = Primitive::Kind::Line;
        bp.prim.primitiveIndex = indexBase; // first index of the segment
        bp.centroid = (a + b) * 0.5f;
    }
}

void cwGeometryItersecter::scheduleObjectRebuild(const Key& key)
{
    m_subBvhs.remove(key);
    // m_dirtyKeys protects against the published-BVH install callback for
    // a build that snapshotted *before* this invalidation — that callback
    // would otherwise re-populate m_subBvhs[key] with the stale sub-BVH.
    m_dirtyKeys.insert(key);
    invalidatePublishedSlot(key);
    qCDebug(lcPick).nospace()
        << "scheduleObjectRebuild {parent=" << key.parentObject
        << ", id=" << key.id << "} — sub-BVH invalidated; "
        << m_subBvhs.size() << " other sub-BVH(s) still cached, "
        << m_dirtyKeys.size() << " dirty";

    m_bvhRestarter.restart([this]() { return launchBuildJob(); });
}

void cwGeometryItersecter::scheduleTopLevelRebuild()
{
    qCDebug(lcPick).nospace()
        << "scheduleTopLevelRebuild — sub-BVH cache untouched ("
        << m_subBvhs.size() << " entries); "
        << m_dirtyKeys.size() << " keys already dirty";
    // m_bvh stays live during the rebuild. The stored modelMatrices /
    // nodesSnapshot are now slightly stale for any Key whose matrix
    // moved or whose entry was removed, but picks against the
    // unaffected Keys remain correct and the rebuild is fast (top-
    // level only). Callers that need a Key gone from picks instantly
    // (removeObject, clear) call invalidatePublishedSlot first.
    m_bvhRestarter.restart([this]() { return launchBuildJob(); });
}

void cwGeometryItersecter::invalidatePublishedSlot(const Key& key)
{
    if (!m_bvh) {
        return;
    }
    auto it = m_bvh->keyToSlot.constFind(key);
    if (it == m_bvh->keyToSlot.constEnd()) {
        return;
    }
    // m_bvh is immutable and may be shared with in-flight query snapshots or
    // worker threads, so don't edit it in place: copy-on-write a fresh
    // BvhData with the dirty Key's slot nulled, then publish it. The copy is
    // cheap (O(objects) — the heavy sub-BVHs stay shared by shared_ptr), and
    // readers holding the old snapshot keep seeing consistent old data.
    auto next = std::make_shared<BvhData>(*m_bvh);
    next->subBvhs[*it].reset();
    m_bvh = std::move(next);
}

cwGeometryItersecter::QuerySnapshot cwGeometryItersecter::snapshotForQuery() const
{
    return QuerySnapshot(m_bvh);
}

QList<QVector3D> cwGeometryItersecter::pointsInBox(const QuerySnapshot& snapshot,
                                                  const QBox3D& box,
                                                  const Key& key)
{
    QList<QVector3D> result;

    const std::shared_ptr<const BvhData>& bvh = snapshot.m_data;
    if (!bvh || box.isNull()) {
        return result;
    }

    const auto slotIt = bvh->keyToSlot.constFind(key);
    if (slotIt == bvh->keyToSlot.constEnd()) {
        return result;
    }
    const int slot = *slotIt;

    const std::shared_ptr<const SubBvh>& subPtr = bvh->subBvhs.at(slot);
    if (!subPtr) {                         // slot nulled mid-rebuild
        return result;
    }
    const SubBvh& sub = *subPtr;

    // Only point clouds answer a point-in-box query.
    const cwGeometry& geometry = sub.object.geometry();
    if (geometry.type() != cwGeometry::Type::Points) {
        return result;
    }
    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    if (positionAttribute == nullptr) {
        return result;
    }

    const QMatrix4x4& worldFromModel = bvh->modelMatrices.at(slot);
    const QMatrix4x4& modelToWorld = bvh->inverseModelMatrices.at(slot);
    // Point clouds register with an identity matrix; skip the per-point
    // model->world map in that common case (it runs once per surviving point).
    const bool identity = worldFromModel.isIdentity();

    // The sub-BVH boxes are model space; bring the world query box into model
    // space (conservative corner AABB) so node pruning is in the same frame.
    // Leaf points are still tested exactly against the world box, so the
    // conservative model box never admits a false positive.
    const QBox3D modelBox = box.transformed(modelToWorld);
    if (modelBox.isNull()) {
        return result;
    }

    const PointPositionReader reader(geometry, positionAttribute);

    QVarLengthArray<uint32_t, kBvhTraversalStackInline> stack;
    stack.append(0);
    while (!stack.isEmpty()) {
        const uint32_t idx = stack.last();
        stack.removeLast();

        const BvhNode& node = sub.bvhNodes[idx];
        if (!node.bbox.intersects(modelBox)) {
            continue;
        }

        if (node.isLeaf) {
            const uint32_t first = node.left;
            const uint32_t primCount = node.right;
            for (uint32_t p = first; p < first + primCount; ++p) {
                const Primitive& prim = sub.primitives.at(p);
                const QVector3D modelPos = reader.at(prim.primitiveIndex);
                const QVector3D worldPos = identity ? modelPos
                                                    : mapPoint(worldFromModel, modelPos);
                if (box.contains(worldPos)) {
                    result.append(worldPos);
                }
            }
        } else {
            stack.append(node.left);
            stack.append(node.right);
        }
    }

    return result;
}

std::shared_ptr<cwGeometryItersecter::SubBvh>
cwGeometryItersecter::buildSubBvh(const Object& object,
                                  QPromise<void>& promise)
{
    const qsizetype primCount = countNodePrimitives(object);
    if (primCount <= 0) {
        return nullptr;
    }

    // Plan Phase A chunks. Each EnumChunk maps to a contiguous slice of
    // the BuildPrim vector so chunk workers can write without contention.
    QVector<EnumChunk> chunks;
    {
        qsizetype filled = 0;
        while (filled < primCount) {
            const qsizetype take = std::min<qsizetype>(primCount - filled, kEnumChunkSize);
            EnumChunk c;
            c.inputBegin = static_cast<uint32_t>(filled);
            c.count = static_cast<uint32_t>(take);
            c.outBegin = static_cast<uint32_t>(filled);
            chunks.append(c);
            filled += take;
        }
    }

    if (promise.isCanceled()) {
        return nullptr;
    }

    QVector<BuildPrim> prims;
    prims.resize(primCount);

    const auto enumerateChunk = [&](const EnumChunk& chunk) {
        if (object.geometry().attribute(cwGeometry::Semantic::Position) == nullptr) {
            return;
        }
        switch (object.geometry().type()) {
        case cwGeometry::Type::Triangles:
            enumerateTrianglesChunk(object, chunk, prims);
            break;
        case cwGeometry::Type::Lines:
            enumerateLinesChunk(object, chunk, prims);
            break;
        case cwGeometry::Type::Points:
            enumeratePointsChunk(object, chunk, prims);
            break;
        default:
            break;
        }
    };

    // Parallelize Phase A only when the Object is large enough to amortize
    // the cwConcurrent::map overhead. Small Objects (most scraps, line
    // segments, single triangles) run inline.
    if (chunks.size() > 1) {
        auto enumFuture = cwConcurrent::map(chunks, [&](EnumChunk& chunk) {
            if (promise.isCanceled()) {
                return;
            }
            enumerateChunk(chunk);
        });
        waitOnPool(enumFuture);
    } else {
        enumerateChunk(chunks.at(0));
    }

    if (promise.isCanceled()) {
        return nullptr;
    }

    // Phase B: serial top split + parallel subtree builders. Same pipeline
    // as the old flat-BVH build, just scoped to one Object.
    const qsizetype upperBoundCount =
        4 * ((prims.size() + kBvhLeafSize - 1) / kBvhLeafSize) + 1;
    QVector<BvhNode> bvhNodes;
    bvhNodes.resize(upperBoundCount);

    std::atomic<uint32_t> nextNode{0};
    QVector<SubRange> subRanges;
    QVector<uint32_t> upperInnerNodes;

    BuildContext serialCtx{object, prims, bvhNodes, nextNode,
                           /*leafPrimCounter=*/nullptr,
                           /*splitProgress=*/nullptr};
    serialSplitToFanout(serialCtx, 0, prims.size(),
                        kParallelFanoutDepth,
                        subRanges, upperInnerNodes);

    if (promise.isCanceled()) {
        return nullptr;
    }

    if (!subRanges.isEmpty()) {
        std::atomic<qsizetype> phaseBPrims{0};
        BuildContext parallelCtx{object, prims, bvhNodes, nextNode, &phaseBPrims};

        auto buildFuture = cwConcurrent::map(subRanges,
            [&parallelCtx, &promise](SubRange& r) {
                if (promise.isCanceled()) {
                    return;
                }
                buildBvhSubtree(parallelCtx, r.begin, r.end, r.rootSlot);
            });
        waitOnPool(buildFuture);
    }

    // Bottom-up bbox pass: visit children before parents so each upper
    // inner node sees finalized child boxes (pre-order recording, reverse
    // iteration here).
    for (int i = upperInnerNodes.size() - 1; i >= 0; --i) {
        const uint32_t slot = upperInnerNodes[i];
        BvhNode& self = bvhNodes[slot];
        QBox3D box = bvhNodes[self.left].bbox;
        box.unite(bvhNodes[self.right].bbox);
        self.bbox = box;
    }

    bvhNodes.resize(nextNode.load(std::memory_order_relaxed));
    bvhNodes.squeeze();

    if (promise.isCanceled()) {
        return nullptr;
    }

    // Extract Primitives. Their nodeIndex was only meaningful during the
    // build (singleton list index 0); runtime traversal accesses the
    // Object via SubBvh::object, not via prim.nodeIndex.
    QVector<Primitive> finalPrims;
    finalPrims.resize(prims.size());
    for (qsizetype i = 0; i < prims.size(); ++i) {
        finalPrims[i] = prims[i].prim;
    }
    prims.clear();
    prims.squeeze();

    auto sub = std::make_shared<SubBvh>();
    sub->bvhNodes = std::move(bvhNodes);
    sub->primitives = std::move(finalPrims);
    sub->modelRootBox = !sub->bvhNodes.isEmpty() ? sub->bvhNodes[0].bbox : QBox3D();
    sub->object = object;
    return sub;
}

QVector<cwGeometryItersecter::BvhNode>
cwGeometryItersecter::buildTopLevel(const QVector<QBox3D>& worldBoxes)
{
    QVector<BvhNode> out;
    if (worldBoxes.isEmpty()) {
        return out;
    }

    // Single-Object leaves: each top-level leaf stores left=subBvhSlot,
    // right=1. The slot indexes into BvhData::subBvhs / modelMatrices /
    // inverseModelMatrices parallel arrays — left untouched (in
    // caller-supplied order) so traversal can index by slot directly.
    struct TopPrim {
        uint32_t slot;
        QVector3D centroid;
        QBox3D box;
    };

    QVector<TopPrim> prims;
    prims.reserve(worldBoxes.size());
    for (qsizetype i = 0; i < worldBoxes.size(); ++i) {
        const QBox3D& b = worldBoxes[i];
        if (b.isNull()) {
            continue;
        }
        prims.append({static_cast<uint32_t>(i),
                      (b.minimum() + b.maximum()) * 0.5f, b});
    }
    if (prims.isEmpty()) {
        return out;
    }

    // Binary BVH with leaves = prims.size(): inner = leaves - 1, total =
    // 2*leaves - 1. Reserve up front so recursive appends never reallocate
    // (otherwise refs into out[] taken between appends would dangle).
    out.reserve(2 * prims.size());

    struct RecursiveBuilder {
        QVector<TopPrim>& prims;
        QVector<BvhNode>& out;

        uint32_t operator()(qsizetype begin, qsizetype end) {
            const qsizetype count = end - begin;
            const uint32_t selfIdx = static_cast<uint32_t>(out.size());
            out.append(BvhNode{});

            if (count == 1) {
                out[selfIdx].bbox = prims[begin].box;
                out[selfIdx].left = prims[begin].slot;
                out[selfIdx].right = 1;
                out[selfIdx].isLeaf = true;
                return selfIdx;
            }

            QBox3D centroidBox;
            for (qsizetype i = begin; i < end; ++i) {
                centroidBox.unite(prims[i].centroid);
            }
            const QVector3D extent = centroidBox.maximum() - centroidBox.minimum();
            int axis = dominantSplitAxis(extent);
            // Co-located Objects (same origin) produce a degenerate centroid
            // box. Fall back to axis 0 — the (begin+end)/2 split below still
            // halves the range so recursion terminates at the count==1 leaf.
            if (axis < 0) {
                axis = 0;
            }

            const qsizetype mid = (begin + end) / 2;
            std::nth_element(prims.begin() + begin,
                             prims.begin() + mid,
                             prims.begin() + end,
                             [axis](const TopPrim& a, const TopPrim& b) {
                                 return a.centroid[axis] < b.centroid[axis];
                             });

            const uint32_t leftIdx = (*this)(begin, mid);
            const uint32_t rightIdx = (*this)(mid, end);

            QBox3D box = out[leftIdx].bbox;
            box.unite(out[rightIdx].bbox);
            out[selfIdx].bbox = box;
            out[selfIdx].left = leftIdx;
            out[selfIdx].right = rightIdx;
            out[selfIdx].splitAxis = static_cast<uint8_t>(axis);
            out[selfIdx].isLeaf = false;
            return selfIdx;
        }
    };

    RecursiveBuilder{prims, out}(0, prims.size());
    return out;
}

QFuture<void> cwGeometryItersecter::launchBuildJob()
{
    // Snapshot Nodes + the sub-BVH cache + the dirty set. QList/QHash are
    // implicitly shared, so these are cheap header copies that pin the
    // worker's view stable against UI-thread mutations during the build.
    // Workers must use const access (.at(), const ref) so they never
    // detach.
    auto nodesSnapshot = Nodes;
    auto subBvhSnapshot = m_subBvhs;
    auto dirtyKeysSnapshot = m_dirtyKeys;
    m_dirtyKeys.clear();

    if (lcPick().isDebugEnabled()) {
        int triNodes = 0;
        int lineNodes = 0;
        int pointNodes = 0;
        int otherNodes = 0;
        qsizetype totalPrims = 0;
        int dirty = 0;
        int cached = 0;
        for (const Node& n : std::as_const(nodesSnapshot)) {
            const qsizetype primCount = countNodePrimitives(n.Object);
            totalPrims += primCount;
            switch (n.Object.geometry().type()) {
            case cwGeometry::Type::Triangles: ++triNodes; break;
            case cwGeometry::Type::Lines:     ++lineNodes; break;
            case cwGeometry::Type::Points:    ++pointNodes; break;
            default:                          ++otherNodes; break;
            }
            const Key key{n.Object.parent(), n.Object.id()};
            if (subBvhSnapshot.contains(key) && !dirtyKeysSnapshot.contains(key)) {
                ++cached;
            }
            if (dirtyKeysSnapshot.contains(key)) {
                ++dirty;
            }
        }
        qCDebug(lcPick).nospace()
            << "launchBuildJob snapshot: " << nodesSnapshot.size() << " source nodes"
            << " (Triangles=" << triNodes
            << " Lines=" << lineNodes
            << " Points=" << pointNodes
            << " other=" << otherNodes << ")"
            << " totalPrims=" << totalPrims
            << " cached=" << cached
            << " dirty=" << dirty;
    }

    auto resultSlot = std::make_shared<std::shared_ptr<BvhData>>();
    auto builtSlot = std::make_shared<QHash<Key, std::shared_ptr<const SubBvh>>>();

    QFuture<void> worker = cwConcurrent::run(
        [nodesSnapshot, subBvhSnapshot, dirtyKeysSnapshot, resultSlot, builtSlot]
        (QPromise<void>& promise) mutable {

        const auto buildStart = std::chrono::steady_clock::now();

        struct Task {
            uint32_t snapshotIndex;
            Key key;
        };
        QVector<Task> tasks;
        for (qsizetype i = 0; i < nodesSnapshot.size(); ++i) {
            const Node& n = nodesSnapshot.at(i);
            if (countNodePrimitives(n.Object) <= 0) {
                continue;
            }
            const Key key{n.Object.parent(), n.Object.id()};
            const bool missing = !subBvhSnapshot.contains(key);
            const bool dirty = dirtyKeysSnapshot.contains(key);
            if (missing || dirty) {
                tasks.append({static_cast<uint32_t>(i), key});
            }
        }

        if (promise.isCanceled()) {
            return;
        }

        promise.setProgressRange(0, kProgressResolution);
        promise.setProgressValue(0);

        const bool debug = lcPick().isDebugEnabled();
        if (debug) {
            qsizetype reusedPrims = 0;
            qsizetype rebuiltPrims = 0;
            for (qsizetype i = 0; i < nodesSnapshot.size(); ++i) {
                const Node& n = nodesSnapshot.at(i);
                const qsizetype primCount = countNodePrimitives(n.Object);
                if (primCount <= 0) {
                    continue;
                }
                const Key key{n.Object.parent(), n.Object.id()};
                if (!subBvhSnapshot.contains(key) || dirtyKeysSnapshot.contains(key)) {
                    rebuiltPrims += primCount;
                } else {
                    reusedPrims += primCount;
                }
            }
            qCDebug(lcPick).nospace()
                << "worker: building " << tasks.size() << " sub-BVH(s) ("
                << rebuiltPrims << " prims rebuilt, "
                << reusedPrims << " prims reused from cache)";
        }

        // buildSubBvh internally parallelizes Phase A/B for large clouds
        // via nested cwConcurrent::map — the QThreadPool absorbs the
        // double-release via waitOnPool's release/reserve dance.
        QVector<std::shared_ptr<SubBvh>> built(tasks.size());
        QVector<double> taskMs(tasks.size(), 0.0);
        if (!tasks.isEmpty()) {
            std::atomic<qsizetype> done{0};
            auto fut = cwConcurrent::map(tasks, [&](Task& t) {
                if (promise.isCanceled()) {
                    return;
                }
                const qsizetype slot = &t - tasks.constData();
                const auto t0 = std::chrono::steady_clock::now();
                built[slot] = buildSubBvh(nodesSnapshot.at(t.snapshotIndex).Object, promise);
                taskMs[slot] = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0).count();
                const qsizetype d = done.fetch_add(1, std::memory_order_relaxed) + 1;
                promise.setProgressValue(static_cast<int>(
                    (d * kProgressResolution) / std::max<qsizetype>(tasks.size(), 1)));
            });
            waitOnPool(fut);
        }

        if (debug && !tasks.isEmpty()) {
            // Cap per-task dump so a many-Objects rebuild doesn't blast
            // thousands of lines.
            constexpr int kTaskDumpCap = 16;
            const int dumpCount = std::min<int>(tasks.size(), kTaskDumpCap);
            for (int i = 0; i < dumpCount; ++i) {
                const qsizetype prims = built[i]
                    ? built[i]->primitives.size()
                    : qsizetype(0);
                qCDebug(lcPick).nospace()
                    << "  task[" << i << "] {parent=" << tasks[i].key.parentObject
                    << ", id=" << tasks[i].key.id << "}"
                    << " prims=" << prims
                    << " ms=" << taskMs[i];
            }
            if (tasks.size() > kTaskDumpCap) {
                qCDebug(lcPick).nospace()
                    << "  ... " << (tasks.size() - kTaskDumpCap) << " more tasks elided";
            }
        }

        if (promise.isCanceled()) {
            return;
        }

        for (qsizetype i = 0; i < tasks.size(); ++i) {
            if (built[i]) {
                subBvhSnapshot[tasks[i].key] = built[i];
                (*builtSlot)[tasks[i].key] = built[i];
            }
        }

        auto out = std::make_shared<BvhData>();
        out->nodesSnapshot = nodesSnapshot;
        out->subBvhs.reserve(nodesSnapshot.size());
        out->modelMatrices.reserve(nodesSnapshot.size());
        out->inverseModelMatrices.reserve(nodesSnapshot.size());

        QVector<QBox3D> worldBoxes;
        worldBoxes.reserve(nodesSnapshot.size());
        out->keyToSlot.reserve(nodesSnapshot.size());
        for (qsizetype i = 0; i < nodesSnapshot.size(); ++i) {
            const Node& n = nodesSnapshot.at(i);
            const Key key{n.Object.parent(), n.Object.id()};
            auto it = subBvhSnapshot.constFind(key);
            if (it == subBvhSnapshot.constEnd() || !*it) {
                continue;
            }
            const SubBvh& sb = **it;
            const QMatrix4x4 m = n.Object.modelMatrix();
            const QBox3D wb = sb.modelRootBox.transformed(m);
            const int slot = static_cast<int>(out->subBvhs.size());
            out->subBvhs.append(*it);
            out->modelMatrices.append(m);
            out->inverseModelMatrices.append(m.inverted());
            worldBoxes.append(wb);
            out->keyToSlot.insert(key, slot);
            out->maxPickRadius = std::max(out->maxPickRadius, sb.object.pickRadius());
        }

        if (promise.isCanceled()) {
            return;
        }

        const auto topStart = std::chrono::steady_clock::now();
        out->topLevel = buildTopLevel(worldBoxes);
        const double topMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - topStart).count();
        const double totalMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - buildStart).count();

        if (debug) {
            qCDebug(lcPick).nospace()
                << "worker done: " << totalMs << "ms total ("
                << topMs << "ms top-level), " << tasks.size()
                << " sub-BVH(s) built, " << out->subBvhs.size()
                << " in final BvhData, " << out->topLevel.size()
                << " top-level nodes";
        }

        *resultSlot = std::move(out);
    });

    // Install the freshly built BVH on the UI thread. .context(this, ...)
    // is auto-disconnected by Qt when `this` is destroyed, so the lambda
    // never fires after destruction.
    AsyncFuture::observe(worker).context(this, [this, resultSlot, builtSlot]() {
        if (!*resultSlot) {
            qCDebug(lcPick) << "build worker finished without producing a BvhData"
                            << "(canceled or zero prims)";
            return;
        }
        m_bvh = *resultSlot;
        // A worker that finished computing just before cancel still
        // publishes here. Re-apply the dirty filter on this side of the
        // swap — mutator-site invalidatePublishedSlot was a no-op against
        // the old m_bvh and would otherwise miss these Keys.
        for (const Key& dirtyKey : std::as_const(m_dirtyKeys)) {
            invalidatePublishedSlot(dirtyKey);
        }
        // Promote freshly built sub-BVHs to the cache, skipping any Key
        // that's been re-dirtied since the build started — those will be
        // rebuilt by the next launched job.
        for (auto it = builtSlot->constBegin(); it != builtSlot->constEnd(); ++it) {
            if (!m_dirtyKeys.contains(it.key())) {
                m_subBvhs[it.key()] = it.value();
            }
        }
        qCDebug(lcPick).nospace()
            << "bvhReady installed: topLevel=" << m_bvh->topLevel.size()
            << " subBvhs=" << m_bvh->subBvhs.size()
            << " sourceNodes=" << m_bvh->nodesSnapshot.size()
            << " cached=" << m_subBvhs.size();
        emit bvhReady();
    });

    return worker;
}

cwRayHit cwGeometryItersecter::rayTriangleMT(const QRay3D &rayModel,
                                             const QVector3D &a,
                                             const QVector3D &b,
                                             const QVector3D &c,
                                             bool cullBackfaces)
{
    cwRayHit result;

    const QVector3D edge1 = b - a;
    const QVector3D edge2 = c - a;

    const QVector3D pvec = QVector3D::crossProduct(rayModel.direction(), edge2);
    const float det = QVector3D::dotProduct(edge1, pvec);

    const float kEps = 1e-7f;

    if (cullBackfaces) {
        if (det <= kEps) {
            return result;
        }
    } else {
        if (std::fabs(det) < kEps) {
            return result; // parallel or nearly parallel
        }
    }

    const float invDet = 1.0f / det;
    const QVector3D tvec = rayModel.origin() - a;

    const float u = QVector3D::dotProduct(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return result;
    }

    const QVector3D qvec = QVector3D::crossProduct(tvec, edge1);
    const float v = QVector3D::dotProduct(rayModel.direction(), qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return result;
    }

    const float t = QVector3D::dotProduct(edge2, qvec) * invDet;
    if (t <= 0.0f) {
        return result;
    }

    result.m_hit = true;
    result.m_tModel = t;
    result.m_u = u;
    result.m_v = v;
    result.m_pointModel = rayModel.origin() + t * rayModel.direction();
    // Unnormalized normal; normalize for safety
    result.m_normalModel = QVector3D::normal(a, b, c).normalized();
    return result;
}


cwGeometryItersecter::Node::Node(const QBox3D boundingBox,
                                 const cwGeometryItersecter::Object &object) :
    BoundingBox(boundingBox),
    Object(object)
{

}

/**
 * @brief cwGeometryItersecter::Node::triangleToBoundingBox
 * @return Creates a bounding box around a triangle
 */
QBox3D cwGeometryItersecter::Node::triangleToBoundingBox(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3)
{

    QVector3D maxPoint(qMax(p1.x(), qMax(p2.x(), p3.x())),
                       qMax(p1.y(), qMax(p2.y(), p3.y())),
                       qMax(p1.z(), qMax(p2.z(), p3.z())));

    QVector3D minPoint(qMin(p1.x(), qMin(p2.x(), p3.x())),
                       qMin(p1.y(), qMin(p2.y(), p3.y())),
                       qMin(p1.z(), qMin(p2.z(), p3.z())));

    return QBox3D(minPoint, maxPoint);
}
