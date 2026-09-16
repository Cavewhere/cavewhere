// cwPointOctreeManifest.h
#pragma once

//Qt includes
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVector3D>

//Std includes
#include <array>
#include <optional>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPointOctree.h"
#include "qbox3d.h"

/**
 * One node of the octree. The cell coordinates address the node inside the
 * root cube at its level, so its bounds are derived rather than stored.
 */
struct CAVEWHERE_LIB_EXPORT cwPointOctreeNode {
    int level = 0;
    quint32 x = 0;
    quint32 y = 0;
    quint32 z = 0;
    quint32 pointCount = 0;
    qint64 byteSize = 0;    //pointCount * cw::octree::kBytesPerPoint

    //Indices into cwPointOctreeManifest::nodes, -1 when the octant is empty
    std::array<int, cw::octree::kChildCount> children = {-1, -1, -1, -1, -1, -1, -1, -1};
};

/**
 * The table of contents of a point octree: the root cube, the data bounds, and
 * every node, depth first with the root at index 0. It is a copyable value
 * type, so a worker takes its own snapshot; nodeName() caches a parent table,
 * so one instance belongs to one thread.
 */
class CAVEWHERE_LIB_EXPORT cwPointOctreeManifest
{
public:
    QVector3D rootMin;
    double rootSize = 0.0;

    qint64 pointCount = 0;
    QVector3D bboxMin;
    QVector3D bboxMax;

    float meanSpacingXY = 0.0f;
    QString fingerprint;

    QVector<cwPointOctreeNode> nodes;

    QBox3D nodeBounds(int index) const;
    double spacing(int level) const;
    QString nodeName(int index) const;

    bool isValid() const;

    QByteArray serialize() const;
    static std::optional<cwPointOctreeManifest> deserialize(const QByteArray& data);

    //! Parent index per node, -1 for the root. Built on first use and cached,
    //! so it belongs to the thread holding this manifest.
    const QVector<int>& parents() const;

private:
    //Parent index per node, rebuilt when its size stops matching nodes
    mutable QVector<int> m_parents;
};
