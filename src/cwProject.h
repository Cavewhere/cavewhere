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


    void setFilename(QString newFilename);
    QString filename() const;

    void addImages(QStringList noteImagePath, QObject* reciever, const char* slot);

    static int addImage(const QSqlDatabase& database, const cwImageData& imageData);

    //    static QString uniqueFile(QDir baseDirectory, QString subFile);
signals:
    void filenameChanged(QString newFilename);

public slots:
     void load(QString filename);

private:

    //Static strings
    static const QString CavesDir;
    static const QString TripsDir;
    static const QString NotesDir;

    //If this is a temp project directory on not
    bool TempProject;
    QString ProjectFile;
    QSqlDatabase ProjectDatabase;

    //The project directory
//    QDir ProjectDir;

//    //Links a path to cwCave* and cwTrip*
//    QMap<cwCave*, QDir> CaveLookup; //The pointer val
//    QMap<cwTrip*, QDir> TripLookup;

//    //Looks up a id to a path image pathname
//    int MaxImage;
//    QHash<int, QString> ImageDatabase;

    //The region that this project looks after
    cwCavingRegion* Region;

    //For loading images from the disk into this project
    QThread* LoadSaveThread;

    void createTempProjectFile();
    void createDefaultSchema();

    void connectRegion();
    void connectCave(cwCave* cave);

//    void addTripDirectories(cwCave* parentCave, int beginTrip, int endTrip);

//    void createNewCaveDirectory(cwCave* cave);
//    void createNewTripDirectory(cwCave* parentCave, cwTrip* trip);

//    static QString removeEvilCharacters(QString filename);



private slots:
//    void addCaveDirectories(int beginCave, int endCave);
//    void addTripDirectories(int beginTrip, int endTrip);

    void UpdateRegionData(cwCavingRegion* region);

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

#endif // CWXMLPROJECT_H
