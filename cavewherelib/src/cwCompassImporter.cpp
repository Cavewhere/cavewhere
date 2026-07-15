/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCompassImporter.h"
#include "cwConcurrent.h"
#include "cwStationRenamer.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLength.h"
#include "cwDistanceReading.h"

//Qt includes
#include <QFile>
#include <QFileInfo>
#include <QPromise>
#include <QRegularExpression>

namespace {
    //Compass only stores measurements to 1/100 of a unit, so metric values are
    //rounded to the nearest centimeter.
    constexpr double CentimetersPerMeter = 100.0;

    //The progress bar advances over a fixed number of steps rather than raw
    //byte counts: byte totals would make setProgressValue churn on every line.
    //Bytes parsed are mapped onto this range so the determinate bar moves
    //smoothly regardless of file size.
    constexpr int kProgressSteps = 1000;

    //In a 15-char Compass FORMAT string the 14th character controls the
    //redundant backsight columns: 'B' (raw) or 'C' (corrected) means AZM2/INC2
    //are present, 'N' means none.
    constexpr int kBacksightFlagIndex = 13;
    constexpr QChar kNoBacksightsFlag = u'N';
}

/**
 * @brief The cwCompassImporter::Worker struct
 *
 * Holds all parse state for a single import run. A fresh Worker is created per
 * cwCompassImporter::run() call on the background thread, so the state is never
 * shared between imports.
 */
struct cwCompassImporter::Worker
{
    Worker(QStringList compassDataFiles, QPromise<Result>& promise) :
        m_compassDataFiles(std::move(compassDataFiles)),
        m_promise(promise),
        m_surveyNameRegExp(QStringLiteral("^SURVEY NAME:\\s*")),
        m_dateRegExp(QStringLiteral("SURVEY DATE:\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)")),
        m_calibrationRegExp(QStringLiteral("DECLINATION:\\s*(\\S+)(?:\\s+FORMAT:\\s*)?(\\w+)(?:\\s+CORRECTIONS:\\s*(\\S+)\\s+(\\S+)\\s+(\\S+)\\s*)?(?:\\s+CORRECTIONS2:\\s*(\\S+)\\s+(\\S+))?.*"))
    { }

    Result run();

private:
    //Input
    QStringList m_compassDataFiles;
    QPromise<Result>& m_promise;

    //Output
    QStringList m_messages;

    //Status info
    QList<cwCave*> m_caves;
    int m_lineCount = 0;
    QString m_currentFilename;
    cwCave* m_currentCave = nullptr;
    cwTrip* m_currentTrip = nullptr;
    bool m_currentFileGood = true;

    //Progress tracking (byte based)
    qint64 m_totalBytes = 0;
    qint64 m_bytesFromCompletedFiles = 0;
    int m_lastProgressStep = -1;

    //Regex
    QRegularExpression m_surveyNameRegExp;
    QRegularExpression m_dateRegExp;
    QRegularExpression m_calibrationRegExp;
    cwStationRenamer m_stationRenamer;

    void updateProgress(qint64 bytesProcessed);

    void verifyCompassDataFileExists();
    void parseFile();
    void parseSurvey(QFile* file);
    bool isFileGood(QFile* file, QString infoHelp);

    void parseCaveName(QFile* file);
    void parseTripName(QFile* file);
    void parseTripDate(QFile* file);
    void parseSurveyTeam(QFile* file);
    void parseSurveyFormatAndCalibration(QFile* file);
    void parseSurveyData(QFile* file);

    bool convertNumber(QString numberString, QString field, double* value);
};

/**
 * @brief cwCompassImporter::run
 * @param compassDataFiles - The compass .dat files to import
 * @return A future that resolves to the imported caves and any parse messages
 *
 * Runs the import on the shared task thread pool. The QPromise unlocks
 * determinate progress (setProgressRange/setProgressValue) and cancellation
 * (isCanceled) through QtConcurrent::run.
 */
QFuture<cwCompassImporter::Result> cwCompassImporter::run(QStringList compassDataFiles)
{
    return cwConcurrent::run([compassDataFiles = std::move(compassDataFiles)](QPromise<Result>& promise) mutable {
        Worker worker(std::move(compassDataFiles), promise);
        promise.addResult(worker.run());
    });
}

/**
 * @brief cwCompassImporter::Worker::run
 *
 * This begins the parser
 */
cwCompassImporter::Result cwCompassImporter::Worker::run()
{
    //Sum the file sizes for the determinate progress bar. If nothing is
    //readable, fall back to an indeterminate range.
    for(const auto& filename : m_compassDataFiles) {
        m_totalBytes += QFileInfo(filename).size();
    }

    if(m_totalBytes > 0) {
        m_promise.setProgressRange(0, kProgressSteps);
    } else {
        m_promise.setProgressRange(0, 0);
    }

    for(int i = 0; i < m_compassDataFiles.size(); i++) {
        if(m_promise.isCanceled()) { break; }

        m_currentFilename = m_compassDataFiles.at(i);
        m_caves.append(new cwCave());
        m_currentCave = m_caves.last();
        m_currentFileGood = true;
        m_currentTrip = nullptr;
        m_lineCount = 0;

        //Make sure file is good
        verifyCompassDataFileExists();

        //Parse the current file
        parseFile();

        if(!m_currentFileGood) {
            //Parsing error in the last cave, remove it from the list and free
            //it (the list owns the raw cwCave*; removeLast alone would leak).
            m_messages.append(QStringLiteral("Couldn't parse cave found in %1").
                            arg(m_currentFilename));
            delete m_caves.takeLast();
        }

        m_bytesFromCompletedFiles += QFileInfo(m_currentFilename).size();
        updateProgress(m_bytesFromCompletedFiles);
    }

    Result result;

    //A canceled import contributes nothing: hand back an empty result so no
    //partially parsed caves can be added to the caving region. Free the
    //half-built caves rather than leaking them.
    if(m_promise.isCanceled()) {
        qDeleteAll(m_caves);
        m_caves.clear();
        return result;
    }

    //Copy all the cave data
    result.caves.reserve(m_caves.size());
    for(auto cave : m_caves) {
        result.caves.append(cave->data());
        delete cave;
    }
    m_caves.clear();
    result.messages = m_messages;

    return result;
}

/**
 * @brief cwCompassImporter::Worker::updateProgress
 * @param bytesProcessed - Total bytes parsed so far across all files
 *
 * Maps the bytes parsed onto kProgressSteps and only pushes a new value when
 * the mapped step actually changes, so the future watcher isn't spammed.
 */
void cwCompassImporter::Worker::updateProgress(qint64 bytesProcessed)
{
    if(m_totalBytes <= 0) { return; }

    const int step = qRound(kProgressSteps *
                            (static_cast<double>(bytesProcessed) / static_cast<double>(m_totalBytes)));
    if(step != m_lastProgressStep) {
        m_lastProgressStep = step;
        m_promise.setProgressValue(step);
    }
}

/**
 * @brief cwCompassImporter::Worker::verifyCompassDataFileExists
 */
void cwCompassImporter::Worker::verifyCompassDataFileExists()
{
    if(!m_currentFileGood) { return; }

    QFileInfo fileInfo(m_currentFilename);
    if(!fileInfo.exists()) {
        //TODO: Fix error message
        m_messages.append(QStringLiteral("I can't parse %1 because it does not exist!").arg(m_currentFilename));
        m_currentFileGood = false;
    }

    if(!fileInfo.isReadable()) {
        //TODO: Fix error message
        m_messages.append(QStringLiteral("I can't parse %1 because it's not readable, change the permissions?").arg(m_currentFilename));
        m_currentFileGood = false;
    }

}

/**
 * @brief cwCompassImporter::Worker::parseFile
 *
 * Tries to parse the compass import file
 */
void cwCompassImporter::Worker::parseFile()
{
    if(!m_currentFileGood) { return; }

    //Open the file
    QFile file(m_currentFilename);
    bool okay = file.open(QFile::ReadOnly);

    if(!okay) {
        //TODO: Fix error message
        m_messages.append(QStringLiteral("I couldn't open %1").arg(m_currentFilename));
        m_currentFileGood = false;
        return;
    }

    //A survey that errors stops consuming input, so keep looping only while the
    //file is still good; otherwise parseSurvey would spin without advancing the
    //read position.
    while (!file.atEnd() && m_currentFileGood) {
        if(m_promise.isCanceled()) { return; }
        parseSurvey(&file);
        updateProgress(m_bytesFromCompletedFiles + file.pos());
    }
}

/**
 * @brief cwCompassImporter::Worker::parseSurvey
 * @param file
 *
 * This tries to parse the survey out of the file
 */
void cwCompassImporter::Worker::parseSurvey(QFile *file)
{
    if(!m_currentFileGood) { return; }

    m_currentTrip = new cwTrip();
    m_currentCave->addTrip(m_currentTrip);

    parseCaveName(file);
    parseTripName(file);
    parseTripDate(file);
    parseSurveyTeam(file);
    parseSurveyFormatAndCalibration(file);
    parseSurveyData(file);

    qint64 position = file->pos();
    QByteArray lastLine = file->readLine();
    if(!lastLine.contains(0x1A)) {
        file->seek(position); //Go back
    }
}

/**
 * @brief cwCompassImporter::Worker::isFileGood
 * @param file
 * @param infoHelp
 * @return True if the file is still good, and false if it has errored
 */
bool cwCompassImporter::Worker::isFileGood(QFile *file, QString infoHelp)
{
    if(file->error() != QFile::NoError) {
        //TODO: Fix error message
        m_messages.append(QStringLiteral("Hmm, while trying to parse %1, the Compass data file (%1) has error: %2. In %3 on line %4")
                        .arg(file->errorString())
                        .arg(infoHelp)
                        .arg(m_currentFilename)
                        .arg(m_lineCount));
        m_currentFileGood = false;
        return false;
    }
    return true;
}

/**
 * @brief cwCompassImporter::Worker::parseCaveName
 * @param file
 */
void cwCompassImporter::Worker::parseCaveName(QFile *file)
{
    if(!m_currentFileGood) { return; }
    QString caveName = file->readLine();

    caveName = caveName.trimmed();

    m_lineCount++;

    if(caveName.size() > 80) {
        m_messages.append(QStringLiteral("I found the cave name to be longer than 80 characters. I'm trimming it to 80 characters, in %1 on line %2")
                        .arg(m_currentFilename)
                        .arg(m_lineCount));
        caveName.resize(80);
    }


    if(!isFileGood(file, "cave name")) { return; }
    m_currentCave->setName(caveName);
}

/**
 * @brief cwCompassImporter::Worker::parseTripName
 * @param file
 */
void cwCompassImporter::Worker::parseTripName(QFile *file)
{
    if(!m_currentFileGood) { return; }
    QString tripName = file->readLine();
    tripName.remove(m_surveyNameRegExp);
    tripName = tripName.trimmed();

    m_lineCount++;

    if(!isFileGood(file, "survey name")) { return; }

    m_currentTrip->setName(tripName);
}

/**
 * @brief cwCompassImporter::Worker::parseTripDate
 * @param file
 *
 * This parses the trip's date from the input file
 */
void cwCompassImporter::Worker::parseTripDate(QFile *file)
{
    if(!m_currentFileGood) { return; }

    QString dateString = file->readLine();

    m_lineCount++;

    if(!isFileGood(file, "survey date")) { return; }

    // Perform the match
    QRegularExpressionMatch match = m_dateRegExp.match(dateString);

    if (!match.hasMatch()) {
        // Couldn't parse the date
        // TODO: Add warning that we couldn't parse the date
        m_messages.append(QStringLiteral("I couldn't parse the date in %1 on line %2")
                        .arg(m_currentFilename)
                        .arg(m_lineCount));
    } else {
        QString monthString = match.captured(1);
        QString dayString = match.captured(2);
        QString yearString = match.captured(3);

        bool okay;
        int month = monthString.toInt(&okay);
        if(!okay) {
            //TODO: Add warning that we couldn't parse the date
            m_messages.append(QStringLiteral("I couldn't understand the month.  I found \"%1\". It needs to be a number. Line %2")
                            .arg(monthString)
                            .arg(m_lineCount));
            return;
        }

        if(month < 1 || month > 12) {
            //Bad month
            m_messages.append(QStringLiteral("I found the month to be \"%1\" it needs to be between 1 and 12. Line %2")
                            .arg(month)
                            .arg(m_lineCount));

            return;
        }

        int day = dayString.toInt(&okay);
        if(!okay) {
            //TODO: Add warning that we couldn't parse the date
            m_messages.append(QStringLiteral("I couldn't understand the day.  I found \"%1\". It needs to be a number. Line %2")
                            .arg(dayString)
                            .arg(m_lineCount));
            return;
        }

        if(day < 1 || day > 31) {
            m_messages.append(QStringLiteral("I found an wrong day of the month, %1 on line %2. It should be between 1 and 31.")
                            .arg(day)
                            .arg(m_lineCount));
            return;
        }

        int year = yearString.toInt(&okay);
        if(!okay) {
            m_messages.append(QStringLiteral("I found the year isn't a number, on line %1").arg(m_lineCount));
            return;
        }

        if(year < 0) {
            m_messages.append(QStringLiteral("I found that the is negitive, on line %1").arg(m_lineCount));
            return;
        }

        if(year < 1900) {
            int newYear = 1900 + year;
            m_messages.append(QStringLiteral("I assuming year that %1 is really %2 on line %3")
                            .arg(year)
                            .arg(newYear)
                            .arg(m_lineCount));
            year = newYear;
        }

        QDate tripDate(year, month, day);
        m_currentTrip->setDate(QDateTime(tripDate, QTime()));
    }
}

/**
 * @brief cwCompassImporter::Worker::parseSurveyTeam
 * @param file
 */
void cwCompassImporter::Worker::parseSurveyTeam(QFile *file)
{
    if(!m_currentFileGood) { return; }
    QString surveyTeamLabel = file->readLine();
    surveyTeamLabel = surveyTeamLabel.trimmed();

    m_lineCount++;
    if(!isFileGood(file, "survey team")) { return; }

    if(surveyTeamLabel.compare("SURVEY TEAM:") != 0) {
        m_messages.append(QStringLiteral("I was expecting to find \"SURVEY TEAM:\" but instead found \"%1\", in %2 on line %3")
                        .arg(surveyTeamLabel)
                        .arg(m_currentFilename)
                        .arg(m_lineCount));
    }

    QString surveyTeam = file->readLine();
    surveyTeam = surveyTeam.trimmed();

    m_lineCount++;
    if(!isFileGood(file, "survey team")) { return; }

    if(surveyTeam.size() > 100) {
        m_messages.append(QStringLiteral("I found the team to be longer than 100 characters. I'm trimming it to 100 characters, in %1 on line %2")
                        .arg(m_currentFilename)
                        .arg(m_lineCount));
        surveyTeam.resize(100);
    }

    QRegularExpression delimiter;
    if (surveyTeam.contains(';')) {
        delimiter = QRegularExpression("\\s*;\\s*");
    } else {
        delimiter = QRegularExpression("\\s\\s+|\\s*,\\s*");
    }

    QStringList teamList = surveyTeam.split(delimiter, Qt::SkipEmptyParts);
    if(!teamList.isEmpty()) {

        QList<cwTeamMember> members;
        members.reserve(teamList.size());

        for(const QString& name : teamList) {
            cwTeamMember member(name, QStringList());
            members.append(member);
        }

        m_currentTrip->team()->setTeamMembers(members);
    }
}

/**
 * @brief cwCompassImporter::Worker::parseSurveyFormatAndCalibration
 * @param file
 */
void cwCompassImporter::Worker::parseSurveyFormatAndCalibration(QFile *file)
{
    if(!m_currentFileGood) { return; }
    QString calibrationLine = file->readLine();
    calibrationLine = calibrationLine.trimmed();

    m_lineCount++;
    if(!isFileGood(file, "calibration")) { return; }

    QRegularExpressionMatch match = m_calibrationRegExp.match(calibrationLine);

    if (match.hasMatch()) {
        QString declinationString = match.captured(1);
        QString fileFormatString = match.captured(2);
        QString compassCorrectionString = match.captured(3);
        QString clinoCorrectionString = match.captured(4);
        QString lengthCorrectionString = match.captured(5);

        double declination;
        if(convertNumber(declinationString, "declination", &declination)) {
            m_currentTrip->calibrations()->setDeclination(declination);
        }

        if(!(compassCorrectionString.isEmpty() ||
             clinoCorrectionString.isEmpty() ||
             lengthCorrectionString.isEmpty()))
        {

            double compassCorrection;
            double clinoCorrection;
            double lengthCorrection;

            if(convertNumber(compassCorrectionString, "compass correction", &compassCorrection)) {
                m_currentTrip->calibrations()->setFrontCompassCalibration(compassCorrection);
                m_currentTrip->calibrations()->setBackCompassCalibration(compassCorrection);
            }

            if(convertNumber(clinoCorrectionString, "clino correction", &clinoCorrection)) {
                m_currentTrip->calibrations()->setFrontClinoCalibration(clinoCorrection);
                m_currentTrip->calibrations()->setBackClinoCalibration(clinoCorrection);
            }

            if(convertNumber(lengthCorrectionString, "length correcton", &lengthCorrection)) {
                m_currentTrip->calibrations()->setTapeCalibration(lengthCorrection);
            }
        }

        if(fileFormatString.size() == 11 ||
                fileFormatString.size() == 12 ||
                fileFormatString.size() == 13 ||
                fileFormatString.size() == 15) {
            if(fileFormatString.at(0) != 'D') {
                m_messages.append(QStringLiteral("I can only understand Degrees for the Bearing Units. Converting all Bearing units to Degrees. In %1 on line %2")
                                .arg(m_currentFilename)
                                .arg(m_lineCount));
            }

            if(fileFormatString.at(1) == 'D') {
                m_currentTrip->calibrations()->setDistanceUnit(cwUnits::Feet);
            } else if(fileFormatString.at(1) == 'M') {
                m_currentTrip->calibrations()->setDistanceUnit(cwUnits::Meters);
            } else {
                m_currentTrip->calibrations()->setDistanceUnit(cwUnits::Feet);
                m_messages.append(QStringLiteral("I can't use Feet and Inches.  Converting all length measurements to decimal feet. In %1 on line %2")
                                .arg(m_currentFilename)
                                .arg(m_lineCount));
            }

            if(fileFormatString.at(3) != 'D') {
                m_messages.append(QStringLiteral("I can only understand Degrees for the Inclination Units. Converting all Inclination units to Degrees. In %1 on line %2")
                                .arg(m_currentFilename)
                                .arg(m_lineCount));
            }

            if(fileFormatString.size() >= 11) {
                bool hasBackSights = false;
                if(fileFormatString.size() >= 15) {
                    //The lowercase a/d in a 15-char format's shot-order section
                    //are always present and don't imply recorded backsights;
                    //the 14th character is what marks the AZM2/INC2 columns.
                    hasBackSights = fileFormatString.at(kBacksightFlagIndex) != kNoBacksightsFlag;
                } else if(fileFormatString.contains('B') || fileFormatString.contains('b')) {
                    hasBackSights = true;
                }
                m_currentTrip->calibrations()->setBackSights(hasBackSights);
            }

        } else if(fileFormatString.size() == 1) {
            //No format, so use some defaults
            m_currentTrip->calibrations()->setDistanceUnit(cwUnits::Feet);
            m_currentTrip->calibrations()->setBackSights(false);
        } else {
            m_messages.append(QStringLiteral("I found that the file format to be %1. It must be 11, 12, 13, 15 characters long. In file %2, on line %3")
                            .arg(fileFormatString.size())
                            .arg(m_currentFilename)
                            .arg(m_lineCount)
                            );
        }

    } else {
        m_messages.append(QStringLiteral("I couldn't understand the calibration line found in %1 on line %2")
                        .arg(m_currentFilename)
                        .arg(m_lineCount));
        m_currentFileGood = false;
    }
}

/**
 * @brief cwCompassImporter::Worker::parseSurveyData
 * @param file
 */
void cwCompassImporter::Worker::parseSurveyData(QFile *file)
{
    //Skip 3 lines
    file->readLine();
    m_lineCount++;
    if(!isFileGood(file, "")) { return; }

    file->readLine();
    m_lineCount++;
    if(!isFileGood(file, "")) { return; }

    file->readLine();
    m_lineCount++;
    if(!isFileGood(file, "")) { return; }

    QString dataLine;
    while(!file->atEnd()) {
        dataLine = file->readLine();
        m_lineCount++;
        if(!isFileGood(file, "data line")) { return; }

        //Make sure not at the end of the survey section
        if(!dataLine.isEmpty() && (dataLine.toLocal8Bit().at(0) == 0x0C
                                   || dataLine.toLocal8Bit().at(0) == 0x1A)
                ) {
            break;
        }

        QStringList dataStrings = dataLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if(dataStrings.size() >= 9) {
            QString fromStationName = dataStrings.at(0);
            QString toStationName = dataStrings.at(1);

            if(fromStationName == toStationName) {
                continue;
            }

            QString lengthString = dataStrings.at(2);
            QString bearingString = dataStrings.at(3);
            QString inclinationString = dataStrings.at(4);
            QString leftString = dataStrings.at(5);
            QString upString = dataStrings.at(6);
            QString downString = dataStrings.at(7);
            QString rightString = dataStrings.at(8);
            int flagsIndex = 9;
            if(m_currentTrip->calibrations()->hasBackSights()) {
                flagsIndex = 11;
            }
            QString flags = dataStrings.size() > flagsIndex ? dataStrings.at(flagsIndex) : QString();

            cwStation fromStation = m_stationRenamer.createStation(fromStationName);
            cwStation toStation = m_stationRenamer.createStation(toStationName);
            cwShot shot;

            double length;
            double bearing;
            double inclination;
            double left;
            double right;
            double up;
            double down;

            QString missingEntry = "-999.00";
            QString lrudMissingEntry = "-9.90";

            if(lengthString != missingEntry) {
                if(!convertNumber(lengthString, "length", &length)) { m_currentFileGood = false; return; }

                //Compass stores the length in the survey's distance unit already
                //(decimal feet for D/I/F formats, meters for M format), so no
                //unit conversion is needed here.
                if(m_currentTrip->calibrations()->distanceUnit() == cwUnits::Meters) {
                    //Round to the nearest cm - Compass only stores 1/100 of a unit.
                    length = qRound(length * CentimetersPerMeter) / CentimetersPerMeter;
                }
                shot.setDistance(cwDistanceReading(length));
            }

            if(bearingString != missingEntry) {
                if(!convertNumber(bearingString, "bearing", &bearing)) { m_currentFileGood = false; return; }
                shot.setCompass(bearingString);
            }

            if(inclinationString != missingEntry) {
                if(!convertNumber(inclinationString, "inclination", &inclination)) { m_currentFileGood = false; return; }
                shot.setClino(inclinationString);
            }

            auto isLrudMissing = [&missingEntry, &lrudMissingEntry](const QString& value) {
                return value == missingEntry || value == lrudMissingEntry;
            };

            //Like the shot length, the LRUD passage dimensions are stored in
            //the survey's distance unit already, so the raw Compass string is
            //used as-is (matching how compass/clino keep their raw strings).
            //convertNumber still validates the field and flags parse errors.
            if(!isLrudMissing(leftString)) {
                if(!convertNumber(leftString, "left", &left)) { m_currentFileGood = false; return; }
                fromStation.setLeft(leftString);
            }

            if(!isLrudMissing(rightString)) {
                if(!convertNumber(rightString, "right", &right)) { m_currentFileGood = false; return; }
                fromStation.setRight(rightString);
            }

            if(!isLrudMissing(upString)) {
                if(!convertNumber(upString, "up", &up)) { m_currentFileGood = false; return; }
                fromStation.setUp(upString);
            }

            if(!isLrudMissing(downString)) {
                if(!convertNumber(downString, "down", &down)) { m_currentFileGood = false; return; }
                fromStation.setDown(downString);
            }

            if(m_currentTrip->calibrations()->hasBackSights() && dataStrings.size() >= 11) {
                QString backCompassString = dataStrings.at(9);
                QString backClinoString = dataStrings.at(10);

                double backCompass;
                double backClino;

                if(backCompassString != missingEntry) {
                    if(!convertNumber(backCompassString, "back compass", &backCompass)) { m_currentFileGood = false; return; }
                    shot.setBackCompass(backCompassString);
                }

                if(backClinoString != missingEntry) {
                    if(!convertNumber(backClinoString, "back clino", &backClino)) { m_currentFileGood = false; return; }
                    shot.setBackClino(backClinoString);
                }
            }

            //Exclude length from calculation
            if(flags.contains("L")) {
                shot.setDistanceIncluded(false);
            }

            if(shot.isValid()) {
                m_currentTrip->addShotToLastChunk(fromStation, toStation, shot);
                if(!m_currentTrip->chunks().isEmpty()) {
                    auto chunks = m_currentTrip->chunks();
                    cwSurveyChunk* lastChunk = chunks.last();
                    int secondToLastStation = lastChunk->stationCount() - 2;
                    lastChunk->setStation(fromStation, secondToLastStation);
                }
            }
        } else {
            m_messages.append(QStringLiteral("Data string doesn't have enough fields. I need at least 9 but found only %1 in %2 on line %3")
                            .arg(dataStrings.size())
                            .arg(m_currentFilename)
                            .arg(m_lineCount));
        }
    }
}

/**
 * @brief cwCompassImporter::Worker::convertNumber
 * @param numberString
 * @param field
 * @param value
 * @return
 */
bool cwCompassImporter::Worker::convertNumber(QString numberString, QString field, double *value)
{
    bool okay;
    *value = numberString.toDouble(&okay);
    if(!okay) {
        m_messages.append(QStringLiteral("I couldn't read %1 because it's not a number (I found \"%4\" instead) in %2 on line %3")
                        .arg(field)
                        .arg(m_currentFilename)
                        .arg(m_lineCount)
                        .arg(numberString));
        return false;
    }
    return true;
}
