/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOINTOCTREEPICKSET_H
#define CWPOINTOCTREEPICKSET_H

//Qt includes
#include <QByteArray>
#include <QBox3D>
#include <QMutex>
#include <QVector>

//Std includes
#include <array>
#include <memory>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPickProvider.h"
#include "cwPointOctreePickIndex.h"

//! What a streamed point cloud is picked against: the octree nodes resident on
//! the GPU right now.
/*!
    The render thread publishes a fresh snapshot whenever residency changes —
    an upload, an eviction, a release, a new source — and the GUI thread picks
    against whichever snapshot it takes first. Publishing swaps a shared_ptr
    under a mutex, so a query in flight keeps reading the snapshot it took while
    the next one is being built.

    A node carries the very bytes it uploaded (an implicit share, not a second
    allocation), so a pick dequantizes the same points the GPU draws. Because
    the cloud draws with an identity model matrix, node bytes are already in
    world space once dequantized.

    One set has one publisher. A publish replaces the whole snapshot, so if a
    second renderer ever drove the same cwRenderPointCloud, the two back-ends
    would overwrite each other's residency and a release from the hidden one
    would empty the set the visible one picks against. A scene renders through a
    single cwRhiItemRenderer today; a second one needs a set per back-end and a
    provider that unites them.
*/
class CAVEWHERE_LIB_EXPORT cwPointOctreePickSet : public cwPickProvider
{
public:
    //! One resident node: its world bounds, which are also the cube the
    //! payload is quantized into, that payload (cw::octree::kBytesPerPoint per
    //! point), and the leaf and group boxes a pick descends before it reads
    //! points. An empty index means the whole node is scanned.
    struct Node {
        QBox3D bounds;
        QByteArray bytes;
        cwPointOctreePickIndex index;
    };

    //! Replaces what picks see. @a pickRadius is the world-space sphere radius
    //! around every point the exact pick tests against.
    void publish(QVector<Node> nodes, const QBox3D& rootBounds, float pickRadius);

    QBox3D bounds() const override;
    std::optional<PointHit> exactHit(const QRay3D& ray) const override;
    std::optional<PointHit> nearestPoint(const QRay3D& ray,
                                         const cwPickTolerance& tolerance) const override;

private:
    //! One node's world bounds, packed apart from its Node record.
    /*!
        A query slab-tests every node in the snapshot and then reads the bytes
        of the handful it reaches, so the node loop walks bounds and nothing
        else. Striding the Node records for them touches more than twice the
        cache lines this array does, and a pick arrives with cold caches
        because the render thread has been streaming between picks.
    */
    struct NodeBounds {
        std::array<float, cw::octree::kAxisCount> minimum {0.0f, 0.0f, 0.0f};
        std::array<float, cw::octree::kAxisCount> maximum {0.0f, 0.0f, 0.0f};
    };

    struct Snapshot {
        QVector<Node> nodes;
        //! nodes.at(i)'s bounds, in the same order
        QVector<NodeBounds> bounds;
        QBox3D rootBounds;
        float pickRadius = 0.0f;
    };

    //! The snapshot a query reads, taken once so the whole query is consistent.
    std::shared_ptr<const Snapshot> snapshot() const;

    mutable QMutex m_mutex;
    std::shared_ptr<const Snapshot> m_snapshot;
};

#endif // CWPOINTOCTREEPICKSET_H
