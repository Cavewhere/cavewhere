/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTREEIMPORTDATA_H
#define CWTREEIMPORTDATA_H

//Our includes
#include "cwTreeImportDataNode.h"
class cwCave;
class cwTrip;
class cwSurveyChunk;
#include "cwStation.h"
#include "cwUndoer.h"

//Qt includes
#include <QObject>
#include <QList>
#include <QStringList>

class cwTreeImportData : public QObject
{
public:
    cwTreeImportData(QObject* object);

    QList<cwTreeImportDataNode*> blocks() const;
    void setBlocks(QList<cwTreeImportDataNode*> blocks);

    virtual QList<cwCave*> caves() = 0;
    QStringList erros();

private:
    QList<cwTreeImportDataNode*> RootBlocks;
    QStringList ImportErrors;
};

inline QList<cwTreeImportDataNode*> cwTreeImportData::blocks() const {
    return RootBlocks;
}


#endif // CWTREEIMPORTDATA_H
