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
#include "cwItemSelectionModel.h"

//Qt includes
#include <QGLWidget>
#include <QItemSelectionModel>

cwRootData::cwRootData(QObject *parent) :
    QObject(parent),
    DefaultTrip(new cwTrip(this)),
    DefaultTripCalibration(new cwTripCalibration(this))
{
    //Create the project, this saves and load data
    Project = new cwProject(this);
    Region = Project->cavingRegion();

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

    //Setup the survex export manager
    SurveyExportManager = new cwSurveyExportManager(this);
    SurveyExportManager->setCavingRegionTreeModel(RegionTreeModel);
    SurveyExportManager->setRegionSelectionModel(RegionTreeSelectionModel);


}

/**
Sets mainGLWidget
*/
void cwRootData::setGLWidget(QGLWidget* mainGLWidget) {
    if(GLWidget != mainGLWidget) {
        GLWidget = mainGLWidget;
        emit mainGLWidgetChanged();
    }
}
