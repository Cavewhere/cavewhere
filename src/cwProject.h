#ifndef CWXMLPROJECT_H
#define CWXMLPROJECT_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"
#include "cwImageData.h"
class cwCave;
class cwCavingRegion;
class cwAddImageTask;
class cwTrip;

//Qt includes
#include <QSqlDatabase>
#include <QDir>
#include <QThread>
#include <QMap>
#include <QHash>
class QUndoStack;

/**
  This class saves and load a cavewhere project using xml and sqlite

  The file format is create
  */
class cwProject :  public QObject{
Q_OBJECT
    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack WRITE setUndoStack NOTIFY undoStackChanged)

public:
    cwProject(QObject* parent = NULL);
    ~cwProject();

    //! The project owns the region
    cwCavingRegion* cavingRegion() const;

    QUndoStack* undoStack() const;
    void setUndoStack(QUndoStack* undoStack);

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    Q_INVOKABLE void saveAs(QString newFilename);

    Q_INVOKABLE void newProject();

    QString filename() const;

    void addImages(QStringList noteImagePath, QObject* reciever, const char* slot);

    static int addImage(const QSqlDatabase& database, const cwImageData& imageData);
    static bool removeImage(const QSqlDatabase& database, cwImage image);

    bool isTemporaryProject() const;

signals:
    void filenameChanged(QString newFilename);
    void undoStackChanged();

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
    QThread* LoadSaveThread;

    //The undo stack
    QUndoStack* UndoStack;

    void createTempProjectFile();
    void createDefaultSchema();

    void setFilename(QString newFilename);

     void privateSave();
private slots:
    void updateRegionData(cwCavingRegion* region);

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
