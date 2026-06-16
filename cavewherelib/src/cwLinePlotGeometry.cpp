/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLinePlotGeometry.h"
#include "cwStation.h"

#include <QHash>
#include <QLineF>
#include <QMap>
#include <QString>

#include <limits>

namespace {

// Cave station positions keyed by canonical station name, built once per cave
// so each trip can resolve the world position of any station it references.
QHash<QString, QVector3D> caveStationPositions(const cwCaveData& cave)
{
    const QMap<QString, QVector3D> positions = cave.stationPositionModel.positions();
    QHash<QString, QVector3D> out;
    out.reserve(positions.size());
    for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
        out.insert(cwStation::canonicalKey(iter.key()), iter.value());
    }
    return out;
}

} // namespace

Monad::Result<cwLinePlotGeometry::Result>
cwLinePlotGeometry::generate(const cwCavingRegionData& region)
{
    Result result;

    const int caveCount = region.caves.size();
    result.cavesLengthAndDepths.resize(caveCount);

    quint32 runningTripId = 0;

    for (int caveIndex = 0; caveIndex < caveCount; caveIndex++) {
        const cwCaveData& cave = region.caves.at(caveIndex);
        const QHash<QString, QVector3D> stationPositions = caveStationPositions(cave);

        double minDepth = std::numeric_limits<double>::max();
        double maxDepth = -std::numeric_limits<double>::max();
        double length = 0.0;
        bool hasDepth = false;

        for (int tripIndex = 0; tripIndex < cave.trips.size(); tripIndex++) {
            const cwTripData& trip = cave.trips.at(tripIndex);

            // Every trip gets a running id in iteration order, even ones that
            // emit no geometry, so tripUuids stays a dense total-trip-count list
            // aligned with the per-vertex ids.
            const quint32 tripId = runningTripId++;
            result.tripUuids.append(trip.id);

            // De-share across trips: each trip owns its vertices. A station
            // reused by consecutive shots within this trip is one vertex, but a
            // tie-in station shared with another trip is duplicated — so both
            // endpoints of any segment carry the same tripId.
            QHash<QString, unsigned int> tripVertexLookup;

            const auto vertexFor = [&](const QString& stationName) -> int {
                const QString key = cwStation::canonicalKey(stationName);
                auto existing = tripVertexLookup.constFind(key);
                if (existing != tripVertexLookup.constEnd()) {
                    return int(existing.value());
                }
                auto posIt = stationPositions.constFind(key);
                if (posIt == stationPositions.constEnd()) {
                    return -1;
                }
                const unsigned int index = static_cast<unsigned int>(result.points.size());
                result.points.append(posIt.value());
                result.tripIds.append(tripId);
                tripVertexLookup.insert(key, index);
                return int(index);
            };

            for (const cwSurveyChunkData& chunk : trip.chunks) {
                if (chunk.stations.size() < 2) {
                    continue;
                }

                // Empty leading stations (bug #435) have no position —
                // bootstrap from the first station that resolves to a vertex.
                int startIndex = 0;
                int previousVertex = -1;
                while (startIndex < chunk.stations.size()) {
                    previousVertex = vertexFor(chunk.stations.at(startIndex).name());
                    if (previousVertex >= 0) {
                        break;
                    }
                    startIndex++;
                }
                if (previousVertex < 0 || startIndex >= chunk.stations.size() - 1) {
                    continue;
                }

                QVector3D previousPoint = result.points.at(previousVertex);
                minDepth = qMin(minDepth, (double)previousPoint.z());
                maxDepth = qMax(maxDepth, (double)previousPoint.z());
                hasDepth = true;

                for (int stationIndex = startIndex + 1;
                     stationIndex < chunk.stations.size();
                     stationIndex++) {
                    const cwStation& station = chunk.stations.at(stationIndex);
                    const cwShot& shot = chunk.shots.at(stationIndex - 1);

                    const int currentVertex = vertexFor(station.name());
                    if (currentVertex >= 0) {
                        const QVector3D currentPoint = result.points.at(currentVertex);
                        if (shot.isDistanceIncluded()) {
                            minDepth = qMin(minDepth, (double)currentPoint.z());
                            maxDepth = qMax(maxDepth, (double)currentPoint.z());
                            length += QVector3D(currentPoint - previousPoint).length();
                        }
                        previousPoint = currentPoint;

                        result.indices.append(static_cast<unsigned int>(previousVertex));
                        result.indices.append(static_cast<unsigned int>(currentVertex));

                        previousVertex = currentVertex;
                    }
                }
            }
        }

        if (hasDepth) {
            const double depth = maxDepth - minDepth;
            result.cavesLengthAndDepths[caveIndex] =
                cwLinePlotGeometry::CaveLengthAndDepth(length, depth);
        }
    }

    result.points.squeeze();
    result.indices.squeeze();
    result.tripIds.squeeze();
    result.tripUuids.squeeze();

    return Monad::Result<Result>(std::move(result));
}
