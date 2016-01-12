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

const int cwSurvexExporterTripTask::TextPadding = -11; //Left align with 10 spaces

cwSurvexExporterTripTask::cwSurvexExporterTripTask(QObject *parent) :
    cwExporterTask(parent)
{
    Trip = new cwTrip(this);
}

/**
  \brief Sets the trip data
  */
void cwSurvexExporterTripTask::setData(const cwTrip& trip) {
    if(!isRunning()) {
        *Trip = trip;
    } else {
        qWarning("Can't set trip data will the trip exporter is running");
    }
}

/**
  \brief Saves the trip to a survex file

  Both setData and setOutputFile should be set before calling cwTask::start
  */
void cwSurvexExporterTripTask::runTask() {
    setNumberOfSteps(Trip->numberOfStations());

    openOutputFile();
    writeTrip(*OutputStream.data(), Trip);
    closeOutputFile();

    done();
}

/**
  \brief Writes a trip to a stream
  */
void cwSurvexExporterTripTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    //Write header
    stream << "*begin ; " << trip->name() << endl;

    writeDate(stream, trip->date());
    writeTeamData(stream, trip->team());
    writeCalibrations(stream, trip->calibrations()); stream << endl;
    writeShotData(stream, trip); stream << endl;
    writeLRUDData(stream, trip);

    stream << "*end" << endl;
}

/**
  \brief Writes the calibrations to the stream

  This will write all the calibrations for the trip to the stream
  */
void cwSurvexExporterTripTask::writeCalibrations(QTextStream& stream, cwTripCalibration* calibrations) {
    writeLengthUnits(stream, calibrations->distanceUnit());

    writeCalibration(stream, "TAPE", calibrations->tapeCalibration());

    double correctFrontsightCompass = calibrations->hasCorrectedCompassFrontsight() ? -180.0 : 0.0;
    writeCalibration(stream, "COMPASS", calibrations->frontCompassCalibration() + correctFrontsightCompass);

    double correctBacksightCompass = calibrations->hasCorrectedCompassBacksight() ? -180.0 : 0.0;
    writeCalibration(stream, "BACKCOMPASS", calibrations->backCompassCalibration() + correctBacksightCompass);

    double frontClinoScale = calibrations->hasCorrectedClinoFrontsight() ? -1.0 : 1.0;
    writeCalibration(stream, "CLINO", calibrations->frontClinoCalibration(), frontClinoScale);

    double backClinoScale = calibrations->hasCorrectedClinoBacksight() ? -1.0 : 1.0;
    writeCalibration(stream, "BACKCLINO", calibrations->backClinoCalibration(), backClinoScale);

    writeCalibration(stream, "DECLINATION", calibrations->declination());
}

void cwSurvexExporterTripTask::writeCalibration(QTextStream& stream, QString type, double value, double scale) {
    if(value == 0.0 && scale == 1.0) { return; }
    value = -value; //Flip the value be survex is counter intuitive

    QString calibrationString = QString("*calibrate %1 %2")
            .arg(type)
            .arg(value, 0, 'f', 2);

    if(scale != 1.0) {
        QString scaleString = QString(" %3").arg(scale, 0, 'f', 2);
        calibrationString += scaleString;
    }

    stream << calibrationString << endl;
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
        stream << "*units tape feet" << endl;
        break;
    case cwUnits::Yards:
        stream << "*units tape yards" << endl;
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
void cwSurvexExporterTripTask::writeShotData(QTextStream& stream, cwTrip* trip) {
    bool hasFrontSights = trip->calibrations()->hasFrontSights();
    bool hasBackSights = trip->calibrations()->hasBackSights();

    //Make sure we have data to export
    if(!hasFrontSights && !hasBackSights) {
        stream << "; NO DATA (doesn't have front or backsight data)" << endl;
        return;
    }

    QString dataLineComment;

    if(hasFrontSights && hasBackSights) {
        stream << "*data normal from to tape compass backcompass clino backclino" << endl;
        dataLineComment = QString(";%1%2 %3 %4 %5 %6 %7")
                .arg("From", TextPadding)
                .arg("To", TextPadding)
                .arg("Distance", TextPadding)
                .arg("Compass", TextPadding)
                .arg("BackCompass", TextPadding)
                .arg("Clino", TextPadding)
                .arg("BackClino", TextPadding);
    } else if(hasFrontSights) {
        stream << "*data normal from to tape compass clino" << endl;
        dataLineComment = QString(";%1%2 %3 %4 %5")
                .arg("From", TextPadding)
                .arg("To", TextPadding)
                .arg("Distance", TextPadding)
                .arg("Compass", TextPadding)
                .arg("Clino", TextPadding);
    } else if(hasBackSights) {
        stream << "*data normal from to tape backcompass backclino" << endl;
        dataLineComment = QString(";%1%2 %3 %4 %5")
                .arg("From", TextPadding)
                .arg("To", TextPadding)
                .arg("Distance", TextPadding)
                .arg("BackCompass", TextPadding)
                .arg("BackClino", TextPadding);
    }

    //Write out the comment line (this is the column headers)
    stream << dataLineComment << endl;

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
void cwSurvexExporterTripTask::writeLRUDData(QTextStream& stream, cwTrip* trip) {

    QString dataLineTemplate("%1 %2 %3 %4 %5");

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        stream << "*data passage station left right up down ignoreall" << endl;

        foreach(cwStation station, chunk->stations()) {
            if(station.isValid()) {
                QString dataLine = dataLineTemplate
                        .arg(station.name(), TextPadding)
                        .arg(toSupportedLength(station.left(), station.leftInputState()), TextPadding)
                        .arg(toSupportedLength(station.right(), station.rightInputState()), TextPadding)
                        .arg(toSupportedLength(station.up(), station.upInputState()), TextPadding)
                        .arg(toSupportedLength(station.down(), station.downInputState()), TextPadding);

                stream << dataLine << endl;
            }
        }

        stream << endl;
    }
}

/**
  Writes the team data to survex file
  */
void cwSurvexExporterTripTask::writeTeamData(QTextStream &stream, cwTeam* team)
{
    stream << endl;

    QString dataLineTemplate("*team \"%1\"");
    foreach(cwTeamMember teamMember, team->teamMembers()) {
        QString dataLine = dataLineTemplate.arg(teamMember.name());
        stream << dataLine;

        foreach(QString job, teamMember.jobs()) {
            stream << " \"" << job << "\"";
        }

        stream << endl;
    }
}

/**
  Writes the data to th stream
  */
void cwSurvexExporterTripTask::writeDate(QTextStream &stream, QDate date)
{
    if(date.isValid()) {
        stream << "*date " << date.toString("yyyy.MM.dd") << endl;
    }
}

/**
  Survex only supports yard, ft, and meters

  If the current calibration isn't in yard, feet or meters, then this function converts the
  length into meters.
*/
QString cwSurvexExporterTripTask::toSupportedLength(double length, cwDistanceStates::State state) const {
    if(state == cwDistanceStates::Empty) {
        return "-";
    }

    cwUnits::LengthUnit unit = Trip->calibrations()->distanceUnit();
    switch(unit) {
    case cwUnits::Meters:
    case cwUnits::Feet:
    case cwUnits::Yards:
        return QString("%1").arg(length);
    default:
        return QString("%1").arg(cwUnits::convert(length, unit, cwUnits::Meters));
    }
}

/**
  This converts a compass bearing into a string based on the state.
  */
QString cwSurvexExporterTripTask::compassToString(double compass, cwCompassStates::State state) const
{
    switch(state) {
    case cwCompassStates::Empty:
        return QString("-");
    case cwCompassStates::Valid:
        return QString("%1").arg(compass);
    }
    return QString();
}

/**
  This converts a clino into a string based on the state.
  */
QString cwSurvexExporterTripTask::clinoToString(double clino, cwClinoStates::State state) const
{
    switch(state) {
    case cwClinoStates::Empty:
        return QString("-");
    case cwClinoStates::Valid:
        return QString("%1").arg(clino);
    case cwClinoStates::Down:
        return QString("DOWN");
    case cwClinoStates::Up:
        return QString("UP");
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
        dataLineTemplate = QString("%1 %2 %3 %4 %5 %6 %7");
    } else {
        dataLineTemplate = QString("%1 %2 %3 %4 %5");
    }

    for(int i = 0; i < chunk->stationCount() - 1; i++) {

        //Make sure we can still be run
        if(!parentIsRunning() && !isRunning()) { return; }

        cwStation fromStation = chunk->station(i);
        cwStation toStation = chunk->station(i + 1);
        cwShot shot = chunk->shot(i);

        if(!fromStation.isValid() || !toStation.isValid()) { continue; }

        QString distance = toSupportedLength(shot.distance(), cwDistanceStates::Valid);
        QString compass = compassToString(shot.compass(), shot.compassState());
        QString backCompass = compassToString(shot.backCompass(), shot.backCompassState());
        QString clino = clinoToString(shot.clino(), shot.clinoState());
        QString backClino = clinoToString(shot.backClino(), shot.backClinoState());

        //Make sure the model is good
        if(distance.isEmpty()) { continue; }
        if(compass.isEmpty() && backCompass.isEmpty()) {
            if(clino.compare("up", Qt::CaseInsensitive) != 0 &&
                    clino.compare("down", Qt::CaseInsensitive) != 0 &&
                    backClino.compare("up", Qt::CaseInsensitive) != 0 &&
                    backClino.compare("down", Qt::CaseInsensitive) != 0) {
               Errors.append(QString("Error: No compass reading for %1 to %2")
                             .arg(fromStation.name())
                             .arg(toStation.name()));
           }
        }

        if(clino.isEmpty() && backClino.isEmpty()) {
            Errors.append(QString("Error: No Clino reading for %1 to %2")
                          .arg(fromStation.name())
                          .arg(toStation.name()));
        }

        if(compass.isEmpty()) { compass = "-"; }
        if(backCompass.isEmpty()) { backCompass = "-"; }
        if(clino.isEmpty()) { clino = "-"; }
        if(backClino.isEmpty()) { backClino = "-"; }


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

        //Distance should be excluded, mark as duplicate
        if(!shot.isDistanceIncluded()) {
            stream << "*flags duplicate" << endl;
        }

        stream << line << endl;

        //Turn duplication off
        if(!shot.isDistanceIncluded()) {
            stream << "*flags not duplicate" << endl;
        }

//        emit progressed(i);
    }
}
