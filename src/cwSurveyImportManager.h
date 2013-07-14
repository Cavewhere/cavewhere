/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYIMPORTMANAGER_H
#define CWSURVEYIMPORTMANAGER_H

//Qt includes
#include <QObject>
#include <QUndoStack>
#include <QStringList>

//Our includes
class cwCavingRegion;
class cwCompassImporter;

/**
    This class allows qml to import data
  */
class cwSurveyImportManager : public QObject
{
    Q_OBJECT
public:
    explicit cwSurveyImportManager(QObject *parent = 0);
    ~cwSurveyImportManager();
    
    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    void setUndoStack(QUndoStack* undoStack);

    Q_INVOKABLE void importSurvex();
    Q_INVOKABLE void importCompassDataFile();

signals:
    
public slots:

private slots:
    void compassImporterFinished();
    void compassMessages(QString message);

private:
    QThread* ImportThread;

    cwCavingRegion* CavingRegion;
    QUndoStack* UndoStack;

    QStringList QueuedCompassFile;
    cwCompassImporter* CompassImporter;


};

inline cwCavingRegion *cwSurveyImportManager::cavingRegion() const
{
    return CavingRegion;
}

inline void cwSurveyImportManager::setUndoStack(QUndoStack *undoStack)
{
    UndoStack = undoStack;
}



#endif // CWSURVEYIMPORTMANAGER_H
