/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWREGIONSCENEMANAGER_H
#define CWREGIONSCENEMANAGER_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>

//Our includes
#include "cwRenderObjectId.h"
class cwScene;
class cwGLLinePlot;
class cwRenderGridPlane;
class cwCavingRegion;
class cwGLTerrain;
class cwKeywordItemModel;
class cwLazLayersSceneNode;
class cwRenderObject;
class cwRenderLinePlot;
class cwRenderScraps;
#include "cwRenderRadialGradient.h"
#include "cwRenderTexturedItems.h"
#include "cwRenderGridPlane.h"

class cwRegionSceneManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionSceneManager)

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwRenderLinePlot* linePlot READ linePlot NOTIFY linePlotChanged)
    Q_PROPERTY(cwRenderTexturedItems* scraps READ scraps NOTIFY scrapsChanged)
    Q_PROPERTY(cwScene* scene READ scene NOTIFY sceneChanged)
    Q_PROPERTY(cwRenderGridPlane* gridPlane READ gridPlane CONSTANT)
    Q_PROPERTY(cwRenderTexturedItems* items READ items CONSTANT)
    Q_PROPERTY(cwRenderRadialGradient* background READ background CONSTANT)
    Q_PROPERTY(cwLazLayersSceneNode* lazLayersSceneNode READ lazLayersSceneNode CONSTANT)

public:
    explicit cwRegionSceneManager(QObject *parent = 0);

    cwRenderLinePlot* linePlot();
    cwRenderTexturedItems* scraps() const { return m_items; }

    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    cwScene* scene() const;

    // The render objects a map-export capture hides (background gradient, line
    // plot, grid plane), by stable id. Fed to cwOffscreenRenderParameters::
    // hiddenObjectIds per export tile so the capture renders transparent-backed
    // without mutating the live view's visibility.
    QSet<cwRenderObjectId> captureHiddenObjectIds() const;

    cwRenderGridPlane *gridPlane() const { return m_plane; }

    cwRenderTexturedItems *items() const;

    cwLazLayersSceneNode* lazLayersSceneNode() const { return m_lazLayers; }

    cwRenderRadialGradient* background() const;

signals:
    void sceneChanged();
    void cavingRegionChanged();
    void linePlotChanged();
    void scrapsChanged();

public slots:

private:
    void updateGridPlaneElevation();

    cwScene* Scene; //!<

    //The terrain that's rendered
    cwGLTerrain* Terrain;
    cwRenderLinePlot* m_linePlot;
    cwRenderTexturedItems* m_items;
    cwRenderGridPlane* m_plane;
    cwRenderRadialGradient* m_background;

    cwLazLayersSceneNode* m_lazLayers;

    //For rendering label
    QPointer<cwCavingRegion> Region;
};

/**
 * @brief cwRegionSceneManager::scene
 * @return Returns the scene
 */
inline cwScene* cwRegionSceneManager::scene() const {
    return Scene;
}

/**
  \brief Returns the object that renderes the line plot
  */
inline cwRenderLinePlot* cwRegionSceneManager::linePlot() { return m_linePlot; }

inline cwRenderTexturedItems *cwRegionSceneManager::items() const
{
    return m_items;
}

inline cwRenderRadialGradient* cwRegionSceneManager::background() const
{
    return m_background;
}



#endif // CWREGIONSCENEMANAGER_H
