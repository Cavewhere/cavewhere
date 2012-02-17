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

/**
  This class saves and load a cavewhere project using xml and sqlite

  The file format is create
  */
class cwProject :  public QObject{
Q_OBJECT
    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)

public:
    cwProject(QObject* parent = NULL);
    ~cwProject();

    //! The project owns the region
    cwCavingRegion* cavingRegion() const;

    void save();
    void saveAs(QString newFilename);

    void newProject();

    QString filename() const;

    void addImages(QStringList noteImagePath, QObject* reciever, const char* slot);

    static int addImage(const QSqlDatabase& database, const cwImageData& imageData);

    bool isTemporaryProject() const;

signals:
    void filenameChanged(QString newFilename);

public slots:
     void load(QString filename);

private:
    //If this is a temp project directory on not
    bool TempProject;
    QString ProjectFile;
    QSqlDatabase ProjectDatabase;

    //The region that this project looks after
    cwCavingRegion* Region;

    //For loading images from the disk into this project
    QThread* LoadSaveThread;

    void createTempProjectFile();
    void createDefaultSchema();

    void setFilename(QString newFilename);

private slots:
    void updateRegionData(cwCavingRegion* region);

};

/**
  \brief Get's the caving region
  */
inline cwCavingRegion* cwProject::cavingRegion() const {
    return Region;
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
