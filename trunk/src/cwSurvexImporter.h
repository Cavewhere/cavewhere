#ifndef CWSURVEYIMPORTER_H
#define CWSURVEYIMPORTER_H

//Qt includes
#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QMap>

//Our includes
class cwSurveyChunk;
class cwStation;
class cwShot;

class cwSurveyImporter : public QObject
{
Q_OBJECT

public:
    cwSurveyImporter(QObject* parent);

    bool hasErrors();
    QStringList errors();

    QList<cwSurveyChunk*> chunks();

    QString lastImport();

public slots:
    void importSurvex(QString filename);

signals:
    void finishedImporting();

private:

    enum State {
        FirstBegin,
        InsideBegin,
    };

    enum DataFormatType {
        To,
        From,
        Distance,
        Compass,
        BackCompass,
        Clino,
        BackClino
    };

    QList<cwSurveyChunk*> Chunks;
    QStringList Errors;

    State CurrentState;
    QStringList BeginNames;
    int CurrentLine;

    //Data map <Type, index>
    QMap<DataFormatType, int> DataFormat;
    QMap<QString, cwStation*> StationLookup;


    void clear();

    void loadFile(QString filename);
    void parseLine(QString line);
    void saveLastImport(QString filename);

    //Parser helper
    QString removeComment(QString& line);

    //Parsing the data format
    void importDataFormat(QString line);
    void useDefaultDataFormat();
    void importData(QString line);
    QString extractData(const QStringList data, DataFormatType type);
    cwStation* createOrLookupStation(QString stationName);
    void addShotToCurrentChunk(cwStation* fromStation, cwStation* toStation, cwShot* shot);

    //Error Messages
    void addError(QString error);
    void addWarning(QString error);

};

#endif // CWSURVEYIMPORTER_H
