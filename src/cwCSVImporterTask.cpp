//Our includes
#include "cwCSVImporterTask.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwUnits.h"
#include "cwTripCalibration.h"
#include "cwColumnNameModel.h"

cwCSVImporterTask::cwCSVImporterTask()
{

}

void cwCSVImporterTask::setSettings(const cwCSVImporterSettings &settings)
{
    if(isReady()) {
        Settings = settings;
    }
}

cwCSVImporterTask::Output cwCSVImporterTask::output() const
{
    if(isReady()) {
        return CSVOutput;
    }
    return Output();
}

void cwCSVImporterTask::runTask()
{
    CSVOutput = Output(); //Clear the output from last time

    if(!QFileInfo::exists(Settings.filename())) {
        cwError error(QStringLiteral("Can't open \"%1\". %2").arg(Settings.filename()).arg("The system cannot find the file specified."),
                      cwError::Fatal);
        CSVOutput.errors.append(error);
        done();
        return;
    }

    QFile file(Settings.filename());
    bool okay = file.open(QFile::ReadOnly);
    if(!okay) {
        cwError error(QStringLiteral("Can't open \"%1\". %2").arg(Settings.filename()).arg(file.errorString()),
                      cwError::Fatal);
        CSVOutput.errors.append(error);
        done();
        return;
    }

    int tripCount = 1;
    QString tripStr("Trip %1");

    cwTrip* trip = new cwTrip();
    trip->setName(tripStr.arg(tripCount));

    cwCave* cave = new cwCave();
    cave->setName("Imported CSV Cave");
    cave->addTrip(trip);

    //Settings
    int skipHeaderLines = Settings.skipHeaderLines();
    QString sepratator = Settings.seperator();
    bool useFromStationForLRUD = Settings.useFromStationForLRUD();
    QList<cwColumnName> columns = Settings.columns();
    cwUnits::LengthUnit distanceUnit = Settings.distanceUnit();
    trip->calibrations()->setDistanceUnit(distanceUnit);

    int lineCount = 0;

    auto isPreviewLine = [&lineCount, this]() {
        return lineCount <= Settings.previewLines() || Settings.previewLines() < 0;
    };

    auto readLine = [&file, &lineCount, this, isPreviewLine]() {
        lineCount++;
        QString line = file.readLine();
        line.remove('\r');

        if(isPreviewLine()) {
            CSVOutput.text.append(line);
        }

        return line.trimmed();
    };

    auto parseHeaderLines = [&]() {
        for(int i = 0; i < skipHeaderLines && !file.atEnd(); i++) {
            readLine();
        }
    };

    auto makeNewTrip = [&]() {
        if(!trip->chunks().isEmpty()) {
            tripCount++;
            trip = new cwTrip;
            trip->setName(tripStr.arg(tripCount));
            trip->calibrations()->setDistanceUnit(distanceUnit);

            cave->addTrip(trip);
        }
    };

    auto mapColumns = [&](const QStringList& readColumns, cwStation& from, cwStation& to, cwStation& lrudStation, cwShot& shot) {
        lrudStation = useFromStationForLRUD ? from : to;

        if(readColumns.size() != columns.size()) {
            cwError error(QStringLiteral("Looking for %1 columns but found %2 on line %3").arg(columns.size()).arg(readColumns.size()).arg(lineCount));
            CSVOutput.errors.append(error);
        }

        QStringList line;
        line.reserve(columns.size());

        for(int i = 0; i < readColumns.size(); i++) {
            if(i < columns.size()) {
                auto column = columns.at(i);

                QString value = readColumns.at(i).trimmed();
                line.append(value);

                switch(column.columnId()) {
                case ToStation:
                    to.setName(value);
                    break;
                case FromStation:
                    from.setName(value);
                    break;
                case Length:
                    shot.setDistance(value);
                    break;
                case CompassFrontSight:
                    shot.setCompass(value);
                    break;
                case CompassBackSight:
                    shot.setBackCompass(value);
                    break;
                case ClinoFrontSight:
                    shot.setClino(value);
                    break;
                case ClinoBackSight:
                    shot.setBackClino(value);
                    break;
                case Left:
                    lrudStation.setLeft(value);
                    break;
                case Right:
                    lrudStation.setRight(value);
                    break;
                case Up:
                    lrudStation.setUp(value);
                    break;
                case Down:
                    lrudStation.setDown(value);
                    break;
                case Skip:
                    break;
                }
            }
        }

        if(isPreviewLine()) {
            CSVOutput.lines.append(line);
        }
    };

    auto setLastStationLRUD = [&](cwStation& from, cwStation& to, cwStation& lrudStation) {
        auto chunk = trip->chunks().last();
        int stationIndex = lrudStationIndex(chunk, from, to);
        for(int i = 0; i < trip->chunkCount() && stationIndex < 0; i++) {
            chunk = trip->chunk(i);
            stationIndex = lrudStationIndex(chunk, from, to);
        }

        if(stationIndex >= 0) {
            //Update the station LRUD
            auto oldStation = chunk->station(stationIndex);
            lrudStation.setName(oldStation.name());
            chunk->setStation(lrudStation, stationIndex);
        } else {
            //Error can't find shot in trip
            QString stationName = (Settings.useFromStationForLRUD() ? to : from).name();
            cwError error(QStringLiteral("Can't set LRUD data for shot \"%1\" because shot \"%2\" to \"%3\" on line %4.").arg(stationName).arg(from.name()).arg(to.name()).arg(lineCount));
            CSVOutput.errors.append(error);
        }
    };

    auto setShotData = [&](const cwStation& from, const cwStation& to, const cwShot& shot) {
        if(trip->chunks().isEmpty()) {
            cwSurveyChunk* chunk = new cwSurveyChunk();
            trip->addChunk(chunk);
        }

        cwSurveyChunk* chunk = trip->chunks().last();
        if(!chunk->canAddShot(from, to)) {
            chunk = new cwSurveyChunk();
            trip->addChunk(chunk);
        }

        chunk->appendShot(from, to, shot);

        if(Settings.useFromStationForLRUD()) {
            int fromStationIndex = chunk->stationCount() - 2;
            chunk->setStation(from, fromStationIndex);
        }
    };

    parseHeaderLines();

    while(!file.atEnd()) {
        QString line = readLine();
        QStringList readColumns = line.split(sepratator);

        if(line.isEmpty() && Settings.newTripOnEmptyLines()) {
            makeNewTrip();
            continue;
        }

        cwStation from;
        cwStation to;
        cwShot shot;
        cwStation& lrudStation = useFromStationForLRUD ? from : to;

        mapColumns(readColumns, from, to, lrudStation, shot);

        if(shot.distance() == 0.0 && trip->chunkCount() > 0) {
            //LRUD shot, search for the shot. Look through this chunk first, then the whole cave
            setLastStationLRUD(from, to, lrudStation);
        } else {
            //Normal data processing
            setShotData(from, to, shot);
        }
     }

    if(lineCount == 0) {
        cwError error("No lines read, file empty?", cwError::Warning);
        CSVOutput.errors.append(error);
    }

    //Remove last empty trip that exists
    if(cave->tripCount() > 1 && cave->trips().last()->chunkCount() == 0) {
        cave->removeTrip(cave->tripCount() - 1);
    }

    CSVOutput.caves.append(cave);
    CSVOutput.lineCount = lineCount;

    cave->moveToThread(settings().outputThread());

    done();
}

/**
 * This finds LRUD index of the station shot between from and to station. This is useful for updating
 * the last station's LRUD data for the last station.
 */
int cwCSVImporterTask::lrudStationIndex(const cwSurveyChunk *chunk, const cwStation& from, const cwStation& to) const
{
    for(int i = 0; i < chunk->stationCount() - 1; i++) {
        int toStationIdx = i;
        int fromStationIdx = i + 1;
        cwStation station = chunk->station(toStationIdx);
        cwStation nextStation = chunk->station(fromStationIdx);
        if(station.name().compare(from.name(), Qt::CaseInsensitive) == 0 &&
                nextStation.name().compare(to.name(), Qt::CaseInsensitive) == 0)
        {
            //Found the shot
            return Settings.useFromStationForLRUD() ? fromStationIdx : toStationIdx;
        }
    }

    return -1;
}
