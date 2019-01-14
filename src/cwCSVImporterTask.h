#ifndef CWCSVIMPORTERTASK_H
#define CWCSVIMPORTERTASK_H

//Our includes
#include "cwTask.h"
#include "cwCave.h"
#include "cwGlobals.h"
#include "cwCSVImporterSettings.h"
#include "cwError.h"
class cwSurveyChunk;

//Qt includes
#include <QString>

class CAVEWHERE_LIB_EXPORT cwCSVImporterTask : public cwTask
{
    Q_OBJECT
public:
    enum Column {
        ToStation,
        FromStation,
        Length,
        CompassFrontSight,
        CompassBackSight,
        ClinoFrontSight,
        ClinoBackSight,
        Left,
        Right,
        Up,
        Down,
        Skip
    };

    class Output {
    public:
        QList<cwCave> caves;
        QList<cwError> errors;
        QList<QStringList> lines;
    };

    cwCSVImporterTask();
    virtual ~cwCSVImporterTask() {}

    void setSettings(const cwCSVImporterSettings& settings);
    cwCSVImporterSettings settings() const;

    Output output() const;
//    QList<cwCave> caves() const;

protected:
    void runTask();

private:

    //Input
    cwCSVImporterSettings Settings;

    //Output
    Output CSVOutput;

    //Helper functions
    int lrudStationIndex(const cwSurveyChunk *chunk, const cwStation& from, const cwStation& to) const;

};

#endif // CWCSVIMPORTERTASK_H
