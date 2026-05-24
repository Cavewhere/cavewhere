/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwLinePlotTask.h"
#include "cwConcurrent.h"
#include "cwSurvexExporterRegion.h"
#include "cwCavernRunner.h"
#include "cwSurvex3DFileReader.h"
#include "cwLinePlotGeometry.h"
#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwScrap.h"
#include "cwSurveyChunk.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwStationValidator.h"
#include "cwErrorModel.h"

// Qt includes
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtGlobal>
#include <cmath>

cwLinePlotTask::LinePlotCaveData::LinePlotCaveData() :
    DepthLengthChanged(false),
    Depth(0.0),
    Length(0.0),
    StationPostionsChanged(false),
    NetworkChanged(false)
{
}
void cwLinePlotTask::LinePlotResultData::clear()
{
    Caves.clear();
    Trips.clear();
    Scraps.clear();
    StationPositions.clear();
    LinePlotIndexData.clear();
}

cwLinePlotTask::TripDataPtrs::TripDataPtrs(cwTrip *trip)
{
    Trip = trip;
    foreach(cwNote* note, trip->notes()->notes()) {
        foreach(cwScrap* scrap, note->scraps()) {
            Scraps.append(scrap);
        }
    }
}

cwLinePlotTask::CaveDataPtrs::CaveDataPtrs(cwCave *cave)
{
    Cave = cave;
    foreach(cwTrip* trip, cave->trips()) {
        Trips.append(cwLinePlotTask::TripDataPtrs(trip));
    }
}

cwLinePlotTask::RegionDataPtrs::RegionDataPtrs(const cwCavingRegion *region)
{
    foreach(cwCave* cave, region->caves()) {
        Caves.append(cwLinePlotTask::CaveDataPtrs(cave));
    }
}

cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup(cwCave *cave)
{
    for(int tripIndex = 0; tripIndex < cave->tripCount(); tripIndex++) {
        cwTrip* trip = cave->trip(tripIndex);
        foreach(cwSurveyChunk* surveyChunk, trip->chunks()) {
            foreach(cwStation station, surveyChunk->stations()) {
                MapStationToTrip.insert(station.name().toUpper(), tripIndex);
            }
        }

        int scrapIndex = 0;
        foreach(cwNote* note, trip->notes()->notes()) {
            for(int i = 0; i < note->scraps().size(); i++) {
                cwScrap* scrap = note->scrap(i);

                foreach(cwNoteStation noteStation, scrap->stations()) {
                    MapStationToScrap.insert(noteStation.name().toUpper(), QPair<int, int>(tripIndex, scrapIndex));
                }

                scrapIndex++;
            }
        }
    }
}

struct cwLinePlotTask::LinePlotWorker {
    explicit LinePlotWorker(cwLinePlotTask::Input input)
        : InputData(std::move(input))
    {
    }

    cwLinePlotTask::LinePlotResultData run()
    {
        cwLinePlotTask::LinePlotResultData result;

        if (InputData.regionData.caves.isEmpty()) {
            return result;
        }

        // Prepare working copy of region data
        encodeCaveNames(InputData.regionData);
        Region.setData(InputData.regionData);

        initializeCaveStationLookups();

        if (!checkForErrors(result)) {
            return result;
        }

        // QTemporaryDir owns the lifecycle of the .svx input, the .3d output,
        // and cavern's .log/.err sidecars. Auto-removed on scope exit, so
        // failure on any step below leaves /tmp clean.
        QTemporaryDir workDir;
        if (!workDir.isValid()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Export;
            error.message = QStringLiteral("Failed to create temporary directory for solve");
            result.setSolveError(error);
            return result;
        }

        const QString svxPath      = workDir.filePath(QStringLiteral("region.svx"));
        const QString output3dPath = workDir.filePath(QStringLiteral("region.3d"));

        if (!exportSurvex(svxPath, result)) {
            return result;
        }

        if (!runCavern(svxPath, output3dPath, result)) {
            return result;
        }

        cwSurvex3DFileReader reader;
        cwSurvex3DFileReader::NetworkAndLookup parsed = reader.readNetworkAndLookup(output3dPath);
        if (parsed.lookup.isEmpty()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Parse;
            error.message = QStringLiteral("Cavern produced no station positions in %1").arg(output3dPath);
            result.setSolveError(error);
            return result;
        }
        applyWorldOriginOffset(parsed.lookup, InputData.regionData.worldOrigin);
        updateStationPositionForCaves(parsed.lookup, result);
        result.setRegionNetwork(std::move(parsed.network));

        cwLinePlotGeometry::Result geometry = generateGeometry();
        result.setPositions(geometry.points);
        result.setPlotIndexData(geometry.indices);

        updateDepthLength(geometry.cavesLengthAndDepths, result);
        updateCaveNetworks(result);

        return result;
    }

private:
    cwLinePlotTask::Input InputData;
    cwCavingRegion Region;
    QVector<cwStationPositionLookup> CaveStationLookups;
    QVector<cwLinePlotTask::StationTripScrapLookup> TripLookups;

    void encodeCaveNames(cwCavingRegionData& regionData)
    {
        for(int i = 0; i < regionData.caves.size(); i++) {
            cwCaveData& cave = regionData.caves[i];
            cave.name = QString("%1").arg(i);
        }
    }

    void initializeCaveStationLookups()
    {
        const int numCaves = Region.caveCount();
        CaveStationLookups.resize(numCaves);

        for(int i = 0; i < numCaves; i++) {
            cwCave* cave = Region.cave(i);
            CaveStationLookups[i] = cave->stationPositionLookup();
        }
    }

    bool exportSurvex(const QString& svxPath, cwLinePlotTask::LinePlotResultData& result)
    {
        const Monad::ResultBase r =
            cwSurvexExporterRegion::exportRegion(InputData.regionData, svxPath);
        if (r.hasError()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Export;
            error.message = r.errorMessage();
            result.setSolveError(error);
            return false;
        }
        return true;
    }

    bool runCavern(const QString& svxPath,
                   const QString& output3dPath,
                   cwLinePlotTask::LinePlotResultData& result)
    {
        const Monad::Result<cwCavernRunner::Result> r =
            cwCavernRunner::run(svxPath, output3dPath);
        if (r.hasError()) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Cavern;
            error.exitCode = 1;        // non-zero; cwCavernRunner doesn't expose the precise rc on error
            error.message = r.errorMessage();
            error.cavernLog = r.errorMessage();   // for Cavern step, message == log text
            result.setSolveError(error);
            return false;
        }
        const cwCavernRunner::Result cavern = r.value();
        if (!QFileInfo::exists(cavern.output3dPath)) {
            cwLinePlotTask::SolveError error;
            error.step = cwLinePlotTask::SolveError::Step::Cavern;
            error.exitCode = cavern.exitCode;
            error.message = QStringLiteral("Cavern reported success but produced no .3d output");
            error.cavernLog = cavern.logText;
            error.loopClosureStats = cavern.loopClosureStats;
            result.setSolveError(error);
            return false;
        }
        return true;
    }

    cwLinePlotGeometry::Result generateGeometry()
    {
        const Monad::Result<cwLinePlotGeometry::Result> result =
            cwLinePlotGeometry::generate(Region.data());
        if (result.hasError()) {
            return cwLinePlotGeometry::Result();
        }
        return result.value();
    }

    bool checkForErrors(cwLinePlotTask::LinePlotResultData& result)
    {
        bool hasUnconnectedChunks = false;

        for(int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);

            const Monad::Result<QList<cwFindUnconnectedSurveyChunks::Result>> unconnectedResult =
                cwFindUnconnectedSurveyChunks::find(cave->data());
            if (unconnectedResult.hasError()) {
                continue;
            }
            const QList<cwFindUnconnectedSurveyChunks::Result> errorResults = unconnectedResult.value();
            if(!errorResults.isEmpty()) {
                cwLinePlotTask::LinePlotCaveData& caveData = createLinePlotCaveDataAt(i, result);
                caveData.setUnconnectedChunkError(errorResults);
                hasUnconnectedChunks = true;
            }
        }

        return !hasUnconnectedChunks;
    }

    cwLinePlotTask::LinePlotCaveData& createLinePlotCaveDataAt(int index, cwLinePlotTask::LinePlotResultData& result)
    {
        cwCave* externalCave = InputData.regionPointers.Caves.at(index).Cave;

        if(!result.Caves.contains(externalCave)) {
            result.Caves[externalCave] = cwLinePlotTask::LinePlotCaveData();
        }

        return result.Caves[externalCave];
    }

    void addEmptyStationLookup(int caveIndex, cwLinePlotTask::LinePlotResultData& result)
    {
        cwCave* externalCave = InputData.regionPointers.Caves.at(caveIndex).Cave;
        if(!result.Caves.contains(externalCave)) {
            result.Caves.insert(externalCave, cwLinePlotTask::LinePlotCaveData());
        }
    }

    void indexStations()
    {
        TripLookups.resize(Region.caveCount());

        for(int i = 0; i < Region.caveCount(); i++) {
            TripLookups[i] = cwLinePlotTask::StationTripScrapLookup(Region.cave(i));
        }
    }

    QVector<cwStationPositionLookup> splitLookupByCave(const cwStationPositionLookup &stationPostions) const
    {
        double positionPrecision = 3; //position to 3 digits
        double positionFactor = pow(10.0, positionPrecision);

        QString stationPattern = cwStationValidator::validCharactersRegex().pattern();
        QRegularExpression regex(QString("(\\d+)\\.(%1)").arg(stationPattern));

        QVector<cwStationPositionLookup> caveStations;
        caveStations.resize(CaveStationLookups.size());

        QMapIterator<QString, QVector3D> iter(stationPostions.positions());
        while( iter.hasNext() ) {
            iter.next();

            QString name = iter.key();
            QVector3D position = iter.value();

            // std::round keeps the intermediate value in double; qRound returns
            // int and overflows for UTM-scale coordinates (a 5.47e6m northing
            // multiplied by 1000 already exceeds INT_MAX, and the user-visible
            // crash on projects with no worldOrigin / large fixes traced here).
            position.setX(float(std::round(double(position.x()) * positionFactor) / positionFactor));
            position.setY(float(std::round(double(position.y()) * positionFactor) / positionFactor));
            position.setZ(float(std::round(double(position.z()) * positionFactor) / positionFactor));

            QRegularExpressionMatch match = regex.match(name);
            if(match.hasMatch()) {

                QString caveIndexString = match.captured(1); //Extract the index
                QString stationName = match.captured(2);//Extract the station
                QString caveName = caveIndexString;

                bool okay;
                int caveIndex = caveIndexString.toInt(&okay);

                if(!okay) {
                    qDebug() << "Can't convert caveIndex is not an int:" << caveIndexString << LOCATION;
                    return QVector<cwStationPositionLookup>();
                }

                if(caveIndex < 0 || caveIndex >= Region.caveCount()) {
                    qDebug() << "CaveIndex is bad:" << caveIndex << LOCATION;
                    return QVector<cwStationPositionLookup>();
                }

                cwCave* cave = Region.cave(caveIndex);

                if(caveName.compare(cave->name(), Qt::CaseInsensitive) != 0) {
                    qDebug() << "CaveName is invalid:" << caveName << "looking for" << cave->name() << LOCATION;
                    return QVector<cwStationPositionLookup>();
                }

                cwStationPositionLookup& lookup = caveStations[caveIndex];
                lookup.setPosition(stationName, position);

            } else {
                qDebug() << "Couldn't match: " << name << "This is a bug!" << LOCATION;
            }
        }

        return caveStations;
    }

    void setStationAsChanged(int caveIndex, QString stationName, cwLinePlotTask::LinePlotResultData& result)
    {
        addEmptyStationLookup(caveIndex, result);

        const cwLinePlotTask::StationTripScrapLookup& lookup = TripLookups[caveIndex];
        QList<int> tripIndexes = lookup.trips(stationName.toUpper());
        QList<QPair<int, int> > scrapIndexes = lookup.scraps(stationName.toUpper());

        foreach(int tripIndex, tripIndexes) {
            cwTrip* externalTrip = InputData.regionPointers.Caves.at(caveIndex).Trips.at(tripIndex).Trip;
            result.Trips.insert(externalTrip);
        }

        for(int i = 0; i < scrapIndexes.size(); i++) {
            QPair<int, int> index = scrapIndexes.at(i);
            int tripIndex = index.first;
            int scrapIndex = index.second;

            cwScrap* externalScrap = InputData.regionPointers.Caves.at(caveIndex).Trips.at(tripIndex).Scraps.at(scrapIndex);
            cwTrip* externalTrip = InputData.regionPointers.Caves.at(caveIndex).Trips.at(tripIndex).Trip;

            result.Scraps.insert(externalScrap);
            result.Trips.insert(externalTrip);
        }
    }

    void updateInteralCaveStationLookups(QVector<cwStationPositionLookup> caveStations,
                                         cwLinePlotTask::LinePlotResultData& result)
    {
        for(int i = 0; i < caveStations.size(); i++) {
            cwStationPositionLookup& newLookup = caveStations[i];
            cwStationPositionLookup& oldLookup = CaveStationLookups[i];

            if(newLookup.positions().size() != oldLookup.positions().size()) {
                addEmptyStationLookup(i, result);
            }

            QMap<QString, QVector3D> newPositions = newLookup.positions();
            QMap<QString, QVector3D> oldPositions = oldLookup.positions();

            foreach(QString stationName, newPositions.keys()) {
                if(oldPositions.contains(stationName)) {
                    QVector3D newPoint = newPositions.value(stationName);
                    QVector3D oldPoint = oldPositions.value(stationName);
                    if(newPoint != oldPoint) {
                        setStationAsChanged(i, stationName, result);
                    }
                } else {
                    setStationAsChanged(i, stationName, result);
                }
            }

            CaveStationLookups[i] = caveStations[i];
        }
    }

    void updateExteralCaveStationLookups(cwLinePlotTask::LinePlotResultData& result)
    {
        for(int i = 0; i < Region.caveCount(); i++) {
            cwCave* externalCave = InputData.regionPointers.Caves.at(i).Cave;
            if(result.Caves.contains(externalCave)) {

                cwLinePlotTask::LinePlotCaveData& caveData = result.Caves[externalCave];
                caveData.setStationPositions(CaveStationLookups[i]);

                cwCave* cave = Region.cave(i);
                cave->setStationPositionLookup(CaveStationLookups[i]);
            }
        }
    }

    // Translate every station in lookup by -worldOrigin in place. Cavern
    // emits .3d coordinates in our globalCS; subtracting worldOrigin keeps
    // the position lookup (and downstream geometry) close to (0,0,0) for
    // float precision in shaders. No-op when worldOrigin == (0,0,0), which
    // is the un-fixed-project default.
    static void applyWorldOriginOffset(cwStationPositionLookup& lookup,
                                       const cwGeoPoint& worldOrigin)
    {
        const QVector3D offset = worldOrigin.toVector3D();
        if (offset.isNull()) {
            return;
        }
        const QMap<QString, QVector3D> positions = lookup.positions();
        lookup.clearStations();
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            lookup.setPosition(it.key(), it.value() - offset);
        }
    }

    void updateStationPositionForCaves(const cwStationPositionLookup& stationPostions,
                                       cwLinePlotTask::LinePlotResultData& result)
    {
        indexStations();

        QVector<cwStationPositionLookup> caveStationLookups = splitLookupByCave(stationPostions);

        updateInteralCaveStationLookups(caveStationLookups, result);
        updateExteralCaveStationLookups(result);
    }

    void updateDepthLength(const QVector<cwLinePlotGeometry::CaveLengthAndDepth>& lengths,
                           cwLinePlotTask::LinePlotResultData& result)
    {
        Q_ASSERT(Region.caveCount() == lengths.size());

        for(int i = 0; i < Region.caveCount(); i++) {
            cwLinePlotTask::LinePlotCaveData& caveData = createLinePlotCaveDataAt(i, result);
            caveData.setLength(lengths.at(i).length());
            caveData.setDepth(lengths.at(i).depth());
        }
    }

    void updateCaveNetworks(cwLinePlotTask::LinePlotResultData& result)
    {
        auto createNetwork = [](cwCave* cave) {
            cwSurveyNetwork network;

            foreach(cwTrip* trip, cave->trips()) {
                foreach(cwSurveyChunk* chunk, trip->chunks()) {
                    QList<cwStation> stations = chunk->stations();
                    for(int i = 0; i < stations.size() - 1; i++) {
                        cwStation from = stations.at(i);
                        cwStation to = stations.at(i + 1);
                        network.addShot(from.name(), to.name());
                    }
                }
            }

            return network;
        };

        QList<cwCave*> caves = Region.caves();
        for(int i = 0; i < caves.size(); i++) {
            cwCave* cave = caves.at(i);
            cwSurveyNetwork network = createNetwork(cave);
            cwCave* externalCave = InputData.regionPointers.Caves.at(i).Cave;

            if(network != cave->network()) {
                result.Caves[externalCave].setNetwork(network);

                auto changedStations = cwSurveyNetwork::changedStations(cave->network(), network);
                if(changedStations.isEmpty()) {
                    continue;
                }

                for(const auto& station : changedStations) {
                    setStationAsChanged(i, station, result);
                }
            }
        }
    }
};

cwLinePlotTask::Input cwLinePlotTask::buildInput(const cwCavingRegion *region)
{
    Input input;
    if(region != nullptr) {
        input.regionData = region->data();
        input.regionPointers = RegionDataPtrs(region);
    }
    return input;
}

QFuture<cwLinePlotTask::LinePlotResultData> cwLinePlotTask::run(cwLinePlotTask::Input input)
{
    return cwConcurrent::run([input = std::move(input)]() mutable {
        cwLinePlotTask::LinePlotWorker worker(std::move(input));
        return worker.run();
    });
}
