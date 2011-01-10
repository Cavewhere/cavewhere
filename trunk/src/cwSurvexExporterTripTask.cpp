#include "cwSurvexExporterTripTask.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

const int cwSurvexExporterTripTask::TextPadding = -11; //Left align with 10 spaces

cwSurvexExporterTripTask::cwSurvexExporterTripTask(QObject *parent) :
    cwSurvexExporterTask(parent)
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

        //If this isn't the last line
        if(i < chunks.size() - 1) {
            stream << endl;
        }
    }

    stream << "*end" << endl;
}

/**
  \brief Writes a chunk to a stream
  */
void cwSurvexExporterTripTask::writeChunk(QTextStream& stream, cwSurveyChunk* chunk) {
    for(int i = 0; i < chunk->StationCount() - 1; i++) {

        //Make sure we can still be run
        if(!parentIsRunning() && !isRunning()) { return; }

        cwStation* fromStation = chunk->Station(i);
        cwStation* toStation = chunk->Station(i + 1);
        cwShot* shot = chunk->Shot(i);

        if(!fromStation->IsValid() || !toStation->IsValid()) { continue; }

        QString distance = shot->GetDistance().toString();
        QString compass = shot->GetCompass().toString();
        QString backCompass = shot->GetBackCompass().toString();
        QString clino = shot->GetClino().toString();
        QString backClino = shot->GetBackClino().toString();

        //Make sure the model is good
        if(distance.isEmpty()) { continue; }
        if(compass.isEmpty() && backCompass.isEmpty()) {
           if(clino.compare("up", Qt::CaseInsensitive) != 0 &&
                   clino.compare("down", Qt::CaseInsensitive) != 0 &&
                   backClino.compare("up", Qt::CaseInsensitive) != 0 &&
                   backClino.compare("down", Qt::CaseInsensitive) != 0) {
               Errors.append(QString("Error: No compass reading for %1 to %2")
                             .arg(fromStation->GetName())
                             .arg(toStation->GetName()));
               continue;
           }
        }

        if(clino.isEmpty() && backClino.isEmpty()) {
            Errors.append(QString("Error: No Clino reading for %1 to %2")
                          .arg(fromStation->GetName())
                          .arg(toStation->GetName()));
            continue;
        }

        if(compass.isEmpty()) { compass = "-"; }
        if(backCompass.isEmpty()) { backCompass = "-"; }
        if(clino.isEmpty()) { clino = "-"; }
        if(backClino.isEmpty()) { backClino = "-"; }


        QString line = QString("%1 %2 %3 %4 %5 %6 %7")
                .arg(fromStation->GetName(), TextPadding)
                .arg(toStation->GetName(), TextPadding)
                .arg(distance, TextPadding)
                .arg(compass, TextPadding)
                .arg(backCompass, TextPadding)
                .arg(clino, TextPadding)
                .arg(backClino, TextPadding);

        stream << line << endl;

        emit progressed(i);
    }
}
