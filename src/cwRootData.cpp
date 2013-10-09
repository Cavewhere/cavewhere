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

//Qt includes
#include <QItemSelectionModel>
#include <QUndoStack>

//Generated files from qbs
#include "cavewhereVersion.h"

cwRootData::cwRootData(QObject *parent) :
    QObject(parent),
    DefaultTrip(new cwTrip(this)),
    DefaultTripCalibration(new cwTripCalibration(this))
{
    //Create the project, this saves and load data
    Project = new cwProject(this);
    Region = Project->cavingRegion();

    Region->setUndoStack(undoStack());

    //Setup the region tree model, this model allows the user to see Region in tree form
    RegionTreeModel = new cwRegionTreeModel(this);
    RegionTreeModel->setCavingRegion(Region);

    //Notifies the survey exporter when selection has changed
    RegionTreeSelectionModel = new cwItemSelectionModel(RegionTreeModel, this);

    //Setup the loop closer
    LinePlotManager = new cwLinePlotManager(this);
    LinePlotManager->setRegion(Region);

    //Setup the scrap manager
    ScrapManager = new cwScrapManager(this);
    ScrapManager->setProject(Project);
    ScrapManager->setRegion(Region);
    ScrapManager->setLinePlotManager(LinePlotManager);

    //Setup the survey export manager
    SurveyExportManager = new cwSurveyExportManager(this);
    SurveyExportManager->setCavingRegionTreeModel(RegionTreeModel);
    SurveyExportManager->setRegionSelectionModel(RegionTreeSelectionModel);

    //Setup the survey import manager
    SurveyImportManager = new cwSurveyImportManager(this);
    SurveyImportManager->setCavingRegion(Region);
    SurveyImportManager->setUndoStack(undoStack());

    QuickView = NULL;

    QMLReloader = new cwQMLReload(this);

    License = new cwLicenseAgreement(this);
    License->setVersion(version());
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

