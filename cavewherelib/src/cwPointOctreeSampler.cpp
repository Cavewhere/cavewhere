// cwPointOctreeSampler.cpp
#include "cwPointOctreeSampler.h"

//Qt includes
#include <QHash>

//Std includes
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    constexpr int kAxisCount = 3;

    //Enough headroom that resolution^3 stays inside a quint32 packed cell index
    constexpr int kMaxGridResolution = 1024;

    /**
     * Clamping happens in float so the cast is always in range: fmax() and
     * fmin() return the non-NaN operand, so a NaN or an astronomical
     * coordinate lands in a real cell instead of tripping undefined behavior.
     */
    int cellCoordinate(float value, float minimum, float inverseCellSize, int resolution)
    {
        const float scaled = std::fmin(std::fmax((value - minimum) * inverseCellSize, 0.0f),
                                       static_cast<float>(resolution - 1));
        return static_cast<int>(scaled);
    }

    /**
     * The shared body of gridSample() and sampleUp(): one grid over bounds, one
     * winner per occupied cell across every group, removed from the group it
     * came from. Groups are visited in order and points inside a group in
     * order, so the result is deterministic.
     */
    QVector<QVector3D> sampleGroups(const QBox3D& bounds,
                                    int resolution,
                                    const QVector<QVector<QVector3D>*>& groups)
    {
        if(resolution <= 0 || resolution > kMaxGridResolution) {
            return QVector<QVector3D>();
        }

        qsizetype total = 0;
        for(QVector<QVector3D>* group : groups) {
            total += group->size();
        }

        if(total == 0) {
            return QVector<QVector3D>();
        }

        const QVector3D minimum = bounds.minimum();
        const QVector3D size = bounds.size();

        std::array<float, kAxisCount> cellSize = {};
        std::array<float, kAxisCount> inverseCellSize = {};
        for(int axis = 0; axis < kAxisCount; axis++) {
            cellSize[axis] = size[axis] / resolution;
            inverseCellSize[axis] = cellSize[axis] > 0.0f ? 1.0f / cellSize[axis] : 0.0f;
        }

        const qsizetype cellCount = qsizetype(resolution) * resolution * resolution;

        QHash<quint32, qsizetype> bestForCell;
        bestForCell.reserve(std::min(total, cellCount));

        //Parallel to the concatenated groups, so the winner's distance is one lookup away
        QVector<float> bestDistance(total);
        QVector<bool> taken(total, false);

        qsizetype flatIndex = 0;
        for(QVector<QVector3D>* group : groups) {
            for(const QVector3D& point : std::as_const(*group)) {
                std::array<int, kAxisCount> coordinate = {};
                float distance = 0.0f;
                for(int axis = 0; axis < kAxisCount; axis++) {
                    coordinate[axis] = cellCoordinate(point[axis],
                                                      minimum[axis],
                                                      inverseCellSize[axis],
                                                      resolution);
                    const float center = minimum[axis] + (coordinate[axis] + 0.5f) * cellSize[axis];
                    const float offset = point[axis] - center;
                    distance += offset * offset;
                }

                bestDistance[flatIndex] = distance;

                const quint32 packed = (quint32(coordinate[2]) * resolution + coordinate[1]) * resolution
                                       + coordinate[0];

                const auto found = bestForCell.find(packed);
                if(found == bestForCell.end()) {
                    bestForCell.insert(packed, flatIndex);
                } else if(distance < bestDistance.at(found.value())) {
                    found.value() = flatIndex;
                }

                flatIndex++;
            }
        }

        for(auto it = bestForCell.constBegin(); it != bestForCell.constEnd(); ++it) {
            taken[it.value()] = true;
        }

        QVector<QVector3D> sampled;
        sampled.reserve(bestForCell.size());

        flatIndex = 0;
        for(QVector<QVector3D>* group : groups) {
            QVector3D* points = group->data();
            const qsizetype count = group->size();

            qsizetype write = 0;
            for(qsizetype i = 0; i < count; i++, flatIndex++) {
                if(taken.at(flatIndex)) {
                    sampled.append(points[i]);
                } else {
                    points[write] = points[i];
                    write++;
                }
            }

            group->resize(write);
        }

        return sampled;
    }

    bool isEmptyLeaf(const cw::octree::SampledNode& node)
    {
        return node.points.isEmpty()
               && std::all_of(node.children.begin(), node.children.end(), [](int child) { return child < 0; });
    }
}

namespace cw::octree {

QBox3D cellBounds(const Cell& cell, const QVector3D& rootMin, double rootSize)
{
    const double edge = std::ldexp(rootSize, -cell.level);

    const QVector3D minimum(static_cast<float>(rootMin.x() + cell.x * edge),
                            static_cast<float>(rootMin.y() + cell.y * edge),
                            static_cast<float>(rootMin.z() + cell.z * edge));
    const float size = static_cast<float>(edge);

    return QBox3D(minimum, minimum + QVector3D(size, size, size));
}

Cell childCell(const Cell& parent, int octant)
{
    return Cell {
        parent.level + 1,
        parent.x * 2 + quint32(octant & 1),
        parent.y * 2 + quint32((octant >> 1) & 1),
        parent.z * 2 + quint32((octant >> 2) & 1)
    };
}

int octantOf(const QVector3D& point, const QBox3D& parentBounds)
{
    const QVector3D minimum = parentBounds.minimum();
    const QVector3D half = parentBounds.size() * 0.5f;

    int octant = 0;
    for(int axis = 0; axis < kAxisCount; axis++) {
        if(half[axis] <= 0.0f) {
            continue;
        }

        const float side = std::fmin(std::fmax((point[axis] - minimum[axis]) / half[axis], 0.0f), 1.0f);
        octant |= static_cast<int>(side) << axis;
    }

    return octant;
}

QVector<QVector3D> gridSample(const QBox3D& bounds, int resolution, QVector<QVector3D>& points)
{
    return sampleGroups(bounds, resolution, {&points});
}

QVector<QVector3D> sampleUp(const QBox3D& parentBounds, const QVector<QVector<QVector3D>*>& childrenPoints)
{
    return sampleGroups(parentBounds, kSampleGridResolution, childrenPoints);
}

QVector<SampledNode> buildSubtree(QVector<QVector3D> points,
                                  const Cell& root,
                                  const QVector3D& rootMin,
                                  double rootSize,
                                  int leafMaxPoints)
{
    QVector<SampledNode> nodes;

    if(points.size() <= leafMaxPoints || root.level >= kMaxLevel) {
        nodes.append(SampledNode{root, std::move(points)});
        return nodes;
    }

    const QBox3D bounds = cellBounds(root, rootMin, rootSize);

    std::array<QVector<QVector3D>, kChildCount> childPoints;
    const qsizetype childGuess = points.size() / kChildCount + 1;
    for(QVector<QVector3D>& octantPoints : childPoints) {
        octantPoints.reserve(childGuess);
    }

    for(const QVector3D& point : std::as_const(points)) {
        childPoints[octantOf(point, bounds)].append(point);
    }
    points = QVector<QVector3D>();

    std::array<QVector<SampledNode>, kChildCount> childNodes;
    QVector<QVector<QVector3D>*> sampleSources;
    sampleSources.reserve(kChildCount);

    for(int octant = 0; octant < kChildCount; octant++) {
        if(childPoints[octant].isEmpty()) {
            continue;
        }

        childNodes[octant] = buildSubtree(std::move(childPoints[octant]),
                                          childCell(root, octant),
                                          rootMin,
                                          rootSize,
                                          leafMaxPoints);
        sampleSources.append(&childNodes[octant].first().points);
    }

    nodes.append(SampledNode{root, sampleUp(bounds, sampleSources)});

    for(int octant = 0; octant < kChildCount; octant++) {
        QVector<SampledNode>& subtree = childNodes[octant];
        if(subtree.isEmpty() || isEmptyLeaf(subtree.constFirst())) {
            continue;
        }

        const int offset = nodes.size();
        nodes.first().children[octant] = offset;
        nodes.reserve(nodes.size() + subtree.size());

        for(SampledNode& node : subtree) {
            for(int& child : node.children) {
                if(child >= 0) {
                    child += offset;
                }
            }
            nodes.append(std::move(node));
        }
    }

    return nodes;
}

}
