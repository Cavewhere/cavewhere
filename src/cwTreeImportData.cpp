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

void cwTreeImportData::setBlocks(QList<cwTreeImportDataNode*> blocks) {
    foreach(cwTreeImportDataNode* block, blocks) {
        block->setParent(this);
        block->setParentBlock(nullptr);
    }

    RootBlocks = blocks;
}

/**
  \brief Get's the current errors
  */
QStringList cwTreeImportData::erros() {
    return QStringList();
}
