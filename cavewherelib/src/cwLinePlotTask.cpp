/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwLinePlotTask.h"
#include "cwConcurrent.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCavernTask.h"
#include "cwSurvexportTask.h"
#include "cwSurvexportCSVTask.h"
#include "cwLinePlotGeometryTask.h"
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
#include "cwTask.h"

// Qt includes
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QEventLoop>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QtGlobal>
#include <cmath>

using GeometryLengths = QVector<cwLinePlotGeometryTask::LengthAndDepth>;

namespace {

struct GeometryResult {
    QVector<QVector3D> points;
    QVector<unsigned int> indices;
    GeometryLengths lengths;
};

// Synchronous helpers that reuse cwTask-based implementations without spinning up new threads
template <typename Task>
void runTaskInPlace(Task& task)
{
    task.setUsingThreadPool(false);
    task.start();
    task.waitToFinish(cwTask::IgnoreRestart);
}

} // namespace

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

        QString survexFilename = exportSurvex();
        if (survexFilename.isEmpty()) {
            return result;
        }

        QString threeDFile = runCavern(survexFilename);
        if (threeDFile.isEmpty()) {
            return result;
        }

        QString csvFile = runSurvexport(threeDFile);
        if (csvFile.isEmpty()) {
            return result;
        }

        cwStationPositionLookup stationPositions = parseSurvexport(csvFile);
        updateStationPositionForCaves(stationPositions, result);

        GeometryResult geometry = generateGeometry();
        result.setPositions(geometry.points);
        result.setPlotIndexData(geometry.indices);

        updateDepthLength(geometry.lengths, result);
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

    QString exportSurvex()
    {
        QTemporaryFile survexFile;
        survexFile.setAutoRemove(false);
        if (!survexFile.open()) {
            return QString();
        }
        QString filename = survexFile.fileName();
        survexFile.close();

        cwSurvexExporterRegionTask exporter;
        exporter.setUsingThreadPool(false);
        exporter.setData(InputData.regionData);
        exporter.setOutputFile(filename);
        runTaskInPlace(exporter);

        return QFileInfo::exists(filename) ? filename : QString();
    }

    QString runCavern(const QString& survexFilename)
    {
        cwCavernTask cavernTask;
        cavernTask.setUsingThreadPool(false);
        cavernTask.setSurvexFile(survexFilename);
        runTaskInPlace(cavernTask);
        return QFileInfo::exists(cavernTask.output3dFileName()) ? cavernTask.output3dFileName() : QString();
    }

    QString runSurvexport(const QString& threeDFile)
    {
        cwSurvexportTask survexportTask;
        survexportTask.setUsingThreadPool(false);
        survexportTask.setSurvex3DFile(threeDFile);
        runTaskInPlace(survexportTask);
        return QFileInfo::exists(survexportTask.outputFilename()) ? survexportTask.outputFilename() : QString();
    }

    cwStationPositionLookup parseSurvexport(const QString& csvFile)
    {
        cwSurvexportCSVTask parser;
        parser.setUsingThreadPool(false);
        parser.setSurvexportCSVFile(csvFile);
        runTaskInPlace(parser);
        return parser.stationPositions();
    }

    GeometryResult generateGeometry()
    {
        GeometryResult geometry;
        cwLinePlotGeometryTask geometryTask;
        geometryTask.setUsingThreadPool(false);
        geometryTask.setRegion(Region.data());
        runTaskInPlace(geometryTask);

        geometry.points = geometryTask.pointData();
        geometry.indices = geometryTask.indexData();
        geometry.lengths = geometryTask.cavesLengthAndDepths();
        return geometry;
    }

    bool checkForErrors(cwLinePlotTask::LinePlotResultData& result)
    {
        for(int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);

            cwFindUnconnectedSurveyChunksTask unconnectedTask;
            unconnectedTask.setUsingThreadPool(false);
            unconnectedTask.setCave(cave->data());
            runTaskInPlace(unconnectedTask);
            auto errorResults = unconnectedTask.results();
            if(!errorResults.isEmpty()) {
                cwLinePlotTask::LinePlotCaveData& caveData = createLinePlotCaveDataAt(i, result);
                caveData.setUnconnectedChunkError(errorResults);
            }
        }

        for(int i = 0; i < Region.caveCount(); i++) {
            cwCave* cave = Region.cave(i);
            if(cave->errorModel()->fatalCount() > 0) {
                return false;
            }
        }

        return true;
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

            position.setX(qRound(position.x() * positionFactor) / positionFactor);
            position.setY(qRound(position.y() * positionFactor) / positionFactor);
            position.setZ(qRound(position.z() * positionFactor) / positionFactor);

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

    void updateStationPositionForCaves(const cwStationPositionLookup& stationPostions,
                                       cwLinePlotTask::LinePlotResultData& result)
    {
        indexStations();

        QVector<cwStationPositionLookup> caveStationLookups = splitLookupByCave(stationPostions);

        updateInteralCaveStationLookups(caveStationLookups, result);
        updateExteralCaveStationLookups(result);
    }

    void updateDepthLength(const GeometryLengths& lengths, cwLinePlotTask::LinePlotResultData& result)
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
