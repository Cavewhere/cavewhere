#ifndef CWCAVEDATA_H
#define CWCAVEDATA_H

//Qt includes
#include <QString>
#include <QStringList>
#include <QUuid>

//Our includes
#include "cwExternalCenterline.h"
#include "cwTripData.h"
#include "cwUnits.h"
#include "cwFixStation.h"
#include "cwSurveyNodeKind.h"

struct cwCaveData {
    QString name;
    QList<cwTripData> trips;
    QUuid id;
    cwUnits::LengthUnit lengthUnit = cwUnits::Meters;
    cwUnits::LengthUnit depthUnit = cwUnits::Meters;
    QList<cwFixStation> fixStations;
    cwExternalCenterline externalCenterline;

    //! This node's child nodes, in row order. Each is saved under
    //! <nodeDir>/sub/<name>/, so the list is the persisted shape of the tree.
    QList<cwCaveData> nodes;

    cwSurveyNodeKind::Kind kind = cwSurveyNodeKind::Kind::Cave;
    bool readOnly = false;
    QUuid sourceId;
    QString sourcePath;

    //! Snapshot of cwSurveyNode::externalStations() so the driver export can
    //! check a fix against the node's attached file. Derived scan output, never
    //! persisted.
    QStringList externalStations;

    //! Snapshot of cwSurveyNode::cavernLabel(): the naming level cavern builds
    //! for a derived node, from its source's units.json. Derived, never
    //! persisted.
    QString cavernLabel;
};

//! The node a sourced subtree hangs from: sourced, with no path of its own
//! inside the source. The snapshot twin of cwSurveyNode::isSourceRoot().
inline bool isSourceRoot(const cwCaveData& node)
{
    return !node.sourceId.isNull() && node.sourcePath.isEmpty();
}

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
