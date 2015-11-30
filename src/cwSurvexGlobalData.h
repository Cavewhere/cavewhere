/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXGLOBALDATA_H
#define CWSURVEXGLOBALDATA_H

//Our includes
class cwSurvexBlockData;
class cwCave;
class cwTrip;
class cwSurveyChunk;
#include "cwStation.h"
#include "cwUndoer.h"

//Qt includes
#include <QObject>
#include <QList>
#include <QStringList>

class cwSurvexGlobalData : public QObject
{
public:
    cwSurvexGlobalData(QObject* object);

    QList<cwSurvexBlockData*> blocks() const;
    void setBlocks(QList<cwSurvexBlockData*> blocks);

    QList<cwCave*> caves();
    QStringList erros();

private:
    QList<cwSurvexBlockData*> RootBlocks;

    QStringList ImportErrors;

    void cavesHelper(QList<cwCave*>* caves, cwSurvexBlockData* currentBlock, cwCave* currentCave, cwTrip* trip);

    void fixStationNames(cwSurveyChunk* chunk, cwSurvexBlockData* currentBlock);
    void fixDuplicatedStationInShot(cwSurveyChunk* chunk, cwSurvexBlockData* caveSurvexBlock);

    void populateEquateMap(cwSurvexBlockData* block);
    QString generateUniqueStationName(QString oldStationName, cwSurvexBlockData* caveSurvexBlock) const;


};

inline QList<cwSurvexBlockData*> cwSurvexGlobalData::blocks() const {
    return RootBlocks;
}


#endif // CWSURVEXGLOBALDATA_H
