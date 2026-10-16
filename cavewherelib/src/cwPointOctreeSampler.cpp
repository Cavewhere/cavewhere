// cwPointOctreeSampler.cpp
#include "cwPointOctreeSampler.h"

//Qt includes
#include <QHash>
#include <QSemaphore>
#include <QThreadPool>

//Our includes
#include "cwTask.h"

//Std includes
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

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

namespace {
    /**
     * Points a footprint cell holds on average, at least, at the finest grid
     * occupiedCellSpacing() measures on. Four keeps every cell at least two
     * spacings wide, so a filled footprint occupies every cell inside it, and
     * on randomly scattered points, where some cells come out empty, it reads
     * within about a percent of the true spacing.
     */
    constexpr int kMinPointsPerFootprintCell = 4;

    //Finest footprint grid, in halvings of the wide side; keeps cell keys in a quint64
    constexpr int kMaxFootprintHalvings = 30;

    struct Footprint {
        double minX = 0.0;
        double minY = 0.0;
        double width = 0.0;
        double height = 0.0;
    };

    Footprint footprintOf(const QVector<QVector3D>& points)
    {
        double minX = points.constFirst().x();
        double maxX = minX;
        double minY = points.constFirst().y();
        double maxY = minY;
        for(const QVector3D& point : points) {
            minX = std::min(minX, double(point.x()));
            maxX = std::max(maxX, double(point.x()));
            minY = std::min(minY, double(point.y()));
            maxY = std::max(maxY, double(point.y()));
        }
        return Footprint {minX, minY, maxX - minX, maxY - minY};
    }

    /**
     * sqrt(area / count) over the footprint's bounding rectangle. A footprint
     * narrower than one spacing is a line of points, and spreading them over
     * its area would read far too dense, so it counts as one spacing wide:
     * count * s^2 = wide * s, s = wide / count. The two rules meet where
     * narrow == wide / count, so the estimate is continuous.
     */
    double rectangleSpacing(const Footprint& footprint, double count)
    {
        const double narrow = std::min(footprint.width, footprint.height);
        const double wide = std::max(footprint.width, footprint.height);
        const double areaSpacing = std::sqrt(narrow * wide / count);
        return narrow >= areaSpacing ? areaSpacing : wide / count;
    }

    //! Cells of side wide / 2^halvings that hold at least one of @a points
    qsizetype occupiedCells(const QVector<QVector3D>& points, const Footprint& footprint,
                            int halvings)
    {
        const double wide = std::max(footprint.width, footprint.height);
        const qint64 cellsPerSide = qint64(1) << halvings;
        const double cellSize = wide / double(cellsPerSide);
        const auto cellsAlong = [&](double extent) {
            return std::clamp(qint64(std::ceil(extent / cellSize)), qint64(1), cellsPerSide);
        };
        const qint64 columns = cellsAlong(footprint.width);
        const qint64 rows = cellsAlong(footprint.height);
        const auto cellOf = [&](double offset, qint64 cells) {
            return std::clamp(qint64(offset / cellSize), qint64(0), cells - 1);
        };

        std::vector<quint64> keys;
        keys.reserve(size_t(points.size()));
        for(const QVector3D& point : points) {
            const qint64 column = cellOf(double(point.x()) - footprint.minX, columns);
            const qint64 row = cellOf(double(point.y()) - footprint.minY, rows);
            keys.push_back(quint64(row) * quint64(columns) + quint64(column));
        }
        std::sort(keys.begin(), keys.end());
        return qsizetype(std::unique(keys.begin(), keys.end()) - keys.begin());
    }

    /**
     * sqrt(occupied cell area / count) on the finest power-of-two grid over
     * the footprint whose occupied cells hold kMinPointsPerFootprintCell
     * points on average. Halving a cell never empties its parent, so the
     * occupied count only grows with each halving and the finest such grid is
     * found by bisection. Infinite when there are too few points to fill one
     * cell, which leaves the rectangle to measure them.
     */
    double occupiedCellSpacing(const QVector<QVector3D>& points, const Footprint& footprint)
    {
        const qsizetype count = points.size();
        const double wide = std::max(footprint.width, footprint.height);
        if(count < kMinPointsPerFootprintCell || wide <= 0.0) {
            return std::numeric_limits<double>::infinity();
        }

        const auto holdsEnough = [&](int halvings) {
            return occupiedCells(points, footprint, halvings) * kMinPointsPerFootprintCell
                   <= count;
        };

        //One cell holds every point; past log2(count) halvings a line of
        //points already has a cell to itself per point
        int coarse = 0;
        int fine = std::min(int(std::ceil(std::log2(double(count)))) + 1, kMaxFootprintHalvings);
        while(fine - coarse > 1) {
            const int middle = (coarse + fine) / 2;
            if(holdsEnough(middle)) {
                coarse = middle;
            } else {
                fine = middle;
            }
        }
        if(holdsEnough(fine)) {
            coarse = fine;
        }

        const double cellSize = wide / double(qint64(1) << coarse);
        const double occupied = double(occupiedCells(points, footprint, coarse));
        return cellSize * std::sqrt(occupied / double(count));
    }
}

float measureNodeSpacing(const QVector<QVector3D>& points)
{
    //A single point has no neighbor to be spaced from
    if(points.size() < 2) {
        return 0.0f;
    }

    /*
     * Aerial LiDAR is a 2.5D surface, so the spacing is the side of the square
     * each point covers on the footprint the points occupy: sqrt(area / count)
     * over the xy extent of the points, not of the node, so a node the data
     * only partly fills is measured over the part it fills.
     *
     * The primary area is the points' bounding rectangle: it moves by a
     * quantization step when the payload is quantized, so a node measures the
     * same before and after it is written, and it is exact on a footprint the
     * points fill. A diagonal strip, an L of coverage or two separated clusters
     * leave most of their rectangle empty and would read several times too
     * sparse, so the occupied footprint cells cap it. Where the points fill
     * their rectangle every cell inside it is occupied and the cells measure
     * at least the rectangle, which keeps the rectangle in charge.
     *
     * Returns that stack in depth (canopy, walls) put more points over the
     * same footprint, so they read denser than the surface, which only ever
     * errs toward the grid spacing that Measured falls back to.
     */
    const Footprint footprint = footprintOf(points);
    const double count = double(points.size());
    const double spacing = std::min(rectangleSpacing(footprint, count),
                                    occupiedCellSpacing(points, footprint));
    return float(spacing);
}

namespace {
    //! Bits an axis of an index key takes, three axes interleaved in a quint64
    constexpr int kIndexAxisBits = 21;
    constexpr qint64 kIndexAxisCells = qint64(1) << kIndexAxisBits;
    //! The extent an index over a root with no size spans, so its cells keep a size
    constexpr double kEmptyIndexExtent = 1.0;

    //! The search radius under which every k-th neighbor reads the floor
    double minimumSearchRadius()
    {
        return kMinLocalSpacingUnits / kLocalSpacingSurfaceFactor;
    }

    //! @a x, @a y and @a z's bits interleaved, x lowest
    quint64 interleavedKey(quint64 x, quint64 y, quint64 z)
    {
        quint64 key = 0;
        for(int bit = 0; bit < kIndexAxisBits; bit++) {
            key |= ((x >> bit) & 1u) << (kAxisCount * bit);
            key |= ((y >> bit) & 1u) << (kAxisCount * bit + 1);
            key |= ((z >> bit) & 1u) << (kAxisCount * bit + 2);
        }
        return key;
    }

    double squaredDistance(const QVector3D& a, const QVector3D& b)
    {
        double sum = 0.0;
        for(int axis = 0; axis < kAxisCount; axis++) {
            const double delta = double(a[axis]) - double(b[axis]);
            sum += delta * delta;
        }
        return sum;
    }

    /**
     * The kLocalSpacingNeighbors nearest distinct positions offered, nearest
     * first. A position equal to the query's, or to one already held, counts
     * once: duplicate returns are one neighbor, however many of them there are.
     */
    class DistinctNearest {
    public:
        void clear() { m_size = 0; }
        bool full() const { return m_size == kLocalSpacingNeighbors; }
        double farthestSquared() const { return m_neighbors.at(size_t(m_size - 1)).squaredDistance; }

        void offer(double squared, const QVector3D& position)
        {
            if(squared <= 0.0 || (full() && squared >= farthestSquared())) {
                return;
            }
            //Bit-identical positions sit at the same distance, so only those compare
            for(int i = 0; i < m_size; i++) {
                const Neighbor& held = m_neighbors.at(size_t(i));
                if(held.squaredDistance == squared && held.position == position) {
                    return;
                }
            }
            int slot = full() ? m_size - 1 : m_size;
            m_size = full() ? m_size : m_size + 1;
            for(; slot > 0 && m_neighbors.at(size_t(slot - 1)).squaredDistance > squared; slot--) {
                m_neighbors[size_t(slot)] = m_neighbors.at(size_t(slot - 1));
            }
            m_neighbors[size_t(slot)] = Neighbor {squared, position};
        }

    private:
        struct Neighbor {
            double squaredDistance = 0.0;
            QVector3D position;
        };
        std::array<Neighbor, kLocalSpacingNeighbors> m_neighbors = {};
        int m_size = 0;
    };

    /**
     * Every point of a subtree, sorted by the interleaved key of the finest
     * cube it sits in, so any coarser cube of the same grid is one contiguous
     * run: the box around a point, at the coarsest level whose cubes are at
     * least as wide as the box, is at most eight runs.
     */
    class NativeSpacingIndex {
    public:
        NativeSpacingIndex(const QVector<SampledNode>& nodes, const QVector3D& origin,
                           double extent) :
            m_origin(origin),
            m_cellSize(extent / double(kIndexAxisCells))
        {
            qsizetype total = 0;
            for(const SampledNode& node : nodes) {
                total += node.points.size();
            }
            m_entries.reserve(size_t(total));
            for(const SampledNode& node : nodes) {
                for(const QVector3D& point : node.points) {
                    m_entries.push_back(Entry {keyOf(point), point});
                }
            }
            std::sort(m_entries.begin(), m_entries.end(),
                      [](const Entry& left, const Entry& right) { return left.key < right.key; });
        }

        quint64 keyOf(const QVector3D& point) const
        {
            const std::array<qint64, kAxisCount> cell = cellOf(point, 0.0);
            return interleavedKey(quint64(cell[0]), quint64(cell[1]), quint64(cell[2]));
        }

        /**
         * The distance from @a point to its kLocalSpacingNeighbors-th nearest
         * distinct neighbor, searched from @a radius outward, or infinity when
         * fewer than that many lie within @a reach. Each search finds every
         * point within its guaranteed radius, the distance from @a point to the
         * outside of the cubes it read, so a k-th neighbor inside that radius is
         * exact; past it, the search widens to the k-th neighbor it found, or
         * doubles where it found too few.
         */
        double kthDistinctDistance(const QVector3D& point, double radius, double reach) const
        {
            DistinctNearest nearest;
            radius = std::clamp(radius, minimumSearchRadius(), reach);
            for(;;) {
                const bool widest = radius >= reach;
                const double guaranteed = search(point, radius, nearest);
                if(nearest.full() && nearest.farthestSquared() <= guaranteed * guaranteed) {
                    return std::sqrt(nearest.farthestSquared());
                }
                if(widest || guaranteed >= reach) {
                    return std::numeric_limits<double>::infinity();
                }
                const double next = nearest.full() ? std::sqrt(nearest.farthestSquared())
                                                   : 2.0 * guaranteed;
                radius = std::min(std::max(next, 2.0 * radius), reach);
            }
        }

    private:
        struct Entry {
            quint64 key = 0;
            QVector3D point;
        };

        QVector3D m_origin;
        double m_cellSize = 0.0;
        std::vector<Entry> m_entries;

        std::array<qint64, kAxisCount> cellOf(const QVector3D& point, double offset) const
        {
            std::array<qint64, kAxisCount> cell = {};
            for(int axis = 0; axis < kAxisCount; axis++) {
                const double scaled =
                    std::floor((double(point[axis]) + offset - double(m_origin[axis])) / m_cellSize);
                cell[axis] = qint64(std::clamp(scaled, 0.0, double(kIndexAxisCells - 1)));
            }
            return cell;
        }

        //! Offers @a nearest every point in the cubes the box of half-width
        //! @a radius around @a point touches, at the coarsest level whose cubes
        //! are as wide as the box; returns the radius inside which it read
        //! every point
        double search(const QVector3D& point, double radius, DistinctNearest& nearest) const
        {
            const int level = std::clamp(int(std::ceil(std::log2(2.0 * radius / m_cellSize))), 0,
                                         kIndexAxisBits);
            const qint64 lastCube = (kIndexAxisCells - 1) >> level;
            const std::array<qint64, kAxisCount> low = cellOf(point, -radius);
            const std::array<qint64, kAxisCount> high = cellOf(point, radius);
            std::array<qint64, kAxisCount> first = {};
            std::array<qint64, kAxisCount> last = {};
            double guaranteed = std::numeric_limits<double>::infinity();
            const double cubeSize = std::ldexp(m_cellSize, level);
            for(int axis = 0; axis < kAxisCount; axis++) {
                first[axis] = low[axis] >> level;
                last[axis] = high[axis] >> level;
                const double value = double(point[axis]) - double(m_origin[axis]);
                //Nothing lies past the index's own edge, so the edge cube bounds nothing
                if(first[axis] > 0) {
                    guaranteed = std::min(guaranteed, value - double(first[axis]) * cubeSize);
                }
                if(last[axis] < lastCube) {
                    guaranteed = std::min(guaranteed, double(last[axis] + 1) * cubeSize - value);
                }
            }

            nearest.clear();
            const int shift = kAxisCount * level;
            const auto before = [](const Entry& entry, quint64 key) { return entry.key < key; };
            for(qint64 z = first[2]; z <= last[2]; z++) {
                for(qint64 y = first[1]; y <= last[1]; y++) {
                    for(qint64 x = first[0]; x <= last[0]; x++) {
                        const quint64 cube = interleavedKey(quint64(x), quint64(y), quint64(z));
                        const quint64 begin = cube << shift;
                        const quint64 end = (cube + 1) << shift;
                        auto entry = std::lower_bound(m_entries.cbegin(), m_entries.cend(), begin,
                                                      before);
                        for(; entry != m_entries.cend() && entry->key < end; ++entry) {
                            nearest.offer(squaredDistance(entry->point, point), entry->point);
                        }
                    }
                }
            }
            return guaranteed;
        }
    };

    /**
     * Calls @a visit for every index under @a count, on the calling thread
     * and on whichever of cwTask::threadPool()'s threads are idle right now.
     * A helper only starts on a free thread and never waits on anything, so
     * a caller that is itself a pool task finishes even while the pool is
     * full: a nested blockingMap there waits on tasks no thread is left to
     * run.
     */
    template<typename Visit>
    void forEachIndexWithIdleThreads(qsizetype count, const Visit& visit)
    {
        std::atomic<qsizetype> next {0};
        const auto work = [&]() {
            for(qsizetype index = next.fetch_add(1); index < count; index = next.fetch_add(1)) {
                visit(index);
            }
        };

        QThreadPool* pool = cwTask::threadPool();
        QSemaphore finished;
        int helpers = 0;
        const int wanted = int(std::min(qsizetype(pool->maxThreadCount()), count - 1));
        for(; helpers < wanted; helpers++) {
            const bool started = pool->tryStart([&]() {
                work();
                finished.release();
            });
            if(!started) {
                break;
            }
        }
        work();
        finished.acquire(helpers);
    }

    //! The native spacing of each of @a points against @a index, none wider
    //! than @a cap; walked in key order, so each search starts from its
    //! neighbor's answer and a run of duplicates searches once
    QVector<float> nativeSpacings(const NativeSpacingIndex& index,
                                  const QVector<QVector3D>& points, double cap)
    {
        std::vector<std::pair<quint64, qint32>> order;
        order.reserve(size_t(points.size()));
        for(qsizetype i = 0; i < points.size(); i++) {
            order.emplace_back(index.keyOf(points.at(i)), qint32(i));
        }
        std::sort(order.begin(), order.end());

        const double ceiling = std::max(cap, kMinLocalSpacingUnits);
        const double reach = ceiling / kLocalSpacingSurfaceFactor;
        QVector<float> spacings(points.size(), 0.0f);
        double guess = minimumSearchRadius();
        std::optional<QVector3D> previous;
        float previousSpacing = 0.0f;
        for(const auto& [key, i] : order) {
            const QVector3D& point = points.at(i);
            if(previous.has_value() && *previous == point) {
                spacings[i] = previousSpacing;
                continue;
            }
            const double distance = index.kthDistinctDistance(point, guess, reach);
            const bool found = std::isfinite(distance);
            const double spacing = found ? distance * kLocalSpacingSurfaceFactor : ceiling;
            guess = found ? distance : guess;
            previous = point;
            previousSpacing = float(std::clamp(spacing, kMinLocalSpacingUnits, ceiling));
            spacings[i] = previousSpacing;
        }
        return spacings;
    }
}

QVector<QVector<float>> estimateNativeSpacing(const QVector<SampledNode>& nodes,
                                              const QVector3D& rootMin, double rootSize,
                                              int firstCodedLevel,
                                              std::optional<int> firstNodeCapLevel)
{
    //A std::vector, so the workers write their own entries without a shared
    //detach check
    std::vector<QVector<float>> spacings(size_t(nodes.size()));
    std::vector<qsizetype> coded;
    for(qsizetype i = 0; i < nodes.size(); i++) {
        if(nodes.at(i).cell.level >= firstCodedLevel) {
            coded.push_back(i);
        }
    }

    if(!coded.empty()) {
        const NativeSpacingIndex index(nodes, rootMin,
                                       rootSize > 0.0 ? rootSize : kEmptyIndexExtent);
        forEachIndexWithIdleThreads(qsizetype(coded.size()), [&](qsizetype i) {
            const qsizetype node = coded.at(size_t(i));
            const int capLevel = node == 0 && firstNodeCapLevel.has_value()
                                     ? *firstNodeCapLevel
                                     : nodes.at(node).cell.level;
            spacings[size_t(node)] = nativeSpacings(index, nodes.at(node).points,
                                                    levelLocalSpacingCap(rootSize, capLevel));
        });
    }
    return QVector<QVector<float>>(std::make_move_iterator(spacings.begin()),
                                   std::make_move_iterator(spacings.end()));
}

QVector<quint16> encodeLocalSpacings(const QVector<float>& spacings, double cap)
{
    QVector<quint16> codes;
    codes.reserve(spacings.size());
    for(float spacing : spacings) {
        codes.append(encodeLocalSpacing(std::min(double(spacing), cap)));
    }
    return codes;
}

QVector<SampledNode> buildSubtree(QVector<QVector3D> points,
                                  const Cell& root,
                                  const QVector3D& rootMin,
                                  double rootSize,
                                  int leafMaxPoints,
                                  int minSplitPoints)
{
    QVector<SampledNode> nodes;

    if(root.level >= kMaxLevel) {
        nodes.append(SampledNode{root, std::move(points)});
        return nodes;
    }

    const QBox3D bounds = cellBounds(root, rootMin, rootSize);

    //A cell under the leaf threshold still samples at its own level, so the
    //points it draws sit at the spacing its level promises; whatever the grid
    //leaves behind splits into children like any other cell, unless there is
    //too little of it to be worth a subtree, in which case it stays here and
    //the cell is a leaf. Either way the cell holds at most what it sampled
    //from, which is under leafMaxPoints.
    QVector<QVector3D> ownSample;
    const bool sampledFirst = points.size() <= leafMaxPoints;
    if(sampledFirst) {
        ownSample = gridSample(bounds, kSampleGridResolution, points);
        if(points.size() < minSplitPoints) {
            ownSample.append(points);
            points = QVector<QVector3D>();
        }
        if(points.isEmpty()) {
            nodes.append(SampledNode{root, std::move(ownSample)});
            return nodes;
        }
    }

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
                                          leafMaxPoints,
                                          minSplitPoints);
        sampleSources.append(&childNodes[octant].first().points);
    }

    nodes.append(SampledNode{root, sampledFirst ? std::move(ownSample) : sampleUp(bounds, sampleSources)});

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
