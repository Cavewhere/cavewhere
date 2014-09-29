/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProject.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwAddImageTask.h"
#include "cwCavingRegion.h"
#include "cwTaskProgressDialog.h"
#include "cwImageData.h"
#include "cwRegionSaveTask.h"
#include "cwRegionLoadTask.h"
#include "cwGlobals.h"
#include "cwDebug.h"

//Qt includes
#include <QDir>
#include <QTime>
#include <QDateTime>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>
#include <QUndoStack>
#include <QFileDialog>
#include <QSettings>

/**
  By default, a project is open to a temporary directory
  */
cwProject::cwProject(QObject* parent) :
    QObject(parent),
    TempProject(true),
    Region(new cwCavingRegion(this)),
    UndoStack(new QUndoStack(this))
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
    QString projectFile = QString("%1/CavewhereTmpProject-%2.cw")
            .arg(QDir::tempPath())
            .arg(seedTime.toMSecsSinceEpoch(), 0, 16);
    setFilename(projectFile);
    TempProject = true;

    qDebug() << "Creating temp files:" << ProjectFile;

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
    createDefaultSchema(ProjectDatabase);
}

/**
 * @brief cwProject::addTable
 * @param tableName - Used only for warning messages
 * @param sql - The sql that run on the project database
 */
void cwProject::createTable(const QSqlDatabase& database, QString sql)
{
    //Create the caving region
    QSqlQuery createObjectDataTable(database);

    bool couldPrepare = createObjectDataTable.prepare(sql);
    if(!couldPrepare) {
        qDebug() << "Couldn't prepare table:" << createObjectDataTable.lastError().databaseText() << sql << LOCATION;
    }

    bool couldCreate = createObjectDataTable.exec();
    if(!couldCreate) {
        qDebug() << "Couldn't create table:" << createObjectDataTable.lastError().databaseText() << LOCATION;
    }
}

/**
 * @brief cwProject::insertDocumentation
 * @param files - A pair Contains the filename and the path to the content of the file
 * @param pair.first - The filename that's stored in the project
 * @param pair.second - The path, usually a resaurce, to the content
 */
void cwProject::insertDocumentation(const QSqlDatabase& database, QList<QPair<QString, QString> > filenames) //QString filename, QString pathToContent)
{
    QSqlQuery deleteDocsQuery(database);
    QString deleteQuery = "delete * from FileFormatDocumenation";
    bool success = deleteDocsQuery.exec(deleteQuery);
    if(!success) {
        qDebug() << "Couldn't delete delete contents from FileFormatDocumenation" << LOCATION;
    }

    QSqlQuery insertDocsQuery(database);
    QString query =
            QString("insert into FileFormatDocumenation") +
            QString(" (filename, content) values (?, ?)");

    bool couldPrepare = insertDocsQuery.prepare(query);
    if(!couldPrepare) {
        qDebug() << "Couldn't prepare insertDocsQuery:" << insertDocsQuery.lastError().databaseText() << query << LOCATION;
    }

    //Go through all the documenation files
    QPair<QString, QString> filePair;
    foreach(filePair, filenames) {
        QString filename = filePair.first;
        QString pathToFile = filePair.second;


        QFile file(pathToFile);
        bool couldOpen = file.open(QFile::ReadOnly);
        if(!couldOpen) {
            qDebug() << "Couldn't open documentation file:" << pathToFile << LOCATION;
            continue;
        }

        QByteArray fileContent = file.readAll();

        insertDocsQuery.bindValue(0, filename);
        insertDocsQuery.bindValue(1, fileContent);
        bool success = insertDocsQuery.exec();

        if(!success) {
            qDebug() << "Couldn't execute query:" << insertDocsQuery.lastError().databaseText() << query << LOCATION;
        }
    }
}

/**
  \brief Saves the project
  */
void cwProject::save() {
    if(isTemporaryProject()) {
        saveAs();
    } else {
        privateSave();
    }
}


/**
  Save the project, writes all files to the project
  */
void cwProject::privateSave() {
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
 * @brief cwProject::saveAs
 *
    Saves the project as.  This will copy the current project to a different location, leaving
    the original. If the project is a temperary project, the temperary project will be removed
    from disk.
 */
void cwProject::saveAs()
{
    QString filename = QFileDialog::getSaveFileName(nullptr, "Save Cavewhere Project As", "", "Cavewhere Project (*.cw)");
    filename = cwGlobals::addExtension(filename, "cw");
    saveAs(filename);
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
        privateSave();
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
    privateSave();
}

/**
  \brief Creates a new project
  */
void cwProject::newProject() {
    //Creates a temp directory for the project
    createTempProjectFile();

    //Create the caving the caving region that this project mantaines
    Region->clearCaves();

    //Clear undo stack
    UndoStack->clear();
}

/**
  Loads the project, loads all the files to the project
  */
void cwProject::loadFile(QString filename) {

    if(filename.isEmpty()) { return; }

    //Load the region task
    cwRegionLoadTask* loadTask = new cwRegionLoadTask();
    connect(loadTask, SIGNAL(finishedLoading(cwCavingRegion*)), SLOT(updateRegionData(cwCavingRegion*)));
    loadTask->setThread(LoadSaveThread);

    //Set the data for the project
    loadTask->setDatabaseFilename(filename);

    //Start the save thread
    loadTask->start();

}

/**
  Update the project with new region data

  This should only be called by cwRegionLoadTask
  */
void cwProject::updateRegionData(cwCavingRegion* region) {
    TempProject = false;

    cwRegionLoadTask* loadTask = qobject_cast<cwRegionLoadTask*>(sender());

    //Copy the data from the loaded region
    *Region = *region;

    //Update the project filename
    setFilename(loadTask->databaseFilename());
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
    if(receiver == nullptr )  { return; }

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

    cwTaskProgressDialog* progressDialog = new cwTaskProgressDialog();
    progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    progressDialog->setTask(addImageTask);
    progressDialog->show();
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
        qDebug() << "Couldn't create Insert Images query: " << query.lastError() << LOCATION;
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
 * @brief cwProject::updateImage
 * @param database - The database where the image is going to be inserted into
 * @param imageData - The data that going to update the image
 * @param id - The id of the image that needs to be updated
 * @return True if image was update successfully and false, if unsuccessful
 */
bool cwProject::updateImage(const QSqlDatabase &database, const cwImageData &imageData, int id)
{
    QString SQL("UPDATE Images SET type=?, width=?, height=?, dotsPerMeter=?, imageData=? where id=?");

    QSqlQuery query(database);
    bool successful = query.prepare(SQL);

    if(!successful) {
        qDebug() << "Couldn't create Insert Images query: " << query.lastError() << LOCATION;
        return false;
    }

    query.bindValue(0, imageData.format());
    query.bindValue(1, imageData.size().width());
    query.bindValue(2, imageData.size().height());
    query.bindValue(3, imageData.dotsPerMeter());
    query.bindValue(4, imageData.data());
    query.bindValue(5, id);
    return query.exec();
}

/**
 * @brief cwProject::removeImage
 * @param database - The database connection
 * @param image - The Image that going to be removed
 * @return True if the image could be removed, and false if it couldn't be removed
 */
bool cwProject::removeImage(const QSqlDatabase &database, cwImage image)
{
    //Create the delete SQL statement
    QString SQL("DELETE FROM Images WHERE");
    SQL += QString(" id == %1").arg(image.original());
    SQL += QString(" OR ");
    SQL += QString(" id == %1").arg(image.icon());
    foreach(int mipmapId, image.mipmaps()) {
        SQL += QString(" OR ");
        SQL += QString(" id == %1").arg(mipmapId);
    }

    QSqlQuery query(database);
    bool successful = query.prepare(SQL);

    if(!successful) {
        qDebug() << "Couldn't delete images: " << query.lastError();
        return false;
    }

    query.exec();

    return true;
}

/**
 * @brief cwProject::createDefaultSchema
 * @param database
 */
void cwProject::createDefaultSchema(const QSqlDatabase &database)
{

    //Create the database with full vacuum so we don't use up tons of space
    QSqlQuery vacuumQuery(database);
    QString query = QString("PRAGMA auto_vacuum = 1");
    vacuumQuery.exec(query);

    //Create the caving region
    QString objectDataQuery =
            QString("CREATE TABLE IF NOT EXISTS ObjectData (") +
            QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
            QString("protoBuffer BLOB") + //Last index
            QString(")");

    //Create ObjectData
    createTable(database, objectDataQuery);

    QString documentationTableQuery =
            QString("CREATE TABLE IF NOT EXISTS FileFormatDocumenation (") +
            QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
            QString("filename STRING,") +
            QString("content STRING)");
    createTable(database, documentationTableQuery);

    QList< QPair< QString, QString> > docsFiles;
    docsFiles.append(QPair<QString, QString>("README.txt", ":/docs/FileFormatDocumentation.txt"));
    docsFiles.append(QPair<QString, QString>("qt.pro", ":/src/qt.proto"));
    docsFiles.append(QPair<QString, QString>("cavewhere.pro", ":/src/qt.proto"));
    insertDocumentation(database, docsFiles);

    //FIXME: Remove once Proto buffer fixes the file format
    //Create the caving region
    query =
            QString("CREATE TABLE IF NOT EXISTS CavingRegion (") +
            QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
            QString("qCompress_XML BLOB") + //Last index
            QString(")");
    createTable(database, query);

    //Create the caving region
    QString imageTableQuery =
            QString("CREATE TABLE IF NOT EXISTS Images (") +
            QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
            QString("type STRING,") + //Type of image
            QString("shouldDelete BOOL,") + //If the image should be delete
            QString("width INTEGER,") + //The width of the image
            QString("height INTEGER,") + //The height of the image
            QString("dotsPerMeter INTEGER,") + //The resolution of the image
            QString("imageData BLOB)"); //The blob that stores the image data
    createTable(database, imageTableQuery);
}

/**
 * @brief cwProject::setUndoStack
 * @param undoStack - The undo stack for the project
 *
 * The undo stack will be cleared when the use creates a new project
 */
void cwProject::setUndoStack(QUndoStack *undoStack) {
    if(UndoStack != undoStack) {
        UndoStack = undoStack;
        emit undoStackChanged();
    }
}

/**
 * @brief cwProject::load
 *
 * This creates a file widget that asks the user to load a file from disk
 */
void cwProject::load()
{
    QString filename = cwGlobals::openFile("Load Cavewhere Project", "Cavewhere Project (*.cw)", QString("OpenProject"));
    loadFile(filename);
}


void cwProject::setScrapManager(cwScrapManager *manager)
{
    Q_UNUSED(manager)
}
