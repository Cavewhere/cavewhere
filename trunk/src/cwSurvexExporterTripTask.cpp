#include "cwSurvexExporterTripTask.h"
#include "cwTrip.h"
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

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
    writeTrip(OutputStream, Trip);
    closeOutputFile();

    done();
}

/**
  \brief Writes a trip to a stream
  */
void cwSurvexExporterTripTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    //Write header
    stream << "*begin ; " << trip->name() << endl;

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

    writeCalibration(stream, "TAPE", calibrations->tapeCalibration());
    writeCalibration(stream, "COMPASS", calibrations->frontCompassCalibration());

    float correctBacksiteCompass = calibrations->hasCorrectedCompassBacksight() ? -180.0 : 0.0;
    writeCalibration(stream, "BACKCOMPASS", calibrations->backCompassCalibration() + correctBacksiteCompass);

    writeCalibration(stream, "CLINO", calibrations->frontClinoCalibration());

    float backClinoScale = calibrations->hasCorrectedClinoBacksight() ? -1.0 : 1.0;
    writeCalibration(stream, "BACKCLINO", calibrations->backClinoCalibration(), backClinoScale);

    writeCalibration(stream, "DECLINATION", calibrations->declination());
}

void cwSurvexExporterTripTask::writeCalibration(QTextStream& stream, QString type, float value, float scale) {
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
  \brief Writes the shot data to the stream

  This will write the data as normal data
  */
void cwSurvexExporterTripTask::writeShotData(QTextStream& stream, cwTrip* trip) {
    stream << "*data normal from to tape compass backcompass clino backclino" << endl;

    QString dataLineComment = QString(";%1%2 %3 %4 %5 %6 %7")
            .arg("From", TextPadding)
            .arg("To", TextPadding)
            .arg("Distance", TextPadding)
            .arg("Compass", TextPadding)
            .arg("BackCompass", TextPadding)
            .arg("Clino", TextPadding)
            .arg("BackClino", TextPadding);

    stream << dataLineComment << endl;

    QList<cwSurveyChunk*> chunks = trip->chunks();
    for(int i = 0; i < chunks.size(); i++) {
        //Make sure we can still be run
        if(!parentIsRunning() && !isRunning()) { return; }

        cwSurveyChunk* chunk = chunks[i];

        //Write the chunk data
        writeChunk(stream, chunk);
    }
}

/**
  \brief Writes the left right up down for each station in the trip, as
  a comment
  */
void cwSurvexExporterTripTask::writeLRUDData(QTextStream& stream, cwTrip* trip) {
    QString dataLineTemplate(";%1%2 %3 %4 %5");
    QString dataLineComment = dataLineTemplate
            .arg("Station", TextPadding)
            .arg("Left", TextPadding)
            .arg("Right", TextPadding)
            .arg("Up", TextPadding)
            .arg("Down", TextPadding);

    stream << dataLineComment << endl;

    QList<cwStationReference> stations = trip->uniqueStations();
    foreach(cwStationReference station, stations) {
        QString dataLine = dataLineTemplate
                .arg(station.name(), TextPadding)
                .arg(station.left(), TextPadding)
                .arg(station.right(), TextPadding)
                .arg(station.up(), TextPadding)
                .arg(station.down(), TextPadding);

        stream << dataLine << endl;
    }
}

/**
  \brief Writes a chunk to a stream
  */
void cwSurvexExporterTripTask::writeChunk(QTextStream& stream, cwSurveyChunk* chunk) {
    for(int i = 0; i < chunk->StationCount() - 1; i++) {

        //Make sure we can still be run
        if(!parentIsRunning() && !isRunning()) { return; }

        cwStationReference fromStation = chunk->Station(i);
        cwStationReference toStation = chunk->Station(i + 1);
        cwShot* shot = chunk->Shot(i);

        if(!fromStation.isValid() || !toStation.isValid()) { continue; }

        QString distance = shot->distance();
        QString compass = shot->compass();
        QString backCompass = shot->backCompass();
        QString clino = shot->clino();
        QString backClino = shot->backClino();

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
               continue;
           }
        }

        if(clino.isEmpty() && backClino.isEmpty()) {
            Errors.append(QString("Error: No Clino reading for %1 to %2")
                          .arg(fromStation.name())
                          .arg(toStation.name()));
            continue;
        }

        if(compass.isEmpty()) { compass = "-"; }
        if(backCompass.isEmpty()) { backCompass = "-"; }
        if(clino.isEmpty()) { clino = "-"; }
        if(backClino.isEmpty()) { backClino = "-"; }


        QString line = QString("%1 %2 %3 %4 %5 %6 %7")
                .arg(fromStation.name(), TextPadding)
                .arg(toStation.name(), TextPadding)
                .arg(distance, TextPadding)
                .arg(compass, TextPadding)
                .arg(backCompass, TextPadding)
                .arg(clino, TextPadding)
                .arg(backClino, TextPadding);

        stream << line << endl;

        emit progressed(i);
    }
}
