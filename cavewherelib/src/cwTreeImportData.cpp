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
    foreach(cwTreeImportDataNode* node, nodes) {
        node->setParent(this);
        node->setParentNode(nullptr);
    }

    RootNodes = nodes;
}
