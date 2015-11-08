/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyImportManager.h"
#include "cwImportSurvexDialog.h"
#include "cwCompassImporter.h"
#include "cwWallsImporter.h"
#include "cwCavingRegion.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"

//Qt includes
#include <QFileDialog>
#include <QThread>
#include <QSettings>

cwSurveyImportManager::cwSurveyImportManager(QObject *parent) :
    QObject(parent),
    ImportThread(new QThread()),
    CavingRegion(nullptr),
    CompassImporter(new cwCompassImporter()),
    WallsImporter(new cwWallsImporter()),
    MessageListFont(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    CompassImporter->setThread(ImportThread);
    connect(CompassImporter, &cwCompassImporter::finished, this, &cwSurveyImportManager::compassImporterFinished);
    connect(CompassImporter, &cwCompassImporter::statusMessage, this, &cwSurveyImportManager::compassMessages);

    WallsImporter->setThread(ImportThread);
    connect(WallsImporter, &cwWallsImporter::finished, this, &cwSurveyImportManager::wallsImporterFinished);
    connect(WallsImporter, &cwWallsImporter::message, this, &cwSurveyImportManager::wallsMessages);
}

cwSurveyImportManager::~cwSurveyImportManager()
{
    ImportThread->quit();
    ImportThread->wait();
    ImportThread->deleteLater();
    CompassImporter->deleteLater();
    WallsImporter->deleteLater();
}

void cwSurveyImportManager::setCavingRegion(cwCavingRegion *region)
{
    if(CavingRegion != region) {
        CavingRegion = region;
        emit cavingRegionChanged();
    }
}

/**
  \brief Opens the survex importer dialog
  */
void cwSurveyImportManager::importSurvex() {

    cwImportSurvexDialog* survexImportDialog = new cwImportSurvexDialog(cavingRegion());
    survexImportDialog->setUndoStack(UndoStack);
    survexImportDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    survexImportDialog->open();
}

/**
 * @brief cwSurveyImportManager::importCompassDataFile
 *
 * Open a compass file to import
 */
void cwSurveyImportManager::importCompassDataFile(QList<QUrl> filenames)
{
    QStringList dataFiles = urlsToStringList(filenames);

    if(CompassImporter->isReady()) {
        CompassImporter->setCompassDataFiles(dataFiles + QueuedCompassFile);
        emit messagesCleared();
        CompassImporter->start();
    } else if(CompassImporter->isRunning()) {
        QueuedCompassFile.append(dataFiles);
    }
}

/**
 * @brief cwSurveyImportManager::compassImporterFinished
 *
 * Called when the compass importer has finished running
 */
void cwSurveyImportManager::compassImporterFinished()
{
    Q_ASSERT(CompassImporter->isReady());

    UndoStack->beginMacro("Compass Import");

    //Add new caves
    foreach(cwCave cave, CompassImporter->caves()) {
        cwCave* newCave = new cwCave(cave); //Copy the caves
        CavingRegion->addCave(newCave);
    }

    UndoStack->endMacro();

    if(!QueuedCompassFile.isEmpty()) {
        //Rerun the compass data file with the queued compass files
        CompassImporter->setCompassDataFiles(QueuedCompassFile);
        CompassImporter->start();
    }
}

/**
 * @brief cwSurveyImportManager::compassMessages
 * @param message
 *
 * Reports messages
 * TODO: Make this report to the gui, in a meaning full way
 */
void cwSurveyImportManager::compassMessages(QString message)
{
    qDebug() << "Compass Importer:" << message;
//    emit messageAdded(message);
}

/**
 * @brief cwSurveyImportManager::importWallsDataFile
 *
 * Open a walls file to import
 */
void cwSurveyImportManager::importWallsDataFile(QList<QUrl> filenames)
{
    QStringList dataFiles = urlsToStringList(filenames);

    if(WallsImporter->isReady()) {
        WallsImporter->setWallsDataFiles(dataFiles + QueuedWallsFile);
        emit messagesCleared();
        WallsImporter->start();
    } else if(WallsImporter->isRunning()) {
        QueuedWallsFile.append(dataFiles);
    }
}

/**
 * @brief cwSurveyImportManager::wallsImporterFinished
 *
 * Called when the walls importer has finished running
 */
void cwSurveyImportManager::wallsImporterFinished()
{
    Q_ASSERT(WallsImporter->isReady());

    UndoStack->beginMacro("Walls Import");

    //Add new caves
    foreach(cwCave cave, WallsImporter->caves()) {
        cwCave* newCave = new cwCave(cave); //Copy the caves
        CavingRegion->addCave(newCave);
    }

    UndoStack->endMacro();

    if(!QueuedWallsFile.isEmpty()) {
        //Rerun the walls data file with the queued walls files
        WallsImporter->setWallsDataFiles(QueuedWallsFile);
        WallsImporter->start();
    }
}

/**
 * @brief cwSurveyImportManager::wallsMessages
 * @param message
 *
 * Reports messages
 * TODO: Make this report to the gui, in a meaning full way
 */
void cwSurveyImportManager::wallsMessages(QString severity, QString message, QString source,
                   int startLine, int startColumn, int endLine, int endColumn)
{
    qDebug() << "Walls Importer:" << message;
    emit messageAdded(severity, message, source, startLine, startColumn, endLine, endColumn);
}

/**
 * @brief cwSurveyImportManager::urlsToStringList
 * @param urls
 * @return The converted urls as a stringlist
 */
QStringList cwSurveyImportManager::urlsToStringList(QList<QUrl> urls)
{
    QStringList filenames;
    foreach(QUrl url, urls) {
        filenames.append(url.toLocalFile());
    }
    return filenames;
}

/**
 * @brief cwSurveyImportManager::cavingRegion
 * @return Returns the current caving region that the import will add to
 */
cwCavingRegion *cwSurveyImportManager::cavingRegion() const
{
    return CavingRegion;
}

/**
* @brief cwSurveyImportManager::undoStack
* @return
*/
QUndoStack* cwSurveyImportManager::undoStack() const {
    return UndoStack;
}

/**
* @brief cwSurveyImportManager::setUndoStack
* @param undoStack
*/
void cwSurveyImportManager::setUndoStack(QUndoStack* undoStack) {
    if(UndoStack != undoStack) {
        UndoStack = undoStack;
        emit undoStackChanged();
    }
}
