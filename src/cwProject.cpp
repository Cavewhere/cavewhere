//Our includes
#include "cwProject.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwAddImageTask.h"
#include "cwCavingRegion.h"
//#include "cwTaskProgressDialog.h"
#include "cwImageData.h"
#include "cwRegionSaveTask.h"
#include "cwRegionLoadTask.h"

//Qt includes
#include <QDir>
#include <QTime>
#include <QDateTime>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>

/**
  By default, a project is open to a temporary directory
  */
cwProject::cwProject(QObject* parent) :
    QObject(parent),
    TempProject(true),
    Region(new cwCavingRegion(this))
{
    newProject();

    //Create a new thread
    LoadSaveThread = new QThread(this);
    LoadSaveThread->start();
}

cwProject::~cwProject()
{
    LoadSaveThread->quit();
    LoadSaveThread->wait();
}

/**
  Creates a new tempDirectoryPath for the temp project
  */
void cwProject::createTempProjectFile() {

    if(isTemporaryProject()) {
        //Remove the old temp project file
        if(QFileInfo(filename()).exists()) {
            QFile::remove(filename());
        }
    }

    QDateTime seedTime = QDateTime::currentDateTime();

    //Create the with a hex number
    ProjectFile = QString("%1/CavewhereTmpProject-%2.cw")
            .arg(QDir::tempPath())
            .arg(seedTime.toMSecsSinceEpoch(), 0, 16);
    TempProject = true;


    //Create and open a new database connection
    ProjectDatabase = QSqlDatabase::addDatabase("QSQLITE", "ProjectConnection");
    ProjectDatabase.setDatabaseName(ProjectFile);
    bool couldOpen = ProjectDatabase.open();
    if(!couldOpen) {
        qDebug() << "Couldn't open temp project file: " << ProjectFile;
        return;
    }

    //Create default schema
    createDefaultSchema();
}

/**
  \brief This creates the default empty schema for the project

  The schema simple,
  Tables:
  1. CavingRegion
  2. Images

  Columns CavingRegion:
  id | gunzipCompressedXML

  Columns Images:
  id | type | shouldDelete | data

  */
void cwProject::createDefaultSchema() {

    //Create the caving region
    QSqlQuery createCavingRegionTable(ProjectDatabase);
    QString query =
            QString("CREATE TABLE IF NOT EXISTS CavingRegion (") +
            QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
            QString("qCompress_XML BLOB") + //Last index
            QString(")");

    bool couldPrepare = createCavingRegionTable.prepare(query);
    if(!couldPrepare) {
        qDebug() << "Couldn't prepare table Caving Region:" << createCavingRegionTable.lastError().databaseText() << query;
    }

    bool couldCreate = createCavingRegionTable.exec();
    if(!couldCreate) {
        qDebug() << "Couldn't create table Caving Region: " << createCavingRegionTable.lastError().databaseText();
    }

    //Create the caving region
    QSqlQuery createImagesTable(ProjectDatabase);
    query =
            QString("CREATE TABLE IF NOT EXISTS Images (") +
            QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
            QString("type STRING,") + //Type of image
            QString("shouldDelete BOOL,") + //If the image should be delete
            QString("width INTEGER,") + //The width of the image
            QString("height INTEGER,") + //The height of the image
            QString("dotsPerMeter INTEGER,") + //The resolution of the image
            QString("imageData BLOB)"); //The blob that stores the image data

    couldPrepare = createImagesTable.prepare(query);
    if(!couldPrepare) {
        qDebug() << "Couldn't prepare table images:" << createImagesTable.lastError().databaseText() << query;
    }

    couldCreate = createImagesTable.exec();
    if(!couldCreate) {
        qDebug() << "Couldn't create table Images: " << createImagesTable.lastError().databaseText();
    }
}

/**
  Save the project, writes all files to the project
  */
void cwProject::save() {
    cwRegionSaveTask* saveTask = new cwRegionSaveTask();
    connect(saveTask, SIGNAL(finished()), saveTask, SLOT(deleteLater()));
    connect(saveTask, SIGNAL(stopped()), saveTask, SLOT(deleteLater()));
    saveTask->setThread(LoadSaveThread);

    //Set the data for the project
    qDebug() << "Saving project to:" << ProjectFile;
    saveTask->setCavingRegion(*Region);
    saveTask->setDatabaseFilename(ProjectFile);

    //Start the save thread
    saveTask->start();
}

/**
  Saves the project as a new file

    Saves the project as.  This will copy the current project to a different location, leaving
    the original. If the project is a temperary project, the temperary project will be removed
    from disk.
  */
void cwProject::saveAs(QString newFilename){
    //Just save it the user is overwritting it
    if(newFilename == filename()) {
        save();
        return;
    }

    //Try to remove the existing file
    if(QFileInfo(newFilename).exists()) {
        bool couldRemove = QFile::remove(newFilename);
        if(!couldRemove) {
            qDebug() << "Couldn't remove " << newFilename;
            return;
        }
    }

    //Copy the old file to the new location
    bool couldCopy = QFile::copy(filename(), newFilename);
    if(!couldCopy) {
        qDebug() << "Couldn't copy " << filename() << "to" << newFilename;
        return;
    }

    if(isTemporaryProject()) {
        QFile::remove(filename());
    }

    //Update the project filename
    setFilename(newFilename);
    TempProject = false;

    //Save the current data
    save();
}

/**
  \brief Creates a new project
  */
void cwProject::newProject() {
    //Creates a temp directory for the project
    createTempProjectFile();

    //Create the caving the caving region that this project mantaines
    Region->clearCaves();
}

/**
  Loads the project, loads all the files to the project
  */
void cwProject::load(QString filename) {

    //Load the region task
    cwRegionLoadTask* loadTask = new cwRegionLoadTask();
    connect(loadTask, SIGNAL(finishedLoading(cwCavingRegion*)), SLOT(updateRegionData(cwCavingRegion*)));
    loadTask->setThread(LoadSaveThread);

    //Set the data for the project
    qDebug() << "Loading project to:" << filename;
    loadTask->setDatabaseFilename(filename);

    //Start the save thread
    loadTask->start();

}

/**
  Update the project with new region data

  This should only be called by cwRegionLoadTask
  */
void cwProject::updateRegionData(cwCavingRegion* region) {
    cwRegionLoadTask* loadTask = qobject_cast<cwRegionLoadTask*>(sender());

    //Copy the data from the loaded region
    *Region = *region;

    //Update the project filename
    setFilename(loadTask->databaseFilename());
    TempProject = false;
}

/**
  \brief Sets the current project file

  \param newFilename - The new filename that this project will be moved to.
  */
void cwProject::setFilename(QString newFilename) {
    if(newFilename != filename()) {
        ProjectFile = newFilename;
        emit filenameChanged(ProjectFile);
    }
}

/**
  This will add images to the database

  \param noteImagePath - A list of all the image paths that'll be added to the project
  \param receiver - The reciever of the addedImages signal
  \param slot - The slot that'll handle the addImages signal

  This will also popup a dialog when the images are being loaded
  */
void cwProject::addImages(QStringList noteImagePath, QObject* receiver, const char* slot) {
    if(receiver == NULL )  { return; }

    //Create a new image task
    cwAddImageTask* addImageTask = new cwAddImageTask();
    connect(addImageTask, SIGNAL(addedImages(QList<cwImage>)), receiver, slot);
    connect(addImageTask, SIGNAL(finished()), addImageTask, SLOT(deleteLater()));
    connect(addImageTask, SIGNAL(stopped()), addImageTask, SLOT(deleteLater()));
    addImageTask->setThread(LoadSaveThread);

    //Set the project path
    addImageTask->setDatabaseFilename(filename());

    //Set all the noteImagePath
    addImageTask->setNewImagesPath(noteImagePath);

    //Run the addImageTask, in an asyncus way
    addImageTask->start();

//    cwTaskProgressDialog* progressDialog = new cwTaskProgressDialog();
//    progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
//    progressDialog->setTask(addImageTask);
//    progressDialog->show();
}

/**
  \brief Adds an image to the project file

  This static function takes a database and adds the imageData to the database

  This returns the id of the image in the database
  */
int cwProject::addImage(const QSqlDatabase& database, const cwImageData& imageData) {
    QString SQL = "INSERT INTO Images (type, shouldDelete, width, height, dotsPerMeter, imageData) "
            "VALUES (?, ?, ?, ?, ?, ?)";

    QSqlQuery query(database);
    bool successful = query.prepare(SQL);

    if(!successful) {
        qDebug() << "Couldn't create Insert Images query: " << query.lastError();
        return -1;
    }

    query.bindValue(0, imageData.format());
    query.bindValue(1, false);
    query.bindValue(2, imageData.size().width());
    query.bindValue(3, imageData.size().height());
    query.bindValue(4, imageData.dotsPerMeter());
    query.bindValue(5, imageData.data());
    query.exec();

    //Get the id of the last inserted id
    return query.lastInsertId().toInt();
}

/**
  Adds new directories back into the cwXMLProject
  */
//void cwProject::addCaveDirectories(int beginCave, int endCave) {
//    for(int i = beginCave; i <= endCave; i++) {
//        cwCave* cave = Region->cave(i);

//        //Connect the cave
//        connectCave(cave);

//        if(cave != NULL) {
//            //See if the cave already exist
//            if(CaveLookup.contains(cave)) {
//                //Directory already exists
//                return;
//            } else {
//                //Create a new directory
//                createNewCaveDirectory(cave);
//            }
//        }

//        //Add trip directories for existing trips
//        if(cave->hasTrips()) {
//            addTripDirectories(cave, 0, cave->tripCount() - 1);
//        }
//    }
//}

/**
  \brief This create a new directory for the cave

  This makes sure that the cave's name is politically correct with the filesystem.
  All illegal charactors must be discared.

  This will uses the cave's name. To try to create the directory
  */
//void cwProject::createNewCaveDirectory(cwCave* cave) {
//    //Where the new cave will be added to
//    QDir baseCavesDirectory = ProjectDir.absolutePath() + "/" + CavesDir;

//    //Get the unique directoryName
//    QString directoryName = uniqueFile(baseCavesDirectory, cave->name());

//    //Create this directory
//    bool couldCreate = baseCavesDirectory.mkpath(directoryName);

//    if(!couldCreate) {
//        qDebug() << "Couldn't create cave directory: " << baseCavesDirectory.absoluteFilePath(directoryName);
//        return;
//    }

//    //Add the directory to the lookup
//    QDir caveDir(baseCavesDirectory.absoluteFilePath(directoryName));
//    CaveLookup.insert(cave, caveDir);
//}

///**
//  \brief Adds trip directories for the project
//  */
//void cwProject::addTripDirectories(int beginTrip, int endTrip) {
//    //Sender is always a cave!!!
//    cwCave* parentCave = static_cast<cwCave*>(sender());

//    addTripDirectories(parentCave, beginTrip, endTrip);
//}

///**
//  \brief Adds trip directories for the project

//  \param parentCave - The parent cave
//  \param beginTrip - The first trip index in the cave
//  \param endTrip - The last trip index in the cave
//  */
// void cwProject::addTripDirectories(cwCave* parentCave, int beginTrip, int endTrip) {
//     for(int i = beginTrip; i <= endTrip; i++) {
//         cwTrip* trip = parentCave->trip(i);

//         if(!TripLookup.contains(trip)) {
//             createNewTripDirectory(parentCave, trip);
//         }
//     }
// }

///**
//  \brief Adds a new trip directory for the project
//  */
//void cwProject::createNewTripDirectory(cwCave* parentCave, cwTrip* trip) {

//    if(parentCave == NULL || !CaveLookup.contains(parentCave)) {
//        qDebug() << "Parent Cave is invalid:" << parentCave;
//        return;
//    }

//    //Find the parentCave directory
//    QDir parentCaveDirectory = CaveLookup.value(parentCave, QDir());

//    if(!parentCaveDirectory.exists()) {
//        //Parent cave directory doesn't exist!
//        qDebug() << "Parent cave directory, " << parentCaveDirectory << "doesn't exist!!!";
//    }

//    //Try to make the trips directory
//    parentCaveDirectory.mkdir(TripsDir);

//    //Trip to make the trip directory
//    QDir baseTripsDirectory(parentCaveDirectory.absolutePath() + "/" + TripsDir);
//    QString tripDirectory = uniqueFile(baseTripsDirectory, trip->name());

//    //Create the trip directory
//    bool couldCreate = baseTripsDirectory.mkdir(tripDirectory);

//    if(!couldCreate) {
//        qDebug() << "Couldn't create trips directory" << baseTripsDirectory.absoluteFilePath(tripDirectory);
//    }

//    QDir tripDir(baseTripsDirectory.absoluteFilePath(tripDirectory));
//    TripLookup.insert(trip, tripDir);
//}

///**
//  \brief This removes all the evil directory charactors that windows doesn't like
//  */
//QString cwProject::removeEvilCharacters(QString filename) {
//    filename.remove('<');
//    filename.remove('>');
//    filename.remove(':');
//    filename.remove('"');
//    filename.remove('/');
//    filename.remove('\\');
//    filename.remove('|');
//    filename.remove('?');
//    filename.remove('*');
//    return filename;
//}

///**
//  \brief This makes sure that baseDirectory + file is a unique file

//  If it's a unquie directory then a unmodified file is return, else
//  file is incremented.

//  This also tries to remove all evil characters.
//  */
//QString cwProject::uniqueFile(QDir baseDirectory, QString file) {
//    //Remove evil windows characters
//    file = removeEvilCharacters(file);

//    //Check to see if the name already exists
//    QString checkName = file;
//    int counter = 1;
//    while(baseDirectory.exists(checkName)) {
//        checkName = QString("%1 %2").arg(file).arg(counter);
//        counter++;
//    }

//    return checkName;
//}
