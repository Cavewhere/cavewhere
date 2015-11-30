#ifndef CWTREEDATAIMPORTER_H
#define CWTREEDATAIMPORTER_H

//Qt includes
#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include "cwTask.h"

//Our includes
#include "cwStation.h"
class cwSurvexGlobalData;

class cwTreeDataImporter : public cwTask
{
Q_OBJECT

public:
    cwTreeDataImporter(QObject* parent = nullptr);

    virtual bool hasErrors();
    virtual QStringList errors();

    virtual QString lastImport();

    virtual cwSurvexGlobalData* data();

public slots:
    virtual void setSurvexFile(QString filename);

protected:
    virtual void runTask();
};

#endif // CWTREEDATAIMPORTER_H
