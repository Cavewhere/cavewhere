/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWMETACAVESAVETASK_H
#define CWMETACAVESAVETASK_H

//Qt includes
#include <QString>

//Our includes
#include "cwRegionIOTask.h"
#include "cwReadingStates.h"
class cwCave;
class cwTrip;
class cwTeam;
class cwTeamMember;
class cwTripCalibration;
class cwSurveyChunk;
class cwStation;
class cwShot;

/**
 * @brief The cwMetaCaveSaveTask class
 *
 * This class will save data entered by the user into MetaCave format. Given an
 * orginial MetaCave JSON file, this will to preserve extra data for objects.
 *
 * See MetaCave formate documentation at https://github.com/jedwards1211/metacave/blob/master/README.md
 */
class cwMetaCaveSaveTask : public cwRegionIOTask
{
public:
    cwMetaCaveSaveTask();

protected:
    void runTask();

    QVariantMap saveRegion();
    QVariantMap saveCave(const cwCave* cave);
    QVariantMap saveTrip(const cwTrip* trip);
    QVariantList saveSurveyData(const cwTrip* trip);
    QVariantMap saveTeam(const cwTeam* team);
    QVariantMap saveTeamMember(const cwTeamMember& teamMember);
    QVariantList saveSurveyChunkData(const cwSurveyChunk* chunk);
    QVariantMap saveStation(const cwStation& station);
    QVariantMap saveShot(const cwShot& shot);


    void saveCalibration(QVariantMap &map, const cwTripCalibration *calibration);
    void saveClinoReading(QVariantMap &map, QString name, double value, cwClinoStates::State state);
    void saveCompassReading(QVariantMap &map, QString name, double value, cwCompassStates::State state);

};

#endif // CWMETACAVESAVETASK_H
