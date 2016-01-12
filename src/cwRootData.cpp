/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRootData.h"
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwSurveyExportManager.h"
#include "cwSurveyImportManager.h"
#include "cwItemSelectionModel.h"
#include "cwQMLReload.h"
#include "cwLicenseAgreement.h"
#include "cwRegionSceneManager.h"
#include "cwEventRecorderModel.h"
#include "cwTaskManagerModel.h"
#include "cwPageSelectionModel.h"

//Qt includes
#include <QItemSelectionModel>
#include <QUndoStack>
#include <QSettings>
#include <QStandardPaths>

//Generated files from qbs
#include "cavewhereVersion.h"

cwRootData::cwRootData(QObject *parent) :
    QObject(parent),
    DefaultTrip(new cwTrip(this)),
    DefaultTripCalibration(new cwTripCalibration(this)),
    LeadsVisible(true)
{

    //Task Manager, allows the users to see running tasks
    TaskManagerModel = new cwTaskManagerModel(this);

    //Create the project, this saves and load data
    Project = new cwProject(this);
    Project->setTaskManager(TaskManagerModel);

    Region = Project->cavingRegion();
    Region->setUndoStack(undoStack());

    //Setup the region tree
    RegionTreeModel = new cwRegionTreeModel(this);
    RegionTreeModel->setCavingRegion(Region);

    //Setup the loop closer
    LinePlotManager = new cwLinePlotManager(this);
    LinePlotManager->setRegion(Region);

    //Setup the scrap manager
    ScrapManager = new cwScrapManager(this);
    ScrapManager->setProject(Project);
    ScrapManager->setRegionTreeModel(RegionTreeModel);
    ScrapManager->setLinePlotManager(LinePlotManager);
    ScrapManager->setTaskManager(TaskManagerModel);

    //Setup the survey import manager
    SurveyImportManager = new cwSurveyImportManager(this);
    SurveyImportManager->setCavingRegion(Region);
    SurveyImportManager->setUndoStack(undoStack());

    QuickView = nullptr;

    //For debugging
    QMLReloader = new cwQMLReload(this);
    EventRecorderModel = new cwEventRecorderModel(this);

    //For license agreement
    License = new cwLicenseAgreement(this);
    License->setVersion(version());

    RegionSceneManager = new cwRegionSceneManager(this);
    RegionSceneManager->setCavingRegion(Region);

    ScrapManager->setGLScraps(RegionSceneManager->scraps());
    LinePlotManager->setGLLinePlot(RegionSceneManager->linePlot());

    PageSelectionModel = new cwPageSelectionModel(this);
}

/**
Gets undoStack
*/
QUndoStack* cwRootData::undoStack() const {
    return Project->undoStack();
}

/**
Sets quickWindow
*/
void cwRootData::setQuickView(QQuickView* quickView) {
    if(QuickView != quickView) {
        QuickView = quickView;
//        QMLReloader->setQuickView(QuickView);
        emit quickWindowChanged();
    }
}

/**
 * @brief cwRootData::version
 * @return
 *
 * Returns the current version of cavewhere
 */
QString cwRootData::version() const {
    return CavewhereVersion; //Automatically generated from qbs in cavewhereVersion.h
}

/**
* @brief cwRootData::setLeadsVisible
* @param leadsVisible
*
* This function is temperary, should be moved to a layer manager
*/
void cwRootData::setLeadsVisible(bool leadsVisible) {
    if(LeadsVisible != leadsVisible) {
        LeadsVisible = leadsVisible;
        emit leadsVisibleChanged();
    }
}

/**
* @brief cwRootData::lastDirectory
* @return Returns the last directory open by the file dialog. If the directory doesn't exist, this opens the desktopLocation
*/
QUrl cwRootData::lastDirectory() const {
    QSettings settings;
    QUrl dir = settings.value("LastDirectory", QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))).toUrl();
    return dir;
}

/**
* @brief cwRootData::setLastDirectory
* @param lastDirectory - Set the last directory that was opened by the user. This is useful for
* propertly position the file dialogs
*/
void cwRootData::setLastDirectory(QUrl lastDirectory) {
    QFileInfo info(lastDirectory.toLocalFile());
    QUrl dir = QUrl::fromLocalFile(info.path());

    if(this->lastDirectory() != dir) {
        QSettings settings;
        settings.setValue("LastDirectory", dir);
        emit lastDirectoryChanged();
    }
}
