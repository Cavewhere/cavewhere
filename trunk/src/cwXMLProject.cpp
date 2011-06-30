//Our includes
#include "cwXMLProject.h"

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
    QObject(parent)
{
    //Creates a temp directory for the project
    createTempProjectFile();

    qDebug() << "DatabaseFile:" << ProjectFile;

    //Create the caving the caving region that this project mantaines
    Region = new cwCavingRegion(this);
    connectRegion();
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

/**
  Adds new directories back into the cwXMLProject
  */
//void cwProject::addCaveDirectories(int beginCave, int endCave) {

//    for(int i = beginCave; i <= endCave; i++) {
//        cwCave* cave = Region->cave(i);
//        if(cave != NULL) {


//        }
//    }

//}

/**
  \brief Connects the cave to this project
  */
void cwProject::connectRegion() {
    //Connects all the signals from the region so the directory

    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(addCaveDirectories(int,int)));


}
