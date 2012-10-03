#ifndef CWCOMPASSIMPORTER_H
#define CWCOMPASSIMPORTER_H

//Our includes
#include "cwTask.h"
#include "cwCave.h"

//Qt include
#include <QRegExp>
#include <QStringList>
class QFile;


/**
 * @brief The cwCompassImporter class
 *
 * This allow cavewhere to import a compass dat file.
 */
class cwCompassImporter : public cwTask
{
    Q_OBJECT
public:
    explicit cwCompassImporter(QObject *parent = 0);

    void setCompassDataFiles(QStringList filename);

    QList<cwCave> caves() const;

protected:
    void runTask();
    
signals:
    
public slots:
    
private:
    //Input
    QStringList CompassDataFiles;

    //Output
    QList<cwCave> Caves;

    //Status info
    int LineCount;
    QString CurrentFilename;
    cwCave* CurrentCave;
    cwTrip* CurrentTrip;
    bool CurrentFileGood;
    bool UsingBacksight;

    //Regex
    QRegExp SurveyNameRegExp;
    QRegExp DateRegExp;
    QRegExp CalibrationRegExp;

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
 * @brief cwCompassImporter::cave
 * @return Returns the resulting cave.
 */
inline QList<cwCave> cwCompassImporter::caves() const
{
    return Caves;
}

/**
 * @brief cwCompassImporter::setCompassDataFiles
 * @param filenames - Sets the compass data file list
 */
inline void cwCompassImporter::setCompassDataFiles(QStringList filenames)
{
    CompassDataFiles = filenames;
}


#endif // CWCOMPASSIMPORTER_H
