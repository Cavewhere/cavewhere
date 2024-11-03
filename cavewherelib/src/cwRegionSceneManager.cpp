/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwRegionSceneManager.h"
#include "cwScene.h"
#include "cwGLTerrain.h"
#include "cwRenderScraps.h"
#include "cwRenderGridPlane.h"
#include "cwRenderLinePlot.h"
#include "cwRenderRadialGradient.h"


#include "cwCavingRegion.h"

cwRegionSceneManager::cwRegionSceneManager(QObject *parent) :
    QObject(parent),
    Scene(new cwScene(this)),
    Region(nullptr)
{

    //    Terrain = new cwGLTerrain();
    //    Terrain->setNumberOfLevels(10);
    //    connect(Terrain, SIGNAL(redraw()), SLOT(updateGL()));

    //Renders the background, should be drawn first

    m_background = new cwRenderRadialGradient();
    m_linePlot = new cwRenderLinePlot();
    m_scraps = new cwRenderScraps();
    m_plane = new cwRenderGridPlane();

    //    Terrain->setScene(scene());

    m_background->setScene(scene());
    m_linePlot->setScene(scene());
    m_scraps->setScene(scene());
    m_plane->setScene(scene());

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
