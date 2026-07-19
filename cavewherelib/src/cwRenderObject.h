/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBJECT_H
#define CWGLOBJECT_H

//Qt includes
#include <QObject>
#include <QOpenGLFunctions>
#include <QPointer>
class QOpenGLShaderProgram;

//Our includes
class cwCamera;
class cwShaderDebugger;
class cwRhiViewer;
class cwScene;
class cwUpdateDataCommand;
class cwRHIObject;
class cwRhiItemRenderer;
class cwGeometryItersector;
class cwSceneVisibility;
#include "cwScene.h"
#include "cwRenderObjectId.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwRenderObject : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

    friend class cwRhiScene;
    friend class cwScene;

public:
    cwRenderObject(QObject* parent = nullptr);
    ~cwRenderObject();

    void setScene(cwScene *scene);
    cwScene *scene() const;

    // Stable, process-unique identity assigned at construction. Used instead of
    // the raw `this` pointer to correlate a render object with its cwRHIObject
    // across the cwScene / cwRhiScene queues: a freed object's address can be
    // reused by a new object, but ids never repeat (issue #512).
    cwRenderObjectId renderObjectId() const { return m_renderObjectId; }

    cwGeometryItersecter* geometryItersecter() const;

    // The scene's visibility store, or null before scene attach. Facade
    // setters publish through this; pre-attach state lands via
    // publishVisibility() when cwScene::addItem seeds the store.
    cwSceneVisibility* sceneVisibility() const;

    cwCamera* camera() const;

    void update();

    bool isVisible() const;
    void setVisible(bool newVisible);

signals:
    void sceneChange();

    void visibleChanged();

protected:
    virtual cwRHIObject* createRHIObject() { return nullptr; }
    // virtual cwSceneUpdate::Flag updateOnFlags() const { return cwSceneUpdate::Flag::None; }

    // Publish this object's full authoring visibility state into the scene's
    // store. cwScene::addItem calls it at attach, making it the single seed
    // path for state set before the scene was wired. Idempotent — republishing
    // an unchanged state is a store no-op. Overrides publish their sub-item /
    // mask state on top of the base's object flag.
    virtual void publishVisibility();

private:
    // Sever the back-pointer to the scene without touching it. cwScene calls this on
    // each render-object child while destructing, so that when ~QObject later deletes
    // those children, ~cwRenderObject sees a null scene and skips the removeItem()
    // that would otherwise reach into the half-destroyed scene (see ~cwScene()).
    void detachFromScene() { m_scene = nullptr; }

    cwScene* m_scene;

    const cwRenderObjectId m_renderObjectId;

    bool m_visible = true;
};



/**
 * @brief cwRenderObject::scene
 * @returns The scene that is resposible for this object
 */
inline cwScene *cwRenderObject::scene() const
{
    return m_scene;
}

inline bool cwRenderObject::isVisible() const
{
    return m_visible;
}




#endif // CWGLOBJECT_H
