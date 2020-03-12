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

class CAVEWHERE_LIB_EXPORT cwCSVImporterTask
{

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

        ~Output() {
            for(auto cave : caves) {
                delete cave;
            }
        }

        QList<cwCave*> takeCaves() {
            auto cavesToTake = caves;
            caves.clear();
            return cavesToTake;
        }

        QList<cwCave*> caves;
        QList<cwError> errors;
        QList<QStringList> lines;
        QString text;
        int lineCount = 0;
    };

    typedef QSharedPointer<Output> OutputPtr;

    cwCSVImporterTask();
    virtual ~cwCSVImporterTask() {}

    void setSettings(const cwCSVImporterSettings& settings);
    cwCSVImporterSettings settings() const;

    OutputPtr operator()() const;

private:
    //Input
    cwCSVImporterSettings Settings;

    //Helper functions
    int lrudStationIndex(const cwSurveyChunk *chunk, const cwStation& from, const cwStation& to) const;

};

/**
 * Returns the settings for the importer task.
 */
inline cwCSVImporterSettings cwCSVImporterTask::settings() const
{
    return Settings;
}

#endif // CWCSVIMPORTERTASK_H
