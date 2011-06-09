#ifndef CWSURVEYIMPORTER_H
#define CWSURVEYIMPORTER_H

//Qt includes
#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QMap>
#include <cwTask.h>
#include <QFile>

//Our includes
#include <cwStationReference.h>
class cwSurvexBlockData;
class cwSurvexGlobalData;
class cwSurveyChunk;
class cwShot;

class cwSurvexImporter : public cwTask
{
Q_OBJECT

public:
    cwSurvexImporter(QObject* parent = NULL);

    bool hasErrors();
    QStringList errors();

    QString lastImport();

    cwSurvexGlobalData* data();

public slots:
    void setSurvexFile(QString filename);

protected:
    virtual void runTask();

private:

    enum State {
        FirstBegin,
        InsideBegin
    };

    enum DataFormatType {
        To,
        From,
        Distance,
        Compass,
        BackCompass,
        Clino,
        BackClino,
        Ignore,
        IgnoreAll,
        Station,
        Left,
        Right,
        Up,
        Down
    };

    /**
      This are the survex import types, currently only normal and passage are supported
      */
    enum DataEntryType {
        Normal,
        Passage
    };

    class Include {
    public:
        Include(QString file) :
            File(file),
            CurrentLine(0)
        {
        }

        QString File;
        int CurrentLine;
    };

    class BeginEndState {
    public:
        BeginEndState();

        QMap<DataFormatType, int> DataFormat;
        DataEntryType DataType;
        QString Filename;

        static QMap<DataFormatType, int> defaultDataFormat();
    };

    //Root filename
    QString RootFilename;

    //File state to handle includes
    QList<Include> IncludeStack;

    //Already included files
    QStringList IncludeFiles;

    //Handles block state
    QList<BeginEndState> BeginEndStateStack;

    //The data that'll be populated
    cwSurvexBlockData* RootBlock; //All blocks are child of this object
    cwSurvexBlockData* CurrentBlock; //The current block
    cwSurvexGlobalData* GlobalData; //Where all the fix points and other global data is stored

    QStringList Errors;

    State CurrentState;

    //Data map <Type, index>
    QMap<DataFormatType, int> DataFormat;
    QMap<QString, cwStationReference> StationLookup;

    //For progress
    int TotalNumberOfLines;
    int CurrentTotalNumberOfLines;

    void importSurvex(QString filename);

    void clear();

    void loadFile(QString filename);
    bool openFile(QFile& file, QString filename);
    void parseLine(QString line);
    void saveLastImport(QString filename);

    //Parser helper
    QString removeComment(QString& line);

    //Parsing the data format
    void parseDataFormat(QString line);

    //Helper to parseNormalData and parsePassageData
    QStringList parseData(QString line);

    void parseNormalData(QString line);
    QString extractData(const QStringList data, DataFormatType type);
    cwStationReference createOrLookupStation(QString stationName);
    void addShotToCurrentChunk(cwStationReference fromStation,
                               cwStationReference toStation,
                               cwShot* shot);

    void parsePassageData(QString line);

    //Error Messages
    void addError(QString error);
    void addWarning(QString error);
    void addToErrors(QString prefix, QString errorMessage);

    QString fullStationName(QString name);

    QMap<DataFormatType, int> currentDataFormat() const;
    DataEntryType currentDataEntryType() const;
    void setCurrentDataFormat(QMap<DataFormatType, int> format);
    void setCurrentDataEntryType(DataEntryType type);

    QString currentFile() const;
    int currentLineNumber() const;
    void increamentLineNumber();

    bool compare(QString s1, QString s2) const;

    void parseDate(QString args);
    void parseTeamMember(QString line);
    void parseCalibrate(QString line);

    void runStats(QString filename);
};

/**
  \brief Gets all the data from the importer
  */
inline cwSurvexGlobalData* cwSurvexImporter::data() {
    return GlobalData;
}

/**
  \brief compares the to strings with case insensitive
  */
inline bool cwSurvexImporter::compare(QString s1, QString s2) const {
    return s1.compare(s2, Qt::CaseInsensitive) == 0;
}

/**
  \brief Sets the root file for the survex
  */
inline void cwSurvexImporter::setSurvexFile(QString filename) {
    RootFilename = filename;
}

#endif // CWSURVEYIMPORTER_H
