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

    // BVH primitive handles + node layout — internal build / traversal
    // state.
    struct Primitive {
        enum class Kind : uint8_t { Triangle, Point };
        Kind kind = Kind::Triangle;
        uint32_t nodeIndex = 0;       // index into the build-time Nodes snapshot
        uint32_t primitiveIndex = 0;  // triangle: first index into indices(); point: vertex index
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

    // Phase A work-item: a contiguous range of primitives belonging to
    // one source Node, planned by launchBuildJob and consumed by parallel
    // workers. Splitting a single huge Node into multiple chunks is what
    // gives us parallelism for monolithic point clouds.
    struct EnumChunk {
        uint32_t nodeIndex = 0;
        // For Triangles: index of the first triangle (always a multiple
        // of 3 when multiplied by 3 in geometry().indices()).
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

    // Snapshot of Nodes captured at build-job launch and consumed by the
    // worker thread. QList is implicitly shared, so this is a header copy
    // that keeps the buffer alive for traversal even after a later rebuild
    // swaps in a different snapshot via m_bvh.
    struct BvhData {
        QList<Node> nodesSnapshot;
        QVector<BvhNode> bvhNodes;
        QVector<Primitive> primitives;
    };

    // Live BVH. nullptr until the first async build completes. Only
    // accessed from the main thread: pick queries (intersects/
    // intersectsDetailed) and the .context() install callback both run
    // on the thread owning `this`, and scheduleBuild() (which clears
    // m_bvh) is invoked from the same main-thread mutators that touch
    // Nodes. The worker thread never touches m_bvh directly — it writes
    // into a side channel (resultSlot, see launchBuildJob) that the
    // .context() callback drains on the main thread.
    std::shared_ptr<BvhData> m_bvh;

    // Coalesces rapid mutations into a single rebuild and cancels the
    // in-flight build when a new mutation arrives.
    AsyncFuture::Restarter<void> m_bvhRestarter;
    cwFutureManagerToken m_futureManagerToken;

    // BuildPrim, SplitProgress, and BuildContext are defined in the .cpp;
    // declared here so recursive helpers can take them by reference.
    struct BuildPrim;
    struct SplitProgress;
    struct BuildContext;

    void scheduleBuild();
    QFuture<void> launchBuildJob();

    static QBox3D primitiveWorldBox(const QList<Node>& nodes, const Primitive& prim);
    static void testPrimitive(const QList<Node>& nodes,
                              const Primitive& prim,
                              const QRay3D& ray,
                              cwRayHit& best);

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

    double nearestNeighbor(const QRay3D& ray) const;

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
        const QVector3D originModel    = mapPoint(modelToWorld,    rayWorld.origin());
        const QVector3D directionModel = mapDirection(modelToWorld, rayWorld.direction());
        return QRay3D(originModel, directionModel);
    }

    static inline QVector3D transformNormalToWorld(const QMatrix4x4& worldFromModel,
                                                   const QVector3D& nModel)
    {
        const QMatrix4x4 modelFromWorld = worldFromModel.inverted();
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

#endif // CWGEOMETRYITERSECTER_H
