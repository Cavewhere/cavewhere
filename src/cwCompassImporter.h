#ifndef CWCOMPASSIMPORTER_H
#define CWCOMPASSIMPORTER_H

//Our includes
#include "cwTask.h"
#include "cwCave.h"

//Qt include
#include <QRegExp>
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

    void setCompassDataFile(QString filename);

    cwCave cave() const;

protected:
    void runTask();
    
signals:
    
public slots:
    
private:
    //Input
    QString CompassDataFile;

    //Output
    cwCave Cave;

    //Status info
    int LineCount;
    cwTrip* CurrentTrip;
    QRegExp SurveyNameRegExp;
    QRegExp DateRegExp;

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

};

/**
 * @brief cwCompassImporter::setCompassDataFile
 * @param filename
 *
 * Sets the compass data file
 */
inline void cwCompassImporter::setCompassDataFile(QString filename)
{
    CompassDataFile = filename;
}

/**
 * @brief cwCompassImporter::cave
 * @return Returns the resulting cave.
 */
inline cwCave cwCompassImporter::cave() const
{
    return Cave;
}


#endif // CWCOMPASSIMPORTER_H
