//Our includes
#include "cwCompassImporter.h"
#include "cwTrip.h"

//Qt includes
#include <QFileInfo>

cwCompassImporter::cwCompassImporter(QObject *parent) :
    cwTask(parent),
    SurveyNameRegExp("^SURVEY NAME:\\s*"),
    DateRegExp("^SURVEY DATE:\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)")
{

}



/**
 * @brief cwCompassImporter::runTask
 *
 * This begins the parser
 */
void cwCompassImporter::runTask()
{
    Cave = cwCave(); //Clear the current cave

    //Make sure file is good
    verifyCompassDataFileExists();


    done();
}

/**
 * @brief cwCompassImporter::verifyCompassDataFileExists
 */
void cwCompassImporter::verifyCompassDataFileExists()
{
    if(!isRunning()) { return; }

    QFileInfo fileInfo(CompassDataFile);
    if(!fileInfo.exists()) {
        //TODO: Fix error message
        statusMessage(QString("I can't parse %1 because it does not exist!").arg(CompassDataFile));
        stop();
    }

    if(!fileInfo.isReadable()) {
        //TODO: Fix error message
        statusMessage(QString("I can't parse %1 because it's not readable, change the permissions?").arg(CompassDataFile));
        stop();
    }

}

/**
 * @brief cwCompassImporter::parseFile
 *
 * Tries to parse the compass import file
 */
void cwCompassImporter::parseFile()
{
    if(!isRunning()) { return; }

    //Open the file
    QFile file(CompassDataFile);
    bool okay = file.open(QFile::ReadOnly);

    if(!okay) {
        //TODO: Fix error message
        statusMessage(QString("I couldn't open %1").arg(CompassDataFile));
        stop();
    }

    while (!file.atEnd()) {
        parseSurvey(&file);
    }
}

/**
 * @brief cwCompassImporter::parseSurvey
 * @param file
 *
 * This tries to parse the survey out of the file
 */
void cwCompassImporter::parseSurvey(QFile *file)
{
    if(!isRunning()) { return; }

    CurrentTrip = new cwTrip();

    parseCaveName(file);
    parseTripName(file);
    parseTripDate(file);
    parseSurveyTeam(file);
    parseSurveyFormatAndCalibration(file);
    parseSurveyData(file);

}

/**
 * @brief cwCompassImporter::isFileGood
 * @param file
 * @param infoHelp
 * @return True if the file is still good, and false if it has errored
 */
bool cwCompassImporter::isFileGood(QFile *file, QString infoHelp)
{
    if(file->error() != QFile::NoError) {
        //TODO: Fix error message
        statusMessage(QString("Hmm, while trying to parse %1, the Compass data file (%1) has error: %2. On line %3").arg(file->errorString()).arg(infoHelp).arg(LineCount));
        stop();
        return false;
    }
    return true;
}

/**
 * @brief cwCompassImporter::parseCaveName
 * @param file
 */
void cwCompassImporter::parseCaveName(QFile *file)
{
    if(!isRunning()) { return; }
    QString caveName = file->readLine();

    if(caveName.size() > 80) {
        caveName.resize(80);
    }

    LineCount++;
    if(!isFileGood(file, "cave name")) { return; }
    Cave.setName(caveName);
}

/**
 * @brief cwCompassImporter::parseTripName
 * @param file
 */
void cwCompassImporter::parseTripName(QFile *file)
{
    if(!isRunning()) { return; }
    QString tripName = file->readLine();
    tripName.remove(SurveyNameRegExp);

    if(tripName.size() > 12) {
        tripName.resize(12);
    }

    LineCount++;

    if(!isFileGood(file, "survey name")) { return; }

    CurrentTrip->setName(tripName);
}

/**
 * @brief cwCompassImporter::parseTripDate
 * @param file
 *
 * This parses the trip's date from the input file
 */
void cwCompassImporter::parseTripDate(QFile *file)
{
    if(!isRunning()) { return; }

    QString dateString = file->readLine();

    LineCount++;

    if(!isFileGood(file, "survey date")) { return; }

    if(DateRegExp.indexIn(dateString) == -1)  {
        //Couldn't parse the date
        //TODO: Add warning that we couldn't parse the date
        statusMessage(QString("I couldn't parse the date on line %1").arg(LineCount));
    } else {
        QString monthString = DateRegExp.cap(1);
        QString dayString = DateRegExp.cap(2);
        QString yearString = DateRegExp.cap(3);

        bool okay;
        int month = monthString.toInt(&okay);
        if(!okay) {
            //TODO: Add warning that we couldn't parse the date
            statusMessage(QString("I couldn't understand the month.  I found \"%1\". It needs to be a number. Line %2")
                          .arg(monthString)
                          .arg(LineCount));
            return;
        }

        if(month < 1 || month > 12) {
            //Bad month
            statusMessage(QString("I found the month to be \"%1\" it needs to be between 1 and 12. Line %2")
                          .arg(month)
                          .arg(LineCount));

            return;
        }

        int day = dayString.toInt(&okay);
        if(!okay) {
            //TODO: Add warning that we couldn't parse the date
            statusMessage(QString("I couldn't understand the day.  I found \"%1\". It needs to be a number. Line %2")
                          .arg(dayString)
                          .arg(LineCount));
            return;
        }

        if(day < 1 || day > 31) {
            statusMessage(QString("I found an wrong day of the month, %1 on line %2. It should be between 1 and 31.")
                          .arg(day)
                          .arg(LineCount));
            return;
        }

        int year = yearString.toInt(&okay);
        if(!okay) {
            statusMessage(QString("I found the year isn't a number, on line %1").arg(LineCount));
            return;
        }

        if(year < 0) {
            statusMessage(QString("I found that the is negitive, on line %1").arg(LineCount));
            return;
        }

        if(year < 1900) {
            int newYear = 1900 + year;
            statusMessage(QString("I assuming year that %1 is really %2 on line %3")
                          .arg(year)
                          .arg(newYear)
                          .arg(LineCount));
            year = newYear;
        }

        QDate tripDate(year, month, day);
        CurrentTrip->setDate(tripDate);
    }
}

/**
 * @brief cwCompassImporter::parseSurveyTeam
 * @param file
 */
void cwCompassImporter::parseSurveyTeam(QFile *file)
{
//    if(!isRunning())
}

/**
 * @brief cwCompassImporter::parseSurveyFormatAndCalibration
 * @param file
 */
void cwCompassImporter::parseSurveyFormatAndCalibration(QFile *file)
{

}

/**
 * @brief cwCompassImporter::parseSurveyData
 * @param file
 */
void cwCompassImporter::parseSurveyData(QFile *file)
{

}
