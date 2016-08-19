/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYIMPORTER_H
#define CWSURVEYIMPORTER_H

//Qt includes
#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QMap>
#include "cwTreeDataImporter.h"
#include <QFile>

//Our includes
#include "cwStation.h"
#include "cwSurvexGlobalData.h"
class cwSurveyChunk;
class cwShot;
class cwSurvexNodeData;

class cwSurvexImporter : public cwTreeDataImporter
{
Q_OBJECT

public:
    cwSurvexImporter(QObject* parent = nullptr);

    bool hasParseErrors();
    QStringList parseErrors();

    QString lastImport();

    cwTreeImportData* data();

public slots:
    void setInputFiles(QStringList filenames);

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
        Passage,
        NoSurvey
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
    QStringList RootFilenames;

    //File state to handle includes
    QList<Include> IncludeStack;

    //Already included files
    QStringList IncludeFiles;

    //Handles block state
    QList<BeginEndState> BeginEndStateStack;

    //The data that'll be populated
    cwTreeImportDataNode* RootBlock; //All blocks are child of this object
    cwTreeImportDataNode* CurrentBlock; //The current block
    cwSurvexGlobalData* GlobalData; //Where all the fix points and other global data is stored

    cwSurvexNodeData* nodeData(cwTreeImportDataNode* node);

    QStringList Errors;

    State CurrentState;

    //Data map <Type, index>
    QMap<DataFormatType, int> DataFormat;

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
    void addShotToCurrentChunk(cwStation fromStation,
                               cwStation toStation,
                               cwShot shot);
    void addCalibrationToCurrentChunk(cwTripCalibration* calibration);

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
    void parseUnits(QString line);
    void parseEquate(QString line);
    void parseExport(QString line);
    void parseFlags(QString line);

    void runStats(QString filename);

    void updateLRUDForCurrentBlock();
    void updateStationLRUD(cwStation before, cwStation station, cwStation after);
};

/**
  \brief Gets all the data from the importer
  */
inline cwTreeImportData* cwSurvexImporter::data() {
    return static_cast<cwTreeImportData*>(GlobalData);
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
inline void cwSurvexImporter::setInputFiles(QStringList filenames) {
    RootFilenames = filenames;
}

inline cwSurvexNodeData* cwSurvexImporter::nodeData(cwTreeImportDataNode* node) {
    return GlobalData->nodeData(node);
}

#endif // CWSURVEYIMPORTER_H
