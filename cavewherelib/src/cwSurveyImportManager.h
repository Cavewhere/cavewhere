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
#include <QQmlEngine>
#include <QFuture>

//Our includes
class cwCavingRegion;
class cwWallsImporter;
class cwCSVImporterTask;
class cwErrorListModel;
#include "cwGlobals.h"
#include "cwCompassImporter.h"
#include "cwFutureManagerToken.h"

/**
    This class allows qml to import data
  */
class CAVEWHERE_LIB_EXPORT cwSurveyImportManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyImportManager)

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack WRITE setUndoStack NOTIFY undoStackChanged)
    Q_PROPERTY(QFont messageListFont MEMBER MessageListFont CONSTANT)
    Q_PROPERTY(cwErrorListModel* errorModel READ errorModel WRITE setErrorModel NOTIFY errorModelChanged)

public:
    explicit cwSurveyImportManager(QObject *parent = 0);
    ~cwSurveyImportManager();
    
    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    QUndoStack* undoStack() const;
    void setUndoStack(QUndoStack* undoStack);

    cwErrorListModel* errorModel() const;
    void setErrorModel(cwErrorListModel* errorModel);

    void setFutureManagerToken(cwFutureManagerToken token);

    Q_INVOKABLE void importSurvex();
    Q_INVOKABLE void importWalls();
    Q_INVOKABLE void importWallsSrv();
    Q_INVOKABLE void importCompassDataFile(QList<QUrl> filenames);

    void waitForCompassToFinish();

signals:
    void cavingRegionChanged();
    void undoStackChanged();
    void messageAdded(QString severity, QString message, QString source, int startLine, int startColumn, int endLine, int endColumn);
    void messagesCleared();
    void errorModelChanged();
    
public slots:

private slots:
    void wallsMessages(QString severity, QString message, QString source,
                       int startLine, int startColumn, int endLine, int endColumn);
//    void csvImportedFinished();

private:
    inline static const QString ImportSurvexKey = QLatin1String("LastImportSurvexFile");
    inline static const QString ImportWallsKey = QLatin1String("LastImportWallsFile");

    QPointer<cwCavingRegion> CavingRegion;
    QPointer<QUndoStack> UndoStack;
    QPointer<cwErrorListModel> ErrorModel; //!<

    QList<QFuture<cwCompassImporter::Result>> m_compassFutures;
    cwFutureManagerToken m_futureManagerToken;

    void onCompassFinished(const cwCompassImporter::Result& result);

    QStringList urlsToStringList(QList<QUrl> urls);

    QFont MessageListFont;
};




#endif // CWSURVEYIMPORTMANAGER_H
