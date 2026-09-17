/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporter.h"
#include "cwSurvexExporterUtils.h"
#include "cwDebug.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwTeamMember.h"

//Qt includes
#include <QTextStream>
#include <QDebug>

namespace {

//The front-sight reading order. Splays are always front sights, so this is also
//the order a splay block declares whatever the trip's own *data line says.
constexpr QLatin1String kFrontSightDataLine("*data normal from to tape compass clino");

//Survex's anonymous wall station: a splay ends against the passage wall, not at
//a station anybody named. Cavern treats a leg to `..` as a splay on its own, so
//*flags splay around the block is there to make the file say what it means.
constexpr QLatin1String kAnonymousWallStation("..");

//Column width for the padded data lines, negative for left alignment
constexpr int kTextPadding = -11;

} // namespace

int cwSurvexExporter::stationCount(const cwTripData& trip)
{
    int count = 0;
    for(const cwSurveyChunkData& chunk : trip.chunks) {
        count += static_cast<int>(chunk.stations.size());
    }
    return count;
}

/**
  \brief Writes a trip to a stream
  */
void cwSurvexExporter::writeTrip(QTextStream& stream,
                                 const cwTripData& trip,
                                 QStringList& errors,
                                 bool autoDeclinationInScope,
                                 double gridConvergence) {
    //Write header. The `*begin` block is anonymous (the trip name follows as a comment
    //for human readers) and a `*title` directive preserves the name across a round-trip
    //even when it contains characters Survex doesn't allow in block names (e.g. spaces).
    stream << QStringLiteral("*begin ; ") << trip.name << Qt::endl;
    if(!trip.name.isEmpty()) {
        QString sanitized = trip.name;
        sanitized.replace(QLatin1Char('"'), QLatin1Char('\''));
        stream << QStringLiteral("*title \"") << sanitized << QStringLiteral("\"") << Qt::endl;
    }

    writeDate(stream, trip.date.date());
    writeTeamData(stream, trip.team);
    writeCalibrations(stream, trip.calibrations, autoDeclinationInScope, gridConvergence); stream << Qt::endl;
    writeShotData(stream, trip, errors); stream << Qt::endl;
    writeLRUDData(stream, trip);

    stream << QStringLiteral("*end") << Qt::endl;
}

/**
  \brief Writes the calibrations to the stream

  This will write all the calibrations for the trip to the stream
  */
void cwSurvexExporter::writeCalibrations(QTextStream& stream,
                                         const cwTripCalibrationData& calibrations,
                                         bool autoDeclinationInScope,
                                         double gridConvergence) {
    using namespace cwSurvexExporterUtils;

    writeLengthUnits(stream, calibrations.distanceUnit());

    writeCalibration(stream, QStringLiteral("TAPE"), calibrations.tapeCalibration());

    double correctFrontsightCompass = calibrations.hasCorrectedCompassFrontsight() ? -180.0 : 0.0;
    writeCalibration(stream, QStringLiteral("COMPASS"), calibrations.frontCompassCalibration() + correctFrontsightCompass);

    double correctBacksightCompass = calibrations.hasCorrectedCompassBacksight() ? -180.0 : 0.0;
    writeCalibration(stream, QStringLiteral("BACKCOMPASS"), calibrations.backCompassCalibration() + correctBacksightCompass);

    double frontClinoScale = calibrations.hasCorrectedClinoFrontsight() ? -1.0 : 1.0;
    writeCalibration(stream, QStringLiteral("CLINO"), calibrations.frontClinoCalibration(), frontClinoScale);

    double backClinoScale = calibrations.hasCorrectedClinoBacksight() ? -1.0 : 1.0;
    writeCalibration(stream, QStringLiteral("BACKCLINO"), calibrations.backClinoCalibration(), backClinoScale);

    writeDeclinationCalibration(stream, calibrations.autoDeclination(),
                                calibrations.declinationManual(), autoDeclinationInScope,
                                gridConvergence);
}

/**
  \brief This writes length the units for the trip
  */
void cwSurvexExporter::writeLengthUnits(QTextStream &stream,
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
void cwSurvexExporter::writeShotData(QTextStream& stream, const cwTripData& trip, QStringList& errors) {
    bool hasFrontSights = trip.calibrations.hasFrontSights();
    bool hasBackSights = trip.calibrations.hasBackSights();

    //Make sure we have data to export
    if(!hasFrontSights && !hasBackSights) {
        stream << QStringLiteral("; NO DATA (doesn't have front or backsight data)") << Qt::endl;
        return;
    }

    QString dataLineComment;
    QString normalDataLine;

    if(hasFrontSights && hasBackSights) {
        normalDataLine = QStringLiteral("*data normal from to tape compass backcompass clino backclino");
        dataLineComment = QStringLiteral(";%1%2 %3 %4 %5 %6 %7")
                .arg(QStringLiteral("From"), kTextPadding)
                .arg(QStringLiteral("To"), kTextPadding)
                .arg(QStringLiteral("Distance"), kTextPadding)
                .arg(QStringLiteral("Compass"), kTextPadding)
                .arg(QStringLiteral("BackCompass"), kTextPadding)
                .arg(QStringLiteral("Clino"), kTextPadding)
                .arg(QStringLiteral("BackClino"), kTextPadding);
    } else if(hasFrontSights) {
        normalDataLine = kFrontSightDataLine;
        dataLineComment = QStringLiteral(";%1%2 %3 %4 %5")
                .arg(QStringLiteral("From"), kTextPadding)
                .arg(QStringLiteral("To"), kTextPadding)
                .arg(QStringLiteral("Distance"), kTextPadding)
                .arg(QStringLiteral("Compass"), kTextPadding)
                .arg(QStringLiteral("Clino"), kTextPadding);
    } else if(hasBackSights) {
        normalDataLine = QStringLiteral("*data normal from to tape backcompass backclino");
        dataLineComment = QStringLiteral(";%1%2 %3 %4 %5")
                .arg(QStringLiteral("From"), kTextPadding)
                .arg(QStringLiteral("To"), kTextPadding)
                .arg(QStringLiteral("Distance"), kTextPadding)
                .arg(QStringLiteral("BackCompass"), kTextPadding)
                .arg(QStringLiteral("BackClino"), kTextPadding);
    }

    stream << normalDataLine << Qt::endl;

    //Write out the comment line (this is the column headers)
    stream << dataLineComment << Qt::endl;

    for(int i = 0; i < trip.chunks.size(); i++) {
        //Write the chunk data
        writeChunk(stream, hasFrontSights, hasBackSights, trip.calibrations, trip.chunks.at(i), errors);
    }

    writeSplayData(stream, trip, normalDataLine, hasBackSights, errors);
}

/**
  \brief Writes the trip's splays, as legs to survex's anonymous wall station

  Splays hang off a station rather than sitting in the station/shot chain, so
  they follow the trip's centerline legs as their own block — the same shape
  writeLRUDData uses for the other station-attached data. Cavern solves them
  with the same declination and convergence as everything else in the trip.

  \a tripHasBackSights says whether \a normalDataLine declares backsight
  columns, which is the one case the splays need a reading order of their own.
  */
void cwSurvexExporter::writeSplayData(QTextStream& stream,
                                      const cwTripData& trip,
                                      const QString& normalDataLine,
                                      bool tripHasBackSights,
                                      QStringList& errors) {
    QStringList dataLines;

    for(const cwSurveyChunkData& chunk : trip.chunks) {
        for(const cwStation& station : chunk.stations) {
            if(!station.isValid()) { continue; }

            for(int i = 0; i < station.splayCount(); i++) {
                const QString line = splayLine(trip.calibrations, station.name(), station.splayAt(i), errors);
                if(!line.isEmpty()) {
                    dataLines.append(line);
                }
            }
        }
    }

    if(dataLines.isEmpty()) { return; }

    //A trip with backsight columns declares a reading order the splays don't
    //have, so they get their own *data line and hand the trip's back after.
    if(tripHasBackSights) {
        stream << kFrontSightDataLine << Qt::endl;
    }

    stream << QStringLiteral("*flags splay") << Qt::endl;
    stream << dataLines.join(QLatin1Char('\n')) << Qt::endl;
    stream << QStringLiteral("*flags not splay") << Qt::endl;

    if(tripHasBackSights) {
        stream << normalDataLine << Qt::endl;
    }
}

/**
  \brief One `<station> .. <tape> <compass> <clino>` line, or empty when the
  splay has no tape

  Cavern rejects a leg with an omitted tape reading, and a splay with no length
  says nothing about where the wall is, so it is dropped with an error naming
  the station it came from.
  */
QString cwSurvexExporter::splayLine(const cwTripCalibrationData& calibration,
                                    const QString& stationName,
                                    const cwShotMeasurement& splay,
                                    QStringList& errors) {
    if(splay.distance.state() != cwDistanceReading::State::Valid) {
        errors.append(QStringLiteral("Error: Skipped a splay at %1 that has no distance")
                          .arg(stationName));
        return QString();
    }

    QString clino = clinoToString(splay.clino);
    const QString verticalClino = cwSurvexExporterUtils::verticalClinoText(splay.clino);
    if(!verticalClino.isEmpty()) {
        clino = verticalClino;
    }

    //A plumbed splay is the one survex takes without a bearing. The text covers
    //both ways a reading gets there: State::Up/Down, and a Valid reading at ±90
    //that verticalClinoText rewrote.
    const bool isPlumbed = clino.compare(QStringLiteral("up"), Qt::CaseInsensitive) == 0
                           || clino.compare(QStringLiteral("down"), Qt::CaseInsensitive) == 0;

    if(splay.compass.state() == cwCompassReading::State::Empty && !isPlumbed) {
        errors.append(QStringLiteral("Error: Skipped a splay at %1 that has no compass reading")
                          .arg(stationName));
        return QString();
    }

    return QStringLiteral("%1 %2 %3 %4 %5")
        .arg(stationName, kTextPadding)
        .arg(kAnonymousWallStation, kTextPadding)
        .arg(toSupportedLength(calibration, splay.distance), kTextPadding)
        .arg(compassToString(splay.compass), kTextPadding)
        .arg(clino, kTextPadding);
}

/**
  \brief Writes the left right up down for each station in the trip, as
  a comment
  */
void cwSurvexExporter::writeLRUDData(QTextStream& stream, const cwTripData& trip) {

    const QString dataLineTemplate(QStringLiteral("%1 %2 %3 %4 %5"));

    for(const cwSurveyChunkData& chunk : trip.chunks) {
        QStringList dataLines;

        for(const cwStation& station : chunk.stations) {
            if(!station.isValid()) { continue; }

            // Stub "name - - - -" lines make cavern fail with "Cross section
            // specified at non-existent station" when the station isn't in
            // any exported shot (e.g. orphans from dropped empty rows).
            if(!cwSurvexExporterUtils::stationHasLrudData(station)) { continue; }

            dataLines.append(dataLineTemplate
                    .arg(station.name(), kTextPadding)
                    .arg(toSupportedLength(trip.calibrations, station.left()), kTextPadding)
                    .arg(toSupportedLength(trip.calibrations, station.right()), kTextPadding)
                    .arg(toSupportedLength(trip.calibrations, station.up()), kTextPadding)
                    .arg(toSupportedLength(trip.calibrations, station.down()), kTextPadding));
        }

        //A header with no cross sections under it is noise for the reader
        if(dataLines.isEmpty()) { continue; }

        stream << cwSurvexExporterUtils::passageDataHeader() << Qt::endl;
        stream << dataLines.join(QLatin1Char('\n')) << Qt::endl;
        stream << Qt::endl;
    }
}

/**
  Writes the team data to survex file
  */
void cwSurvexExporter::writeTeamData(QTextStream &stream, const cwTeamData& team)
{
    stream << Qt::endl;

    QString dataLineTemplate(QStringLiteral("*team \"%1\""));
    for(const cwTeamMember& teamMember : team.members) {
        QString dataLine = dataLineTemplate.arg(teamMember.name());
        stream << dataLine;

        const QStringList jobs = teamMember.jobs();
        for(const QString& job : jobs) {
            if (cwSurvexExporterUtils::isValidSurvexRole(job)) {
                stream << QStringLiteral(" ") << job;
            }
        }

        stream << Qt::endl;
    }
}

/**
  Writes the data to th stream
  */
void cwSurvexExporter::writeDate(QTextStream &stream, QDate date)
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
QString cwSurvexExporter::toSupportedLength(const cwTripCalibrationData& calibration, const cwDistanceReading& reading) {
    if(reading.state() == cwDistanceReading::State::Empty) {
        return QStringLiteral("-");
    }

    cwUnits::LengthUnit unit = calibration.distanceUnit();
    switch(unit) {
    case cwUnits::Meters:
    case cwUnits::Feet:
    case cwUnits::Yards:
        return cwSurvexExporterUtils::toSurvexNumber(reading.value());
    default:
        return QString::number(cwUnits::convert(reading.toDouble(), unit, cwUnits::Meters));
    }
}

/**
  This converts a compass bearing into a string based on the state.
  */
QString cwSurvexExporter::compassToString(const cwCompassReading& reading)
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
QString cwSurvexExporter::clinoToString(const cwClinoReading& reading)
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
void cwSurvexExporter::writeChunk(QTextStream& stream,
                                  bool hasFrontSights, //True if the dataset has backsights
                                  bool hasBackSights, //True if the dataset has frontsights
                                  const cwTripCalibrationData& calibration,
                                  const cwSurveyChunkData& chunk,
                                  QStringList& errors) {

    if(!hasBackSights && !hasFrontSights) {
        return;
    }

    QString dataLineTemplate;
    if(hasBackSights && hasFrontSights) {
        dataLineTemplate = QStringLiteral("%1 %2 %3 %4 %5 %6 %7");
    } else {
        dataLineTemplate = QStringLiteral("%1 %2 %3 %4 %5");
    }

    //A chunk carries one shot per leg. A snapshot with a short shots list has
    //legs with no reading at all, so stop at the last leg that has one.
    const qsizetype legCount = qMin(chunk.stations.size() - 1, chunk.shots.size());
    for(qsizetype i = 0; i < legCount; i++) {

        const cwStation& fromStation = chunk.stations.at(i);
        const cwStation& toStation = chunk.stations.at(i + 1);
        const cwShot& shot = chunk.shots.at(i);

        if(!fromStation.isValid() || !toStation.isValid()) { continue; }

        // Compass attaches passage dimensions to a dead-end station with a
        // synthetic zero-length shot that has no compass/clino (e.g.
        // "ALT13LRUD ALT13 0 - - - -"). Survex rejects that as a leg, so
        // equate the carrier station to its neighbor: same position, and the
        // *data passage cross section can still reference it.
        if(cwSurvexExporterUtils::isLrudOnlyShot(shot)) {
            stream << QStringLiteral("*equate ")
                   << fromStation.name() << QLatin1Char(' ') << toStation.name() << Qt::endl;
            continue;
        }

        // Stub "from to - - -" lines make cavern fail with "Tape reading may
        // not be omitted". The chunk's errorModel already warns the user.
        if(!shot.isValid()) { continue; }

        QString distance = toSupportedLength(calibration, shot.distance());
        QString compass = compassToString(shot.compass());
        QString backCompass = compassToString(shot.backCompass());
        QString clino = clinoToString(shot.clino());
        QString backClino = clinoToString(shot.backClino());
        const QString verticalClino = cwSurvexExporterUtils::verticalClinoText(shot.clino());
        if(!verticalClino.isEmpty()) {
            clino = verticalClino;
        }

        const QString verticalBackClino = cwSurvexExporterUtils::verticalClinoText(shot.backClino());
        if(!verticalBackClino.isEmpty()) {
            backClino = verticalBackClino;
        }

        //Make sure the model is good
        if(compass.isEmpty() && backCompass.isEmpty()) {
            if(clino.compare(QStringLiteral("up"), Qt::CaseInsensitive) != 0 &&
                    clino.compare(QStringLiteral("down"), Qt::CaseInsensitive) != 0 &&
                    backClino.compare(QStringLiteral("up"), Qt::CaseInsensitive) != 0 &&
                    backClino.compare(QStringLiteral("down"), Qt::CaseInsensitive) != 0) {
               errors.append(QStringLiteral("Error: No compass reading for %1 to %2")
                             .arg(fromStation.name())
                             .arg(toStation.name()));
           }
        }

        if(clino.isEmpty() && backClino.isEmpty()) {
            errors.append(QStringLiteral("Error: No Clino reading for %1 to %2")
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
                    .arg(fromStation.name(), kTextPadding)
                    .arg(toStation.name(), kTextPadding)
                    .arg(distance, kTextPadding)
                    .arg(compass, kTextPadding)
                    .arg(backCompass, kTextPadding)
                    .arg(clino, kTextPadding)
                    .arg(backClino, kTextPadding);
        } else if(hasFrontSights) {
            line = dataLineTemplate
                   .arg(fromStation.name(), kTextPadding)
                   .arg(toStation.name(), kTextPadding)
                   .arg(distance, kTextPadding)
                   .arg(compass, kTextPadding)
                   .arg(clino, kTextPadding);
        } else if(hasBackSights) {
            line = dataLineTemplate
                   .arg(fromStation.name(), kTextPadding)
                   .arg(toStation.name(), kTextPadding)
                   .arg(distance, kTextPadding)
                   .arg(backCompass, kTextPadding)
                   .arg(backClino, kTextPadding);
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
    }
}
