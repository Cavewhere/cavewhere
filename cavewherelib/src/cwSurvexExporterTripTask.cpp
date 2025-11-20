/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterTripTask.h"
#include "cwTrip.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"
#include "cwTeamMember.h"
#include "cwTeam.h"

cwSurvexExporterTripTask::cwSurvexExporterTripTask(QObject *parent) :
    cwExporterTask(parent)
{
}

/**
  \brief Sets the trip data
  */
void cwSurvexExporterTripTask::setData(const cwTripData &trip) {
    if(!isRunning()) {
        Trip = trip;
    } else {
        qDebug() << QStringLiteral("Can't set trip data will the trip exporter is running");
    }
}

/**
  \brief Saves the trip to a survex file

  Both setData and setOutputFile should be set before calling cwTask::start
  */
void cwSurvexExporterTripTask::runTask() {
    auto tripPtr = std::make_unique<cwTrip>();

    setNumberOfSteps(tripPtr->numberOfStations());

    openOutputFile();
    writeTrip(*OutputStream.data(), tripPtr.get());
    closeOutputFile();

    done();
}

/**
  \brief Writes a trip to a stream
  */
void cwSurvexExporterTripTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    //Write header
    stream << QStringLiteral("*begin ; ") << trip->name() << Qt::endl;

    writeDate(stream, trip->date().date());
    writeTeamData(stream, trip->team());
    writeCalibrations(stream, trip->calibrations()); stream << Qt::endl;
    writeShotData(stream, trip); stream << Qt::endl;
    writeLRUDData(stream, trip);

    stream << QStringLiteral("*end") << Qt::endl;
}

/**
  \brief Writes the calibrations to the stream

  This will write all the calibrations for the trip to the stream
  */
void cwSurvexExporterTripTask::writeCalibrations(QTextStream& stream, cwTripCalibration* calibrations) {
    writeLengthUnits(stream, calibrations->distanceUnit());

    writeCalibration(stream, QStringLiteral("TAPE"), calibrations->tapeCalibration());

    double correctFrontsightCompass = calibrations->hasCorrectedCompassFrontsight() ? -180.0 : 0.0;
    writeCalibration(stream, QStringLiteral("COMPASS"), calibrations->frontCompassCalibration() + correctFrontsightCompass);

    double correctBacksightCompass = calibrations->hasCorrectedCompassBacksight() ? -180.0 : 0.0;
    writeCalibration(stream, QStringLiteral("BACKCOMPASS"), calibrations->backCompassCalibration() + correctBacksightCompass);

    double frontClinoScale = calibrations->hasCorrectedClinoFrontsight() ? -1.0 : 1.0;
    writeCalibration(stream, QStringLiteral("CLINO"), calibrations->frontClinoCalibration(), frontClinoScale);

    double backClinoScale = calibrations->hasCorrectedClinoBacksight() ? -1.0 : 1.0;
    writeCalibration(stream, QStringLiteral("BACKCLINO"), calibrations->backClinoCalibration(), backClinoScale);

    writeCalibration(stream, QStringLiteral("DECLINATION"), calibrations->declination());
}

void cwSurvexExporterTripTask::writeCalibration(QTextStream& stream, QString type, double value, double scale) {
    if(value == 0.0 && scale == 1.0) { return; }
    value = -value; //Flip the value be survex is counter intuitive

    QString calibrationString = QStringLiteral("*calibrate %1 %2")
            .arg(type)
            .arg(value, 0, 'f', 2);

    if(scale != 1.0) {
        QString scaleString = QStringLiteral(" %3").arg(scale, 0, 'f', 2);
        calibrationString += scaleString;
    }

    stream << calibrationString << Qt::endl;
}

/**
  \brief This writes length the units for the trip
  */
void cwSurvexExporterTripTask::writeLengthUnits(QTextStream &stream,
                                                cwUnits::LengthUnit unit) {
    switch(unit) {
        //The default type doesn't need to be written
    case cwUnits::Meters:
        return;
    case cwUnits::Feet:
        stream << QStringLiteral("*units tape feet") << Qt::endl;
        break;
    case cwUnits::Yards:
        stream << QStringLiteral("*units tape yards") << Qt::endl;
        break;
    default:
        //All other units are automatically converted to meters through toSupportedLength(QString length)
        break;
    }
}

/**
  \brief Writes the shot data to the stream

  This will write the data as normal data
  */
void cwSurvexExporterTripTask::writeShotData(QTextStream& stream, const cwTrip* trip) {
    bool hasFrontSights = trip->calibrations()->hasFrontSights();
    bool hasBackSights = trip->calibrations()->hasBackSights();

    //Make sure we have data to export
    if(!hasFrontSights && !hasBackSights) {
        stream << QStringLiteral("; NO DATA (doesn't have front or backsight data)") << Qt::endl;
        return;
    }

    QString dataLineComment;

    if(hasFrontSights && hasBackSights) {
        stream << QStringLiteral("*data normal from to tape compass backcompass clino backclino") << Qt::endl;
        dataLineComment = QStringLiteral(";%1%2 %3 %4 %5 %6 %7")
                .arg(QStringLiteral("From"), TextPadding)
                .arg(QStringLiteral("To"), TextPadding)
                .arg(QStringLiteral("Distance"), TextPadding)
                .arg(QStringLiteral("Compass"), TextPadding)
                .arg(QStringLiteral("BackCompass"), TextPadding)
                .arg(QStringLiteral("Clino"), TextPadding)
                .arg(QStringLiteral("BackClino"), TextPadding);
    } else if(hasFrontSights) {
        stream << QStringLiteral("*data normal from to tape compass clino") << Qt::endl;
        dataLineComment = QStringLiteral(";%1%2 %3 %4 %5")
                .arg(QStringLiteral("From"), TextPadding)
                .arg(QStringLiteral("To"), TextPadding)
                .arg(QStringLiteral("Distance"), TextPadding)
                .arg(QStringLiteral("Compass"), TextPadding)
                .arg(QStringLiteral("Clino"), TextPadding);
    } else if(hasBackSights) {
        stream << QStringLiteral("*data normal from to tape backcompass backclino") << Qt::endl;
        dataLineComment = QStringLiteral(";%1%2 %3 %4 %5")
                .arg(QStringLiteral("From"), TextPadding)
                .arg(QStringLiteral("To"), TextPadding)
                .arg(QStringLiteral("Distance"), TextPadding)
                .arg(QStringLiteral("BackCompass"), TextPadding)
                .arg(QStringLiteral("BackClino"), TextPadding);
    }

    //Write out the comment line (this is the column headers)
    stream << dataLineComment << Qt::endl;

    QList<cwSurveyChunk*> chunks = trip->chunks();
    for(int i = 0; i < chunks.size(); i++) {
        //Make sure we can still be run
        if(!parentIsRunning() && !isRunning()) { return; }

        cwSurveyChunk* chunk = chunks[i];

        //Write the chunk data
        writeChunk(stream, hasFrontSights, hasBackSights, chunk);
    }
}

/**
  \brief Writes the left right up down for each station in the trip, as
  a comment
  */
void cwSurvexExporterTripTask::writeLRUDData(QTextStream& stream, const cwTrip* trip) {

    QString dataLineTemplate(QStringLiteral("%1 %2 %3 %4 %5"));

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        stream << QStringLiteral("*data passage station left right up down ignoreall") << Qt::endl;

        foreach(cwStation station, chunk->stations()) {
            if(station.isValid()) {
                const cwTripCalibration* calibration = trip->calibrations();
                QString dataLine = dataLineTemplate
                        .arg(station.name(), TextPadding)
                        .arg(toSupportedLength(calibration, station.left()), TextPadding)
                        .arg(toSupportedLength(calibration, station.right()), TextPadding)
                        .arg(toSupportedLength(calibration, station.up()), TextPadding)
                        .arg(toSupportedLength(calibration, station.down()), TextPadding);

                stream << dataLine << Qt::endl;
            }
        }

        stream << Qt::endl;
    }
}

/**
  Writes the team data to survex file
  */
void cwSurvexExporterTripTask::writeTeamData(QTextStream &stream, const cwTeam* team)
{
    stream << Qt::endl;

    QString dataLineTemplate(QStringLiteral("*team \"%1\""));
    foreach(cwTeamMember teamMember, team->teamMembers()) {
        QString dataLine = dataLineTemplate.arg(teamMember.name());
        stream << dataLine;

        foreach(QString job, teamMember.jobs()) {
            stream << QStringLiteral(" \"") << job << QStringLiteral("\"");
        }

        stream << Qt::endl;
    }
}

/**
  Writes the data to th stream
  */
void cwSurvexExporterTripTask::writeDate(QTextStream &stream, QDate date)
{
    if(date.isValid()) {
        stream << QStringLiteral("*date ") << date.toString(QStringLiteral("yyyy.MM.dd")) << Qt::endl;
    }
}

/**
  Survex only supports yard, ft, and meters

  If the current calibration isn't in yard, feet or meters, then this function converts the
  length into meters.
*/
QString cwSurvexExporterTripTask::toSupportedLength(const cwTripCalibration *calibration, const cwDistanceReading& reading) const {
    if(reading.state() == cwDistanceReading::State::Empty) {
        return "-";
    }

    cwUnits::LengthUnit unit = calibration->distanceUnit();
    switch(unit) {
    case cwUnits::Meters:
    case cwUnits::Feet:
    case cwUnits::Yards:
        return reading.value();
    default:
        return QString::number(cwUnits::convert(reading.toDouble(), unit, cwUnits::Meters));
    }
}

/**
  This converts a compass bearing into a string based on the state.
  */
QString cwSurvexExporterTripTask::compassToString(const cwCompassReading& reading) const
{
    switch(reading.state()) {
    case cwCompassReading::State::Empty:
        return QStringLiteral("-");
    case cwCompassReading::State::Invalid:
        qDebug() << "Compass reading is invalid:" << reading.value() << LOCATION;
        //This should fallthrough to the valid case, just write the invalid data and have survex handle it
    case cwCompassReading::State::Valid:
        return reading.value();
    }
    return QString();
}

/**
  This converts a clino into a string based on the state.
  */
QString cwSurvexExporterTripTask::clinoToString(const cwClinoReading& reading) const
{
    switch(reading.state()) {
    case cwClinoReading::State::Empty:
        return QStringLiteral("-");
    case cwClinoReading::State::Invalid:
        qDebug() << "Clino reading is invalid:" << reading.value() << LOCATION;
        //This should fallthrough to the valid case, just write the invalid data and have survex handle it
    case cwClinoReading::State::Valid:
        return reading.value();
    case cwClinoReading::State::Down:
        return QStringLiteral("DOWN");
    case cwClinoReading::State::Up:
        return QStringLiteral("UP");

    }
    return QString();
}

/**
  \brief Writes a chunk to a stream
  */
void cwSurvexExporterTripTask::writeChunk(QTextStream& stream,
                                          bool hasFrontSights, //True if the dataset has backsights
                                          bool hasBackSights, //True if the dataset has frontsights
                                          cwSurveyChunk* chunk) {

    if(!hasBackSights && !hasFrontSights) {
        return;
    }

    QString dataLineTemplate;
    if(hasBackSights && hasFrontSights) {
        dataLineTemplate = QStringLiteral("%1 %2 %3 %4 %5 %6 %7");
    } else {
        dataLineTemplate = QStringLiteral("%1 %2 %3 %4 %5");
    }

    //Iterate over all the shots
    for(int i = 0; i < chunk->stationCount() - 1; i++) {

        //Make sure we can still be run
        if(!parentIsRunning() && !isRunning()) { return; }

        cwStation fromStation = chunk->station(i);
        cwStation toStation = chunk->station(i + 1);
        cwShot shot = chunk->shot(i);

        if(!fromStation.isValid() || !toStation.isValid()) { continue; }

        QString distance = toSupportedLength(chunk->parentTrip()->calibrations(), shot.distance());
        QString compass = compassToString(shot.compass());
        QString backCompass = compassToString(shot.backCompass());
        QString clino = clinoToString(shot.clino());
        QString backClino = clinoToString(shot.backClino());

        //Make sure the model is good
        if(distance.isEmpty()) { continue; }
        if(compass.isEmpty() && backCompass.isEmpty()) {
            if(clino.compare(QStringLiteral("up"), Qt::CaseInsensitive) != 0 &&
                    clino.compare(QStringLiteral("down"), Qt::CaseInsensitive) != 0 &&
                    backClino.compare(QStringLiteral("up"), Qt::CaseInsensitive) != 0 &&
                    backClino.compare(QStringLiteral("down"), Qt::CaseInsensitive) != 0) {
               Errors.append(QStringLiteral("Error: No compass reading for %1 to %2")
                             .arg(fromStation.name())
                             .arg(toStation.name()));
           }
        }

        if(clino.isEmpty() && backClino.isEmpty()) {
            Errors.append(QStringLiteral("Error: No Clino reading for %1 to %2")
                          .arg(fromStation.name())
                          .arg(toStation.name()));
        }

        if(compass.isEmpty()) { compass = QStringLiteral("-"); }
        if(backCompass.isEmpty()) { backCompass = QStringLiteral("-"); }
        if(clino.isEmpty()) { clino = QStringLiteral("-"); }
        if(backClino.isEmpty()) { backClino = QStringLiteral("-"); }

        if((clino.compare(QStringLiteral("up"), Qt::CaseInsensitive) == 0 &&
                backClino.compare(QStringLiteral("up"), Qt::CaseInsensitive) == 0) ||
                (clino.compare(QStringLiteral("down"), Qt::CaseInsensitive) == 0 &&
                backClino.compare(QStringLiteral("down"), Qt::CaseInsensitive) == 0)) {
            // survex errors on "up up" or "down down" when backsights are corrected
            backClino = QStringLiteral("-");
        }

        //Figure out the line of data
        QString line;
        if(hasFrontSights && hasBackSights) {
             line = dataLineTemplate
                    .arg(fromStation.name(), TextPadding)
                    .arg(toStation.name(), TextPadding)
                    .arg(distance, TextPadding)
                    .arg(compass, TextPadding)
                    .arg(backCompass, TextPadding)
                    .arg(clino, TextPadding)
                    .arg(backClino, TextPadding);
        } else if(hasFrontSights) {
            line = dataLineTemplate
                   .arg(fromStation.name(), TextPadding)
                   .arg(toStation.name(), TextPadding)
                   .arg(distance, TextPadding)
                   .arg(compass, TextPadding)
                   .arg(clino, TextPadding);
        } else if(hasBackSights) {
            line = dataLineTemplate
                   .arg(fromStation.name(), TextPadding)
                   .arg(toStation.name(), TextPadding)
                   .arg(distance, TextPadding)
                   .arg(backCompass, TextPadding)
                   .arg(backClino, TextPadding);
        }

        //Add chunk calibrations
        cwTripCalibration* calibration = chunk->calibrations().value(i, nullptr);
        if(calibration != nullptr) {
            writeCalibrations(stream, calibration);
        }

        //Distance should be excluded, mark as duplicate
        if(!shot.isDistanceIncluded()) {
            stream << QStringLiteral("*flags duplicate") << Qt::endl;
        }

        stream << line << Qt::endl;

        //Turn duplication off
        if(!shot.isDistanceIncluded()) {
            stream << QStringLiteral("*flags not duplicate") << Qt::endl;
        }

//        emit progressed(i);
    }
}
