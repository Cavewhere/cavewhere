/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwRegionSceneManager.h"
#include "cwScene.h"
#include "cwGLTerrain.h"
#include "cwGLScraps.h"
#include "cwGLGridPlane.h"
#include "cwGLLinePlot.h"
#include "cwCavingRegion.h"

cwRegionSceneManager::cwRegionSceneManager(QObject *parent) :
    QObject(parent),
    Scene(new cwScene(this)),
    Region(nullptr)
{

//    Terrain = new cwGLTerrain();
//    Terrain->setNumberOfLevels(10);
    //    connect(Terrain, SIGNAL(redraw()), SLOT(updateGL()));

    LinePlot = new cwGLLinePlot();
    Scraps = new cwGLScraps();
    Plane = new cwGLGridPlane();

//    Terrain->setScene(scene());
    // LinePlot->setScene(scene());
    // Scraps->setScene(scene());
    Plane->setScene(scene());

}

/**
  \brief Sets the caving region for the renderer
  */
void cwRegionSceneManager::setCavingRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;
        emit cavingRegionChanged();
    }
}

/**
  \brief Returns the caving region that's owned by the renderer
  */
cwCavingRegion* cwRegionSceneManager::cavingRegion() const {
    return Region;
}
