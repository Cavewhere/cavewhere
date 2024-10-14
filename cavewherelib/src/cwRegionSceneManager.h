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
class cwGLGridPlane;
class cwCavingRegion;
class cwGLTerrain;
class cwGLObject;

class cwRegionSceneManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionSceneManager)

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwGLLinePlot* linePlot READ linePlot NOTIFY linePlotChanged)
    Q_PROPERTY(cwGLScraps* scraps READ scraps NOTIFY scrapsChanged)
    Q_PROPERTY(cwScene* scene READ scene NOTIFY sceneChanged)

public:
    explicit cwRegionSceneManager(QObject *parent = 0);

    cwGLLinePlot* linePlot();
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
    cwGLLinePlot* LinePlot;
    cwGLScraps* Scraps;
    cwGLGridPlane* Plane;

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
inline cwGLLinePlot* cwRegionSceneManager::linePlot() { return LinePlot; }

/**
 * @brief cwScene::scraps
 * @return Returns the GL scraps of the scene
 */
inline cwGLScraps* cwRegionSceneManager::scraps() const
{
    return Scraps;
}


#endif // CWREGIONSCENEMANAGER_H
