/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPointOctreePickSet.h"
#include "cwPointOctree.h"
#include "cwProfileLog.h"
#include "cwRaySphere.h"

//Qt includes
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QRay3D>
#include <QVarLengthArray>
#include <QVector3D>
#include <QtEndian>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace {

    using cw::octree::kAxisCount;

    //! One value per axis, passed by value: the box tests take these by the
    //! thousand.
    using AxisTriple = std::array<float, kAxisCount>;

    //! The depth a query with nothing accepted yet compares boxes against
    constexpr double kNoBestDepth = std::numeric_limits<double>::max();

    AxisTriple axisTripleOf(const QVector3D& vector)
    {
        return AxisTriple {vector.x(), vector.y(), vector.z()};
    }

    //! What every box test needs from the ray, computed once per query: one
    //! query slab-tests thousands of boxes against the same ray.
    struct RayAxes {
        AxisTriple origin {0.0f, 0.0f, 0.0f};
        AxisTriple direction {0.0f, 0.0f, 0.0f};
        AxisTriple inverseDirection {0.0f, 0.0f, 0.0f};

        //! projectedDistance() divides by this, so a box's ray depth is one
        //! dot product and one multiply.
        float inverseLengthSquared = 0.0f;

        //! False on an axis the ray never moves along, where the slab test
        //! becomes "is the origin between the faces".
        std::array<bool, kAxisCount> moves {false, false, false};
    };

    RayAxes rayAxesOf(const QRay3D& ray)
    {
        const QVector3D direction = ray.direction();

        RayAxes axes;
        axes.origin = axisTripleOf(ray.origin());
        axes.direction = axisTripleOf(direction);
        for (int axis = 0; axis < kAxisCount; axis++) {
            axes.moves[axis] = axes.direction[axis] != 0.0f;
            if (axes.moves[axis]) {
                axes.inverseDirection[axis] = 1.0f / axes.direction[axis];
            }
        }

        const float lengthSquared = direction.lengthSquared();
        if (lengthSquared > 0.0f) {
            axes.inverseLengthSquared = 1.0f / lengthSquared;
        }
        return axes;
    }

    //! The ray's own constants for the per-point depth, hoisted out of the
    //! point loop: QRay3D::projectedDistance lives in another translation unit,
    //! so a leaf of 64 points paid 64 calls for an expression of nine
    //! floating point operations. depthOf evaluates that expression exactly as
    //! written there — the same order, the same division — so what a point
    //! ranks at is unchanged.
    struct RayProjection {
        QVector3D origin;
        QVector3D direction;
        float lengthSquared = 0.0f;

        float depthOf(const QVector3D& point) const
        {
            return QVector3D::dotProduct(point - origin, direction) / lengthSquared;
        }
    };

    RayProjection rayProjectionOf(const QRay3D& ray)
    {
        return RayProjection {ray.origin(), ray.direction(),
                              ray.direction().lengthSquared()};
    }

    //! The least and greatest ray depth the box [@a minimum, @a maximum] holds
    struct RayDepthRange {
        float nearest = 0.0f;
        float farthest = 0.0f;
    };

    //! projectedDistance is affine, so each end of the range sits on the corner
    //! every axis picks by the sign of the direction — the same two values
    //! eight corner projections would find, for a third of the work.
    RayDepthRange rayDepthRange(AxisTriple minimum, AxisTriple maximum, const RayAxes& axes)
    {
        RayDepthRange range;
        for (int axis = 0; axis < kAxisCount; axis++) {
            const float low = (minimum[axis] - axes.origin[axis]) * axes.direction[axis];
            const float high = (maximum[axis] - axes.origin[axis]) * axes.direction[axis];
            range.nearest += std::min(low, high);
            range.farthest += std::max(low, high);
        }

        range.nearest *= axes.inverseLengthSquared;
        range.farthest *= axes.inverseLengthSquared;
        return range;
    }

    //! How deep along the ray the box [@a minimum, @a maximum] starts, when the
    //! ray reaches it at or past its origin: the slab test
    //! QBox3D::intersection does, without its per-call plane walk. A ray that
    //! starts inside the box enters it at the origin, so the depth is clamped
    //! at zero.
    std::optional<float> rayEntryDepth(AxisTriple minimum, AxisTriple maximum,
                                       const RayAxes& axes)
    {
        float entryT = 0.0f;
        float exitT = std::numeric_limits<float>::max();

        for (int axis = 0; axis < kAxisCount; axis++) {
            if (!axes.moves[axis]) {
                if (axes.origin[axis] < minimum[axis] || axes.origin[axis] > maximum[axis]) {
                    return std::nullopt;
                }
                continue;
            }

            float nearT = (minimum[axis] - axes.origin[axis]) * axes.inverseDirection[axis];
            float farT = (maximum[axis] - axes.origin[axis]) * axes.inverseDirection[axis];
            if (nearT > farT) {
                std::swap(nearT, farT);
            }

            entryT = std::max(entryT, nearT);
            exitT = std::min(exitT, farT);
            if (entryT > exitT) {
                return std::nullopt;
            }
        }

        return entryT;
    }

    //! How a query measures one box: is it reachable at all, and how near along
    //! the ray a point it holds can be.
    /*!
        The reach test runs on the box inflated by @a pad, because an accepted
        point can sit @a pad outside it, so the inflated box's entry is one
        lower bound on the ranking depth. The box's own nearest ray depth, less
        @a depthBias, is a second: the bias is how far ahead of a point the
        query's ranking depth can lie — nothing for nearestPoint, which ranks
        by the point's own ray depth, and the pick radius for exactHit, which
        ranks by where the point's sphere starts. Both bounds hold, so the
        query takes the tighter one.
    */
    std::optional<float> boxDepth(AxisTriple minimum, AxisTriple maximum,
                                  float pad, float depthBias, const RayAxes& axes)
    {
        AxisTriple low {0.0f, 0.0f, 0.0f};
        AxisTriple high {0.0f, 0.0f, 0.0f};
        for (int axis = 0; axis < kAxisCount; axis++) {
            low[axis] = minimum[axis] - pad;
            high[axis] = maximum[axis] + pad;
        }

        const std::optional<float> entryDepth = rayEntryDepth(low, high, axes);
        if (!entryDepth.has_value()) {
            return std::nullopt;
        }

        const float nearest = rayDepthRange(minimum, maximum, axes).nearest;
        return std::max(entryDepth.value(), std::max(0.0f, nearest - depthBias));
    }

    //! The widest tolerance radius any point inside the box [@a minimum,
    //! @a maximum] can be accepted at: radiusAt() grows with depth, so the
    //! box's deepest corner bounds it. The fine test still applies the exact
    //! radius at the candidate's own depth.
    float tolerancePad(AxisTriple minimum, AxisTriple maximum,
                       const RayAxes& axes, const cwPickTolerance& tolerance)
    {
        const float farthest = rayDepthRange(minimum, maximum, axes).farthest;
        return float(tolerance.radiusAt(std::max(0.0f, farthest)));
    }

    //! The world size of one quantization step of a node's payload
    float quantizationScale(const QBox3D& bounds)
    {
        return float((bounds.maximum().x() - bounds.minimum().x())
                     / double(cw::octree::kQuantMax));
    }

    //! The points of one node, dequantized on the fly the way the shader does
    //! it. The cloud draws with an identity model matrix, so a dequantized
    //! point is already in world space.
    class NodePoints
    {
    public:
        explicit NodePoints(const cwPointOctreePickSet::Node& node) :
            m_bytes(node.bytes),
            m_origin(node.bounds.minimum()),
            m_scale(quantizationScale(node.bounds))
        {
        }

        QVector3D at(qsizetype index) const
        {
            //Read through the void* overload: a QByteArray gives no alignment
            //guarantee a quint16* could rest on.
            const char* axes = m_bytes.constData() + index * cw::octree::kBytesPerPoint;
            constexpr int kAxisBytes = int(sizeof(quint16));
            return QVector3D(
                m_origin.x() + float(qFromLittleEndian<quint16>(axes)) * m_scale,
                m_origin.y() + float(qFromLittleEndian<quint16>(axes + kAxisBytes)) * m_scale,
                m_origin.z() + float(qFromLittleEndian<quint16>(axes + 2 * kAxisBytes)) * m_scale);
        }

    private:
        QByteArray m_bytes;
        QVector3D m_origin;
        float m_scale;
    };

    //! What one query fills for its cw.profile.pick line: the nodes it scanned,
    //! and how much of them it actually touched.
    struct PickCounts {
        QList<float> entryDepths;
        qint64 points = 0;
        qint64 groups = 0;
        qint64 leaves = 0;
    };

    //! One box the ray reaches — a node, one of its groups, or one of its
    //! leaves — and the nearest depth a point it holds can rank at. A query
    //! walks these in depth order and stops at the first one that cannot beat
    //! the hit it already has.
    struct ReachedBox {
        float depth = 0.0f;
        qsizetype index = 0;
    };

    template <typename Boxes>
    void sortByDepth(Boxes& boxes)
    {
        std::sort(boxes.begin(), boxes.end(),
                  [](const ReachedBox& left, const ReachedBox& right) {
                      return left.depth < right.depth;
                  });
    }

    //! What a node's quantized boxes step from: its payload origin per axis and
    //! the world size of one quantization step.
    struct NodeFrame {
        AxisTriple origin {0.0f, 0.0f, 0.0f};
        float scale = 0.0f;
    };

    NodeFrame nodeFrameOf(const QBox3D& bounds)
    {
        return NodeFrame {axisTripleOf(bounds.minimum()), quantizationScale(bounds)};
    }

    //! One quantized sub-box of a node, measured in world space with the pad
    //! @a padOf gives it. Sizing the pad per box rather than per node is what
    //! makes a leaf near the camera a tight test inside a node that reaches far
    //! away.
    template <typename PadOf>
    std::optional<float> boxDepth(const cwPointOctreePickIndex::QuantizedBox& box,
                                  const NodeFrame& frame, float depthBias,
                                  const RayAxes& axes, PadOf padOf)
    {
        AxisTriple minimum {0.0f, 0.0f, 0.0f};
        AxisTriple maximum {0.0f, 0.0f, 0.0f};
        for (int axis = 0; axis < kAxisCount; axis++) {
            minimum[axis] = frame.origin[axis] + float(box.min[axis]) * frame.scale;
            maximum[axis] = frame.origin[axis] + float(box.max[axis]) * frame.scale;
        }

        return boxDepth(minimum, maximum, padOf(minimum, maximum), depthBias, axes);
    }

    //! Most groups a node's index holds: kLeafMaxPoints points cut into leaves
    //! of kPickLeafPoints, then into groups of kPickGroupLeaves. A sampled
    //! interior node can hold more, and then the array spills to the heap.
    constexpr qsizetype kGroupsOnStack =
        (cw::octree::kLeafMaxPoints / cw::octree::kPickLeafPoints
         + cw::octree::kPickGroupLeaves - 1) / cw::octree::kPickGroupLeaves;

    //! Hands @a scanRange the point ranges of @a node the ray can still accept
    //! a point from, descending group boxes then leaf boxes, nearest first.
    /*!
        A box is reached when the ray comes within @a padOf of it, and it is
        measured by the nearest depth a point inside it can rank at (boxDepth).
        A box that cannot rank nearer than @a bestDepth() holds nothing this
        query would keep and is skipped whole. @a bestDepth is read again at
        every box, so a leaf that lands a hit tightens everything after it —
        which is why the boxes are walked in depth order rather than the Morton
        order they are stored in.

        A node with an empty index is scanned linearly.
    */
    template <typename PadOf, typename BestDepth, typename ScanRange>
    void visitPickRanges(const cwPointOctreePickSet::Node& node, const RayAxes& axes,
                         float depthBias, PadOf padOf, BestDepth bestDepth,
                         ScanRange scanRange, PickCounts& counts)
    {
        const qsizetype count = node.bytes.size() / cw::octree::kBytesPerPoint;
        if (node.index.isEmpty()) {
            scanRange(qsizetype(0), count);
            counts.points += count;
            return;
        }

        const NodeFrame frame = nodeFrameOf(node.bounds);
        const qsizetype leafCount = node.index.leaves.size();

        QVarLengthArray<ReachedBox, kGroupsOnStack> reachedGroups;
        for (qsizetype group = 0; group < node.index.groups.size(); group++) {
            const std::optional<float> depth =
                boxDepth(node.index.groups.at(group), frame, depthBias, axes, padOf);
            if (depth.has_value() && double(depth.value()) < bestDepth()) {
                reachedGroups.append(ReachedBox {depth.value(), group});
            }
        }
        counts.groups += node.index.groups.size();
        sortByDepth(reachedGroups);

        for (const ReachedBox& group : reachedGroups) {
            if (double(group.depth) >= bestDepth()) {
                break;
            }

            const qsizetype firstLeaf = group.index * cw::octree::kPickGroupLeaves;
            const qsizetype lastLeaf =
                std::min(firstLeaf + cw::octree::kPickGroupLeaves, leafCount);
            counts.leaves += lastLeaf - firstLeaf;

            // The leaves of one group are neighbors in space as well as in the
            // payload, so they are taken as they come: ordering a handful of
            // boxes that all start within a leaf's width of each other left
            // the points scanned unchanged and read the payload out of order.
            for (qsizetype leaf = firstLeaf; leaf < lastLeaf; leaf++) {
                const std::optional<float> depth =
                    boxDepth(node.index.leaves.at(leaf), frame, depthBias, axes, padOf);
                if (!depth.has_value() || double(depth.value()) >= bestDepth()) {
                    continue;
                }

                const qsizetype first = leaf * cw::octree::kPickLeafPoints;
                const qsizetype last = std::min(first + cw::octree::kPickLeafPoints, count);
                scanRange(first, last);
                counts.points += last - first;
            }
        }
    }

    //! The nodes the ray reaches, nearest first, from the snapshot's packed
    //! bounds. @a padOf sizes a box's pad from its own world bounds, so a
    //! node, its groups, and its leaves each get one of their own.
    template <typename NodeBoundsList, typename PadOf>
    QVector<ReachedBox> reachedNodes(const NodeBoundsList& bounds, const RayAxes& axes,
                                     float depthBias, PadOf padOf)
    {
        QVector<ReachedBox> reached;
        for (qsizetype i = 0; i < bounds.size(); i++) {
            const AxisTriple minimum = bounds.at(i).minimum;
            const AxisTriple maximum = bounds.at(i).maximum;
            const std::optional<float> depth =
                boxDepth(minimum, maximum, padOf(minimum, maximum), depthBias, axes);
            if (depth.has_value()) {
                reached.append(ReachedBox {depth.value(), i});
            }
        }

        sortByDepth(reached);
        return reached;
    }
    //! One cw.profile.pick line. @a counts.entryDepths holds the ranking depth
    //! of every node the query scanned, so "prunable" is the count the
    //! best-depth cut-off could still have skipped.
    void logPickQuery(QLatin1StringView kind, qsizetype snapshotNodes,
                      const PickCounts& counts, const QElapsedTimer& timer,
                      const std::optional<cwPickProvider::PointHit>& best)
    {
        int prunable = 0;
        if (best.has_value()) {
            for (const float depth : counts.entryDepths) {
                if (double(depth) > best->rayDepth) {
                    prunable++;
                }
            }
        }

        cw::profile::write(
            lcProfilePick(),
            QStringLiteral("pick kind=%1 nodes=%2 passing=%3 groups=%4 leaves=%5 points=%6 "
                           "us=%7 hit=%8 prunable=%9")
                .arg(kind)
                .arg(snapshotNodes)
                .arg(counts.entryDepths.size())
                .arg(counts.groups)
                .arg(counts.leaves)
                .arg(counts.points)
                .arg(cw::profile::elapsedUs(timer))
                .arg(best.has_value() ? 1 : 0)
                .arg(prunable));
    }
}

void cwPointOctreePickSet::publish(QVector<Node> nodes, const QBox3D& rootBounds,
                                   float pickRadius)
{
    auto published = std::make_shared<Snapshot>();
    published->nodes = std::move(nodes);
    published->rootBounds = rootBounds;
    published->pickRadius = pickRadius;

    published->bounds.reserve(published->nodes.size());
    for (const Node& node : std::as_const(published->nodes)) {
        published->bounds.append(NodeBounds {axisTripleOf(node.bounds.minimum()),
                                             axisTripleOf(node.bounds.maximum())});
    }

    //The snapshot being replaced may hold the last share of every evicted
    //node's mirror; it dies out here so the lock is not held across the frees.
    std::shared_ptr<const Snapshot> previous;
    {
        const QMutexLocker locker(&m_mutex);
        previous = std::exchange(m_snapshot, std::move(published));
    }
}

std::shared_ptr<const cwPointOctreePickSet::Snapshot> cwPointOctreePickSet::snapshot() const
{
    const QMutexLocker locker(&m_mutex);
    return m_snapshot;
}

QBox3D cwPointOctreePickSet::bounds() const
{
    const std::shared_ptr<const Snapshot> taken = snapshot();
    return taken ? taken->rootBounds : QBox3D();
}

std::optional<cwPickProvider::PointHit>
cwPointOctreePickSet::exactHit(const QRay3D& ray) const
{
    const std::shared_ptr<const Snapshot> taken = snapshot();
    if (!taken || taken->pickRadius <= 0.0f) {
        return std::nullopt;
    }

    const float radius = taken->pickRadius;

    const bool profiling = lcProfilePick().isDebugEnabled();
    QElapsedTimer timer;
    PickCounts counts;
    if (profiling) {
        timer.start();
    }

    std::optional<PointHit> best;
    const auto bestDepth = [&best]() {
        return best.has_value() ? best->rayDepth : kNoBestDepth;
    };

    const RayAxes axes = rayAxesOf(ray);
    const auto padOf = [radius](AxisTriple, AxisTriple) { return radius; };

    // A sphere of the pick radius around a point starts up to that radius ahead
    // of the point itself, measured in the ray's own units, so a box's points
    // can rank that much nearer than the box does.
    const float depthBias = radius * std::sqrt(axes.inverseLengthSquared);

    for (const ReachedBox& entry : reachedNodes(taken->bounds, axes, depthBias, padOf)) {
        if (double(entry.depth) >= bestDepth()) {
            break;
        }

        const Node& node = taken->nodes.at(entry.index);
        const NodePoints points(node);
        if (profiling) {
            counts.entryDepths.append(entry.depth);
        }

        // Rank by sphere-entry depth, the way the BVH's point primitives do: it
        // blends depth and perpendicular distance, so a head-on hit beats a
        // grazing one at the same center depth. tNear is already that depth in
        // projectedDistance units. The reported point stays the data point, so
        // a coordinate readout snaps to it.
        const auto scanRange = [&](qsizetype first, qsizetype last) {
            for (qsizetype i = first; i < last; i++) {
                const QVector3D center = points.at(i);
                const cw::RaySphereHit sphere =
                    cw::raySphereIntersectDouble(ray, center, radius);
                if (!sphere.hit || sphere.tNear <= 0.0) {
                    continue;
                }
                if (best && sphere.tNear >= best->rayDepth) {
                    continue;
                }

                best = PointHit{center, sphere.tNear};
            }
        };

        visitPickRanges(node, axes, depthBias, padOf, bestDepth, scanRange, counts);
    }

    if (profiling) {
        logPickQuery(QLatin1StringView("exactHit"), taken->nodes.size(), counts, timer, best);
    }

    return best;
}

std::optional<cwPickProvider::PointHit>
cwPointOctreePickSet::nearestPoint(const QRay3D& ray,
                                   const cwPickTolerance& tolerance) const
{
    const std::shared_ptr<const Snapshot> taken = snapshot();
    if (!taken || !tolerance.enabled()) {
        return std::nullopt;
    }

    const bool profiling = lcProfilePick().isDebugEnabled();
    QElapsedTimer timer;
    PickCounts counts;
    if (profiling) {
        timer.start();
    }

    std::optional<PointHit> best;
    const auto bestDepth = [&best]() {
        return best.has_value() ? best->rayDepth : kNoBestDepth;
    };

    const RayAxes axes = rayAxesOf(ray);
    const RayProjection projection = rayProjectionOf(ray);
    const auto padOf = [&axes, &tolerance](AxisTriple minimum, AxisTriple maximum) {
        return tolerancePad(minimum, maximum, axes, tolerance);
    };

    // This query ranks a point by its own ray depth, which is never nearer than
    // its box's, so the cut-off needs no slack.
    constexpr float kNoDepthBias = 0.0f;

    for (const ReachedBox& entry : reachedNodes(taken->bounds, axes, kNoDepthBias, padOf)) {
        if (double(entry.depth) >= bestDepth()) {
            break;
        }

        const Node& node = taken->nodes.at(entry.index);
        const NodePoints points(node);
        if (profiling) {
            counts.entryDepths.append(entry.depth);
        }

        const auto scanRange = [&](qsizetype first, qsizetype last) {
            for (qsizetype i = first; i < last; i++) {
                const QVector3D point = points.at(i);
                const double rayDepth = projection.depthOf(point);
                if (rayDepth <= 0.0) {
                    continue;  // behind the camera
                }
                if (best && rayDepth >= best->rayDepth) {
                    continue;
                }

                // Radius 0 never reports a hit, but dSq — the double-precision
                // perpendicular distance to the ray — is always filled.
                const double dSq = cw::raySphereIntersectDouble(ray, point, 0.0f).dSq;
                const double radius = tolerance.radiusAt(rayDepth);
                if (dSq > radius * radius) {
                    continue;
                }

                best = PointHit{point, rayDepth};
            }
        };

        visitPickRanges(node, axes, kNoDepthBias, padOf, bestDepth, scanRange, counts);
    }

    if (profiling) {
        logPickQuery(QLatin1StringView("nearestPoint"), taken->nodes.size(), counts, timer, best);
    }

    return best;
}
