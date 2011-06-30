#ifndef CWXMLPROJECT_H
#define CWXMLPROJECT_H

//Our includes
#include "cwTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"

//Qt includes
#include <QSqlDatabase>

/**
  This class saves and load a cavewhere project using xml and sqlite

  The file format is create
  */
class cwProject :  public QObject{
Q_OBJECT
public:
//    class ImageRow {
//    public:
//        QString type;
//        QByteArray ImageData;
//    };

    cwProject(QObject* parent);

    //! The project owns the region
    cwCavingRegion* cavingRegion() const;

    void save();
    void load();

//    QList<int> addImages(QList<cwProject::ImageRow> images);

    QString projectPath();


private:
    bool TempProject;
    QString ProjectFile;
    QSqlDatabase ProjectDatabase;

    cwCavingRegion* Region;
    void createTempProjectFile();

    void connectRegion();
    void connectCave(cwCave* cave);

    void createDefaultSchema();



private slots:

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
inline QString cwProject::projectPath() {
    return ProjectFile;
}

#endif // CWXMLPROJECT_H
