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
class cwCave;
class cwCavingRegion;
class cwAddImageTask;
class cwTrip;
class cwScrapManager;
class cwTaskManagerModel;
class cwRegionLoadTask;

//Qt includes
#include <QSqlDatabase>
#include <QDir>
#include <QThread>
#include <QMap>
#include <QHash>
#include <QPointer>
class QUndoStack;

/**
  This class saves and load a cavewhere project using xml and sqlite

  The file format is create
  */
class CAVEWHERE_LIB_EXPORT cwProject :  public QObject{
Q_OBJECT
    Q_PROPERTY(QString filename READ filename NOTIFY filenameChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack WRITE setUndoStack NOTIFY undoStackChanged)
    Q_PROPERTY(bool temporaryProject READ isTemporaryProject NOTIFY temporaryProjectChanged)

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

    void setTaskManager(cwTaskManagerModel* manager);
    cwTaskManagerModel* taskManager() const;

    void addImages(QList<QUrl> noteImagePath, QObject* reciever, const char* slot);

    static int addImage(const QSqlDatabase& database, const cwImageData& imageData);
    static bool updateImage(const QSqlDatabase& database, const cwImageData& imageData, int id);
    static bool removeImage(const QSqlDatabase& database, cwImage image, bool withTransaction = true);

    static void createDefaultSchema(const QSqlDatabase& database);

    bool isTemporaryProject() const;

    void waitToFinish();

signals:
    void filenameChanged(QString newFilename);
    void undoStackChanged();
    void temporaryProjectChanged();
    void regionChanged();

public slots:
     void loadFile(QString filename);

private:

    //If this is a temp project directory on not
    bool TempProject;
    QString ProjectFile;
    QSqlDatabase ProjectDatabase;

    //The region that this project looks after
    cwCavingRegion* Region;

    //For loading images from the disk into this project
    cwRegionLoadTask* LoadTask;
    QThread* LoadSaveThread;

    //The undo stack
    QUndoStack* UndoStack;

    //Task manager, for visualizing running tasks
    QPointer<cwTaskManagerModel> TaskManager;

    void createTempProjectFile();
    void createDefaultSchema();

    static void createTable(const QSqlDatabase& database, QString sql); //Helpers to createDefaultSchema
    static void insertDocumentation(const QSqlDatabase& database, QList<QPair<QString, QString> > filenames); //Helpers to createDefaultSchema

    void setFilename(QString newFilename);

    void privateSave();

    QString convertFromURL(QString fileUrl) const;
private slots:
    void updateRegionData();
    void startDeleteImageTask();
    void deleteImageTask();

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

#endif // CWXMLPROJECT_H
