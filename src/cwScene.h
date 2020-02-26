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
#include <QOpenGLFunctions>
class QPainter;

//Our includes
class cwGLObject;
class cwCamera;
class cwShaderDebugger;
class cwSceneCommand;
class cwGeometryItersecter;

/**
 * @brief The cwScene class
 *
 * This class manages OpenGL scene resources.
 */
class cwScene : public QObject, public QOpenGLFunctions
{
    Q_OBJECT

    Q_PROPERTY(cwShaderDebugger* shaderDebugger READ shaderDebugger NOTIFY shaderDebuggerChanged)

public:
    explicit cwScene(QObject *parent = 0);
    virtual ~cwScene();

    void paint();

    void addItem(cwGLObject* item);
    void removeItem(cwGLObject* item);

    void addSceneCommand(cwSceneCommand* command);

    void setCamera(cwCamera* camera);
    cwCamera *camera() const;

    //For doing intersection tests
    cwGeometryItersecter* geometryItersecter() const;

    cwShaderDebugger* shaderDebugger() const;

    void update();

    void checkForGLError(const QByteArray &location);
signals:
    void shaderDebuggerChanged();
    void needsRendering();

public slots:

private:
    //Items to render
    QList<cwGLObject*> RenderingObjects;

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

inline cwShaderDebugger *cwScene::shaderDebugger() const
{
    return ShaderDebugger;
}



#endif // CWSCENE_H
