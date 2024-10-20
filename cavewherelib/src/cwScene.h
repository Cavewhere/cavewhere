/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCENE_H
#define CWSCENE_H

//Qt includes
#include "cwGLObject.h"
#include <QObject>
#include <QList>
#include <QQueue>
#include <QOpenGLFunctions>
#include <QQmlEngine>
class QRhiCommandBuffer;
class QPainter;

//Our includes
class cwGLObject;
class cwCamera;
class cwShaderDebugger;
class cwSceneCommand;
class cwGeometryItersecter;
class cwRhiItemRenderer;

/**
 * @brief The cwScene class
 *
 * This class manages OpenGL scene resources.
 */
class cwScene : public QObject, public QOpenGLFunctions
{
    friend class cwSceneRenderer;

    Q_OBJECT
    QML_NAMED_ELEMENT(Scene)

    // Q_PROPERTY(cwShaderDebugger* shaderDebugger READ shaderDebugger NOTIFY shaderDebuggerChanged)

public:
    explicit cwScene(QObject *parent = 0);
    virtual ~cwScene();

    // void paint();

    void addItem(cwGLObject* item);
    void removeItem(cwGLObject* item);

    // void addSceneCommand(cwSceneCommand* command);

    void setCamera(cwCamera* camera);
    cwCamera *camera() const;

    //For doing intersection tests
    cwGeometryItersecter* geometryItersecter() const;

    // cwShaderDebugger* shaderDebugger() const;

    void update();

    // void checkForGLError(const QByteArray &location);

    void releaseResources();

signals:
    void shaderDebuggerChanged();
    void needsRendering();

public slots:

private:
    //Items to render
    QList<cwGLObject*> m_renderingObjects;
    QList<cwGLObject*> m_newRenderObjects;
    QList<cwGLObject*> m_toDeleteRenderObjects;

    //For interaction
    cwGeometryItersecter* GeometryItersecter;

    //Shaders for testing
    cwShaderDebugger* ShaderDebugger;

    //The main camera for the viewer
    cwCamera* Camera;

    //All the Queued scene command
    QQueue<cwSceneCommand*> CommandQueue;
    bool ExcutingCommands;

    void excuteSceneCommands();

};

class cwSceneRenderer {
public:
    void initialize();
    void synchroize(cwScene* scene, cwRhiItemRenderer* renderer);
    void render(QRhiCommandBuffer *cb, cwRhiItemRenderer* renderer);

private:
    struct RenderObject {
        cwGLObject* glObject;
        cwRHIObject* rhiObject;
    };

    QList<RenderObject> m_rhiObjectsToInitilize;
    QList<RenderObject> m_rhiObjects;
    QList<RenderObject> m_rhiNeedResourceUpdate;
//     cwScene* scene;

};

// inline cwShaderDebugger *cwScene::shaderDebugger() const
// {
//     return ShaderDebugger;
// }



#endif // CWSCENE_H
