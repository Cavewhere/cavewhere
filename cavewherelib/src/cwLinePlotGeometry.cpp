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

QString fullStationName(int caveIndex, const QString& caveName, const QString& stationName)
{
    return QString::number(caveIndex) + QStringLiteral("-") + caveName
           + QStringLiteral(".") + cwStation::canonicalKey(stationName);
}

void addStationPositions(int caveIndex,
                         const cwCavingRegionData& region,
                         QVector<QVector3D>& pointData,
                         QHash<QString, unsigned int>& stationIndexLookup)
{
    const cwCaveData& cave = region.caves.at(caveIndex);

    const QMap<QString, QVector3D> positions = cave.stationPositionModel.positions();
    for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
        const QString fullName = fullStationName(caveIndex, cave.name, iter.key());
        stationIndexLookup.insert(fullName, pointData.size());
        pointData.append(iter.value());
    }
}

void addShotLines(int caveIndex,
                  const cwCavingRegionData& region,
                  const QVector<QVector3D>& pointData,
                  const QHash<QString, unsigned int>& stationIndexLookup,
                  QVector<unsigned int>& indexData,
                  QVector<cwLinePlotGeometry::CaveLengthAndDepth>& cavesLengthAndDepths)
{
    if (pointData.isEmpty()) {
        return;
    }

    const cwCaveData& cave = region.caves.at(caveIndex);

    double minDepth = std::numeric_limits<double>::max();
    double maxDepth = -std::numeric_limits<double>::max();
    double length = 0.0;

    for (int tripIndex = 0; tripIndex < cave.trips.size(); tripIndex++) {
        const cwTripData& trip = cave.trips.at(tripIndex);

        for (const cwSurveyChunkData& chunk : trip.chunks) {
            if (chunk.stations.size() < 2) {
                continue;
            }

            // Empty leading stations (bug #435) won't appear in the lookup —
            // bootstrap from the first station that does.
            int startIndex = 0;
            auto bootstrapIt = stationIndexLookup.constEnd();
            while (startIndex < chunk.stations.size()) {
                auto it = stationIndexLookup.constFind(
                    fullStationName(caveIndex, cave.name,
                                    chunk.stations.at(startIndex).name()));
                if (it != stationIndexLookup.constEnd()) {
                    bootstrapIt = it;
                    break;
                }
                startIndex++;
            }
            if (startIndex >= chunk.stations.size() - 1) {
                continue;
            }

            unsigned int previousStationIndex = bootstrapIt.value();

            QVector3D previousPoint = pointData.at(previousStationIndex);
            minDepth = qMin(minDepth, (double)previousPoint.z());
            maxDepth = qMax(maxDepth, (double)previousPoint.z());

            for (int stationIndex = startIndex + 1;
                 stationIndex < chunk.stations.size();
                 stationIndex++) {
                const cwStation& station = chunk.stations.at(stationIndex);
                const cwShot& shot = chunk.shots.at(stationIndex - 1);

                auto it = stationIndexLookup.constFind(
                    fullStationName(caveIndex, cave.name, station.name()));
                if (it != stationIndexLookup.constEnd()) {
                    unsigned int currentStationIndex = it.value();

                    QVector3D currentPoint = pointData.at(currentStationIndex);
                    if (shot.isDistanceIncluded()) {
                        minDepth = qMin(minDepth, (double)currentPoint.z());
                        maxDepth = qMax(maxDepth, (double)currentPoint.z());
                        length += QVector3D(currentPoint - previousPoint).length();
                    }
                    previousPoint = currentPoint;

                    indexData.append(previousStationIndex);
                    indexData.append(currentStationIndex);

                    previousStationIndex = currentStationIndex;
                }
            }
        }
    }

    const double depth = maxDepth - minDepth;
    cavesLengthAndDepths[caveIndex] = cwLinePlotGeometry::CaveLengthAndDepth(length, depth);
}

} // namespace

Monad::Result<cwLinePlotGeometry::Result>
cwLinePlotGeometry::generate(const cwCavingRegionData& region)
{
    Result result;

    const int caveCount = region.caves.size();
    result.cavesLengthAndDepths.resize(caveCount);

    QHash<QString, unsigned int> stationIndexLookup;

    for (int caveIndex = 0; caveIndex < caveCount; caveIndex++) {
        addStationPositions(caveIndex, region, result.points, stationIndexLookup);
        addShotLines(caveIndex, region, result.points, stationIndexLookup,
                     result.indices, result.cavesLengthAndDepths);
    }

    result.points.squeeze();
    result.indices.squeeze();

    return Monad::Result<Result>(result);
}
