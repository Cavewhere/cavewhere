/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwRegionSceneManager.h"
#include "cwScene.h"
#include "cwRenderScraps.h"
#include "cwRenderGridPlane.h"
#include "cwRenderLinePlot.h"
#include "cwRenderRadialGradient.h"
#include "cwRenderGLTF.h"


#include "cwCavingRegion.h"

cwRegionSceneManager::cwRegionSceneManager(QObject *parent) :
    QObject(parent),
    Scene(new cwScene(this)),
    Region(nullptr)
{

    //Renders the background, should be drawn first
    m_background = new cwRenderRadialGradient();
    m_linePlot = new cwRenderLinePlot();
    m_scraps = new cwRenderScraps();
    m_plane = new cwRenderGridPlane();

    //For testing
    m_gltf = new cwRenderGLTF();
    // m_gltf->setGLTFFilePath("/Users/cave/Desktop/lidarTest/jaws of the beast/Jaws of the Beast/trips/2019c154_-_party_fault/notes/9_15_2025 3.glb");

    // QMatrix4x4 matrix;
    // matrix.rotate(90.0, 1.0, 0.0, 0.0);
    // m_gltf->setModelMatrix(matrix);
    // m_gltf->setGLTFFilePath("/Users/cave/Downloads/9_9_2025.glb");
    // qDebug() << "Do loading!";

    //    Terrain->setScene(scene());

    m_background->setScene(scene());
    m_linePlot->setScene(scene());
    m_scraps->setScene(scene());
    m_gltf->setScene(scene());
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


