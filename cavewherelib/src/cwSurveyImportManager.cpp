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
#include "cwSurveyImportManager.h"
#include "cwErrorListModel.h"

//Qt includes
#include <QFileDialog>
#include <QSettings>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QWindow>

namespace {
    //Transient parent keeps the dialog above the main window on macOS (issue #481).
    QWindow* mainQuickWindow() {
        const auto topLevel = QGuiApplication::topLevelWindows();
        for(QWindow* candidate : topLevel) {
            if(qobject_cast<QQuickWindow*>(candidate) && candidate->isVisible()) {
                return candidate;
            }
        }
        return nullptr;
    }

    void launchImportDialog(cwImportTreeDataDialog::Names names,
                            cwTreeDataImporter* importer,
                            cwCavingRegion* region,
                            QUndoStack* undoStack,
                            const QStringList& files) {
        auto* dialog = new cwImportTreeDataDialog(names, importer, region, mainQuickWindow());
        dialog->setUndoStack(undoStack);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->open();
        dialog->setInputFiles(files);
    }
}

cwSurveyImportManager::cwSurveyImportManager(QObject *parent) :
    QObject(parent),
    CavingRegion(nullptr),
    CompassImporter(new cwCompassImporter()),
    MessageListFont(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    connect(CompassImporter, &cwCompassImporter::finished, this, &cwSurveyImportManager::compassImporterFinished);
    connect(CompassImporter, &cwCompassImporter::statusMessage, this, &cwSurveyImportManager::compassMessages);
}

cwSurveyImportManager::~cwSurveyImportManager()
{
    CompassImporter->stop();
    CompassImporter->waitToFinish(cwTask::IgnoreRestart);
    delete CompassImporter;
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

    QSettings settings;
    QString lastFile = settings.value(ImportSurvexKey).toString();
    QString filename = QFileDialog::getOpenFileName(nullptr, "Import Survex", lastFile, "Survex *.svx");

    if (QFileInfo(filename).exists()) {
        settings.setValue(ImportSurvexKey, filename);
        cwImportTreeDataDialog::Names names{"Survex Importer", "Survex Errors"};
        launchImportDialog(names, new cwSurvexImporter(), cavingRegion(), UndoStack, {filename});
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

void cwSurveyImportManager::waitForCompassToFinish()
{
    CompassImporter->waitToFinish();
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
    foreach(const auto& cave, CompassImporter->caves()) {
        cwCave* newCave = new cwCave();
        newCave->setData(cave);
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
    if(ErrorModel) {
        ErrorModel->append(cwError(message, cwError::Warning));
    }
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
        launchImportDialog(names, new cwWallsImporter(), cavingRegion(), UndoStack, {filename});
    }
}

void cwSurveyImportManager::importWallsSrv() {
    QSettings settings;
    QString lastDir = settings.value(ImportWallsKey).toString();
    QStringList filenames = QFileDialog::getOpenFileNames(nullptr, "Import Walls", lastDir, "Walls *.srv");

    if (!filenames.isEmpty()) {
        settings.setValue(ImportWallsKey, filenames[0]);
        cwImportTreeDataDialog::Names names{"Walls Importer", "Walls Errors"};
        launchImportDialog(names, new cwWallsImporter(), cavingRegion(), UndoStack, filenames);
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

/**
*
*/
void cwSurveyImportManager::setErrorModel(cwErrorListModel* errorModel) {
    if(ErrorModel != errorModel) {
        ErrorModel = errorModel;
        emit errorModelChanged();
    }
}

/**
*
*/
cwErrorListModel* cwSurveyImportManager::errorModel() const {
    return ErrorModel;
}
