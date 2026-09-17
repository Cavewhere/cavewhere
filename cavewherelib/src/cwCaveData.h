#ifndef CWCAVEDATA_H
#define CWCAVEDATA_H

//Qt includes
#include <QString>
#include <QUuid>

//Our includes
#include "cwEquate.h"
#include "cwExternalCenterline.h"
#include "cwTripData.h"
#include "cwStationPositionLookup.h"
#include "cwUnits.h"
#include "cwFixStation.h"
#include "cwSurveyNodeKind.h"

struct cwCaveData {
    QString name;
    QList<cwTripData> trips;
    cwStationPositionLookup stationPositionModel; //TODO: remove stationPositionModel?
    QUuid id;
    cwUnits::LengthUnit lengthUnit = cwUnits::Meters;
    cwUnits::LengthUnit depthUnit = cwUnits::Meters;
    QList<cwFixStation> fixStations;
    cwExternalCenterline externalCenterline;
    QList<cwEquate> equates;

    //! This node's child nodes, in row order. Each is saved under
    //! <nodeDir>/nodes/<name>/, so the list is the persisted shape of the tree.
    QList<cwCaveData> nodes;

    cwSurveyNodeKind::Kind kind = cwSurveyNodeKind::Kind::Cave;
    bool readOnly = false;
    QUuid sourceId;
    QString sourcePath;
};

#endif // CWCAVEDATA_H
