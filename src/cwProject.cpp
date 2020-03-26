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
#include "cwSQLManager.h"
#include "cwTaskManagerModel.h"
#include "cwAsyncFuture.h"
#include "cwErrorListModel.h"

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
#include <QCoreApplication>
#include <QtConcurrent>
#include <QSharedPointer>

//Async Future
#include <asyncfuture.h>

QAtomicInt cwProject::ConnectionCounter;

/**
  By default, a project is open to a temporary directory
  */
cwProject::cwProject(QObject* parent) :
    QObject(parent),
    TempProject(true),
    FileVersion(cwRegionIOTask::protoVersion()),
    Region(new cwCavingRegion(this)),
    UndoStack(new QUndoStack(this)),
    ErrorModel(new cwErrorListModel(this))
{
    newProject();
}

cwProject::~cwProject()
{
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

    //Create the with a hex number
    QString projectFile = createTemporaryFilename();
    setFilename(projectFile);

    setTemporaryProject(true);

    //Create and open a new database connection
    int nextConnectonName = ConnectionCounter.fetchAndAddAcquire(1);
    ProjectDatabase = QSqlDatabase::addDatabase("QSQLITE", QString("ProjectConnection-%1").arg(nextConnectonName));
    ProjectDatabase.setDatabaseName(ProjectFile);
    bool couldOpen = ProjectDatabase.open();
    if(!couldOpen) {
        ErrorModel->append(cwError(QString("Couldn't open temp project file: %1. Temp directory not writable?").arg(ProjectFile), cwError::Fatal));
        return;
    }

    //Create default schema
    createDefaultSchema();

    emit canSaveDirectlyChanged();
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
    QSqlQuery createObjectDataTable(database);

    //Create the caving region
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
    QSqlQuery hasDocsQuery(database);
    QString hasDocsQuerySQL = "SELECT name FROM sqlite_master WHERE type='table' AND name='FileFormatDocumenation'";
    bool success = hasDocsQuery.exec(hasDocsQuerySQL);
    if(success && hasDocsQuery.next()) {
        Q_ASSERT(hasDocsQuery.value(0).toString() == QString("FileFormatDocumenation"));
        QSqlQuery deleteDocsQuery(database);
        QString deleteQuery = "delete from FileFormatDocumenation";
        bool success = deleteDocsQuery.exec(deleteQuery);
        if(!success) {
            qDebug() << "Couldn't delete contents from FileFormatDocumenation" << deleteDocsQuery.lastError().text() << LOCATION;
        }
    } else {
        if(!success) {
            qDebug() << "Couldn't run query" << hasDocsQuery.lastError().text() << LOCATION;
        }
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
  \brief Saves the project.

  Make sure the project isn't a temporary project. If it is a temporary project,
 call saveAs(filename) instead
  */
void cwProject::save() {
    Q_ASSERT(canSaveDirectly());
    privateSave();
}


/**
  Save the project, writes all files to the project
  */
void cwProject::privateSave() {

    auto region = QSharedPointer<cwCavingRegion>::create(*Region);
    QString filename = this->filename();
    region->moveToThread(nullptr);

    auto future = QtConcurrent::run([region, filename]() {
        cwRegionSaveTask saveTask;
        saveTask.setDatabaseFilename(filename);
        return saveTask.save(region.get());
    });

    if(FutureManager) {
        FutureManager->addJob({future, "Saving"});
    }

    SaveFuture = AsyncFuture::observe(future).subscribe([future, this](){
        auto errors = future.result();
        ErrorModel->append(errors);
        if(!cwError::containsFatal(errors)) {
            emit fileSaved();
        }
    }).future();
}

bool cwProject::saveWillCauseDataLoss() const
{
    return FileVersion > cwRegionIOTask::protoVersion();
}

void cwProject::setTemporaryProject(bool isTemp)
{
    if(TempProject != isTemp) {
        TempProject = isTemp;
        emit isTemporaryProjectChanged();
    }
}

/**
  Saves the project as a new file

    Saves the project as.  This will copy the current project to a different location, leaving
    the original. If the project is a temperary project, the temperary project will be removed
    from disk.
  */
void cwProject::saveAs(QString newFilename){
    newFilename = cwGlobals::addExtension(newFilename, "cw");
    newFilename = cwGlobals::convertFromURL(newFilename);

    if(QFileInfo(newFilename) == QFileInfo(filename()) && saveWillCauseDataLoss()) {
        errorModel()->append(cwError(QString("Can't overwrite %1 because file is newer that the current version of CaveWhere. To solve this, save it somewhere else").arg(filename()), cwError::Fatal));
        return;
    }

    //Just save it the user is overwritting it
    if(QFileInfo(newFilename) == QFileInfo(filename())) {
        privateSave();
        return;
    }

    //Try to remove the existing file
    if(QFileInfo(newFilename).exists()) {
        bool couldRemove = QFile::remove(newFilename);
        if(!couldRemove) {
            errorModel()->append(cwError(QString("Couldn't remove %1").arg(newFilename), cwError::Fatal));
            return;
        }
    }

    //Copy the old file to the new location
    bool couldCopy = QFile::copy(filename(), newFilename);
    if(!couldCopy) {
        errorModel()->append(cwError(QString("Couldn't copy %1 to %2").arg(filename()).arg(newFilename), cwError::Fatal));
        return;
    }

    if(isTemporaryProject()) {
        QFile::remove(filename());
    }

    //Update the project filename
    setFilename(newFilename);
    setTemporaryProject(false);

    //Save the current data
    privateSave();

//    emit canSaveDirectly();
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
 * @brief cwProject::setTaskManager
 * @param manager
 *
 * When adding images to the project, this will allow the user to see the image progress
 */
void cwProject::setTaskManager(cwTaskManagerModel *manager)
{
    TaskManager = manager;
}

/**
 * @brief cwProject::taskManager
 * @return Return's the current taskManager
 */
cwTaskManagerModel *cwProject::taskManager() const
{
    return TaskManager;
}

/**
  Loads the project, loads all the files to the project
  */
void cwProject::loadFile(QString filename) {
    if(filename.isEmpty()) { return; }

    //Only load one file at a time
    LoadFuture.cancel();

    filename = cwGlobals::convertFromURL(filename);

    //Run the load task async
    auto loadFuture = QtConcurrent::run([filename](){
        cwRegionLoadTask loadTask;
        loadTask.setDatabaseFilename(filename);
        return loadTask.load();
    });

    if(FutureManager) {
        FutureManager->addJob({loadFuture, "Loading"});
    }

    auto updateRegion = [this, filename](const cwRegionLoadResult& result) {
        setFilename(result.filename());
        setTemporaryProject(result.isTempFile());
        *Region = *(result.cavingRegion().data());\
        FileVersion = result.fileVersion();
        emit canSaveDirectly();
    };

    LoadFuture = AsyncFuture::observe(loadFuture)
            .subscribe([loadFuture, updateRegion, this]()
    {
        auto result = loadFuture.result();
        if(result.errors().isEmpty()) {
            updateRegion(result);
        } else {
            if(!cwError::containsFatal(result.errors())) {
                //Just warnings, we should be able to load
                updateRegion(result);
            }
            ErrorModel->append(result.errors());
        }
    }).future();
}

/**
  \brief Sets the current project file

  \param newFilename - The new filename that this project will be moved to.
  */
void cwProject::setFilename(QString newFilename) {
    if(newFilename != filename()) {
        ProjectFile = newFilename;
        qDebug() << "Setting filename:" << ProjectFile;
        emit filenameChanged(ProjectFile);
    }
}

/**
  \brief Adds an image to the project file

  This static function takes a database and adds the imageData to the database

  This returns the id of the image in the database
  */
int cwProject::addImage(const QSqlDatabase& database, const cwImageData& imageData) {
    cwSQLManager::Transaction transaction(database);

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
    cwSQLManager::Transaction transaction(database);

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
bool cwProject::removeImage(const QSqlDatabase &database, cwImage image, bool withTransaction)
{
    if(withTransaction) {
        cwSQLManager::instance()->beginTransaction(database);
    }

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

        if(withTransaction) {
            cwSQLManager::instance()->endTransaction(database);
        }

        return false;
    }

    query.exec();

    if(withTransaction) {
        cwSQLManager::instance()->endTransaction(database);
    }

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

    cwSQLManager::Transaction transaction(database);

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

QString cwProject::createTemporaryFilename()
{
    QDateTime seedTime = QDateTime::currentDateTime();
    return QString("%1/CavewhereTmpProject-%2.cw")
                .arg(QDir::tempPath())
                .arg(seedTime.toMSecsSinceEpoch(), 0, 16);
}

/**
 * @brief cwProject::waitToFinish
 *
 * Will cause the project to block until the underlying task is finished. This is useful
 * for unit testing.
 */
void cwProject::waitLoadToFinish()
{
    cwAsyncFuture::waitForFinished(LoadFuture);
}

/**
 * @brief cwProject::waitSaveToFinish
 *
 * Will cause the project to block until the underlying save task is finished. This is
 * useful
 */
void cwProject::waitSaveToFinish()
{
    cwAsyncFuture::waitForFinished(SaveFuture);
}

/**
 * Returns true if the user has modified the file, and false if haven't
 */
bool cwProject::isModified() const
{
    cwRegionSaveTask saveTask;
    QByteArray saveData = saveTask.serializedData(Region);

    if(isTemporaryProject()) {
        return Region->caveCount() > 0;
    }

    cwRegionLoadTask loadTask;
    loadTask.setDatabaseFilename(filename());
    loadTask.setDeleteOldImages(false);
    auto result = loadTask.load();
    loadTask.waitToFinish();

    QByteArray currentData = saveTask.serializedData(result.cavingRegion().data());

    return saveData != currentData;
}

void cwProject::addImages(QList<QUrl> noteImagePath,
                          QObject *context,
                          std::function<void (QList<cwImage>)> func)
{
    if(context == nullptr )  { return; }

    //Create a new image task
    for(QUrl url : noteImagePath) {
        QString path = url.toLocalFile();

        cwAddImageTask addImageTask;
        addImageTask.setDatabaseFilename(filename());
        addImageTask.setContext(context);

        //Set all the noteImagePath
        addImageTask.setNewImagesPath({path}); //noteImagePath);

        auto imagesFuture = addImageTask.images();

        FutureManager->addJob({imagesFuture, "Adding Image"});

        AsyncFuture::observe(imagesFuture)
                .context(context,
                         [imagesFuture, func]()
        {
            func(imagesFuture.results());
        });
    }
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

void cwProject::setFutureManagerModel(cwFutureManagerModel* futureManager) {
    if(FutureManager != futureManager) {
        FutureManager = futureManager;
    }
}

cwFutureManagerModel* cwProject::futureManagerModel() const {
    return FutureManager;
}


