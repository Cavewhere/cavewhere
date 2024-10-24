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
class cwGLScraps;
class cwRenderGridPlane;
class cwCavingRegion;
class cwGLTerrain;
class cwRenderObject;
class cwRenderLinePlot;

class cwRegionSceneManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionSceneManager)

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwRenderLinePlot* linePlot READ linePlot NOTIFY linePlotChanged)
    Q_PROPERTY(cwGLScraps* scraps READ scraps NOTIFY scrapsChanged)
    Q_PROPERTY(cwScene* scene READ scene NOTIFY sceneChanged)

public:
    explicit cwRegionSceneManager(QObject *parent = 0);

    cwRenderLinePlot* linePlot();
    cwGLScraps* scraps() const;

    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    cwScene* scene() const;

signals:
    void sceneChanged();
    void cavingRegionChanged();
    void linePlotChanged();
    void scrapsChanged();

public slots:

private:
    cwScene* Scene; //!<

    //The terrain that's rendered
    cwGLTerrain* Terrain;
    cwRenderLinePlot* m_linePlot;
    cwGLScraps* Scraps;
    cwRenderGridPlane* Plane;

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

/**
 * @brief cwScene::scraps
 * @return Returns the GL scraps of the scene
 */
inline cwGLScraps* cwRegionSceneManager::scraps() const
{
    return Scraps;
}


#endif // CWREGIONSCENEMANAGER_H
