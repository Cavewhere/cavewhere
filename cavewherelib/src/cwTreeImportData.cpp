/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTreeImportData.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwDebug.h"

cwTreeImportData::cwTreeImportData(QObject* parent) :
    QObject(parent)
{

}

void cwTreeImportData::setNodes(QList<cwTreeImportDataNode*> nodes) {
    //Importers build the node tree on a worker thread. Move the whole tree onto
    //this object's thread before reparenting so setParent doesn't cross threads.
    foreach(cwTreeImportDataNode* node, nodes) {
        cwTreeImportDataNode::moveTreeToThread(node, thread());
        node->setParent(this);
        node->setParentNode(nullptr);
    }

    RootNodes = nodes;
}
