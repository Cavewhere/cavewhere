#include "cwSurvexExporterRule.h"
#include "cwCavingRegion.h"
#include "cwConcurrent.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"

//Qt includes
#include <QTextStream>

//Monad include
#include "Monad/Result.h"

using namespace Monad;

cwSurvexExporterRule::cwSurvexExporterRule(QObject* parent)
    : cwAbstractRule(parent),
    m_surveyData(nullptr),
    m_survexFilename(nullptr),
    m_survexFileArtifact(new cwFutureFileNameArtifact(this))
{
}

cwSurvexExporterRule::~cwSurvexExporterRule() {}

cwSurveyDataArtifact* cwSurvexExporterRule::surveyData() const {
    return m_surveyData;
}

void cwSurvexExporterRule::setSurveyData(cwSurveyDataArtifact* surveyData) {
    if (m_surveyData != surveyData) {
        if(!m_surveyData.isNull()) {
            disconnect(m_surveyData, nullptr, this, nullptr);
        }

        m_surveyData = surveyData;

        if(!m_surveyData.isNull()) {
            connect(m_surveyData, &cwSurveyDataArtifact::surveyDataChanged, this, &cwSurvexExporterRule::updatePipeline);
            updatePipeline();
        }

        emit surveyDataChanged();
    }
}

void cwSurvexExporterRule::setSurvexFileName(cwFileNameArtifact* survexFile) {
    if (m_survexFilename != survexFile) {
        if(!m_survexFilename.isNull()) {
            disconnect(m_survexFilename, nullptr, this, nullptr);
        }

        m_survexFilename = survexFile;

        if(m_survexFilename.isNull()) {
            connect(m_survexFilename, &cwFileNameArtifact::filenameChanged, this, &cwSurvexExporterRule::updatePipeline);
            updatePipeline();
        }

        emit survexFileChanged();
    }
}

ResultBase cwSurvexExporterRule::writeTrip(QTextStream &stream, const cwTrip *trip)
{
    //Write header
    stream << "*begin ; " << trip->name() << Qt::endl;

    writeDate(stream, trip->date().date());
    writeTeamData(stream, trip->team());
    writeCalibrations(stream, trip->calibrations());
    stream << Qt::endl;
    writeShotData(stream, trip);
    stream << Qt::endl;
    writeLRUDData(stream, trip);

    stream << "*end" << Qt::endl;

    return ResultBase();
}

ResultBase cwSurvexExporterRule::writeCave(QTextStream &stream, const cwCave *cave)
{
    QString caveName = cave->name().remove(" ");

    stream << "*begin " << caveName << " ;" << cave->name() << Qt::endl << Qt::endl;

    //Add fix station to tie the cave down
    fixFirstStation(stream, cave);


    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        writeTrip(stream, trip);
        stream << Qt::endl;
    }

    stream << "*end ; End of " << cave->name() << Qt::endl;

    return ResultBase();
}

ResultBase cwSurvexExporterRule::writeRegion(QTextStream &stream, const cwCavingRegion *region)
{
    stream << "*begin  ;All the caves" << Qt::endl;

    for(int i = 0; i < region->caveCount(); i++) {
        cwCave* cave = region->cave(i);

        auto result = writeCave(stream, cave);
        if(result.hasError()) {
            return result;
        }
    }

    stream << "*end" << Qt::endl;

    return ResultBase();
}

void cwSurvexExporterRule::updatePipeline()
{
    //Cancel the old future
    m_survexFileArtifact->filename().cancel();

    //Copy
    auto survexFilename = m_survexFilename->filename();
    cwCavingRegion region = *(m_surveyData->region());

    auto future = cwConcurrent::run([survexFilename, region = std::move(region)]()->ResultString {
        QSaveFile file(survexFilename);
        file.open(QIODeviceBase::WriteOnly);
        QTextStream stream(&file);

        writeRegion(stream, &region);

        file.commit();
        return survexFilename;
    });

    m_survexFileArtifact->setFilename(future);
}

/**
  \brief Writes the calibrations to the stream

  This will write all the calibrations for the trip to the stream
  */
void cwSurvexExporterRule::writeCalibrations(QTextStream& stream, cwTripCalibration* calibrations) {
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

void cwSurvexExporterRule::writeCalibration(QTextStream& stream, QString type, double value, double scale) {
    if(value == 0.0 && scale == 1.0) { return; }
    value = -value; //Flip the value be survex is counter intuitive

    QString calibrationString = QString("*calibrate %1 %2")
                                    .arg(type)
                                    .arg(value, 0, 'f', 2);

    if(scale != 1.0) {
        QString scaleString = QString(" %3").arg(scale, 0, 'f', 2);
        calibrationString += scaleString;
    }

    stream << calibrationString << Qt::endl;
}

/**
  \brief This writes length the units for the trip
  */
void cwSurvexExporterRule::writeLengthUnits(QTextStream &stream,
                                                cwUnits::LengthUnit unit) {
    switch(unit) {
        //The default type doesn't need to be written
    case cwUnits::Meters:
        return;
    case cwUnits::Feet:
        stream << "*units tape feet" << Qt::endl;
        break;
    case cwUnits::Yards:
        stream << "*units tape yards" << Qt::endl;
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
void cwSurvexExporterRule::writeShotData(QTextStream& stream, const cwTrip* trip, int textPadding) {
    bool hasFrontSights = trip->calibrations()->hasFrontSights();
    bool hasBackSights = trip->calibrations()->hasBackSights();

    //Make sure we have data to export
    if(!hasFrontSights && !hasBackSights) {
        stream << "; NO DATA (doesn't have front or backsight data)" << Qt::endl;
        return;
    }

    QString dataLineComment;

    if(hasFrontSights && hasBackSights) {
        stream << "*data normal from to tape compass backcompass clino backclino" << Qt::endl;
        dataLineComment = QString(";%1%2 %3 %4 %5 %6 %7")
                              .arg("From", textPadding)
                              .arg("To", textPadding)
                              .arg("Distance", textPadding)
                              .arg("Compass", textPadding)
                              .arg("BackCompass", textPadding)
                              .arg("Clino", textPadding)
                              .arg("BackClino", textPadding);
    } else if(hasFrontSights) {
        stream << "*data normal from to tape compass clino" << Qt::endl;
        dataLineComment = QString(";%1%2 %3 %4 %5")
                              .arg("From", textPadding)
                              .arg("To", textPadding)
                              .arg("Distance", textPadding)
                              .arg("Compass", textPadding)
                              .arg("Clino", textPadding);
    } else if(hasBackSights) {
        stream << "*data normal from to tape backcompass backclino" << Qt::endl;
        dataLineComment = QString(";%1%2 %3 %4 %5")
                              .arg("From", textPadding)
                              .arg("To", textPadding)
                              .arg("Distance", textPadding)
                              .arg("BackCompass", textPadding)
                              .arg("BackClino", textPadding);
    }

    //Write out the comment line (this is the column headers)
    stream << dataLineComment << Qt::endl;

    QList<cwSurveyChunk*> chunks = trip->chunks();
    for(int i = 0; i < chunks.size(); i++) {
        const cwSurveyChunk* chunk = chunks.at(i);

        //Write the chunk data
        writeChunk(stream, hasFrontSights, hasBackSights, trip, chunk);
    }
}

/**
  \brief Writes the left right up down for each station in the trip, as
  a comment
  */
void cwSurvexExporterRule::writeLRUDData(QTextStream& stream, const cwTrip* trip, int textPadding) {

    QString dataLineTemplate("%1 %2 %3 %4 %5");

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        stream << "*data passage station left right up down ignoreall" << Qt::endl;

        foreach(cwStation station, chunk->stations()) {
            if(station.isValid()) {
                QString dataLine = dataLineTemplate
                                       .arg(station.name(), textPadding)
                                       .arg(toSupportedLength(trip, station.left(), station.leftInputState()), textPadding)
                                       .arg(toSupportedLength(trip, station.right(), station.rightInputState()), textPadding)
                                       .arg(toSupportedLength(trip, station.up(), station.upInputState()), textPadding)
                                       .arg(toSupportedLength(trip, station.down(), station.downInputState()), textPadding);

                stream << dataLine << Qt::endl;
            }
        }

        stream << Qt::endl;
    }
}

/**
  Writes the team data to survex file
  */
void cwSurvexExporterRule::writeTeamData(QTextStream &stream, const cwTeam* team)
{
    stream << Qt::endl;

    QString dataLineTemplate("*team \"%1\"");
    foreach(cwTeamMember teamMember, team->teamMembers()) {
        QString dataLine = dataLineTemplate.arg(teamMember.name());
        stream << dataLine;

        foreach(QString job, teamMember.jobs()) {
            stream << " \"" << job << "\"";
        }

        stream << Qt::endl;
    }
}

/**
  Writes the data to th stream
  */
void cwSurvexExporterRule::writeDate(QTextStream &stream, QDate date)
{
    if(date.isValid()) {
        stream << "*date " << date.toString("yyyy.MM.dd") << Qt::endl;
    }
}

/**
  Survex only supports yard, ft, and meters

  If the current calibration isn't in yard, feet or meters, then this function converts the
  length into meters.
*/
QString cwSurvexExporterRule::toSupportedLength(const cwTrip* trip, double length, cwDistanceStates::State state) {
    if(state == cwDistanceStates::Empty) {
        return "-";
    }

    cwUnits::LengthUnit unit = trip->calibrations()->distanceUnit();
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
QString cwSurvexExporterRule::compassToString(double compass, cwCompassStates::State state)
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
QString cwSurvexExporterRule::clinoToString(double clino, cwClinoStates::State state)
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
ResultBase cwSurvexExporterRule::writeChunk(QTextStream& stream,
                                      bool hasFrontSights, //True if the dataset has backsights
                                      bool hasBackSights, //True if the dataset has frontsights
                                      const cwTrip* trip,
                                      const cwSurveyChunk* chunk,
                                      int textPadding) {

    if(!hasBackSights && !hasFrontSights) {
        return ResultBase();
    }

    QString dataLineTemplate;
    if(hasBackSights && hasFrontSights) {
        dataLineTemplate = QString("%1 %2 %3 %4 %5 %6 %7");
    } else {
        dataLineTemplate = QString("%1 %2 %3 %4 %5");
    }

    QStringList errors;

    //Iterate over all the shots
    for(int i = 0; i < chunk->stationCount() - 1; i++) {

        cwStation fromStation = chunk->station(i);
        cwStation toStation = chunk->station(i + 1);
        cwShot shot = chunk->shot(i);

        if(!fromStation.isValid() || !toStation.isValid()) { continue; }

        QString distance = toSupportedLength(trip, shot.distance(), cwDistanceStates::Valid);
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
                errors.append(QString("Error: No compass reading for %1 to %2")
                                  .arg(fromStation.name())
                                  .arg(toStation.name()));
            }
        }

        if(clino.isEmpty() && backClino.isEmpty()) {
            errors.append(QString("Error: No Clino reading for %1 to %2")
                              .arg(fromStation.name())
                              .arg(toStation.name()));
        }

        if(compass.isEmpty()) { compass = "-"; }
        if(backCompass.isEmpty()) { backCompass = "-"; }
        if(clino.isEmpty()) { clino = "-"; }
        if(backClino.isEmpty()) { backClino = "-"; }

        if((clino.compare("up", Qt::CaseInsensitive) == 0 &&
             backClino.compare("up", Qt::CaseInsensitive) == 0) ||
            (clino.compare("down", Qt::CaseInsensitive) == 0 &&
             backClino.compare("down", Qt::CaseInsensitive) == 0)) {
            // survex errors on "up up" or "down down" when backsights are corrected
            backClino = "-";
        }

        //Figure out the line of data
        QString line;
        if(hasFrontSights && hasBackSights) {
            line = dataLineTemplate
                       .arg(fromStation.name(), textPadding)
                       .arg(toStation.name(), textPadding)
                       .arg(distance, textPadding)
                       .arg(compass, textPadding)
                       .arg(backCompass, textPadding)
                       .arg(clino, textPadding)
                       .arg(backClino, textPadding);
        } else if(hasFrontSights) {
            line = dataLineTemplate
                       .arg(fromStation.name(), textPadding)
                       .arg(toStation.name(), textPadding)
                       .arg(distance, textPadding)
                       .arg(compass, textPadding)
                       .arg(clino, textPadding);
        } else if(hasBackSights) {
            line = dataLineTemplate
                       .arg(fromStation.name(), textPadding)
                       .arg(toStation.name(), textPadding)
                       .arg(distance, textPadding)
                       .arg(backCompass, textPadding)
                       .arg(backClino, textPadding);
        }

        //Add chunk calibrations
        cwTripCalibration* calibration = chunk->calibrations().value(i, nullptr);
        if(calibration != nullptr) {
            writeCalibrations(stream, calibration);
        }

        //Distance should be excluded, mark as duplicate
        if(!shot.isDistanceIncluded()) {
            stream << "*flags duplicate" << Qt::endl;
        }

        stream << line << Qt::endl;

        //Turn duplication off
        if(!shot.isDistanceIncluded()) {
            stream << "*flags not duplicate" << Qt::endl;
        }

        //        emit progressed(i);
    }

    if(!errors.isEmpty()) {
        return ResultBase(errors.join('\n'));
    }
    return ResultBase();
}

/**
 * @brief cwSurvexExporterCaveTask::fixFirstStation
 * @param stream
 * @param cave
 *
 * This fixes the first station in the cave, if the cave has any stations.
 */
void cwSurvexExporterRule::fixFirstStation(QTextStream &stream, const cwCave *cave)
{
    if(cave != nullptr) {
        if(!cave->trips().isEmpty()) {
            const auto trips = cave->trips();
            const cwTrip* firstTrip = trips.first();
            if(!firstTrip->chunks().isEmpty()) {
                const auto chunks = firstTrip->chunks();
                const cwSurveyChunk* firstChunk = chunks.first();
                if(!firstChunk->stations().isEmpty()) {
                    const auto stations = firstChunk->stations();
                    const cwStation station = stations.first();

                    stream << "*fix " << station.name() << " " << 0 << " " << 0 << " " << 0 << Qt::endl;
                }
            }
        }
    }
}


