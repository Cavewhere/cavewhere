/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGEOMETRYITERSECTER_H
#define CWGEOMETRYITERSECTER_H

//Qt includes
#include <QObject>
#include <QVector>
#include <QVector3D>
#include <QRay3D>
#include <QBox3D>
#include <QHash>
#include <QSet>
#include <atomic>
#include <cstdint>
#include <memory>

//Our includes
#include "cwRayHit.h"
#include "cwGeometry.h"
#include "cwFutureManagerToken.h"
#include "asyncfuture.h"
#include "CaveWhereLibExport.h"
class cwRenderObject;

class CAVEWHERE_LIB_EXPORT cwGeometryItersecter : public QObject
{
    Q_OBJECT

public:
    struct Key {
        cwRenderObject* parentObject = nullptr;
        uint64_t id = 0;

        bool operator==(const Key& other) const noexcept {
            return parentObject == other.parentObject && id == other.id;
        }
    };

    class Object {
    public:
        Object() { }

        // pickRadius is the world-space sphere radius around each vertex used
        // for ray-vs-point intersection. Only consulted when geometry.type()
        // == cwGeometry::Type::Points.
        Object(Key key,
               cwGeometry geometry,
               QMatrix4x4 modelMatrix = QMatrix4x4(),
               float pickRadius = 0.0f) :
            m_key(key),
            m_geometry(geometry),
            m_modelMatrix(modelMatrix),
            m_pickRadius(pickRadius)
        {}

        Key key() const { return m_key; }
        const cwGeometry& geometry() const { return m_geometry; }

        cwRenderObject* parent() const { return m_key.parentObject; }
        uint64_t id() const { return m_key.id; }

        void setModelMatrix(const QMatrix4x4 matrix) { m_modelMatrix = matrix; }
        const QMatrix4x4& modelMatrix() const { return m_modelMatrix; }

        float pickRadius() const { return m_pickRadius; }

    private:

        Key m_key;
        cwGeometry m_geometry;
        QMatrix4x4 m_modelMatrix;
        float m_pickRadius = 0.0f;
    };

    explicit cwGeometryItersecter(QObject* parent = nullptr);
    ~cwGeometryItersecter() override = default;

    void addObject(const cwGeometryItersecter::Object& object);
    void clear(cwRenderObject* parentObject = nullptr);
    void removeObject(cwRenderObject* parentObject, uint64_t id);
    void removeObject(const Key& objectKey);

    void setModelMatrix(cwRenderObject* parentObject, uint64_t id, const QMatrix4x4& modelMatrix);
    void setModelMatrix(const Key& objectKey, const QMatrix4x4& modelMatrix);

    QBox3D boundingBox(const Key& objectKey) const;

    double intersects(const QRay3D& ray) const;
    cwRayHit intersectsDetailed(const QRay3D& ray) const;

    // Escape hatch for the BVH-tube fallback that snaps to a sphere-miss
    // within kTubeFactor * pickRadius of the ray. Disabling reverts to
    // strict sphere intersection — picks return no-hit when the cursor
    // sits in a sub-pixel gap, and the AABB box tests stop being
    // inflated, recovering the pre-tube traversal cost on huge clouds.
    void setTubePickEnabled(bool enabled) { m_tubePickEnabled = enabled; }
    bool isTubePickEnabled() const { return m_tubePickEnabled; }

    // Debug-only snapshot of what the picker currently knows about. Reads
    // the built BVH's source-node snapshot if available (post-bvhReady);
    // falls back to the live Nodes list otherwise. Use from tests and the
    // cw.picking log handler; not meant for production hot paths.
    struct DebugStatistics {
        bool hasBvh = false;             // true if read from m_bvh, false from Nodes
        qsizetype sourceNodeCount = 0;   // total source nodes (one per addObject call)
        qsizetype triangleSourceNodes = 0;
        qsizetype lineSourceNodes = 0;
        qsizetype pointSourceNodes = 0;
        qsizetype totalPrimitives = 0;   // sum of countNodePrimitives across source nodes
        qsizetype bvhNodeCount = 0;      // BvhNode count if bvh built, else 0
    };
    DebugStatistics debugStatistics() const;

    // Wired by cwRootData so the async BVH build shows up as a job in the
    // task panel. No-op until called.
    void setFutureManagerToken(cwFutureManagerToken token);

    // Test-only: spin the event loop until the most recently scheduled
    // build has completed (or returns immediately if no build is in
    // flight). Mirrors cwProject::waitForFinish() / cwScrapManager::
    // waitForFinish() — production code should listen for the bvhReady()
    // signal instead, because waitForFinished spins a nested event loop.
    void waitForFinish();

signals:
    // Emitted on the UI thread each time a fresh BVH atomically replaces
    // the previous one. Picks made before this fires return no-hit.
    void bvhReady();

private:

    class Node {
    public:
        Node();
        Node(const cwGeometryItersecter::Object& object, int indexInIndexes);
        Node(const QBox3D boundingBox, const cwGeometryItersecter::Object& object);

        QBox3D BoundingBox;
        cwGeometryItersecter::Object Object;

        static QBox3D triangleToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes);
        static QBox3D triangleToBoundingBox(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3);
        static QBox3D lineToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes);
    };

    // BVH primitive handle. One slot per triangle (primitiveIndex is
    // the first index into indices()) or per point (primitiveIndex is
    // the vertex index). The owning SubBvh's `object` field provides the
    // geometry context.
    struct Primitive {
        enum class Kind : uint8_t { Triangle, Point };
        Kind kind = Kind::Triangle;
        uint32_t primitiveIndex = 0;
    };

    struct BvhNode {
        QBox3D bbox;
        // Inner: left = leftChild, right = rightChild. Leaf: left = firstPrim, right = primitiveCount.
        uint32_t left = 0;
        uint32_t right = 0;
        uint8_t splitAxis = 0;
        bool isLeaf = false;
    };

    // SubRange records a terminal range from the serial split that the
    // parallel pass picks up. rootSlot is pre-claimed during serial split.
    struct SubRange {
        uint32_t rootSlot;
        qsizetype begin;
        qsizetype end;
    };

    // Phase A work-item: a contiguous range of primitives. Splitting one
    // big Object into multiple chunks is what gives us parallelism for
    // monolithic point clouds.
    struct EnumChunk {
        // For Triangles: index of the first triangle (multiplied by 3
        // when indexing into geometry().indices()).
        // For Points: starting vertex index.
        uint32_t inputBegin = 0;
        uint32_t count = 0;       // primitives, not indices/floats
        uint32_t outBegin = 0;    // first slot in the shared BuildPrim array
    };

    // Result of median-axis selection. axis < 0 means the centroid box is
    // degenerate on every axis and the range can't be subdivided.
    struct MedianSplitResult {
        qsizetype mid;
        int axis;
    };

    QList<Node> Nodes;

    // Per-Object acceleration structure built once in **model space** and
    // cached across rebuilds. Defined in the .cpp because callers only need
    // shared_ptr<const SubBvh> here.
    //
    // Model-space layout lets setModelMatrix() avoid invalidating the
    // sub-BVH entirely: a model-matrix change refreshes only the top-level
    // bbox (the snapshot of modelMatrices in BvhData). A geometry replace
    // for one Object invalidates only that one entry.
    struct SubBvh;

    // Snapshot of Nodes captured at build-job launch and consumed by the
    // worker thread. QList is implicitly shared, so this is a header copy
    // that keeps the buffer alive for traversal even after a later rebuild
    // swaps in a different snapshot via m_bvh.
    //
    // Two-level layout: topLevel is a small world-space BVH whose leaves
    // index into subBvhs / modelMatrices (parallel arrays). Each sub-BVH
    // is model-space; traversal transforms the world ray to model space
    // once per Object root.
    //
    // Single-owner invariant: m_bvh is the only shared_ptr<BvhData> that
    // exists at steady state — invalidatePublishedSlot mutates subBvhs
    // via non-const operator[] and relies on refcount==1 to avoid a
    // QVector detach (deep copy of the whole parallel-arrays buffer).
    // Don't snapshot a BvhData; snapshot individual SubBvhs by their
    // shared_ptr instead.
    struct BvhData {
        QList<Node> nodesSnapshot;
        QVector<BvhNode> topLevel;
        // Parallel arrays — one slot per source Node that contributed to
        // the BVH. topLevel leaves store indices into these.
        QVector<std::shared_ptr<const SubBvh>> subBvhs;
        QVector<QMatrix4x4> modelMatrices;
        // Precomputed inverse of each modelMatrices entry — avoids paying
        // QMatrix4x4::inverted() per top-level-leaf pick in the hot path.
        QVector<QMatrix4x4> inverseModelMatrices;
        // Max pickRadius across nodesSnapshot — cached once at build time
        // so the per-pick tube-box test doesn't have to rescan. Triangles
        // and zero-radius Points contribute 0; harmless for those paths
        // since they don't take the tube fallback.
        float maxPickRadius = 0.0f;
        // Reverse index: Key → slot in the parallel arrays above. Lets a
        // mutator null out the dirty Key's sub-BVH in the published BVH so
        // it stops contributing to picks the instant its cache entry is
        // invalidated — without dropping the rest of the BVH while a
        // rebuild is in flight. A null entry in subBvhs is the in-flight
        // marker; traversal skips it. See invalidatePublishedSlot().
        QHash<Key, int> keyToSlot;
    };

    bool m_tubePickEnabled = true;

    // Live BVH. nullptr until the first async build completes. Only
    // accessed from the main thread: pick queries (intersects/
    // intersectsDetailed) and the .context() install callback both run
    // on the thread owning `this`, and scheduleBuild() (which clears
    // m_bvh) is invoked from the same main-thread mutators that touch
    // Nodes. The worker thread never touches m_bvh directly — it writes
    // into a side channel (resultSlot, see launchBuildJob) that the
    // .context() callback drains on the main thread.
    std::shared_ptr<BvhData> m_bvh;

    // Cache of per-Object sub-BVHs, keyed by source Node identity. Kept
    // alive across rebuilds — only entries whose geometry actually changed
    // are evicted via m_dirtyKeys. Worker threads read from a shallow QHash
    // copy snapshot; UI-thread mutators are the only writers.
    QHash<Key, std::shared_ptr<const SubBvh>> m_subBvhs;

    // Keys whose cached sub-BVH was invalidated since the last snapshot —
    // any rebuild job must produce fresh sub-BVHs for these even if the
    // QHash entry happened to still exist (e.g. add → schedule → add again
    // for the same Key before the first build observed it).
    QSet<Key> m_dirtyKeys;

    // Coalesces rapid mutations into a single rebuild and cancels the
    // in-flight build when a new mutation arrives.
    AsyncFuture::Restarter<void> m_bvhRestarter;
    cwFutureManagerToken m_futureManagerToken;

    // BuildPrim, SplitProgress, and BuildContext are defined in the .cpp;
    // declared here so recursive helpers can take them by reference.
    struct BuildPrim;
    struct SplitProgress;
    struct BuildContext;

    // Per-pick rejection counters used by the cw.picking logging category.
    // Defined in the .cpp because it's purely a debug aid.
    struct PickStats;
    friend QDebug operator<<(QDebug d, const cwGeometryItersecter::PickStats& s);

    // Tube-pick fallback: tracks the closest sphere-miss seen during a
    // traversal so intersectsDetailed can snap to a point the user almost
    // clicked. Defined in the .cpp because it's an internal traversal
    // detail.
    struct NearMissResult;

    // Invalidate the cached sub-BVH for one Object and schedule a rebuild.
    // Use for addObject (which always replaces) and any mutation that
    // changed the Object's geometry. The schedule call is coalesced via
    // m_bvhRestarter so back-to-back invalidations don't queue extra work.
    void scheduleObjectRebuild(const Key& key);

    // Schedule a rebuild that touches only the top-level BVH; cached
    // sub-BVHs are reused unchanged. Use for setModelMatrix (which leaves
    // model-space geometry intact) and removeObject (which only shrinks
    // the set of objects).
    void scheduleTopLevelRebuild();

    // Release the published sub-BVH for one Key while a rebuild is in
    // flight. Leaves the rest of m_bvh intact so unrelated Objects keep
    // serving picks — without holding a second copy of the dirty Key's
    // sub-BVH alongside the worker's new one (critical when that sub-BVH
    // is a multi-GB point cloud). No-op if m_bvh is null or doesn't
    // contain key. Safe to call from the main thread alongside picks.
    void invalidatePublishedSlot(const Key& key);

    QFuture<void> launchBuildJob();

    // Build a model-space sub-BVH for one Object using the existing
    // serialSplitToFanout + parallel buildBvhSubtree pipeline. Returns
    // nullptr for Objects that contribute no primitives.
    static std::shared_ptr<SubBvh> buildSubBvh(const Object& object,
                                               QPromise<void>& promise);

    // Build the world-space top-level BVH over the per-Object root boxes.
    // Sequenced after all sub-BVHs are present.
    static QVector<BvhNode> buildTopLevel(const QVector<QBox3D>& worldBoxes);

    // Release this thread's pool slot, block on the inner future, then
    // re-acquire. Prevents nested cwConcurrent::map from deadlocking on
    // small machines where the outer worker holds the only thread.
    template <typename Future>
    static void waitOnPool(Future& future);

    // Per-primitive bounding box in the Object's **model space**. Used
    // when constructing leaf bboxes for a sub-BVH.
    static QBox3D primitiveModelBox(const Object& object, const Primitive& prim);
    // Per-primitive ray test. rayModel/worldFromModel/modelToWorld are
    // computed once per top-level leaf by intersectsDetailed and passed
    // through so this hot-path function never re-inverts the matrix.
    // `object` comes from the SubBvh — decoupling from BvhData::
    // nodesSnapshot ordering, which is not stable across rebuilds.
    static void testPrimitive(const Object& object,
                              const Primitive& prim,
                              const QRay3D& ray,
                              const QRay3D& rayModel,
                              const QMatrix4x4& worldFromModel,
                              const QMatrix4x4& modelToWorld,
                              cwRayHit& best,
                              NearMissResult* nearMiss,
                              PickStats* stats);

    // Shared cwRayHit field fill for any Point primitive — used by the
    // sphere-hit path in testPrimitive and the tube-pick promote in
    // tryPromoteNearMiss. Pre-computed by the caller because the two
    // paths source tModel and the world point differently.
    static void fillPointHit(cwRayHit& best,
                             const Object& object,
                             const Primitive& prim,
                             const QRay3D& ray,
                             const QRay3D& rayModel,
                             const QVector3D& centerModel,
                             const QVector3D& centerWorld,
                             double tModel,
                             double tWorld);

    // Post-traversal tube-pick promote. Snaps best to the closest tracked
    // sphere-miss whose perpendicular distance is within kTubeFactor *
    // pickRadius. No-op when best already hit, nearMiss is empty, or the
    // miss is beyond the tube.
    static void tryPromoteNearMiss(cwRayHit& best,
                                   const NearMissResult& nearMiss,
                                   const QRay3D& ray,
                                   bool debug);

    // Debug-only per-primitive diagnostic; gated on lcPick debug.
    static void dumpLeafPrimitive(const Object& object,
                                  const Primitive& prim,
                                  const QRay3D& ray,
                                  uint32_t leafIdx,
                                  uint32_t localIdx);

    // Phase A: how many BVH primitives a single Node contributes. Lines
    // stay out of the BVH and contribute zero. Points contribute zero
    // unless pickRadius > 0.
    static qsizetype countNodePrimitives(const Object& object);

    // Phase A workers: fill BuildPrim slots for one chunk of a Triangles
    // or Points-typed Node, respectively.
    static void enumerateTrianglesChunk(const Object& object,
                                        const EnumChunk& chunk,
                                        QVector<BuildPrim>& prims);
    static void enumeratePointsChunk(const Object& object,
                                     const EnumChunk& chunk,
                                     QVector<BuildPrim>& prims);

    // Phase B: compute the centroid AABB, pick the longest axis, and
    // partition prims by median centroid along that axis. Returns
    // {-1, 0} when the centroid box is degenerate on every axis.
    static MedianSplitResult medianSplit(QVector<BuildPrim>& prims,
                                         qsizetype begin,
                                         qsizetype end);

    // Top-down split that allocates upper inner nodes serially and stops at
    // depthLeft == 0 (or when the range is leaf-sized / degenerate),
    // recording each terminal range as a SubRange for parallel processing.
    // Upper inner-node slots get their left/right/splitAxis set here; their
    // bboxes stay default and are filled by the bottom-up pass after the
    // parallel subtree builders finish. outUpperInnerNodes captures upper
    // inner-node slots in pre-order so the bottom-up pass can iterate
    // children-before-parents in reverse.
    static uint32_t serialSplitToFanout(BuildContext& ctx,
                                        qsizetype begin,
                                        qsizetype end,
                                        int depthLeft,
                                        QVector<SubRange>& outSubRanges,
                                        QVector<uint32_t>& outUpperInnerNodes);

    // Fills the subtree rooted at selfIndex via sequential top-down median
    // split, claiming child slots via ctx.nextNode.fetch_add. Safe to call
    // concurrently on disjoint subranges: outNodes is pre-sized to its
    // upper bound (no reallocations during the parallel pass) and each
    // call writes only to slots it itself claims.
    static void buildBvhSubtree(BuildContext& ctx,
                                qsizetype begin,
                                qsizetype end,
                                uint32_t selfIndex);

    template <typename Iterator>
    Iterator findNodeImpl(Iterator begin, Iterator end, const Key& objectKey) const {
        return std::find_if(begin, end, [&](const Node& node) {
            return node.Object.parent() == objectKey.parentObject &&
                   node.Object.id() == objectKey.id;
        });
    }

    auto findNode(const Key& objectKey) {
        return findNodeImpl(Nodes.begin(), Nodes.end(), objectKey);
    }

    auto findNode(const Key& objectKey) const {
        return findNodeImpl(Nodes.cbegin(), Nodes.cend(), objectKey);
    }

    void addTriangles(const cwGeometryItersecter::Object& object);
    void addLines(const cwGeometryItersecter::Object& object);
    void addPoints(const cwGeometryItersecter::Object& object);

    // Erase the Node matching `key` from Nodes plus its cached sub-BVH and
    // dirty mark, without scheduling. Returns true if a Node was erased.
    // Used by add* (which then re-appends and schedules an object rebuild)
    // and removeObject (which schedules a top-level rebuild).
    bool eraseNodeIfPresent(const Key& key);

    // Helpers: transform point (w=1) and direction (w=0)
    static inline QVector3D mapPoint(const QMatrix4x4& m, const QVector3D& p) {
        const QVector4D hp = m * QVector4D(p, 1.0f);
        return (hp.w() != 0.0f) ? (hp.toVector3D() / hp.w()) : hp.toVector3D();
    }

    static inline QVector3D mapDirection(const QMatrix4x4& m, const QVector3D& v) {
        const QVector4D hv = m * QVector4D(v, 0.0f);
        return hv.toVector3D();
    }

    static inline QRay3D transformRayToModel(const QMatrix4x4& modelMatrix,
                                             const QRay3D& rayWorld)
    {
        const QMatrix4x4 modelToWorld = modelMatrix.inverted();
        return transformRayWithInverse(modelToWorld, rayWorld);
    }

    // Same as transformRayToModel but takes the already-inverted matrix.
    // Used in the pick hot path where BvhData caches inverses up front.
    static inline QRay3D transformRayWithInverse(const QMatrix4x4& modelToWorld,
                                                 const QRay3D& rayWorld)
    {
        const QVector3D originModel    = mapPoint(modelToWorld,    rayWorld.origin());
        const QVector3D directionModel = mapDirection(modelToWorld, rayWorld.direction());
        return QRay3D(originModel, directionModel);
    }

    static inline QVector3D transformNormalToWorld(const QMatrix4x4& worldFromModel,
                                                   const QVector3D& nModel)
    {
        const QMatrix4x4 modelFromWorld = worldFromModel.inverted();
        return transformNormalWithInverse(modelFromWorld, nModel);
    }

    // Variant that takes the pre-inverted matrix — avoids the QMatrix4x4::inverted()
    // call inside the pick hot path when BvhData has it cached.
    static inline QVector3D transformNormalWithInverse(const QMatrix4x4& modelFromWorld,
                                                       const QVector3D& nModel)
    {
        const QMatrix4x4 normalXform = modelFromWorld.transposed();
        const QVector4D hn = normalXform * QVector4D(nModel, 0.0f);
        return hn.toVector3D().normalized();
    }

    static cwRayHit rayTriangleMT(const QRay3D& rayModel,
                                  const QVector3D& a,
                                  const QVector3D& b,
                                  const QVector3D& c,
                                  bool cullBackfaces);
};

inline uint32_t qHash(const cwGeometryItersecter::Object& object) {
    return object.id();
}

inline size_t qHash(const cwGeometryItersecter::Key& key, size_t seed = 0) noexcept {
    return qHashMulti(seed,
                      reinterpret_cast<quintptr>(key.parentObject),
                      key.id);
}

#endif // CWGEOMETRYITERSECTER_H
