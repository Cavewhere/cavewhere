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
#include <QPointer>
#include <QList>
#include <QUrl>

//Our includes
class cwCavingRegion;
class cwCompassImporter;

/**
    This class allows qml to import data
  */
class cwSurveyImportManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack WRITE setUndoStack NOTIFY undoStackChanged)

public:
    explicit cwSurveyImportManager(QObject *parent = 0);
    ~cwSurveyImportManager();
    
    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    QUndoStack* undoStack() const;
    void setUndoStack(QUndoStack* undoStack);

    Q_INVOKABLE void importSurvex();
    Q_INVOKABLE void importCompassDataFile(QList<QUrl> filenames);

signals:
    void cavingRegionChanged();
    void undoStackChanged();
    
public slots:

private slots:
    void compassImporterFinished();
    void compassMessages(QString message);

private:
    QThread* ImportThread;

    QPointer<cwCavingRegion> CavingRegion;
    QPointer<QUndoStack> UndoStack;

    QStringList QueuedCompassFile;
    cwCompassImporter* CompassImporter;

    QStringList urlsToStringList(QList<QUrl> urls);
};




#endif // CWSURVEYIMPORTMANAGER_H
