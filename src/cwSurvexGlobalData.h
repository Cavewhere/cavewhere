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

//Qt includes
#include <QObject>
#include <QList>
#include <QStringList>

class cwSurvexGlobalData : public cwTreeImportData
{
public:
    cwSurvexGlobalData(QObject* object);

    QList<cwCave*> caves();

private:
    void cavesHelper(QList<cwCave*>* caves, cwTreeImportDataNode* currentBlock, cwCave* currentCave, cwTrip* trip);

    void fixStationNames(cwSurveyChunk* chunk, cwTreeImportDataNode* currentBlock);
    void fixDuplicatedStationInShot(cwSurveyChunk* chunk, cwTreeImportDataNode* caveSurvexBlock);

    void populateEquateMap(cwTreeImportDataNode* block);
    QString generateUniqueStationName(QString oldStationName, cwTreeImportDataNode* caveSurvexBlock) const;
};

#endif // CWSURVEXGLOBALDATA_H
