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
#include "cwGeometryItersecter.h"
#include "cwScene.h"

class cwRenderObject : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT

    friend class cwSceneRenderer;

public:
    cwRenderObject(QObject* parent = nullptr);
    ~cwRenderObject();

    //These methods should only be called in the rendering thread
    [[deprecated("Should be remove. No longer used.")]]
    virtual void initialize() {}
    [[deprecated("Should be remove. No longer used.")]]
    virtual void releaseResources() {}
    [[deprecated("Should be remove. No longer used.")]]
    virtual void draw() {}

    void setScene(cwScene *scene);
    cwScene *scene() const;

    cwGeometryItersecter* geometryItersecter() const;

    cwCamera* camera() const;

    [[deprecated("Use update instead.")]]
    void markDataAsDirty();
    void update();

signals:
    void sceneChange();

protected:
    virtual cwRHIObject* createRHIObject() { return nullptr; }
    // virtual cwSceneUpdate::Flag updateOnFlags() const { return cwSceneUpdate::Flag::None; }

private:
    cwScene* m_scene;


};



/**
 * @brief cwRenderObject::scene
 * @returns The scene that is resposible for this object
 */
inline cwScene *cwRenderObject::scene() const
{
    return m_scene;
}




#endif // CWGLOBJECT_H
