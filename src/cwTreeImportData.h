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

    QList<cwTreeImportDataNode*> nodes() const;
    void setNodes(QList<cwTreeImportDataNode*> nodes);

    virtual QList<cwCave*> caves() = 0;

private:
    QList<cwTreeImportDataNode*> RootNodes;
};

inline QList<cwTreeImportDataNode*> cwTreeImportData::nodes() const {
    return RootNodes;
}


#endif // CWTREEIMPORTDATA_H
