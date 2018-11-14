/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyImportManager.h"
#include "cwImportTreeDataDialog.h"
#include "cwSurvexImporter.h"
#include "cwCompassImporter.h"
#include "cwWallsImporter.h"
#include "cwCavingRegion.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwCSVImporterTask.h"

//Qt includes
#include <QFileDialog>
#include <QSettings>

cwSurveyImportManager::cwSurveyImportManager(QObject *parent) :
    QObject(parent),
    CavingRegion(nullptr),
    CompassImporter(new cwCompassImporter()),
    CSVImporter(new cwCSVImporterTask()),
    MessageListFont(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    connect(CompassImporter, &cwCompassImporter::finished, this, &cwSurveyImportManager::compassImporterFinished);
    connect(CompassImporter, &cwCompassImporter::statusMessage, this, &cwSurveyImportManager::compassMessages);

    connect(CSVImporter, &cwCompassImporter::finished, this, &cwSurveyImportManager::csvImportedFinished);
}

cwSurveyImportManager::~cwSurveyImportManager()
{
    CompassImporter->stop();
    CompassImporter->waitToFinish();
    CompassImporter->deleteLater();
    CSVImporter->stop();
    CSVImporter->waitToFinish();
    CSVImporter->deleteLater();
}

void cwSurveyImportManager::setCavingRegion(cwCavingRegion *region)
{
    if(CavingRegion != region) {
        CavingRegion = region;
        emit cavingRegionChanged();
    }
}

const QString cwSurveyImportManager::ImportSurvexKey = "LastImportSurvexFile";
const QString cwSurveyImportManager::ImportWallsKey = "LastImportWallsFile";

/**
  \brief Opens the survex importer dialog
  */
void cwSurveyImportManager::importSurvex() {

    QSettings settings;
    QString lastFile = settings.value(ImportSurvexKey).toString();
    QString filename = QFileDialog::getOpenFileName(nullptr, "Import Survex", lastFile, "Survex *.svx");

    if (QFileInfo(filename).exists()) {
        settings.setValue(ImportSurvexKey, filename);
        cwImportTreeDataDialog::Names names{"Survex Importer", "Survex Errors"};
        cwImportTreeDataDialog* survexImportDialog = new cwImportTreeDataDialog(names, new cwSurvexImporter(), cavingRegion());
        survexImportDialog->setUndoStack(UndoStack);
        survexImportDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        survexImportDialog->open();
        QStringList files;
        files << filename;
        survexImportDialog->setInputFiles(files);
    }
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
 * Starts the import task for CSV on filename
 */
void cwSurveyImportManager::importCSV(QUrl filename)
{
    if(CSVImporter->isReady()) {
        CSVImporter->setFilename(filename.toLocalFile());
        CSVImporter->start();
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
  \brief Opens the Walls importer dialog
  */
void cwSurveyImportManager::importWalls() {

    QSettings settings;
    QString lastFile = settings.value(ImportWallsKey).toString();
    QString filename = QFileDialog::getOpenFileName(nullptr, "Import Walls", lastFile, "Walls *.wpj");

    if (QFileInfo(filename).exists()) {
        settings.setValue(ImportWallsKey, filename);
        cwImportTreeDataDialog::Names names{"Walls Importer", "Walls Errors"};
        cwImportTreeDataDialog* wallsImportDialog = new cwImportTreeDataDialog(names, new cwWallsImporter(), cavingRegion());
        wallsImportDialog->setUndoStack(UndoStack);
        wallsImportDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        wallsImportDialog->open();
        QStringList files;
        files << filename;
        wallsImportDialog->setInputFiles(files);
    }
}

void cwSurveyImportManager::importWallsSrv() {
    QSettings settings;
    QString lastDir = settings.value(ImportWallsKey).toString();
    QStringList filenames = QFileDialog::getOpenFileNames(nullptr, "Import Walls", lastDir, "Walls *.srv");

    if (!filenames.isEmpty()) {
        settings.setValue(ImportWallsKey, filenames[0]);
        cwImportTreeDataDialog::Names names{"Walls Importer", "Walls Errors"};
        cwImportTreeDataDialog* wallsImportDialog = new cwImportTreeDataDialog(names, new cwWallsImporter(), cavingRegion());
        wallsImportDialog->setUndoStack(UndoStack);
        wallsImportDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        wallsImportDialog->open();
        wallsImportDialog->setInputFiles(filenames);
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
 * @brief cwSurveyImportManager::csvImportedFinished
 *
 * Adds the imported cave to cavewhere
 */
void cwSurveyImportManager::csvImportedFinished()
{
    foreach(cwCave cave, CSVImporter->caves()) {
        cwCave* newCave = new cwCave(cave); //Copy the caves
        CavingRegion->addCave(newCave);
    }
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
