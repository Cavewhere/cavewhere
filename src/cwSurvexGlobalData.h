#ifndef CWSURVEXGLOBALDATA_H
#define CWSURVEXGLOBALDATA_H

//Our includes
class cwSurvexBlockData;
class cwCave;
class cwTrip;
class cwSurveyChunk;
#include "cwUndoer.h"

//Qt includes
#include <QObject>
#include <QList>
#include <QStringList>

class cwSurvexGlobalData : public QObject
{
    friend class cwSurvexImporter;

public:
    cwSurvexGlobalData(QObject* object);

    QList<cwSurvexBlockData*> blocks() const;

    QList<cwCave*> caves();
    QStringList erros();

private:
    QList<cwSurvexBlockData*> RootBlocks;

    QStringList ImportErrors;

    void setBlocks(QList<cwSurvexBlockData*> blocks);

    void cavesHelper(QList<cwCave*>* caves, cwSurvexBlockData* currentBlock, cwCave* currentCave, cwTrip* trip);

    void fixEquatedStationNames(cwSurveyChunk* chunk, cwSurvexBlockData* currentBlock);

};

inline QList<cwSurvexBlockData*> cwSurvexGlobalData::blocks() const {
    return RootBlocks;
}


#endif // CWSURVEXGLOBALDATA_H
