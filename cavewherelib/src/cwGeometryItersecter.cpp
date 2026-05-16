/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwDebug.h"
#include "cwGeometryItersecter.h"
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
#include <QSphere3D>
#include <QThreadPool>
#include <QVarLengthArray>

namespace {
    // Multiplier applied to a point's pickRadius when expanding the cloud's
    // broad-phase AABB so that rays passing tangentially through the
    // outermost spheres aren't rejected by the box test.
    constexpr float kPointAabbPadScale = 1.0f;

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
 * @return Closes intersection to on the ray, or if no match, use nearest neighbor search
 */
double cwGeometryItersecter::intersects(const QRay3D &ray) const
{
    const cwRayHit hit = intersectsDetailed(ray);
    if (hit.hit()) {
        return hit.tWorld();
    }
    return nearestNeighbor(ray);
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

    for(int i = 0; i < object.geometry().indices().size(); i+=2) {
        Node node = Node(object, i);

        //Make sure the node is valid
        if(!node.BoundingBox.isNull()) {
            Nodes.append(node);
        }
    }
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
    scheduleBuild();
}


/**
 * @brief cwGeometryItersecter::nearestNeighbor
 * @param ray
 * @return Finds the point on the ray that's the nearest neigbor of the ray
 */
double cwGeometryItersecter::nearestNeighbor(const QRay3D &ray) const
{
    std::array<QVector3D, 8> points;

    double bestT = 0.0;
    double bestDistance = std::numeric_limits<double>::max();

    for(const Node& node : Nodes) {
        if (!isPickable(node.Object)) {
            continue;
        }

        if(node.Object.geometry().type() == cwGeometry::Type::Triangles) {
            continue;
        }

        QVector3D min = node.BoundingBox.minimum();
        QVector3D max = node.BoundingBox.maximum();

        points[0] = min;
        points[1] = QVector3D(max.x(), min.y(), min.z());
        points[2] = QVector3D(max.x(), max.y(), min.z());
        points[3] = QVector3D(min.x(), max.y(), min.z());
        points[4] = QVector3D(min.x(), min.y(), max.z());
        points[5] = QVector3D(max.x(), min.y(), max.z());
        points[6] = QVector3D(min.x(), max.y(), max.z());
        points[7] = max;

        for(const QVector3D& point : points) {
            double distance = ray.distance(point);
            if(distance < bestDistance) {
                double t = ray.projectedDistance(point);
                if(t > 0.0) {
                    bestDistance = distance;
                    bestT = t;
                }
            }
        }
    }

    if(bestT == 0.0) {
        return qSNaN();
    }

    return bestT;
}

cwRayHit cwGeometryItersecter::intersectsDetailed(const QRay3D &ray) const
{
    cwRayHit best;

    // Snapshot the current BVH so a concurrent build can swap m_bvh
    // mid-traversal without invalidating our pointers.
    std::shared_ptr<BvhData> bvh = std::atomic_load(&m_bvh);
    if (!bvh || bvh->bvhNodes.isEmpty()) {
        return best;
    }

    const QVector<BvhNode>& nodes = bvh->bvhNodes;
    const QVector<Primitive>& prims = bvh->primitives;
    const QList<Node>& nodeSnapshot = bvh->nodesSnapshot;

    // Stack-based traversal with near-first child ordering. 64 is plenty for
    // realistic BVH depths (~log2 of primitive count, well under 30 for 1M).
    QVarLengthArray<uint32_t, 64> stack;
    stack.append(0);

    while (!stack.isEmpty()) {
        const uint32_t idx = stack.last();
        stack.removeLast();

        const BvhNode& bn = nodes[idx];

        // Box reject: skip if ray misses the AABB or the entry t is already
        // farther than our current best hit (closest-wins early exit).
        const double tBox = bn.bbox.intersection(ray);
        if (qIsNaN(tBox)) {
            continue;
        }
        if (best.hit() && tBox >= best.tWorld()) {
            continue;
        }

        if (bn.isLeaf) {
            const uint32_t first = bn.left;
            const uint32_t count = bn.right;
            for (uint32_t p = first; p < first + count; ++p) {
                testPrimitive(nodeSnapshot, prims[p], ray, best);
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

    return best;
}

void cwGeometryItersecter::testPrimitive(const QList<Node>& nodes,
                                         const Primitive& prim,
                                         const QRay3D& ray,
                                         cwRayHit& best)
{
    const Node& node = nodes[prim.nodeIndex];
    if (!isPickable(node.Object)) {
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
            return;
        }

        const QVector3D pWorld = mapPoint(worldFromModel, local.pointModel());
        const QVector3D nWorld = transformNormalToWorld(worldFromModel, local.normalModel());
        const double tWorld = ray.projectedDistance(pWorld);

        if (tWorld > 0.0 && (!best.hit() || tWorld < best.tWorld())) {
            best = local;
            best.m_hit = true;
            best.m_pointWorld = pWorld;
            best.m_normalWorld = nWorld;
            best.m_tWorld = tWorld;
            best.m_object = node.Object.parent();
            best.m_objectId = node.Object.id();
            best.m_firstIndex = i;
        }
        return;
    }

    // Point primitive — ray-vs-sphere using the Object's pickRadius.
    const float radius = node.Object.pickRadius();
    if (radius <= 0.0f) {
        return;
    }

    const char* base = geometry.vertexBuffer(positionAttribute->bufferIndex)->constData()
                       + positionAttribute->byteOffsetInBuffer;
    const int stride = positionAttribute->bufferStride;
    const float* p = reinterpret_cast<const float*>(base + prim.primitiveIndex * stride);
    const QVector3D center(p[0], p[1], p[2]);

    float tNear = 0.0f;
    float tFar = 0.0f;
    if (!QSphere3D(center, radius).intersection(rayModel, &tNear, &tFar)) {
        return;
    }
    if (tNear <= 0.0f) {
        // Ray points away from sphere, or origin sits inside it.
        return;
    }

    const QVector3D pWorld = mapPoint(worldFromModel, center);
    const double tWorld = ray.projectedDistance(pWorld);
    if (tWorld <= 0.0 || (best.hit() && tWorld >= best.tWorld())) {
        return;
    }

    best.m_hit = true;
    best.m_tModel = tNear;
    best.m_u = std::numeric_limits<float>::quiet_NaN();
    best.m_v = std::numeric_limits<float>::quiet_NaN();
    best.m_pointModel = center;
    best.m_normalModel = -rayModel.direction().normalized();
    best.m_pointWorld = pWorld;
    best.m_normalWorld = -ray.direction().normalized();
    best.m_tWorld = tWorld;
    best.m_object = node.Object.parent();
    best.m_objectId = node.Object.id();
    best.m_firstIndex = static_cast<int>(prim.primitiveIndex);
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
        }

        if (promise.isCanceled()) {
            return;
        }

        auto out = std::make_shared<BvhData>();
        out->nodesSnapshot = std::move(snapshot);
        out->bvhNodes = std::move(bvhNodes);
        out->primitives.resize(prims.size());
        for (qsizetype i = 0; i < prims.size(); ++i) {
            out->primitives[i] = prims[i].prim;
        }
        *resultSlot = std::move(out);
    });

    // Install the freshly built BVH on the UI thread. .context(this, ...)
    // is auto-disconnected by Qt when `this` is destroyed, so the lambda
    // never fires after destruction.
    AsyncFuture::observe(worker).context(this, [this, resultSlot]() {
        if (*resultSlot) {
            std::atomic_store(&m_bvh, *resultSlot);
            emit bvhReady();
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
