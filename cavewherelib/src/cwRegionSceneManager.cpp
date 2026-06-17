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
#include "cwLazLayersSceneNode.h"
#include "cwLazLayerModel.h"

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

    m_background->setScene(scene());
    m_linePlot->setScene(scene());
    m_items->setScene(scene());

    //Render the plane last
    m_plane->setScene(scene());

    m_lazLayers = new cwLazLayersSceneNode(this);
    m_lazLayers->setScene(scene());

    //Keep the grid plane sitting at the lowest cave depth (issue #534). The
    //plane's default elevation already covers the no-geometry state, so only the
    //signal wiring is needed here.
    connect(m_linePlot, &cwRenderLinePlot::geometryChanged,
            this, &cwRegionSceneManager::updateGridPlaneElevation);
}

/**
  \brief Snaps the grid plane's elevation to the lowest depth of the survey

  Only the elevation (z) tracks the line plot; the plane stays horizontal and
  centred on the world origin in x/y. Empty geometry leaves min/max inverted
  (the setGeometry sentinels), so the plane is left at its default until there
  is real geometry. setPlane suppresses the write when the elevation is
  unchanged.
  */
void cwRegionSceneManager::updateGridPlaneElevation()
{
    if(m_linePlot->minZValue() > m_linePlot->maxZValue()) {
        return;
    }

    QPlane3D plane = m_plane->plane();
    QVector3D origin = plane.origin();
    origin.setZ(m_linePlot->minZValue());
    plane.setOrigin(origin);
    m_plane->setPlane(plane);
}

/**
  \brief Sets the caving region for the renderer
  */
void cwRegionSceneManager::setCavingRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;
        m_lazLayers->setLazLayerModel(region ? region->lazLayers() : nullptr);
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
    m_lazLayers->setVisibleForAll(!m_capturing);
    emit capturingChanged();
}
