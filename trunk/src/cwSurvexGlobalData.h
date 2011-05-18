#ifndef CWSURVEXGLOBALDATA_H
#define CWSURVEXGLOBALDATA_H

//Our includes
class cwSurvexBlockData;
class cwCave;
class cwTrip;
#include "cwUndoer.h"

//Qt includes
#include <QObject>
#include <QList>
#include <QStringList>

class cwSurvexGlobalData : public QObject, public cwUndoer
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
};

inline QList<cwSurvexBlockData*> cwSurvexGlobalData::blocks() const {
    return RootBlocks;
}


#endif // CWSURVEXGLOBALDATA_H
