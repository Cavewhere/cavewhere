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

// Closest sphere-miss seen during a traversal; tryPromoteNearMiss snaps
// best to it when no true hit exists.
struct cwGeometryItersecter::NearMissResult {
    bool valid = false;
    double dSq = (std::numeric_limits<double>::max)();
    double tCenterModel = 0.0;
    Primitive prim;
    float radius = 0.0f;
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

    // Job name surfaced in the cwFutureManagerModel UI while the async
    // BVH build is running.
    constexpr auto kAcceleratingPickingJobName = QLatin1StringView("Accelerating picking");

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
    const QList<Node>& nodes;
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
    case cwGeometry::Type::Points:
        return object.pickRadius() > 0.0f ? geometry.vertexCount() : 0;
    default:
        return 0;
    }
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
        stats.bvhNodeCount = m_bvh->bvhNodes.size();
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
        Nodes.clear();
        scheduleBuild();
        return;
    }

    QList<Node>::iterator iter = Nodes.begin();
    while(iter != Nodes.end()) {
        Node& currentNode = *iter;
        if(currentNode.Object.parent() == parentObject) {
            iter = Nodes.erase(iter);
        } else {
            iter++;
        }
    }
    scheduleBuild();
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
    auto iter = findNode(objectKey);
    if (iter != Nodes.end()) {
        qCDebug(lcPick).nospace()
            << "removeObject {parent=" << objectKey.parentObject
            << ", id=" << objectKey.id << "}"
            << " Nodes.size before=" << Nodes.size();
        Nodes.erase(iter);
        scheduleBuild();
    }
}

void cwGeometryItersecter::setModelMatrix(cwRenderObject *parentObject, uint64_t id, const QMatrix4x4 &modelMatrix)
{
    setModelMatrix(Key(parentObject, id), modelMatrix);
}

void cwGeometryItersecter::setModelMatrix(const Key &objectKey, const QMatrix4x4& modelMatrix)
{
    auto iter = findNode(objectKey);
    if (iter != Nodes.end()) {
        iter->Object.setModelMatrix(modelMatrix);
        scheduleBuild();
    }
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

/**
 * @brief cwGeometryItersecter::intersects
 * @param ray
 * @return Closest intersection along the ray, or NaN when the BVH (with
 *         the tube-pick fallback in intersectsDetailed) finds nothing.
 *         Callers can then fall back to a grid plane / projected pivot.
 */
double cwGeometryItersecter::intersects(const QRay3D &ray) const
{
    const cwRayHit hit = intersectsDetailed(ray);
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

    removeObject(object.parent(), object.id());

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
    scheduleBuild();
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
    //Make sure the object has the right number of indices
    if(object.geometry().indices().size() % 2 != 0) {
        qDebug() << "Can't add object" << object.parent() << object.id() << "because it has an invalid indexes" << LOCATION;
        return;
    }

    removeObject(object.parent(), object.id());

    int linesAppended = 0;
    for(int i = 0; i < object.geometry().indices().size(); i+=2) {
        Node node = Node(object, i);

        //Make sure the node is valid
        if(!node.BoundingBox.isNull()) {
            Nodes.append(node);
            ++linesAppended;
        }
    }
    qCDebug(lcPick).nospace()
        << "addLines " << formatKey(object)
        << " segments=" << (object.geometry().indices().size() / 2)
        << " nodesAppended=" << linesAppended
        << " Nodes.size=" << Nodes.size()
        << " (note: Lines are currently not picker-visible — the BVH skips them)";
    scheduleBuild();
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

    removeObject(object.parent(), object.id());

    const char* base = geometry.vertexBuffer(positionAttribute->bufferIndex)->constData()
                       + positionAttribute->byteOffsetInBuffer;
    const int stride = positionAttribute->bufferStride;

    QBox3D box;
    for (qsizetype i = 0; i < vertexCount; ++i) {
        const float* p = reinterpret_cast<const float*>(base + i * stride);
        box.unite(QVector3D(p[0], p[1], p[2]));
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
    scheduleBuild();
}


cwRayHit cwGeometryItersecter::intersectsDetailed(const QRay3D &ray) const
{
    cwRayHit best;

    const bool debug = lcPick().isDebugEnabled();
    PickStats stats;
    PickStats* statsPtr = debug ? &stats : nullptr;

    if (!m_bvh || m_bvh->bvhNodes.isEmpty()) {
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

    const QVector<BvhNode>& nodes = m_bvh->bvhNodes;
    const QVector<Primitive>& prims = m_bvh->primitives;
    const QList<Node>& nodeSnapshot = m_bvh->nodesSnapshot;

    // AABB tests are inflated by tubeDist so leaves the tube-pick fallback
    // might want to consider actually get visited. A single global max
    // (vs per-subtree padding) keeps BVH memory flat for 300M-point clouds.
    // Disabling the toggle collapses the inflation to a tight test.
    const float tubeDist = m_tubePickEnabled
                           ? m_bvh->maxPickRadius * kTubeFactor
                           : 0.0f;
    const QVector3D tubePad(tubeDist, tubeDist, tubeDist);

    NearMissResult nearMiss;
    NearMissResult* nearMissPtr = m_tubePickEnabled ? &nearMiss : nullptr;

    if (debug) {
        qCDebug(lcPick).nospace()
            << "intersectsDetailed: BVH nodes=" << nodes.size()
            << ", prims=" << prims.size()
            << ", source nodes=" << nodeSnapshot.size()
            << ", maxPickRadius=" << m_bvh->maxPickRadius
            << ", tubeDist=" << tubeDist
            << ", ray.origin=" << ray.origin()
            << ", ray.dir=" << ray.direction();
    }

    // Stack-based traversal with near-first child ordering. 64 is plenty for
    // realistic BVH depths (~log2 of primitive count, well under 30 for 1M).
    QVarLengthArray<uint32_t, 64> stack;
    stack.append(0);

    while (!stack.isEmpty()) {
        const uint32_t idx = stack.last();
        stack.removeLast();

        const BvhNode& bn = nodes[idx];
        if (debug) {
            ++stats.nodesVisited;
        }

        // Box reject. Skip the inflated-box construction (which re-sorts
        // min/max via qMin/qMax) when tubeDist is 0 — toggle off, no
        // Point objects, or all-zero radii — so the disabled path
        // matches the pre-tube traversal cost.
        const double tBox = (tubeDist == 0.0f)
            ? bn.bbox.intersection(ray)
            : QBox3D(bn.bbox.minimum() - tubePad,
                     bn.bbox.maximum() + tubePad).intersection(ray);
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

        if (bn.isLeaf) {
            if (debug) {
                ++stats.leavesVisited;
            }
            const uint32_t first = bn.left;
            const uint32_t primCount = bn.right;
            if (debug) {
                qCDebug(lcPick).nospace()
                    << "leaf " << idx << ": " << primCount << " prims, bbox=["
                    << bn.bbox.minimum() << " .. " << bn.bbox.maximum() << "]"
                    << " bestSoFar="
                    << (best.hit() ? QString("tWorld=%1").arg(best.tWorld())
                                   : QStringLiteral("none"));
            }
            for (uint32_t p = first; p < first + primCount; ++p) {
                if (debug) {
                    dumpLeafPrimitive(nodeSnapshot, prims[p], ray, idx, p - first);
                }
                testPrimitive(nodeSnapshot, prims[p], ray, best, nearMissPtr, statsPtr);
            }
        } else {
            uint32_t nearChild = bn.left;
            uint32_t farChild = bn.right;
            if (ray.direction()[bn.splitAxis] < 0.0f) {
                std::swap(nearChild, farChild);
            }
            // Push far first so near pops first.
            stack.append(farChild);
            stack.append(nearChild);
        }
    }

    // Without the tube fallback, a sub-pixel cursor gap in a dense LAZ
    // surface returns NaN from intersects() and the camera-pivot path in
    // cwBaseTurnTableInteraction snaps to the grid plane at the wrong
    // depth, jerking the view.
    if (m_tubePickEnabled) {
        tryPromoteNearMiss(best, nearMiss, nodeSnapshot, ray, debug);
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

            // Linear scan over prims to summarize by source cwRenderObject.
            // Capped because enabling debug on a 100M-primitive cloud
            // shouldn't freeze the UI thread on every miss.
            constexpr qsizetype kMissDumpPrimCap = 1'000'000;
            const qsizetype scanLimit = std::min<qsizetype>(prims.size(), kMissDumpPrimCap);
            QHash<const cwRenderObject*, qsizetype> primsBySource;
            QHash<const cwRenderObject*, const char*> typeBySource;
            primsBySource.reserve(nodeSnapshot.size());
            typeBySource.reserve(nodeSnapshot.size());
            for (qsizetype i = 0; i < scanLimit; ++i) {
                const Node& n = nodeSnapshot.at(prims.at(i).nodeIndex);
                primsBySource[n.Object.parent()]++;
                typeBySource[n.Object.parent()] =
                    cwGeometry::typeName(n.Object.geometry().type());
            }
            qCDebug(lcPick).nospace()
                << "  live BVH sources: " << primsBySource.size()
                << " distinct cwRenderObject*"
                << (prims.size() > kMissDumpPrimCap
                    ? QStringLiteral(" (sampled first %1 of %2 prims)")
                          .arg(kMissDumpPrimCap).arg(prims.size())
                    : QString{});
            for (auto it = primsBySource.constBegin();
                 it != primsBySource.constEnd(); ++it) {
                const cwRenderObject* parent = it.key();
                const char* parentClass = parent != nullptr
                    ? parent->metaObject()->className()
                    : "(null)";
                qCDebug(lcPick).nospace()
                    << "    " << QLatin1String(parentClass)
                    << "@" << parent
                    << " type=" << typeBySource.value(parent, "?")
                    << " prims=" << it.value()
                    << " visible=" << (parent != nullptr ? parent->isVisible() : true);
            }
        }
    }

    return best;
}

void cwGeometryItersecter::testPrimitive(const QList<Node>& nodes,
                                         const Primitive& prim,
                                         const QRay3D& ray,
                                         cwRayHit& best,
                                         NearMissResult* nearMiss,
                                         PickStats* stats)
{
    if (stats != nullptr) {
        ++stats->primsTested;
    }

    const Node& node = nodes[prim.nodeIndex];
    if (!isPickable(node.Object)) {
        if (stats != nullptr) {
            ++stats->primsNotPickable;
        }
        return;
    }

    const QMatrix4x4& worldFromModel = node.Object.modelMatrix();
    const QRay3D rayModel = transformRayToModel(worldFromModel, ray);
    const cwGeometry& geometry = node.Object.geometry();
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
        const QVector3D nWorld = transformNormalToWorld(worldFromModel, local.normalModel());
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
        best.m_object = node.Object.parent();
        best.m_objectId = node.Object.id();
        best.m_firstIndex = i;
        if (stats != nullptr) {
            ++stats->primsAccepted;
        }
        return;
    }

    // Point primitive — ray-vs-sphere using the Object's pickRadius.
    const float radius = node.Object.pickRadius();
    if (radius <= 0.0f) {
        if (stats != nullptr) {
            ++stats->primsPointRadiusZero;
        }
        return;
    }

    const char* base = geometry.vertexBuffer(positionAttribute->bufferIndex)->constData()
                       + positionAttribute->byteOffsetInBuffer;
    const int stride = positionAttribute->bufferStride;
    const float* p = reinterpret_cast<const float*>(base + prim.primitiveIndex * stride);
    const QVector3D center(p[0], p[1], p[2]);

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

    fillPointHit(best, node, prim, ray, rayModel,
                 center, pWorld, double(sphere.tNear), tWorld);
    if (stats != nullptr) {
        ++stats->primsAccepted;
    }
}

void cwGeometryItersecter::fillPointHit(cwRayHit& best,
                                        const Node& node,
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
    best.m_object = node.Object.parent();
    best.m_objectId = node.Object.id();
    best.m_firstIndex = static_cast<int>(prim.primitiveIndex);
}

void cwGeometryItersecter::tryPromoteNearMiss(cwRayHit& best,
                                              const NearMissResult& nearMiss,
                                              const QList<Node>& nodeSnapshot,
                                              const QRay3D& ray,
                                              bool debug)
{
    if (best.hit() || !nearMiss.valid) {
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

    const Node& node = nodeSnapshot.at(nearMiss.prim.nodeIndex);
    const cwGeometry& geometry = node.Object.geometry();
    const auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);

    const QMatrix4x4& worldFromModel = node.Object.modelMatrix();
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
    fillPointHit(best, node, nearMiss.prim, ray, rayModel,
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

void cwGeometryItersecter::dumpLeafPrimitive(const QList<Node>& nodes,
                                             const Primitive& prim,
                                             const QRay3D& ray,
                                             uint32_t leafIdx,
                                             uint32_t localIdx)
{
    const Node& node = nodes.at(prim.nodeIndex);
    const cwGeometry& geometry = node.Object.geometry();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    if (positionAttribute == nullptr) {
        return;
    }
    const QMatrix4x4& worldFromModel = node.Object.modelMatrix();
    const QRay3D rayModel = transformRayToModel(worldFromModel, ray);

    const cwRenderObject* parent = node.Object.parent();
    const char* parentClass = parent != nullptr
        ? parent->metaObject()->className()
        : "(null)";

    if (prim.kind == Primitive::Kind::Point) {
        const char* base = geometry.vertexBuffer(positionAttribute->bufferIndex)->constData()
                           + positionAttribute->byteOffsetInBuffer;
        const int stride = positionAttribute->bufferStride;
        const float* p = reinterpret_cast<const float*>(base + prim.primitiveIndex * stride);
        const QVector3D centerModel(p[0], p[1], p[2]);
        const QVector3D centerWorld = mapPoint(worldFromModel, centerModel);

        const float radius = node.Object.pickRadius();
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

QBox3D cwGeometryItersecter::primitiveWorldBox(const QList<Node>& nodes,
                                               const Primitive& prim)
{
    const Node& node = nodes[prim.nodeIndex];
    const cwGeometry& geometry = node.Object.geometry();
    const QMatrix4x4& worldFromModel = node.Object.modelMatrix();
    auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);

    if (prim.kind == Primitive::Kind::Triangle) {
        const QVector<uint32_t>& indices = geometry.indices();
        const QVector3D a = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 0)));
        const QVector3D b = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 1)));
        const QVector3D c = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(prim.primitiveIndex + 2)));
        return Node::triangleToBoundingBox(a, b, c);
    }

    // Point primitive — pad the vertex by pickRadius on each axis.
    const char* base = geometry.vertexBuffer(positionAttribute->bufferIndex)->constData()
                       + positionAttribute->byteOffsetInBuffer;
    const int stride = positionAttribute->bufferStride;
    const float* p = reinterpret_cast<const float*>(base + prim.primitiveIndex * stride);
    const QVector3D centerWorld = mapPoint(worldFromModel, QVector3D(p[0], p[1], p[2]));
    const float radius = node.Object.pickRadius();
    const QVector3D padVec(radius, radius, radius);
    return QBox3D(centerWorld - padVec, centerWorld + padVec);
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
            box.unite(primitiveWorldBox(ctx.nodes, ctx.prims[i].prim));
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
    const QMatrix4x4& worldFromModel = object.modelMatrix();
    const QVector<uint32_t>& indices = geometry.indices();

    for (uint32_t i = 0; i < chunk.count; ++i) {
        const uint32_t triIdx = chunk.inputBegin + i;
        const uint32_t indexBase = triIdx * 3;
        const QVector3D a = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 0)));
        const QVector3D b = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 1)));
        const QVector3D c = mapPoint(worldFromModel,
            geometry.value<QVector3D>(positionAttribute, indices.at(indexBase + 2)));

        BuildPrim& bp = prims[chunk.outBegin + i];
        bp.prim.kind = Primitive::Kind::Triangle;
        bp.prim.nodeIndex = chunk.nodeIndex;
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
    const QMatrix4x4& worldFromModel = object.modelMatrix();
    const char* base = geometry.vertexBuffer(positionAttribute->bufferIndex)->constData()
                       + positionAttribute->byteOffsetInBuffer;
    const int stride = positionAttribute->bufferStride;

    for (uint32_t i = 0; i < chunk.count; ++i) {
        const uint32_t vertIdx = chunk.inputBegin + i;
        const float* p = reinterpret_cast<const float*>(base + vertIdx * stride);
        const QVector3D centerWorld = mapPoint(worldFromModel,
                                               QVector3D(p[0], p[1], p[2]));

        BuildPrim& bp = prims[chunk.outBegin + i];
        bp.prim.kind = Primitive::Kind::Point;
        bp.prim.nodeIndex = chunk.nodeIndex;
        bp.prim.primitiveIndex = vertIdx;
        bp.centroid = centerWorld;
    }
}

void cwGeometryItersecter::scheduleBuild()
{
    // Drop the current BVH. Every caller of scheduleBuild() has just
    // mutated Nodes or a Node's modelMatrix, so the existing acceleration
    // is functionally wrong from this point on — keeping it alive only
    // delays freeing several GiB on large point clouds. Picks return
    // no-hit (handled by the null check in intersectsDetailed) until the
    // new build completes.
    m_bvh.reset();
    m_bvhRestarter.restart([this]() { return launchBuildJob(); });
}

QFuture<void> cwGeometryItersecter::launchBuildJob()
{
    // Snapshot Nodes on the caller (UI) thread. QList is implicitly shared,
    // so this is a cheap header copy that bumps the buffer refcount;
    // subsequent UI-side mutations branch to a fresh detach, leaving the
    // worker's view stable. Workers must use const access (.at(), const
    // ref) so they never trigger their own detach — concurrent detaches
    // on a shared buffer race and free memory still being read.
    auto snapshot = Nodes;

    if (lcPick().isDebugEnabled()) {
        int triNodes = 0;
        int lineNodes = 0;
        int pointNodes = 0;
        int otherNodes = 0;
        qsizetype totalPrims = 0;
        for (const Node& n : std::as_const(snapshot)) {
            const qsizetype primCount = countNodePrimitives(n.Object);
            totalPrims += primCount;
            switch (n.Object.geometry().type()) {
            case cwGeometry::Type::Triangles: ++triNodes; break;
            case cwGeometry::Type::Lines:     ++lineNodes; break;
            case cwGeometry::Type::Points:    ++pointNodes; break;
            default:                          ++otherNodes; break;
            }
        }
        qCDebug(lcPick).nospace()
            << "launchBuildJob snapshot: " << snapshot.size() << " source nodes"
            << " (Triangles=" << triNodes
            << " Lines=" << lineNodes
            << " Points=" << pointNodes
            << " other=" << otherNodes << ")"
            << " totalPrims=" << totalPrims;

        // Per-source-node breakdown, capped so a 10k-scrap project doesn't
        // blast 10k lines per rebuild.
        const int dumpCap = 32;
        const int dumpCount = std::min<int>(snapshot.size(), dumpCap);
        for (int i = 0; i < dumpCount; ++i) {
            const Node& n = snapshot.at(i);
            const cwRenderObject* parent = n.Object.parent();
            qCDebug(lcPick).nospace()
                << "  [" << i << "] " << formatKey(n.Object)
                << " type=" << cwGeometry::typeName(n.Object.geometry().type())
                << " prims=" << countNodePrimitives(n.Object)
                << " visible=" << (parent != nullptr ? parent->isVisible() : true)
                << " box=[" << n.BoundingBox.minimum()
                << " .. " << n.BoundingBox.maximum() << "]";
        }
        if (snapshot.size() > dumpCap) {
            qCDebug(lcPick).nospace()
                << "  ... " << (snapshot.size() - dumpCap) << " more source nodes elided";
        }
    }

    // Worker writes its produced BvhData into a slot that survives even if
    // `this` is destroyed mid-build. The .context(this) callback below
    // installs it into m_bvh on the UI thread, and Qt's automatic
    // disconnect on QObject destruction ensures we never touch `this` after
    // it goes away.
    auto resultSlot = std::make_shared<std::shared_ptr<BvhData>>();

    QFuture<void> worker = cwConcurrent::run([snapshot, resultSlot](QPromise<void>& promise) {
        // Count total primitives + plan Phase A chunks. Each EnumChunk maps
        // to a contiguous slice of the final BuildPrim vector so workers
        // can write without contention.
        QVector<EnumChunk> chunks;
        qsizetype totalPrims = 0;
        for (int n = 0; n < snapshot.size(); ++n) {
            const qsizetype primCount = countNodePrimitives(snapshot.at(n).Object);
            if (primCount <= 0) {
                continue;
            }
            qsizetype filled = 0;
            while (filled < primCount) {
                const qsizetype take = std::min<qsizetype>(primCount - filled, kEnumChunkSize);
                EnumChunk c;
                c.nodeIndex = static_cast<uint32_t>(n);
                c.inputBegin = static_cast<uint32_t>(filled);
                c.count = static_cast<uint32_t>(take);
                c.outBegin = static_cast<uint32_t>(totalPrims);
                chunks.append(c);
                filled += take;
                totalPrims += take;
            }
        }

        if (promise.isCanceled()) {
            return;
        }

        // Phase B-1 is otherwise silent for seconds on large clouds, so
        // reserve a slice of the bar for it. Each of its recursion levels
        // runs nth_element over ~totalPrims items in aggregate.
        const qsizetype phaseB1Budget = static_cast<qsizetype>(kParallelFanoutDepth) * totalPrims;
        const qsizetype totalProgress = totalPrims + phaseB1Budget + totalPrims;
        promise.setProgressRange(0, kProgressResolution);
        promise.setProgressValue(0);

        // Cancel guard right before the largest allocation in the worker.
        // A canceled worker that gets here would otherwise spend ~6 GiB on
        // a 269M-point cloud before the next isCanceled() check at line ~886.
        if (promise.isCanceled()) {
            return;
        }

        QVector<BuildPrim> prims;
        prims.resize(totalPrims);

        if (totalPrims > 0 && !chunks.isEmpty()) {
            std::atomic<qsizetype> primsDone{0};
            const ProgressScaler phaseAScaler{promise, /*base=*/0, totalProgress};

            // Workers push their own progress when they finish a chunk —
            // setProgressValue is thread-safe, so the orchestrator can
            // just block on waitForFinished() without polling.
            auto enumFuture = cwConcurrent::map(chunks, [&snapshot, &prims, &primsDone, &promise, &phaseAScaler](EnumChunk& chunk) {
                if (promise.isCanceled()) {
                    return;
                }
                const Node& node = snapshot.at(chunk.nodeIndex);
                const cwGeometry& geometry = node.Object.geometry();
                if (geometry.attribute(cwGeometry::Semantic::Position) == nullptr) {
                    return;
                }

                if (geometry.type() == cwGeometry::Type::Triangles) {
                    enumerateTrianglesChunk(node.Object, chunk, prims);
                } else if (geometry.type() == cwGeometry::Type::Points) {
                    enumeratePointsChunk(node.Object, chunk, prims);
                }

                const qsizetype done = primsDone.fetch_add(chunk.count, std::memory_order_relaxed) + chunk.count;
                phaseAScaler.report(done);
            });

            // Release our pool slot so mapped workers can't deadlock
            // against us on small machines, then block on the inner
            // future via its underlying condition variable.
            QThreadPool* pool = cwTask::threadPool();
            pool->releaseThread();
            enumFuture.waitForFinished();
            pool->reserveThread();
        }

        if (promise.isCanceled()) {
            return;
        }

        // Phase B: parallel top-down median split.
        //
        // 1. Serial split splits the root range until we hit
        //    kParallelFanoutDepth levels deep (or run out of primitives),
        //    pre-claiming a slot for every node it touches. Each terminal
        //    range gets recorded as a SubRange with a pre-claimed root
        //    slot for the parallel pass. Progress is reported per
        //    successful medianSplit (see splitProgress* fields below).
        //
        // 2. Parallel pass: cwConcurrent::map runs buildBvhSubtree on
        //    every SubRange. Subtree builders write to disjoint slot
        //    ranges (each fetch_add'd from the same atomic), so the
        //    pre-sized bvhNodes vector serves as a shared lock-free
        //    output buffer — no contention.
        //
        // 3. Bottom-up bbox: the serial pass left upper inner-node
        //    bboxes default. We walk the recorded upper-inner-node
        //    slots in reverse pre-order and union each one from its
        //    children's bboxes.
        //
        // 4. Truncate bvhNodes to the actual node count.
        QVector<BvhNode> bvhNodes;
        if (!prims.isEmpty()) {
            // Tight upper bound for median split with leaf threshold L:
            // a split where both halves are leaves makes each half ~L/2,
            // so worst-case leaf count is 2*ceil(N/L). Inner nodes add
            // (leaves - 1), giving 4*ceil(N/L) - 1 total. Add slack for
            // degenerate-centroid leaves that terminate early.
            const qsizetype upperBoundCount =
                4 * ((prims.size() + kBvhLeafSize - 1) / kBvhLeafSize) + 1;
            // Pre-size (not just reserve) so concurrent writers can
            // safely index into the vector. BvhNode is POD-ish; default
            // construction is cheap.
            bvhNodes.resize(upperBoundCount);

            std::atomic<uint32_t> nextNode{0};
            QVector<SubRange> subRanges;
            QVector<uint32_t> upperInnerNodes;

            SplitProgress splitProgress{
                ProgressScaler{promise, /*base=*/totalPrims, /*total=*/totalProgress},
                /*done=*/0};
            BuildContext serialCtx{snapshot, prims, bvhNodes, nextNode,
                                   /*leafPrimCounter=*/nullptr,
                                   /*splitProgress=*/&splitProgress};
            serialSplitToFanout(serialCtx, 0, prims.size(),
                                kParallelFanoutDepth,
                                subRanges, upperInnerNodes);

            if (promise.isCanceled()) {
                return;
            }

            if (!subRanges.isEmpty()) {
                std::atomic<qsizetype> phaseBPrims{0};
                BuildContext parallelCtx{snapshot, prims, bvhNodes, nextNode, &phaseBPrims};

                const ProgressScaler phaseB2Scaler{promise,
                                                   /*base=*/totalPrims + phaseB1Budget,
                                                   /*total=*/totalProgress};
                auto buildFuture = cwConcurrent::map(subRanges,
                    [&parallelCtx, &phaseBPrims, &promise, &phaseB2Scaler](SubRange& r) {
                        if (promise.isCanceled()) {
                            return;
                        }
                        buildBvhSubtree(parallelCtx, r.begin, r.end, r.rootSlot);
                        // Push progress once per subtree: ~16 updates across
                        // Phase B-2, which is plenty for a visibly-moving bar
                        // and avoids serializing on the QPromise mutex from
                        // deep inside the recursion.
                        phaseB2Scaler.report(phaseBPrims.load(std::memory_order_relaxed));
                    });

                QThreadPool* pool = cwTask::threadPool();
                pool->releaseThread();
                buildFuture.waitForFinished();
                pool->reserveThread();
            }

            // Bottom-up bbox pass. upperInnerNodes is in pre-order, so
            // reverse iteration visits children before parents (parent's
            // selfIndex was fetch_add'd before its children's).
            for (int i = upperInnerNodes.size() - 1; i >= 0; --i) {
                const uint32_t slot = upperInnerNodes[i];
                BvhNode& self = bvhNodes[slot];
                QBox3D box = bvhNodes[self.left].bbox;
                box.unite(bvhNodes[self.right].bbox);
                self.bbox = box;
            }

            bvhNodes.resize(nextNode.load(std::memory_order_relaxed));
            // The pre-size at line ~923 uses a loose upper bound
            // (4*ceil(N/L) + 1); the actual count is typically ~half that.
            // Without squeeze, the unused capacity (~1.5 GiB on a 269M-point
            // cloud) stays allocated for the lifetime of this BvhData.
            bvhNodes.squeeze();
        }

        if (promise.isCanceled()) {
            return;
        }

        // Extract Primitives from BuildPrims into a fresh, tight vector,
        // then release the BuildPrim temporary explicitly. This bounds the
        // window where both vectors coexist to just the copy loop instead
        // of holding the 6 GiB BuildPrim buffer through the BvhData
        // assembly + resultSlot handoff.
        QVector<Primitive> finalPrims;
        finalPrims.resize(prims.size());
        for (qsizetype i = 0; i < prims.size(); ++i) {
            finalPrims[i] = prims[i].prim;
        }
        prims.clear();
        prims.squeeze();

        auto out = std::make_shared<BvhData>();
        out->nodesSnapshot = std::move(snapshot);
        out->bvhNodes = std::move(bvhNodes);
        out->primitives = std::move(finalPrims);
        for (const Node& n : out->nodesSnapshot) {
            out->maxPickRadius = std::max(out->maxPickRadius, n.Object.pickRadius());
        }
        *resultSlot = std::move(out);
    });

    // Install the freshly built BVH on the UI thread. .context(this, ...)
    // is auto-disconnected by Qt when `this` is destroyed, so the lambda
    // never fires after destruction.
    AsyncFuture::observe(worker).context(this, [this, resultSlot]() {
        if (*resultSlot) {
            m_bvh = *resultSlot;
            qCDebug(lcPick).nospace()
                << "bvhReady installed: bvhNodes=" << m_bvh->bvhNodes.size()
                << " primitives=" << m_bvh->primitives.size()
                << " sourceNodes=" << m_bvh->nodesSnapshot.size();
            emit bvhReady();
        } else {
            qCDebug(lcPick) << "build worker finished without producing a BvhData"
                            << "(canceled or zero prims)";
        }
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


cwGeometryItersecter::Node::Node(const cwGeometryItersecter::Object &object, int indexInIndexes) :
    Object(object)
{

    switch(object.geometry().type()) {
    case cwGeometry::Type::Triangles:
        BoundingBox = triangleToBoundingBox(object, indexInIndexes);
        break;
    case cwGeometry::Type::Lines:
        BoundingBox = lineToBoundingBox(object, indexInIndexes);
        break;
    default:
        break;
    }

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
QBox3D cwGeometryItersecter::Node::triangleToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes)
{
    auto positionAttribute = object.geometry().attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);
    QVector3D p1 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes));
    QVector3D p2 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes + 1));
    QVector3D p3 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes + 2));

    return triangleToBoundingBox(p1, p2, p3);
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

/**
 * @brief cwGeometryItersecter::Node::lineToBoundingBox
 * @return Creates a bounding box around a line
 */
QBox3D cwGeometryItersecter::Node::lineToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes)
{
    auto positionAttribute = object.geometry().attribute(cwGeometry::Semantic::Position);
    Q_ASSERT(positionAttribute);
    QVector3D p1 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes));
    QVector3D p2 = object.geometry().value<QVector3D>(positionAttribute, object.geometry().indices().at(indexInIndexes + 1));

    QVector3D maxPoint(qMax(p1.x(), p2.x()),
                       qMax(p1.y(), p2.y()),
                       qMax(p1.z(), p2.z()));

    QVector3D minPoint(qMin(p1.x(), p2.x()),
                       qMin(p1.y(), p2.y()),
                       qMin(p1.z(), p2.z()));

    return QBox3D(minPoint, maxPoint);
}
