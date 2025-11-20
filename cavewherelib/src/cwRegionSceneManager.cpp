/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwRegionSceneManager.h"
#include "cwScene.h"
#include "cwRenderGridPlane.h"
#include "cwRenderLinePlot.h"
#include "cwRenderRadialGradient.h"
#include "cwRenderTexturedItems.h"


#include "cwCavingRegion.h"

cwRegionSceneManager::cwRegionSceneManager(QObject *parent) :
    QObject(parent),
    Scene(new cwScene(this)),
    Region(nullptr)
{

    //Renders the background, should be drawn first
    m_background = new cwRenderRadialGradient();
    m_linePlot = new cwRenderLinePlot();
    m_plane = new cwRenderGridPlane();
    m_items = new cwRenderTexturedItems();

    //    Terrain->setScene(scene());

    m_background->setScene(scene());
    m_linePlot->setScene(scene());
    m_items->setScene(scene());

    //Render the plane last
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

void cwRegionSceneManager::setCapturing(bool newCapturing)
{
    if (m_capturing == newCapturing) {
        return;
    }
    m_capturing = newCapturing;
    m_background->setVisible(!m_capturing);
    m_linePlot->setVisible(!m_capturing);
    m_plane->setVisible(!m_capturing);
    emit capturingChanged();
}



