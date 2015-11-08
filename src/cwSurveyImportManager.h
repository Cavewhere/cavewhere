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
#include <QFont>
#include <QFontDatabase>

//Our includes
class cwCavingRegion;
class cwCompassImporter;
class cwWallsImporter;

/**
    This class allows qml to import data
  */
class cwSurveyImportManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack WRITE setUndoStack NOTIFY undoStackChanged)
    Q_PROPERTY(QFont messageListFont MEMBER MessageListFont)

public:
    explicit cwSurveyImportManager(QObject *parent = 0);
    ~cwSurveyImportManager();
    
    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    QUndoStack* undoStack() const;
    void setUndoStack(QUndoStack* undoStack);

    Q_INVOKABLE void importSurvex();
    Q_INVOKABLE void importCompassDataFile(QList<QUrl> filenames);
    Q_INVOKABLE void importWallsDataFile(QList<QUrl> filenames);

signals:
    void cavingRegionChanged();
    void undoStackChanged();
    void messageAdded(QString severity, QString message, QString source, int startLine, int startColumn, int endLine, int endColumn);
    void messagesCleared();
    
public slots:

private slots:
    void compassImporterFinished();
    void wallsImporterFinished();
    void compassMessages(QString message);
    void wallsMessages(QString severity, QString message, QString source,
                       int startLine, int startColumn, int endLine, int endColumn);

private:
    QThread* ImportThread;

    QPointer<cwCavingRegion> CavingRegion;
    QPointer<QUndoStack> UndoStack;

    QStringList QueuedCompassFile;
    cwCompassImporter* CompassImporter;

    QStringList QueuedWallsFile;
    cwWallsImporter* WallsImporter;

    QStringList urlsToStringList(QList<QUrl> urls);

    QFont MessageListFont;
};




#endif // CWSURVEYIMPORTMANAGER_H
