/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWXMLPROJECT_H
#define CWXMLPROJECT_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"
#include "cwImageData.h"
#include "cwGlobals.h"
#include "cwRegionLoadResult.h"
#include "cwError.h"
#include "cwFutureManagerToken.h"
class cwCave;
class cwCavingRegion;
class cwAddImageTask;
class cwTrip;
class cwScrapManager;
class cwTaskManagerModel;
class cwRegionLoadTask;
class cwRegionSaveTask;
class cwErrorListModel;
class cwFutureManagerModel;

//Qt includes
#include <QSqlDatabase>
#include <QDir>
#include <QMap>
#include <QHash>
#include <QPointer>
#include <QFuture>
#include <QQmlEngine>
class QUndoStack;

/**
  This class saves and load a cavewhere project using xml and sqlite

  The file format is create
  */
class CAVEWHERE_LIB_EXPORT cwProject :  public QObject{
    Q_OBJECT
    QML_NAMED_ELEMENT(Project)

    Q_PROPERTY(QString filename READ filename NOTIFY filenameChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack WRITE setUndoStack NOTIFY undoStackChanged)
    Q_PROPERTY(bool canSaveDirectly READ canSaveDirectly NOTIFY canSaveDirectlyChanged)
    Q_PROPERTY(bool isTemporaryProject READ isTemporaryProject NOTIFY isTemporaryProjectChanged)
    Q_PROPERTY(cwErrorListModel* errorModel READ errorModel CONSTANT)

public:
    cwProject(QObject* parent = nullptr);
    ~cwProject();

    //! The project owns the region
    cwCavingRegion* cavingRegion() const;

    QUndoStack* undoStack() const;
    void setUndoStack(QUndoStack* undoStack);

    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs(QString newFilename);

    Q_INVOKABLE void newProject();

    QString filename() const;

    // void setTaskManager(cwTaskManagerModel* manager);
    // cwTaskManagerModel* taskManager() const;

    cwFutureManagerToken futureManagerToken() const;
    void setFutureManagerToken(cwFutureManagerToken futureManagerToken);

    static void createDefaultSchema(const QSqlDatabase& database);
    static QString createTemporaryFilename();
    static QSqlDatabase createDatabaseConnection(const QString& connectionName, const QString& databasePath);

    Q_INVOKABLE void waitLoadToFinish();
    void waitSaveToFinish();

    Q_INVOKABLE bool isModified() const;

    cwErrorListModel* errorModel() const;

    bool canSaveDirectly() const;
    bool isTemporaryProject() const;

    void addImages(QList<QUrl> noteImagePath, std::function<void (QList<cwImage> images)> func);

    static QString supportedImageFormats();

signals:
    void filenameChanged(QString newFilename);
    void undoStackChanged();
    void canSaveDirectlyChanged();
    void isTemporaryProjectChanged();
    void regionChanged();
    void fileSaved();
    void loaded();

public slots:
     void loadFile(QString filename);

private:

    //If this is a temp project directory on not
    bool TempProject;
    QString ProjectFile;
    QSqlDatabase ProjectDatabase;
    int FileVersion;

    //The region that this project looks after
    cwCavingRegion* Region;

    QFuture<void> LoadFuture;
    QFuture<void> SaveFuture;

    //The undo stack
    QUndoStack* UndoStack;

    //Task manager, for visualizing running tasks
    QPointer<cwTaskManagerModel> TaskManager;
    cwFutureManagerToken FutureToken; //!<

    cwErrorListModel* ErrorModel; //!<

    //For keeping database connection unique
    static QAtomicInt ConnectionCounter;

    void createTempProjectFile();
    void createDefaultSchema();

    static void createTable(const QSqlDatabase& database, QString sql); //Helpers to createDefaultSchema
    static void insertDocumentation(const QSqlDatabase& database, QList<QPair<QString, QString> > filenames); //Helpers to createDefaultSchema

    void setFilename(QString newFilename);

    void privateSave();

    bool saveWillCauseDataLoss() const;
    void setTemporaryProject(bool isTemp);

    void addImageHelper(std::function<void (QList<cwImage>)> outputCallBackFunc,
                        std::function<void (cwAddImageTask*)> setImagesFunc);
};

/**
  \brief Get's the caving region
  */
inline cwCavingRegion* cwProject::cavingRegion() const {
    return Region;
}

inline QUndoStack *cwProject::undoStack() const
{
    return UndoStack;
}

/**
  \brief Returns the open project path

  This should always be valid
  */
inline QString cwProject::filename() const {
    return ProjectFile;
}

/**
  Returns true if the user hasn't save the project and false if they have
  */
inline bool cwProject::isTemporaryProject() const {
    return TempProject;
}

inline cwErrorListModel* cwProject::errorModel() const {
    return ErrorModel;
}

inline bool cwProject::canSaveDirectly() const {
    return !saveWillCauseDataLoss() && !isTemporaryProject();
}




#endif // CWXMLPROJECT_H
