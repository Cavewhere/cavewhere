//Our includes
#include "cwGlobalQMLData.h"
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QGLWidget>

cwGlobalQMLData::cwGlobalQMLData(QObject *parent) :
    QObject(parent),
    DefaultTrip(new cwTrip(this)),
    DefaultTripCalibration(new cwTripCalibration(this))
{
    //Create the project, this saves and load data
    Project = new cwProject(this);
    Region = Project->cavingRegion();

    RegionTreeModel = new cwRegionTreeModel(this);
    RegionTreeModel->setCavingRegion(Region);

    //Setup the loop closer
    LinePlotManager = new cwLinePlotManager(this);
    LinePlotManager->setRegion(Region);

    //Setup the scrap manager
    ScrapManager = new cwScrapManager(this);
    ScrapManager->setProject(Project);
    ScrapManager->setRegion(Region);
}

/**
Sets mainGLWidget
*/
void cwGlobalQMLData::setGLWidget(QGLWidget* mainGLWidget) {
    if(GLWidget != mainGLWidget) {
        GLWidget = mainGLWidget;
        emit mainGLWidgetChanged();
    }
}
