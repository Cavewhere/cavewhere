#ifndef CWWALLSGLOBALDATA_H
#define CWWALLSGLOBALDATA_H

//Our includes
class cwWallsBlockData;
class cwCave;
class cwTrip;
#include "cwUndoer.h"

//Qt includes
#include <QObject>
#include <QList>
#include <QStringList>

class cwWallsGlobalData : public QObject
{
    friend class cwWallsImporter;

public:
    cwWallsGlobalData(QObject* object);

    QList<cwWallsBlockData*> blocks() const;

    QList<cwCave*> caves();
    QStringList erros();

private:
    QList<cwWallsBlockData*> RootBlocks;

    QStringList ImportErrors;

    void setBlocks(QList<cwWallsBlockData*> blocks);

    void cavesHelper(QList<cwCave*>* caves, cwWallsBlockData* currentBlock, cwCave* currentCave, cwTrip* trip);
};

inline QList<cwWallsBlockData*> cwWallsGlobalData::blocks() const {
    return RootBlocks;
}


#endif // CWWALLSGLOBALDATA_H
