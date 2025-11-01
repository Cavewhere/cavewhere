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

    Q_PROPERTY(bool capturing READ isCapturing WRITE setCapturing NOTIFY capturingChanged FINAL)

public:
    explicit cwRegionSceneManager(QObject *parent = 0);

    cwRenderLinePlot* linePlot();
    cwRenderTexturedItems* scraps() const { return m_items; }

    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    cwScene* scene() const;

    bool isCapturing() const;
    void setCapturing(bool newCapturing);

    cwRenderGridPlane *gridPlane() const { return m_plane; }

    cwRenderTexturedItems *items() const;

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
    cwRenderTexturedItems* m_items;
    cwRenderGridPlane* m_plane;
    cwRenderRadialGradient* m_background;

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

inline bool cwRegionSceneManager::isCapturing() const
{
    return m_capturing;
}

inline cwRenderTexturedItems *cwRegionSceneManager::items() const
{
    return m_items;
}



#endif // CWREGIONSCENEMANAGER_H
