//Our includes
#include "cwSurveyImportManager.h"
#include "cwImportSurvexDialog.h"
#include "cwCompassImporter.h"
#include "cwCavingRegion.h"

//Qt includes
#include <QFileDialog>
#include <QThread>

cwSurveyImportManager::cwSurveyImportManager(QObject *parent) :
    QObject(parent),
    ImportThread(new QThread()),
    CavingRegion(NULL),
    CompassImporter(new cwCompassImporter())
{
    CompassImporter->setThread(ImportThread);
}

cwSurveyImportManager::~cwSurveyImportManager()
{
    ImportThread->quit();
    ImportThread->wait();
    ImportThread->deleteLater();
    CompassImporter->deleteLater();
}

void cwSurveyImportManager::setCavingRegion(cwCavingRegion *region)
{
    if(CavingRegion != region) {
        CavingRegion = region;
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
void cwSurveyImportManager::importCompassDataFile()
{
    QStringList dataFiles = QFileDialog::getOpenFileNames(NULL, "Compass Data File", QString(), "*.dat");

    if(CompassImporter->isReady()) {
        CompassImporter->setCompassDataFiles(dataFiles + QueuedCompassFile);
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
}
