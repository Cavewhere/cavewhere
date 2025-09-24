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

//Our includes
class cwScene;
class cwGLLinePlot;
class cwRenderGridPlane;
class cwCavingRegion;
class cwGLTerrain;
class cwRenderObject;
class cwRenderLinePlot;
class cwRenderScraps;
class cwRenderRadialGradient;
class cwRenderGLTF;
#include "cwRenderScraps.h"
#include "cwRenderGridPlane.h"

class cwRegionSceneManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionSceneManager)

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwRenderLinePlot* linePlot READ linePlot NOTIFY linePlotChanged)
    Q_PROPERTY(cwRenderScraps* scraps READ scraps NOTIFY scrapsChanged)
    Q_PROPERTY(cwScene* scene READ scene NOTIFY sceneChanged)
    Q_PROPERTY(cwRenderGridPlane* gridPlane READ gridPlane CONSTANT)

    Q_PROPERTY(bool capturing READ isCapturing WRITE setCapturing NOTIFY capturingChanged FINAL)


public:
    explicit cwRegionSceneManager(QObject *parent = 0);

    cwRenderLinePlot* linePlot();
    cwRenderScraps* scraps() const;

    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    cwScene* scene() const;

    bool isCapturing() const;
    void setCapturing(bool newCapturing);

    cwRenderGridPlane *gridPlane() const { return m_plane; }

signals:
    void sceneChanged();
    void cavingRegionChanged();
    void linePlotChanged();
    void scrapsChanged();

    void capturingChanged();

public slots:

private:
    cwScene* Scene; //!<

    //The terrain that's rendered
    cwGLTerrain* Terrain;
    cwRenderLinePlot* m_linePlot;
    cwRenderScraps* m_scraps;
    cwRenderGridPlane* m_plane;
    cwRenderRadialGradient* m_background;

    //For testing gltf
    cwRenderGLTF* m_gltf;

    //For rendering label
    QPointer<cwCavingRegion> Region;
    bool m_capturing;
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

/**
 * @brief cwScene::scraps
 * @return Returns the GL scraps of the scene
 */
inline cwRenderScraps* cwRegionSceneManager::scraps() const
{
    return m_scraps;
}

inline bool cwRegionSceneManager::isCapturing() const
{
    return m_capturing;
}


#endif // CWREGIONSCENEMANAGER_H
