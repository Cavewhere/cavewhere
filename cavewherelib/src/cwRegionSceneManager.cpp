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
#include "cwRenderLinePlot.h"

#include "cwCavingRegion.h"

cwRegionSceneManager::cwRegionSceneManager(QObject *parent) :
    QObject(parent),
    Scene(new cwScene(this)),
    Region(nullptr)
{

//    Terrain = new cwGLTerrain();
//    Terrain->setNumberOfLevels(10);
    //    connect(Terrain, SIGNAL(redraw()), SLOT(updateGL()));

    m_linePlot = new cwRenderLinePlot();
    Scraps = new cwGLScraps();
    Plane = new cwRenderGridPlane();

//    Terrain->setScene(scene());
    m_linePlot->setScene(scene());
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
