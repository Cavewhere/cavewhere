#ifndef CWCAVEDATA_H
#define CWCAVEDATA_H

//Qt includes
#include <QString>
#include <QStringList>
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
    //! <nodeDir>/sub/<name>/, so the list is the persisted shape of the tree.
    QList<cwCaveData> nodes;

    cwSurveyNodeKind::Kind kind = cwSurveyNodeKind::Kind::Cave;
    bool readOnly = false;
    QUuid sourceId;
    QString sourcePath;
};

/**
 * Calls visit(nodeData, nodePath) on every node in \a nodes and on their descendants,
 * pre-order. nodePath is the node-name chain from the region root down, root excluded,
 * which is what composes the node's directory on disk.
 */
template <class Visitor>
void walkCaveDataTree(const QList<cwCaveData>& nodes,
                      const QStringList& parentNodePath,
                      const Visitor& visit)
{
    for (const cwCaveData& nodeData : nodes) {
        const QStringList nodePath = parentNodePath + QStringList{nodeData.name};
        visit(nodeData, nodePath);
        walkCaveDataTree(nodeData.nodes, nodePath, visit);
    }
}

//! walkCaveDataTree() over a whole loaded region, whose caves are the root's children.
template <class Visitor>
void walkCaveDataTree(const QList<cwCaveData>& nodes, const Visitor& visit)
{
    walkCaveDataTree(nodes, QStringList(), visit);
}

#endif // CWCAVEDATA_H
