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
#include <QtEndian>

//Std includes
#include <algorithm>
#include <optional>
#include <utility>

namespace {

    constexpr int kBoxCornerCount = 8;
    constexpr int kCornerMaskX = 1;
    constexpr int kCornerMaskY = 2;
    constexpr int kCornerMaskZ = 4;

    //! Where the ray enters @a box, when it reaches it at or past its origin.
    //! cw.profile.pick compares that depth against the hit the query settled on.
    std::optional<float> rayEntryDepth(const QBox3D& box, const QRay3D& ray)
    {
        float minimumT = 0.0f;
        float maximumT = 0.0f;
        if (!box.intersection(ray, &minimumT, &maximumT) || maximumT < 0.0f) {
            return std::nullopt;
        }

        return minimumT;
    }

    QBox3D inflated(const QBox3D& box, float pad)
    {
        if (pad <= 0.0f) {
            return box;
        }
        const QVector3D padding(pad, pad, pad);
        return QBox3D(box.minimum() - padding, box.maximum() + padding);
    }

    //! The widest tolerance radius any point inside @a box can be accepted at.
    //! projectedDistance is affine, so its maximum over the box sits on a
    //! corner, and radiusAt() grows with depth — the fine test still applies the
    //! exact radius at the candidate's own depth.
    float tolerancePad(const QRay3D& ray, const QBox3D& box,
                       const cwPickTolerance& tolerance)
    {
        const QVector3D minimum = box.minimum();
        const QVector3D maximum = box.maximum();
        double farDepth = 0.0;
        for (int corner = 0; corner < kBoxCornerCount; corner++) {
            const QVector3D point((corner & kCornerMaskX) ? maximum.x() : minimum.x(),
                                  (corner & kCornerMaskY) ? maximum.y() : minimum.y(),
                                  (corner & kCornerMaskZ) ? maximum.z() : minimum.z());
            farDepth = std::max(farDepth, double(ray.projectedDistance(point)));
        }
        return float(tolerance.radiusAt(farDepth));
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
            m_scale(float((node.bounds.maximum().x() - m_origin.x())
                          / double(cw::octree::kQuantMax)))
        {
        }

        qsizetype count() const { return m_bytes.size() / cw::octree::kBytesPerPoint; }

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

    //! One cw.profile.pick line. @a entryDepths holds the ray-entry depth of
    //! every node the query scanned, so "prunable" is the count a near-to-far
    //! order with a best-depth cut-off could have skipped.
    void logPickQuery(QLatin1StringView kind, qsizetype snapshotNodes,
                      const QList<float>& entryDepths, qint64 pointsScanned,
                      const QElapsedTimer& timer,
                      const std::optional<cwPickProvider::PointHit>& best)
    {
        int prunable = 0;
        if (best.has_value()) {
            for (const float depth : entryDepths) {
                if (double(depth) > best->rayDepth) {
                    prunable++;
                }
            }
        }

        qCDebug(lcProfilePick).noquote()
            << QStringLiteral("pick kind=%1 nodes=%2 passing=%3 points=%4 us=%5 hit=%6 prunable=%7")
                   .arg(kind)
                   .arg(snapshotNodes)
                   .arg(entryDepths.size())
                   .arg(pointsScanned)
                   .arg(cw::profile::elapsedUs(timer))
                   .arg(best.has_value() ? 1 : 0)
                   .arg(prunable);
    }
}

void cwPointOctreePickSet::publish(QVector<Node> nodes, const QBox3D& rootBounds,
                                   float pickRadius)
{
    auto published = std::make_shared<Snapshot>();
    published->nodes = std::move(nodes);
    published->rootBounds = rootBounds;
    published->pickRadius = pickRadius;

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
    QList<float> entryDepths;
    qint64 pointsScanned = 0;
    if (profiling) {
        timer.start();
    }

    std::optional<PointHit> best;
    for (const Node& node : taken->nodes) {
        const std::optional<float> entryDepth =
            rayEntryDepth(inflated(node.bounds, radius), ray);
        if (!entryDepth.has_value()) {
            continue;
        }

        const NodePoints points(node);
        const qsizetype count = points.count();
        if (profiling) {
            entryDepths.append(entryDepth.value());
            pointsScanned += count;
        }

        for (qsizetype i = 0; i < count; i++) {
            const QVector3D center = points.at(i);
            const cw::RaySphereHit sphere =
                cw::raySphereIntersectDouble(ray, center, radius);
            // Rank by sphere-entry depth, the way the BVH's point primitives
            // do: it blends depth and perpendicular distance, so a head-on hit
            // beats a grazing one at the same center depth. tNear is already
            // that depth in projectedDistance units. The reported point stays
            // the data point, so a coordinate readout snaps to it.
            if (!sphere.hit || sphere.tNear <= 0.0) {
                continue;
            }
            if (best && sphere.tNear >= best->rayDepth) {
                continue;
            }

            best = PointHit{center, sphere.tNear};
        }
    }

    if (profiling) {
        logPickQuery(QLatin1StringView("exactHit"), taken->nodes.size(), entryDepths,
                     pointsScanned, timer, best);
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
    QList<float> entryDepths;
    qint64 pointsScanned = 0;
    if (profiling) {
        timer.start();
    }

    std::optional<PointHit> best;
    for (const Node& node : taken->nodes) {
        const std::optional<float> entryDepth = rayEntryDepth(
            inflated(node.bounds, tolerancePad(ray, node.bounds, tolerance)), ray);
        if (!entryDepth.has_value()) {
            continue;
        }

        const NodePoints points(node);
        const qsizetype count = points.count();
        if (profiling) {
            entryDepths.append(entryDepth.value());
            pointsScanned += count;
        }

        for (qsizetype i = 0; i < count; i++) {
            const QVector3D point = points.at(i);
            const double rayDepth = ray.projectedDistance(point);
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
    }

    if (profiling) {
        logPickQuery(QLatin1StringView("nearestPoint"), taken->nodes.size(), entryDepths,
                     pointsScanned, timer, best);
    }

    return best;
}
