#ifndef CWXMLPROJECT_H
#define CWXMLPROJECT_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"
class cwCave;
class cwCavingRegion;
class cwAddImageTask;
class cwTrip;

//Qt includes
//#include <QSqlDatabase>
#include <QDir>
#include <QThread>
#include <QMap>

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

    void addNoteImages(cwTrip* trip, QStringList noteImagePath);

signals:
    void noteImagesAdded(QList<cwImage> images);

private:

    //Static strings
    static const QString CavesDir;
    static const QString TripsDir;
    static const QString NotesDir;

    //If this is a temp project directory on not
    bool TempProject;

    //The project directory
    QDir ProjectDir;

    //Links a path to cwCave*
    QMap<cwCave*, QDir> CaveLookup; //The pointer val
    QMap<cwTrip*, QDir> TripLookup;

    //The region that this project looks after
    cwCavingRegion* Region;

    //For loading images from the disk into this project
    QThread* AddImageThread;

    void createTempProjectFile();

    void connectRegion();
    void connectCave(cwCave* cave);

    //    void createDefaultSchema();

    void addTripDirectories(cwCave* parentCave, int beginTrip, int endTrip);

    void createNewCaveDirectory(cwCave* cave);
    void createNewTripDirectory(cwCave* parentCave, cwTrip* trip);

    QString removeEvilCharacters(QString filename);
    QString uniqueDirectory(QDir baseDirectory, QString subDirectory);

private slots:

    void addCaveDirectories(int beginCave, int endCave);
    void addTripDirectories(int beginTrip, int endTrip);



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
    return ProjectDir.absolutePath();
}

#endif // CWXMLPROJECT_H
