/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXGLOBALDATA_H
#define CWSURVEXGLOBALDATA_H

//Our includes
#include "cwTreeImportData.h"
class cwCave;
class cwTrip;
class cwSurveyChunk;
#include "cwStation.h"
#include "cwUndoer.h"
class cwSurvexNodeData;

//Qt includes
#include <QHash>
#include <QObject>
#include <QList>
#include <QStringList>

class cwSurvexGlobalData : public cwTreeImportData
{
public:
    cwSurvexGlobalData(QObject* object = nullptr);

    QList<cwCave*> caves();

    cwSurvexNodeData* nodeData(cwTreeImportDataNode* node);

private:
    QHash<cwTreeImportDataNode*, cwSurvexNodeData*> NodeData;

    void cavesHelper(QList<cwCave*>* caves, cwTreeImportDataNode* currentBlock, cwCave* currentCave, cwTrip* trip);

    void fixStationNames(cwSurveyChunk* chunk, cwTreeImportDataNode* currentBlock);
    void fixDuplicatedStationInShot(cwSurveyChunk* chunk, cwTreeImportDataNode* caveSurvexBlock);

    void populateEquateMap(cwTreeImportDataNode* block);
    QString generateUniqueStationName(QString oldStationName, cwTreeImportDataNode* caveSurvexBlock);
};

#endif // CWSURVEXGLOBALDATA_H
