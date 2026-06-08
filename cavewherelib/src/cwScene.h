/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCENE_H
#define CWSCENE_H

//Qt includes
#include <QObject>
#include <QList>
#include <QQueue>
#include <QQmlEngine>
#include <QHash>

//Our includes
#include "cwSceneUpdate.h"
#include "CaveWhereLibExport.h"
class cwRenderObject;
class cwCamera;
class cwShaderDebugger;
class cwSceneCommand;
class cwGeometryItersecter;
class cwRhiItemRenderer;
class cwEDLSettings;


/**
 * @brief The cwScene class
 */
class CAVEWHERE_LIB_EXPORT cwScene : public QObject
{
    friend class cwRhiScene;

    Q_OBJECT
    QML_NAMED_ELEMENT(Scene)

    // Live Eye-Dome Lighting tuning. Pulled by cwRhiScene::synchroize() and
    // pushed to the render-thread EDL effect (see cwEDLSettings / EDL.frag).
    Q_PROPERTY(cwEDLSettings* edl READ edl CONSTANT)

public:
    explicit cwScene(QObject *parent = 0);
    virtual ~cwScene();

    cwEDLSettings* edl() const { return m_edl; }

    void addItem(cwRenderObject* item);
    void removeItem(cwRenderObject* item);
    void updateItem(cwRenderObject* item);

    void setCamera(cwCamera* camera);
    cwCamera *camera() const;

    //For doing intersection tests
    cwGeometryItersecter* geometryItersecter() const;

    void update();

    void releaseResources();

    // Live pointers held in queues that cwRhiScene::synchroize() dereferences.
    int pendingItemCount() const;

signals:
    void cameraChanged();
    void needsRendering();

private:
    //Items to render
    QList<cwRenderObject*> m_renderingObjects;
    QList<cwRenderObject*> m_newRenderObjects;
    QList<cwRenderObject*> m_toDeleteRenderObjects;
    QSet<cwRenderObject*> m_toUpdateRenderObjects;

    //For interaction
    cwGeometryItersecter* GeometryItersecter;

    //The main camera for the viewer
    cwCamera* Camera;

    //cwSceneUpdate::Flag flags
    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;

    // Live EDL tuning; owned here, read by cwRhiScene::synchroize().
    cwEDLSettings* m_edl;

};




#endif // CWSCENE_H
