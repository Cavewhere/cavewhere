/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProject.h"
#include "Monad/Monad.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwImageUtils.h"
#include "cwCavingRegion.h"
#include "cwTaskProgressDialog.h"
#include "cwImageData.h"
#include "cwRegionSaveTask.h"
#include "cwRegionLoadTask.h"
#include "cwGlobals.h"
#include "cwDebug.h"
#include "cwSQLManager.h"
#include "cwTaskManagerModel.h"
#include "asyncfuture.h"
#include "cwErrorListModel.h"
#include "cwPDFConverter.h"
#include "cwPDFSettings.h"
#include "cwConcurrent.h"
#include "cwSaveLoad.h"
#include "cwNote.h"
#include "cwNoteLiDAR.h"

//Quick Git
#include <GitRepository.h>

//Qt includes
#include <QDir>
#include <QTime>
#include <QDateTime>
#include <QSqlQuery>
#include <QDebug>
#include <QFileInfo>
#include <QSqlError>
#include <QUndoStack>
#include <QFileDialog>
#include <QSettings>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QSharedPointer>
#include <QSqlRecord>

//Async Future
#include <asyncfuture.h>

//Std includes
#include <ranges>

//Monad includes
using namespace Monad;

QAtomicInt cwProject::ConnectionCounter;

namespace {
// Emits state-change signals if the effective temp/save flags changed during the scope.
class ScopedProjectStateNotifier {
public:
    explicit ScopedProjectStateNotifier(cwProject* project)
        : m_project(project),
          m_wasTemporary(project->isTemporaryProject()),
          m_couldSaveDirectly(project->canSaveDirectly())
    {
    }

    void dismiss() { m_project = nullptr; }

    ~ScopedProjectStateNotifier()
    {
        if (!m_project) {
            return;
        }

        const bool isTemporaryNow = m_project->isTemporaryProject();
        const bool canSaveNow = m_project->canSaveDirectly();

        if (isTemporaryNow != m_wasTemporary) {
            emit m_project->isTemporaryProjectChanged();
        }

        if (canSaveNow != m_couldSaveDirectly) {
            emit m_project->canSaveDirectlyChanged();
        }
    }

private:
    cwProject* m_project = nullptr;
    bool m_wasTemporary = false;
    bool m_couldSaveDirectly = false;
};
} // namespace

/**
  By default, a project is open to a temporary directory
  */
cwProject::cwProject(QObject* parent) :
    QObject(parent),
    m_saveLoad(new cwSaveLoad(this)),
    FileVersion(cwRegionIOTask::protoVersion()),
    SQLiteTempProject(false),
    Region(new cwCavingRegion(this)),
    UndoStack(new QUndoStack(this)),
    ErrorModel(new cwErrorListModel(this))
{
    m_saveLoad->setCavingRegion(Region);
    m_saveLoad->setUndoStack(UndoStack);
    connect(m_saveLoad, &cwSaveLoad::isTemporaryProjectChanged, this, [this]() {
        emit isTemporaryProjectChanged();
        emit canSaveDirectlyChanged();
    });
    connect(m_saveLoad, &cwSaveLoad::fileNameChanged, this, [this]() {
        emit filenameChanged(m_saveLoad->fileName());
    });

    newProject();
}

cwProject::~cwProject()
{
}

/**
  Creates a new tempDirectoryPath for the temp project
  */
// void cwProject::createTempProjectFile() {

//     if(isTemporaryProject()) {
//         //Remove the old temp project file
//         if(QFileInfo(filename()).exists()) {
//             QFile::remove(filename());
//         }
//     }

//     //Create the with a hex number
//     QString projectFile = createTemporaryFilename();
//     setFilename(projectFile);

//     setTemporaryProject(true);

//     //Create and open a new database connection
//     int nextConnectonName = ConnectionCounter.fetchAndAddAcquire(1);
//     ProjectDatabase = QSqlDatabase::addDatabase("QSQLITE", QString("ProjectConnection-%1").arg(nextConnectonName));
//     ProjectDatabase.setDatabaseName(ProjectFile);
//     bool couldOpen = ProjectDatabase.open();
//     if(!couldOpen) {
//         ErrorModel->append(cwError(QString("Couldn't open temp project file: %1. Temp directory not writable?").arg(ProjectFile), cwError::Fatal));
//         return;
//     }

//     //Create default schema
//     createDefaultSchema();

//     emit canSaveDirectlyChanged();
// }

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
// void cwProject::createDefaultSchema() {
//     createDefaultSchema(ProjectDatabase);
// }

// /**
//  * @brief cwProject::addTable
//  * @param tableName - Used only for warning messages
//  * @param sql - The sql that run on the project database
//  */
// void cwProject::createTable(const QSqlDatabase& database, QString sql)
// {
//     QSqlQuery createObjectDataTable(database);

//     //Create the caving region
//     bool couldPrepare = createObjectDataTable.prepare(sql);
//     if(!couldPrepare) {
//         qDebug() << "Couldn't prepare table:" << createObjectDataTable.lastError().databaseText() << sql << LOCATION;
//     }

//     bool couldCreate = createObjectDataTable.exec();
//     if(!couldCreate) {
//         qDebug() << "Couldn't create table:" << createObjectDataTable.lastError().databaseText() << LOCATION;
//     }
// }

// /**
//  * @brief cwProject::insertDocumentation
//  * @param files - A pair Contains the filename and the path to the content of the file
//  * @param pair.first - The filename that's stored in the project
//  * @param pair.second - The path, usually a resaurce, to the content
//  */
// void cwProject::insertDocumentation(const QSqlDatabase& database, QList<QPair<QString, QString> > filenames) //QString filename, QString pathToContent)
// {
//     QSqlQuery hasDocsQuery(database);
//     QString hasDocsQuerySQL = "SELECT name FROM sqlite_master WHERE type='table' AND name='FileFormatDocumenation'";
//     bool success = hasDocsQuery.exec(hasDocsQuerySQL);
//     if(success && hasDocsQuery.next()) {
//         Q_ASSERT(hasDocsQuery.value(0).toString() == QString("FileFormatDocumenation"));
//         QSqlQuery deleteDocsQuery(database);
//         QString deleteQuery = "delete from FileFormatDocumenation";
//         bool success = deleteDocsQuery.exec(deleteQuery);
//         if(!success) {
//             qDebug() << "Couldn't delete contents from FileFormatDocumenation" << deleteDocsQuery.lastError().text() << LOCATION;
//         }
//     } else {
//         if(!success) {
//             qDebug() << "Couldn't run query" << hasDocsQuery.lastError().text() << LOCATION;
//         }
//     }

//     QSqlQuery insertDocsQuery(database);
//     QString query =
//             QString("insert into FileFormatDocumenation") +
//             QString(" (filename, content) values (?, ?)");

//     bool couldPrepare = insertDocsQuery.prepare(query);
//     if(!couldPrepare) {
//         qDebug() << "Couldn't prepare insertDocsQuery:" << insertDocsQuery.lastError().databaseText() << query << LOCATION;
//     }

//     //Go through all the documenation files
//     QPair<QString, QString> filePair;
//     foreach(filePair, filenames) {
//         QString filename = filePair.first;
//         QString pathToFile = filePair.second;


//         QFile file(pathToFile);
//         bool couldOpen = file.open(QFile::ReadOnly);
//         if(!couldOpen) {
//             qDebug() << "Couldn't open documentation file:" << pathToFile << LOCATION;
//             continue;
//         }

//         QByteArray fileContent = file.readAll();

//         insertDocsQuery.bindValue(0, filename);
//         insertDocsQuery.bindValue(1, fileContent);
//         bool success = insertDocsQuery.exec();

//         if(!success) {
//             qDebug() << "Couldn't execute query:" << insertDocsQuery.lastError().databaseText() << query << LOCATION;
//         }
//     }
// }

/**
  \brief Saves the project.

  Make sure the project isn't a temporary project. If it is a temporary project,
 call saveAs(filename) instead
  */
// void cwProject::save() {
//     Q_ASSERT(canSaveDirectly());
//     privateSave();
// }


/**
  Save the project, writes all files to the project
  */
void cwProject::privateSave() {

    qDebug() << "Private Save is broken, TODO fix!";

    // auto region = QSharedPointer<cwCavingRegion>::create(*Region);
    // QString filename = this->filename();
    // region->moveToThread(nullptr);

    // auto future = cwConcurrent::run([region, filename]() {
    //     cwRegionSaveTask saveTask;
    //     saveTask.setDatabaseFilename(filename);
    //     return saveTask.save(region.get());
    // });

    // FutureToken.addJob({QFuture<void>(future), "Saving"});

    // SaveFuture = AsyncFuture::observe(future).subscribe([future, this](){
    //     auto errors = future.result();
    //     ErrorModel->append(errors);
    //     if(!cwError::containsFatal(errors)) {
    //         emit fileSaved();
    //     }
    // }).future();
}

bool cwProject::save()
{
    if(!canSaveDirectly()) {
        return false;
    }

    m_saveLoad->waitForFinished();
    emit fileSaved();
    return true;
}

bool cwProject::saveWillCauseDataLoss() const
{
    return FileVersion > cwRegionIOTask::protoVersion();
}

bool cwProject::saveAs(QString newFilename)
{
    newFilename = cwGlobals::convertFromURL(newFilename);
    if(newFilename.isEmpty()) {
        ErrorModel->append(cwError(QStringLiteral("Save location can't be empty."), cwError::Fatal));
        return false;
    }

    newFilename = cwGlobals::addExtension(newFilename, QStringLiteral("cwproj"));
    if (QFileInfo(newFilename).suffix().compare(QStringLiteral("cw"), Qt::CaseInsensitive) == 0) {
        newFilename = QFileInfo(newFilename).path() + QDir::separator()
            + QFileInfo(newFilename).completeBaseName() + QStringLiteral(".cwproj");
    }

    Monad::ResultBase result = isTemporaryProject()
        ? m_saveLoad->moveProjectTo(newFilename)
        : m_saveLoad->copyProjectTo(newFilename);

    if(result.hasError()) {
        ErrorModel->append(cwError(result.errorMessage(), cwError::Fatal));
        return false;
    }

    emit fileSaved();
    return true;
}

bool cwProject::deleteTemporaryProject()
{
    if(!isTemporaryProject()) {
        ErrorModel->append(cwError(QStringLiteral("Current project is not temporary."), cwError::Warning));
        return false;
    }

    auto result = m_saveLoad->deleteTemporaryProject();
    if(result.hasError()) {
        ErrorModel->append(cwError(result.errorMessage(), cwError::Fatal));
        return false;
    }

    return true;
}

void cwProject::setSqliteTemporaryProject(bool isTemp)
{
    SQLiteTempProject = isTemp;
}

// void cwProject::addImageHelper(std::function<void (QList<cwImage>)> outputCallBackFunc,
//                                std::function<void (cwAddImageTask *)> setImagesFunc)
// {
//     auto format = cwTextureUploadTask::format();

//     cwAddImageTask addImageTask;
//     // addImageTask.setDatabaseFilename(filename());
//     addImageTask.setImageTypesWithFormat(format);

//     //Set all the noteImagePath
//     setImagesFunc(&addImageTask);

//     auto imagesFuture = addImageTask.images();

//     FutureToken.addJob({QFuture<void>(imagesFuture), "Adding Image"});

//     AsyncFuture::observe(imagesFuture)
//         .context(this, [this, imagesFuture, outputCallBackFunc, format, setImagesFunc]()
//                  {
//                      if(cwTextureUploadTask::format() != format) {
//                          //Format has changed, re-run (this isn't true recursion)
//                          addImageHelper(outputCallBackFunc, setImagesFunc);
//                          return;
//                      }

//                      //Convert the images to cwImage
//                      auto results = imagesFuture.results();
//                      QList<cwImage> images = cw::transform(results, [](const cwTrackedImagePtr& imagePtr)
//                                                            {
//                                                                if(imagePtr) {
//                                                                    return imagePtr->take();
//                                                                } else {
//                                                                    qDebug() << "FIXME: imagePtr is null" << LOCATION;
//                                                                    return cwImage();
//                                                                }
//                                                            });

//                      outputCallBackFunc(images);
//                  });
// }

QFuture<ResultBase> cwProject::loadHelper(QString filename)
{
    if(filename.isEmpty()) { QtFuture::makeReadyValueFuture(ResultBase(QStringLiteral("File name is empty"))); }

    FileType type = projectType(filename);

    // //Only load one file at a time
    // LoadFuture.cancel();

    filename = cwGlobals::convertFromURL(filename);

    auto loadFromSQL = [this](const QString& filename) {
        //Run the load task async
        auto loadFuture = cwConcurrent::run([filename](){
            cwRegionLoadTask loadTask;
            loadTask.setDatabaseFilename(filename);
            return loadTask.load();
        });

        FutureToken.addJob({QFuture<void>(loadFuture), QStringLiteral("Loading")});

        auto updateRegion = [this, filename](const cwRegionLoadResult& result) {
            ScopedProjectStateNotifier stateGuard(this);

            //Disable the m_saveLoad, since this should be a temporary project
            m_saveLoad->setCavingRegion(nullptr);

            setFilename(result.filename());
            setSqliteTemporaryProject(result.isTempFile());
            Region->setData(result.cavingRegion());
            FileVersion = result.fileVersion();

            emit loaded();
        };

        return AsyncFuture::observe(loadFuture)
            .context(this, [loadFuture, updateRegion, this]()->ResultBase
                     {
                         auto result = loadFuture.result();

                         if(result.errors().isEmpty()) {
                             updateRegion(result);
                         } else {

                             QString fatalErrors;
                             if(!cwError::containsFatal(result.errors())) {
                                 //Just warnings, we should be able to load
                                 updateRegion(result);
                             } else {
                                 auto errorRange = result.errors()
                                 | std::views::filter(cwError::isFatal)
                                     | std::views::transform([](const cwError& error)->QString { return error.message(); });

                                 fatalErrors = QStringList(errorRange.begin(), errorRange.end()).join('\n');
                             }

                             // qDebug() << "Result error:" << result.errors().size();
                             for(const cwError& error : result.errors()) {
                                 qDebug() << "Message:" << error.message();
                             }

                             ErrorModel->append(result.errors());
                             return fatalErrors.isEmpty() ? ResultBase() : ResultBase(fatalErrors);
                         }

                         return ResultBase();
                     }).future();
    };

    if(type == SqliteFileType) {
        return loadFromSQL(filename);
    } else if (type == GitFileType) {
        auto loadFuture = m_saveLoad->load(filename);

        return AsyncFuture::observe(loadFuture)
            .context(this, [this, loadFuture]() {
                auto result = loadFuture.result();

                if (!result.hasError()) {
                    ScopedProjectStateNotifier stateGuard(this);
                    setSqliteTemporaryProject(false);
                    FileVersion = cwRegionIOTask::protoVersion();
                }

                return result;
            }).future();
    }

    QFileInfo info(filename);

    if(!info.exists()) {
        return QtFuture::makeReadyValueFuture(ResultBase(cwRegionLoadTask::doesNotExistErrorMessage(filename)));
    }

    if(!info.isReadable()) {
        return QtFuture::makeReadyValueFuture(ResultBase(cwRegionLoadTask::fileNotReadableErrorMessage(filename)));
    }

    return QtFuture::makeReadyValueFuture(ResultBase(QStringLiteral("Couldn't open '%1' because it has a unknown file type. Is it corrupted!?").arg(filename)));
}

QFuture<ResultBase> cwProject::convertFromProjectV6Helper(QString oldProjectFilename, const QDir &newProjectDirectory, bool isTemporary)
{
    //Make a temporary project
    auto tempProject = std::make_shared<cwProject>();
    tempProject->setFutureManagerToken(futureManagerToken());

    //Load the old project into the temp project
    auto oldLoadFuture = tempProject->loadHelper(oldProjectFilename);

    // qDebug() << "oldLoadFuture:" << oldLoadFuture.isFinished();

    auto loadTempProjectFuture =
        AsyncFuture::observe(oldLoadFuture)
            .context(this, [this, oldLoadFuture, tempProject, oldProjectFilename, newProjectDirectory]() {
                // qDebug() << "oldLoadFuture:" << oldLoadFuture.result().hasError() << oldLoadFuture.result().errorMessage();

                return mbind(oldLoadFuture, [=, this](const ResultBase& result){
                    // qDebug() << "Save";

                    //Use a shared pointer here, too keep saveLoad alive until, the project is fully saved
                    auto saveLoad = std::make_shared<cwSaveLoad>();
                    auto filenameFuture = saveLoad->saveAllFromV6(newProjectDirectory, tempProject.get(), oldProjectFilename);

                    auto loadFuture
                        = AsyncFuture::observe(filenameFuture)
                              .context(this, [saveLoad, filenameFuture, this]() {
                                  return Monad::mbind(filenameFuture, [this](const Monad::ResultString& filename) {
                                      return loadHelper(filename.value());
                                  });
                              }).future();

                    FutureToken.addJob(cwFuture(QFuture<void>(loadFuture), QStringLiteral("Converting")));
                    return loadFuture;
                });
            }).future();

    auto finalFuture =
        AsyncFuture::observe(loadTempProjectFuture)
            .context(this, [this, loadTempProjectFuture, tempProject, isTemporary](){
                auto result = loadTempProjectFuture.result();
                errorModel()->append(tempProject->errorModel()->toList());

                if (!result.hasError()) {
                    ScopedProjectStateNotifier stateGuard(this);

                    setSqliteTemporaryProject(isTemporary);
                    FileVersion = tempProject->FileVersion;
                }


                // if(result.hasError()) {
                //     qDebug() << "Adding error:" << loadTempProjectFuture.result().errorMessage();
                //     errorModel()->append(cwError(loadTempProjectFuture.result().errorMessage(), cwError::Fatal));
                // }

                return loadTempProjectFuture;
            }).future();


    FutureToken.addJob(cwFuture(QFuture<void>(finalFuture), QStringLiteral("Loading")));


    return finalFuture;
}

/**
  Saves the project as a new file

    Saves the project as.  This will copy the current project to a different location, leaving
    the original. If the project is a temperary project, the temperary project will be removed
    from disk.
  */
// void cwProject::saveAs(QString newFilename){
//     newFilename = cwGlobals::addExtension(newFilename, "cw");
//     newFilename = cwGlobals::convertFromURL(newFilename);

//     if(QFileInfo(newFilename) == QFileInfo(filename()) && saveWillCauseDataLoss()) {
//         errorModel()->append(cwError(QString("Can't overwrite %1 because file is newer that the current version of CaveWhere. To solve this, save it somewhere else").arg(filename()), cwError::Fatal));
//         return;
//     }

//     //Just save it the user is overwritting it
//     if(QFileInfo(newFilename) == QFileInfo(filename())) {
//         privateSave();
//         return;
//     }

//     //Try to remove the existing file
//     if(QFileInfo(newFilename).exists()) {
//         bool couldRemove = QFile::remove(newFilename);
//         if(!couldRemove) {
//             errorModel()->append(cwError(QString("Couldn't remove %1").arg(newFilename), cwError::Fatal));
//             return;
//         }
//     }

//     //Copy the old file to the new location
//     bool couldCopy = QFile::copy(filename(), newFilename);
//     if(!couldCopy) {
//         errorModel()->append(cwError(QString("Couldn't copy %1 to %2").arg(filename()).arg(newFilename), cwError::Fatal));
//         return;
//     }

//     if(isTemporaryProject()) {
//         QFile::remove(filename());
//     }

//     //Update the project filename
//     setFilename(newFilename);
//     setTemporaryProject(false);

//     //Save the current data
//     privateSave();
// }

/**
  \brief Creates a new project
  */
void cwProject::newProject() {
    m_saveLoad->newProject();

    // //Creates a temp directory for the project
    // createTempProjectFile();

    // //Create the caving the caving region that this project mantaines
    // Region->clearCaves();

}

// /**
//  * @brief cwProject::setTaskManager
//  * @param manager
//  *
//  * When adding images to the project, this will allow the user to see the image progress
//  */
// void cwProject::setTaskManager(cwTaskManagerModel *manager)
// {
//     TaskManager = manager;
// }

// /**
//  * @brief cwProject::taskManager
//  * @return Return's the current taskManager
//  */
// cwTaskManagerModel *cwProject::taskManager() const
// {
//     return TaskManager;
// }

/**
  Loads the project, loads all the files to the project
  */
void cwProject::loadFile(QString filename) {
    if(filename.isEmpty()) { return; }

    //Only load one file at a time
    LoadFuture.cancel();
    LoadFuture = loadHelper(filename);
}

/**
  \brief Sets the current project file

  \param newFilename - The new filename that this project will be moved to.
  */
void cwProject::setFilename(QString newFilename) {
    if(newFilename != filename()) {
        m_saveLoad->setFileName(newFilename);
        emit filenameChanged(newFilename);
    }
}

/**
 * @brief cwProject::createDefaultSchema
 * @param database
 */
// void cwProject::createDefaultSchema(const QSqlDatabase &database)
// {

//     //Create the database with full vacuum so we don't use up tons of space
//     QSqlQuery vacuumQuery(database);
//     QString query = QString("PRAGMA auto_vacuum = 1");
//     vacuumQuery.exec(query);

//     cwSQLManager::Transaction transaction(database);

//     //Create the caving region
//     QString objectDataQuery =
//             QString("CREATE TABLE IF NOT EXISTS ObjectData (") +
//             QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
//             QString("protoBuffer BLOB") + //Last index
//             QString(")");

//     //Create ObjectData
//     createTable(database, objectDataQuery);

//     QString documentationTableQuery =
//             QString("CREATE TABLE IF NOT EXISTS FileFormatDocumenation (") +
//             QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
//             QString("filename STRING,") +
//             QString("content STRING)");
//     createTable(database, documentationTableQuery);

//     QList< QPair< QString, QString> > docsFiles;
//     docsFiles.append(QPair<QString, QString>("README.txt", ":/docs/FileFormatDocumentation.txt"));
//     docsFiles.append(QPair<QString, QString>("qt.pro", ":/src/qt.proto"));
//     docsFiles.append(QPair<QString, QString>("cavewhere.pro", ":/src/qt.proto"));
//     insertDocumentation(database, docsFiles);

//     //Create the caving region
//     QString imageTableQuery =
//             QString("CREATE TABLE IF NOT EXISTS Images (") +
//             QString("id INTEGER PRIMARY KEY AUTOINCREMENT,") + //First index
//             QString("type STRING,") + //Type of image
//             QString("shouldDelete BOOL,") + //If the image should be delete
//             QString("width INTEGER,") + //The width of the image
//             QString("height INTEGER,") + //The height of the image
//             QString("dotsPerMeter INTEGER,") + //The resolution of the image
//             QString("imageData BLOB)"); //The blob that stores the image data
//     createTable(database, imageTableQuery);
// }

// QString cwProject::createTemporaryFilename()
// {
//     QDateTime seedTime = QDateTime::currentDateTime();
//     return QString("%1/CavewhereTmpProject-%2.cw")
//             .arg(QDir::tempPath())
//             .arg(seedTime.toMSecsSinceEpoch(), 0, 16);
// }

QSqlDatabase cwProject::createDatabaseConnection(const QString &connectionName, const QString &databasePath)
{
    int nextConnectonName = ConnectionCounter.fetchAndAddAcquire(1);
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", QString("%1-%2").arg(connectionName).arg(nextConnectonName));
    database.setDatabaseName(databasePath);
    bool connected = database.open();
    if(!connected) {
        throw std::runtime_error(QString("Couldn't connect to database for %1 %2 %3").arg(connectionName).arg(databasePath).toStdString());
    }

    return database;
}

/**
 * @brief cwProject::waitToFinish
 *
 * Will cause the project to block until the underlying task is finished. This is useful
 * for unit testing.
 */
void cwProject::waitLoadToFinish()
{
    AsyncFuture::waitForFinished(LoadFuture);
    m_saveLoad->waitForFinished();
}

/**
 * @brief cwProject::waitSaveToFinish
 *
 * Will cause the project to block until the underlying save task is finished. This is
 * useful
 */
void cwProject::waitSaveToFinish()
{
    m_saveLoad->waitForFinished();
    // AsyncFuture::waitForFinished(SaveFuture);
}

/**
 * Returns true if the user has modified the file, and false if haven't
 */
bool cwProject::isModified()
{
    m_saveLoad->waitForFinished();
    m_saveLoad->repository()->checkStatus();
    return m_saveLoad->repository()->modifiedFileCount() > 0;
}

bool cwProject::isNewProject() const
{
    if(!isTemporaryProject()) {
        return false;
    }

    auto repository = m_saveLoad->repository();
    if(!repository) {
        return false;
    }

    if(repository->hasCommits()) {
        return false;
    }

    return Region->caveCount() == 0;
}

/**
 * @brief cwProject::addImages
 * @param noteImagePaths - Source location of the images
 * @param dir - The destination dir of the image
 * @param outputCallBackFunc - once the
 *
 */
void cwProject::addImages(QList<QUrl> noteImagePaths,
                          const QDir& dir,
                          std::function<void (QList<cwImage>)> outputCallBackFunc)
{
    m_saveLoad->addImages(noteImagePaths, dir, outputCallBackFunc);
}

void cwProject::addFiles(QList<QUrl> filePath, const QDir &dir, std::function<void (QList<QString>)> outputCallBackFunc)
{
    m_saveLoad->addFiles(filePath, dir, outputCallBackFunc);
}

void cwProject::loadOrConvert(const QString &filename)
{
    if(filename.isEmpty()) { return; }

    FileType type = projectType(filename);

    LoadFuture.cancel();

    if(type == SqliteFileType) {
        QTemporaryDir dir;
        dir.setAutoRemove(false);
        auto tempDir = QDir(dir.filePath(QFileInfo(filename).baseName()));
        const QFileInfo info(filename);
        const bool temporaryProject = !info.isWritable();
        LoadFuture = convertFromProjectV6Helper(filename, tempDir, temporaryProject);
        // setTemporaryProject(true);
    } else {
        //This could be Git file or a corrupted file
        auto loadFuture = loadHelper(filename);
        LoadFuture = AsyncFuture::observe(loadFuture)
                         .context(this, [loadFuture, this]() {
                             auto result = loadFuture.result();
                             if(result.hasError()) {
                                 errorModel()->append(cwError(result.errorMessage(), cwError::Fatal));
                             }
                         }).future();
    }
}

/**
 * Returns all the image formats
 */
QString cwProject::supportedImageFormats()
{
    QStringList formats = cwImageUtils::supportedImageFormats();

    if(cwPDFConverter::isSupported()) {
        formats.append("pdf");
    }

    QStringList withWildCards;
    std::transform(formats.begin(), formats.end(), std::back_inserter(withWildCards),
                   [](const QString& format)
                   {
                       return "*." + format;
                   });

    return withWildCards.join(' ');
}


void cwProject::convertFromProjectV6(QString oldProjectFilename,
                                     const QDir &newProjectDirectory)
{
    //Make a temporary project
    auto tempProject = std::make_shared<cwProject>();
    tempProject->setFutureManagerToken(futureManagerToken());
    auto loadTempProjectFuture = AsyncFuture::observe(tempProject.get(), &cwProject::loaded)
                                     .context(this, [this, tempProject, oldProjectFilename, newProjectDirectory]()->QFuture<ResultBase> {

                                         //Use a shared pointer here, too keep saveLoad alive until, the project is fully saved
                                         auto saveLoad = std::make_shared<cwSaveLoad>();
                                         auto filenameFuture = saveLoad->saveAllFromV6(newProjectDirectory, tempProject.get(), oldProjectFilename);
                                         qDebug() << "I get here!";

                                         auto loadFuture = AsyncFuture::observe(filenameFuture)
                                                               .context(this, [saveLoad, filenameFuture, this]() {
                                                                   if(!filenameFuture.result().hasError()) {
                                                                       //Load the project into this cwProject
                                                                       qDebug() << "Loading project:" << filenameFuture.result().value();
                                                                       return loadHelper(filenameFuture.result().value());
                                                                   } else {
                                                                       return QtFuture::makeReadyValueFuture<ResultBase>(filenameFuture.result());
                                                                   }
                                                               }).future();

                                         FutureToken.addJob(cwFuture(QFuture<void>(loadFuture), QStringLiteral("Converting")));
                                         return loadFuture;
                                     }).future();

    FutureToken.addJob(cwFuture(QFuture<void>(loadTempProjectFuture), QStringLiteral("Loading")));

    //Load the old project into the temp project
    tempProject->loadFile(oldProjectFilename);
}

cwProject::FileType cwProject::projectType(QString filename) const
{
    filename = cwGlobals::convertFromURL(filename);

    //File is empty, return unknown
    QFileInfo info(filename);
    if(info.size() == 0) {
        return UnknownFileType;
    }

    //Check if we can connect to the sqlite database, v6 and lower
    auto result = Monad::mtry([filename]() {
        auto database = createDatabaseConnection("test_database", filename);

        QSqlQuery query(database);
        query.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='ObjectData'");

        if (!query.exec()) {
            return Monad::ResultBase(QStringLiteral("Failed to query database: %1").arg(query.lastError().text()));
        }

        if(!query.next()) {
            return Monad::ResultBase(QStringLiteral("Sqlite database dosen't have ObjectData"));
        }

        //No error
        return Monad::ResultBase();
    });

    if(result.hasError()) {
        //Check to see
        auto regionResult = cwSaveLoad::loadCavingRegion(filename);
        if(regionResult.hasError()) {
            return UnknownFileType;
        } else {
            return GitFileType;
        }
    } else {
        //V6 or lower
        return SqliteFileType;
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
        m_saveLoad->setUndoStack(UndoStack);
        emit undoStackChanged();
    }
}

void cwProject::setFutureManagerToken(const cwFutureManagerToken &token) {
    FutureToken = token;
    m_saveLoad->setFutureManagerToken(token);
}

cwFutureManagerToken cwProject::futureManagerToken() const {
    return FutureToken;
}



bool cwProject::isTemporaryProject() const {
    return m_saveLoad->isTemporaryProject() || SQLiteTempProject || saveWillCauseDataLoss();
}

QString cwProject::filename() const {
    return m_saveLoad->fileName();
}

/**
 * Returns the absolute path of the file that has a relative path
 * to the project
 */
QString cwProject::absolutePath(const QString &relativePath) const
{
    return m_saveLoad->projectDir().absoluteFilePath(relativePath);

}

QString cwProject::absolutePath(const cwNote* note) const
{
    if(note == nullptr) {
        return QString();
    }

    const auto image = note->image();
    if(image.mode() == cwImage::Mode::Path && !image.path().isEmpty()) {
        return cwSaveLoad::absolutePath(note, image.path());
    }

    return QString();
}

QString cwProject::absolutePath(const cwNoteLiDAR* noteLiDAR) const
{
    if(noteLiDAR == nullptr) {
        return QString();
    }

    if(!noteLiDAR->filename().isEmpty()) {
        return cwSaveLoad::absolutePath(noteLiDAR, noteLiDAR->filename());
    }

    if(!noteLiDAR->iconImagePath().isEmpty()) {
        return cwSaveLoad::absolutePath(noteLiDAR, noteLiDAR->iconImagePath());
    }

    return QString();
}
