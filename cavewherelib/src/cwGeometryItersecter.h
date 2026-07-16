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
#include <cstdint>
#include <memory>
#include <optional>

//Our includes
#include "cwRayHit.h"
#include "cwPickQuery.h"
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

        // Per-vertex visibility mask, one byte per vertex (0 = hidden,
        // nonzero = visible). Empty = all visible — the fast path every
        // object except a partially hidden line plot stays on. Indexed by
        // geometry vertex index, so replacing the geometry (addObject)
        // resets it; callers re-apply ranges afterwards. Mutated only via
        // cwGeometryItersecter::setRangeVisible.
        const QVector<quint8>& visibility() const { return m_visibility; }
        void setVisibility(QVector<quint8> visibility) { m_visibility = std::move(visibility); }

    private:

        Key m_key;
        cwGeometry m_geometry;
        QMatrix4x4 m_modelMatrix;
        float m_pickRadius = 0.0f;
        QVector<quint8> m_visibility;
    };

    // Immutable, thread-safe handle to a built BVH (see snapshotForQuery).
    // Defined below, after the private BvhData it wraps.
    class QuerySnapshot;

    explicit cwGeometryItersecter(QObject* parent = nullptr);
    ~cwGeometryItersecter() override = default;

    void addObject(const cwGeometryItersecter::Object& object);
    void clear(cwRenderObject* parentObject = nullptr);
    void removeObject(cwRenderObject* parentObject, uint64_t id);
    void removeObject(const Key& objectKey);

    void setModelMatrix(cwRenderObject* parentObject, uint64_t id, const QMatrix4x4& modelMatrix);
    void setModelMatrix(const Key& objectKey, const QMatrix4x4& modelMatrix);

    //! Per-Key visibility: a hidden Key is skipped by every pick traversal
    //! (exact pick and pivot anchor) and by visibleBoundingBox() /
    //! isPickableEmpty() — the sub-object analogue of the whole-object
    //! parent->isVisible() gate. This is identity state, not geometry state:
    //! it survives addObject() replacing the Key's geometry (a hidden scrap
    //! whose triangulation updates stays hidden) and may be set before the
    //! Key is registered. removeObject()/clear() forget it. No BVH rebuild —
    //! the published snapshot is republished copy-on-write. Main thread only.
    void setVisible(const Key& objectKey, bool visible);

    //! Per-vertex visibility for vertices [start, start + count) of one Key
    //! — for objects like the line plot where one Key's visibility varies
    //! per primitive. A primitive is visible iff every vertex it references
    //! is visible (a Lines segment: both endpoints). Hidden primitives are
    //! skipped by the pick traversals and excluded from
    //! visibleBoundingBox(). Mask indices are geometry vertex indices, so
    //! addObject() resets the mask to all-visible; callers re-apply ranges
    //! after pushing new geometry (mirrors cwRenderLinePlot's own buffer).
    //! Out-of-range calls and no-op changes are ignored. No BVH rebuild.
    //! Main thread only.
    void setRangeVisible(const Key& objectKey, int start, int count, bool visible);

    QBox3D boundingBox(const Key& objectKey) const;

    //! World-space union of the bounding boxes for the given keys. Null box when empty.
    QBox3D boundingBox(const QList<Key>& keys) const;

    //! World-space union of every object's bounding box. Null box when empty.
    //! Counts hidden objects — use visibleBoundingBox() for the region the
    //! pick paths can actually reach.
    QBox3D boundingBox() const;

    //! Like boundingBox(), but skips geometry the pick traversals skip — a
    //! hidden parent render object, a per-Key-hidden Object (setVisible),
    //! and per-vertex-masked primitives (setRangeVisible) — the box a
    //! camera should frame. Flat O(#Keys) loop over cached per-Node boxes;
    //! only an Object with a live mask pays an O(vertices) walk (the line
    //! plot while partially hidden). Callers run at interaction rate — reset
    //! view, and isPickableEmpty() on the pivot-fallback path — never per
    //! frame. Main-thread only: reads
    //! the authoring object list, so during an async build it already
    //! reflects objects that will become pickable.
    QBox3D visibleBoundingBox() const;

    //! True when no pick can ever hit anything: no objects, every object
    //! hidden, or only degenerate bounds. Mirrors the visibility rule the
    //! traversals apply, so callers gating a fallback on "is there geometry to
    //! anchor against?" agree with what a pick returns.
    //!
    //! isFinite() here is QBox3D's null/finite/infinite *type* flag, not a
    //! NaN check: a box with NaN corners still reports Finite. Nothing in the
    //! codebase builds an Infinite box, so that arm is unreachable today.
    //!
    //! Delegates to visibleBoundingBox(), so during an async build it reports
    //! non-empty and a caller keeps its fallback suppressed rather than
    //! acting on a temporary emptiness.
    bool isPickableEmpty() const;

    // `ray` direction must be unit length: ray-depth (projectedDistance)
    // divides by |dir|^2, and the screen-space line tolerance scales its
    // radius by that depth — a non-normalized direction mis-sizes both.
    double intersects(const QRay3D& ray, const cwPickQuery& query = {}) const;
    cwRayHit intersectsDetailed(const QRay3D& ray, const cwPickQuery& query = {}) const;

    // Geometry-anchored rotation-pivot fallback (issue #562). Returns the
    // world-space point ON real geometry closest to `ray`: a cloud point, the
    // nearest spot on a shot segment, or a spot on a scrap triangle (exact
    // hit, else its nearest edge). Candidates must lie within the query's
    // screen-space tolerance radius at their depth; among accepted candidates
    // the front-most wins, matching intersectsDetailed's nearest-by-depth
    // rule. Unlike intersectsDetailed, points and triangles are reachable at
    // the full tolerance radius (not just pickRadius / exact intersection),
    // so a near-miss on any kind still lands the pivot on the geometry the
    // user aimed at. Hidden objects are skipped. Returns nullopt when nothing
    // is inside the tolerance (or the tolerance is disabled), letting the
    // caller keep the current pivot. Reads the built BVH on the owning
    // thread, like intersectsDetailed.
    std::optional<QVector3D> nearestGeometryPoint(const QRay3D& ray,
                                                  const cwPickQuery& query) const;

    // Capture the current built BVH as an immutable snapshot. Call on the
    // thread that owns this object (the main thread); the returned snapshot
    // can then be read from any thread and keeps the point data alive for its
    // lifetime. isNull() until the first async build completes.
    QuerySnapshot snapshotForQuery() const;

    // World-space point-in-box query against a snapshot: returns every vertex
    // of the point-cloud object `key` whose world position lies inside `box`.
    // A BVH broad-phase — O(log n + k) for k hits rather than a full O(n)
    // scan of the cloud. Reads only the snapshot, so it is safe to run off
    // the main thread. Empty when the key isn't a built point cloud, its slot
    // was invalidated mid-rebuild, the box is null, or the snapshot is null.
    static QList<QVector3D> pointsInBox(const QuerySnapshot& snapshot,
                                        const QBox3D& box,
                                        const Key& key);

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
        Node(const QBox3D boundingBox, const cwGeometryItersecter::Object& object);

        QBox3D BoundingBox;
        cwGeometryItersecter::Object Object;

        static QBox3D triangleToBoundingBox(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3);
    };

    // BVH primitive handle. One slot per triangle or line segment
    // (primitiveIndex is the first index into indices()) or per point
    // (primitiveIndex is the vertex index). The owning SubBvh's `object`
    // field provides the geometry context.
    struct Primitive {
        enum class Kind : uint8_t { Triangle, Point, Line };
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
    // Immutable once published: m_bvh is a shared_ptr<const BvhData> and is
    // only ever replaced wholesale — by the build swap, or by
    // invalidatePublishedSlot copying-on-write a fresh BvhData with one slot
    // nulled. No published BvhData is edited in place, so snapshotForQuery can
    // hand the whole shared_ptr to another thread safely: the copy is
    // O(objects) shared_ptr/array bumps and the heavy sub-BVHs and point
    // buffers stay shared and refcount-pinned for the snapshot's lifetime.
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
        // Reverse index: Key → slot in the parallel arrays above. Lets a
        // mutator null out the dirty Key's sub-BVH in the published BVH so
        // it stops contributing to picks the instant its cache entry is
        // invalidated — without dropping the rest of the BVH while a
        // rebuild is in flight. A null entry in subBvhs is the in-flight
        // marker; traversal skips it. See invalidatePublishedSlot().
        QHash<Key, int> keyToSlot;

        // Per-slot visibility, parallel to subBvhs. slotVisible is the
        // per-Key flag (0 = hidden, see setVisible); visibilityMasks the
        // per-vertex mask (null = all visible, see setRangeVisible). Sized
        // by refreshPublishedVisibility() before every publish; toggles
        // republish copy-on-write (republishSlotVisibility) so held query
        // snapshots keep their captured view — same discipline as
        // invalidatePublishedSlot().
        QVector<quint8> slotVisible;
        QVector<std::shared_ptr<const QVector<quint8>>> visibilityMasks;
    };

public:
    // Immutable handle to a built BVH (see snapshotForQuery / pointsInBox).
    // Copying is a shared_ptr refcount bump; it never aliases m_bvh's mutable
    // identity (m_bvh is replaced wholesale, never edited in place), so a
    // snapshot taken on the main thread can be read from a worker while the
    // main thread publishes a newer BVH. Default-constructed snapshots are
    // isNull(); so are snapshots taken before the first build completes.
    class QuerySnapshot {
    public:
        QuerySnapshot() = default;
        bool isNull() const { return m_data == nullptr; }

    private:
        friend class cwGeometryItersecter;
        explicit QuerySnapshot(std::shared_ptr<const BvhData> data) :
            m_data(std::move(data)) {}
        std::shared_ptr<const BvhData> m_data;
    };

private:

    // Live BVH. nullptr until the first async build completes. Only
    // accessed from the main thread: pick queries (intersects/
    // intersectsDetailed) and the .context() install callback both run
    // on the thread owning `this`, and scheduleBuild() (which clears
    // m_bvh) is invoked from the same main-thread mutators that touch
    // Nodes. The worker thread never touches m_bvh directly — it writes
    // into a side channel (resultSlot, see launchBuildJob) that the
    // .context() callback drains on the main thread.
    std::shared_ptr<const BvhData> m_bvh;

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

    // Keys hidden via setVisible(Key, false). Lives beside Nodes rather
    // than on the Object so the flag survives addObject() replacing the
    // Key's geometry; removeObject()/clear() erase entries.
    QSet<Key> m_hiddenKeys;

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

    // Copy-on-write republish of one Key's visibility (flag + mask) into
    // the published BVH, mirroring the live state. No-op when there is no
    // published BVH (the install callback refreshes from live state) or
    // the Key has no slot. Never edits a published BvhData in place.
    void republishSlotVisibility(const Key& key);

    // Fill bvh's per-slot visibility arrays from the live m_hiddenKeys /
    // Node masks. Called on the main thread on a not-yet-published BvhData,
    // right before the install swap — so a toggle that raced the build
    // worker is never lost.
    void refreshPublishedVisibility(BvhData& bvh) const;

    // World-space box of one Node honoring its per-vertex mask: unmasked
    // Nodes return the cached whole-node box; masked Nodes pay an
    // O(vertices) walk over visible primitives (line plot while partially
    // hidden; interaction rate via visibleBoundingBox()'s callers). If that
    // walk ever shows up in a profile, cache the visible box per Node and
    // invalidate in setRangeVisible.
    static QBox3D visibleNodeBox(const Node& node);

    // True when every vertex `prim` references is visible in the
    // per-vertex mask. Points index vertices directly; Lines and
    // Triangles go through the index buffer.
    static bool isPrimitiveVisible(const QVector<quint8>& mask,
                                   const cwGeometry& geometry,
                                   const Primitive& prim);

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
    // The read-only inputs every primitive test within one top-level leaf
    // shares: the Object, both rays, both matrices, and the position
    // attribute. traverseBvh hoists them once per leaf, so the hot path
    // never re-inverts a matrix or re-looks-up an attribute. `object` comes
    // from the SubBvh — decoupling from BvhData::nodesSnapshot ordering,
    // which is not stable across rebuilds. Defined in the .cpp beside the
    // policies that consume it.
    struct LeafContext;

    // The two policies traverseBvh is instantiated with. They answer
    // different questions — ExactPick "what did the ray hit?", AnchorPick
    // "what lies nearest the ray?" — so each names the kinds it lets consult
    // the screen-space tolerance. See cwPickTolerance for why they disagree
    // on purpose.
    //
    // ToleranceKinds is what traverseBvh reads: it decides which nodes get a
    // tolerance pad and which sub-BVHs are skipped outright when no tolerance
    // is supplied. The per-primitive accept rule is the policy's own
    // testPrimitive — the constant does not enforce it. What keeps the two in
    // step is that they now sit together in one struct, so a policy's reach
    // and the broad phase that must not prune inside it are read as a unit
    // rather than 200 lines apart in two near-identical traversals (which is
    // exactly how their pads drifted apart in the first place).
    //
    // Defined in the .cpp; both instantiations live in that TU.
    struct ExactPick;
    struct AnchorPick;

    // Front-to-back descent: the top-level BVH in world space, then each
    // Object's sub-BVH in model space. Hands every leaf primitive surviving
    // visibility, the kind filter, and the depth prune to `policy`. What
    // counts as a hit lives entirely in the policy, never here.
    template <typename Policy>
    static void traverseBvh(const BvhData& bvh,
                            const QRay3D& ray,
                            const cwPickQuery& query,
                            Policy& policy);

    // Debug-only per-primitive diagnostic; gated on lcPick debug.
    static void dumpLeafPrimitive(const Object& object,
                                  const Primitive& prim,
                                  const QRay3D& ray,
                                  uint32_t leafIdx,
                                  uint32_t localIdx);

    // Phase A: how many BVH primitives a single Node contributes. Lines
    // contribute one per segment (picked via a screen-space tolerance at
    // query time). Points contribute zero unless pickRadius > 0.
    static qsizetype countNodePrimitives(const Object& object);

    // Phase A workers: fill BuildPrim slots for one chunk of a Triangles
    // or Points-typed Node, respectively.
    static void enumerateTrianglesChunk(const Object& object,
                                        const EnumChunk& chunk,
                                        QVector<BuildPrim>& prims);
    static void enumeratePointsChunk(const Object& object,
                                     const EnumChunk& chunk,
                                     QVector<BuildPrim>& prims);
    static void enumerateLinesChunk(const Object& object,
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
