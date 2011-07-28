//Our includes
#include "cwProject.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwAddImageTask.h"
#include "cwCavingRegion.h"
#include "cwTaskProgressDialog.h"
#include "cwImageData.h"

//Qt includes
#include <QDir>
#include <QTime>
#include <QDateTime>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>

//Statics
const QString cwProject::CavesDir = "Caves";
const QString cwProject::TripsDir = "Trips";
const QString cwProject::NotesDir = "Notes";

/**
  By default, a project is open to a temporary directory
  */
cwProject::cwProject(QObject* parent) :
    QObject(parent)
{
    //Creates a temp directory for the project
    createTempProjectFile();

    qDebug() << "DatabaseFile:" << ProjectFile;

    //Create the caving the caving region that this project mantaines
    Region = new cwCavingRegion(this);
    connectRegion();

    //Create a new thread
    AddImageThread = new QThread(this);
    AddImageThread->start();

    //AddImageTask->setThread(AddImageThread);
}

/**
  Creates a new tempDirectoryPath for the temp project
  */
void cwProject::createTempProjectFile() {

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
    qDebug() << "Database is valid: " << ProjectDatabase.isValid();

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
            QString("gunzipCompressedXML BLOB") + //Last index
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


//void cwProject::setCavingRegion(cwCavingRegion* region) {
//    if(region == Region) { return; }

//    //Disconnect all signals and slots
//    if(region != NULL) {
//        disconnect(region, NULL, this, NULL);
//    }

//    region = Region;

//    //Connect the caves to this project
//    connectRegion();
//}

/**
  \brief Sets the project directory

  If the project is current saved to a tempory directory, this will simply move that directory
  to the directory path and rename it.

  Else, this is treated as a save as, all project files are copied and the project is resaved

  The project will also look for new files.
  */
//void cwProject::setProjectDirectory(QString directoryPath) {

//    //Clear the undo / redo stack

//    //Clear the Region tree


//}

/**
  Save the project, writes all files to the project
  */
void cwProject::save() {

}

/**
  Loads the project, loads all the files to the project
  */
void cwProject::load() {

    //Clear the undo / redo stack

    //Clear the region

}

/**
  \brief This will move the file project from it's current path to a new file

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
    addImageTask->setThread(AddImageThread);

    //Set the project path
    addImageTask->setProjectPath(filename());

    //Set all the noteImagePath
    addImageTask->setImagesPath(noteImagePath);

    //Run the addImageTask, in an asyncus way
    addImageTask->start();

    cwTaskProgressDialog* progressDialog = new cwTaskProgressDialog();
    progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    progressDialog->setTask(addImageTask);
    progressDialog->show();
}

///**
//  Creates a new tempDirectoryPath for the temp project
//  */
//void cwProject::createTempProjectFile() {

//    QDateTime seedTime = QDateTime::currentDateTime();

//    //Create the with a hex number
//    QString projectDir = QString("%1/CavewhereTmpProject-%2")
//            .arg(QDir::tempPath())
//            .arg(seedTime.toMSecsSinceEpoch(), 0, 16);
//    TempProject = true;

//    //Create the directory structure
//    ProjectDir = QDir(projectDir);
//    ProjectDir.mkdir("./"); //Create the directory path
//    ProjectDir.mkdir(CavesDir);

//    qDebug() << "Making project file:" << ProjectDir.absolutePath();
//}

/**
  \brief Connects the cave to this project
  */
void cwProject::connectRegion() {
    //Connects all the signals from the region so the directory
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(addCaveDirectories(int,int)));

    for(int i = 0; i < Region->caveCount(); i++) {
        cwCave* cave = Region->cave(i);
        connectCave(cave);
    }
}

/**
  \brief Connects the cave to this project

  This causes the project to update dynamically.
  */
void cwProject::connectCave(cwCave* cave) {
    //Connect if a trip is added
    connect(cave, SIGNAL(insertedTrips(int,int)), SLOT(addTripDirectories(int,int)));
}

/**
  \brief Adds an image to the project file

  This static function takes a database and adds the imageData to the database
  */
int cwProject::addImage(const QSqlDatabase& database, const cwImageData& imageData) {
    QString SQL = "INSERT INTO Images (type, shouldDelete, width, height, imageData) "
            "VALUES (?, ?, ?, ?, ?)";

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
    query.bindValue(4, imageData.data());
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
