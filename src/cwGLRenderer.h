/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLRENDERER_H
#define CWGLRENDERER_H

//Qt includes
#include <QOpenGLBuffer>
#include <QQuaternion>
#include <QStateMachine>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QQuickPaintedItem>
class QGLWidget;

//Our includes
#include "cwCamera.h"
class cwMouseEventTransition;
class cwGLShader;
class cwShaderDebugger;
class cwCavingRegion;
class cwCave;
#include <cwStation.h>
#include <cwRegularTile.h>
#include <cwEdgeTile.h>
#include <cwCollisionRectKdTree.h>


class cwGLRenderer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(cwCamera* camera READ camera NOTIFY cameraChanged)
    Q_PROPERTY(cwShaderDebugger* shaderDebugger READ shaderDebugger NOTIFY shaderDebuggerChanged)

public:
    explicit cwGLRenderer(QQuickItem *parent = 0);
    ~cwGLRenderer();

    cwShaderDebugger* shaderDebugger() const;
    cwGeometryItersecter* geometryItersecter() const;
    cwCamera* camera() const;

signals:
    void glWidgetChanged();
    void cameraChanged();
    void shaderDebuggerChanged();

protected:
    virtual void initializeGL() {}

    //Shaders for testing
    cwShaderDebugger* ShaderDebugger;

    //The main camera for the viewer
    cwCamera* Camera;

    //For interaction
    cwGeometryItersecter* GeometryItersecter;

    bool Initialized;

    virtual QSGNode * updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *data);

protected slots:
    virtual void resizeGL() {}
    void updateRenderer();

private slots:
    void privateResizeGL();

};

inline cwCamera* cwGLRenderer::camera() const { return Camera; }
inline void cwGLRenderer::updateRenderer() { update(); }

inline cwShaderDebugger *cwGLRenderer::shaderDebugger() const
{
    return ShaderDebugger;
}

inline cwGeometryItersecter *cwGLRenderer::geometryItersecter() const
{
    return GeometryItersecter;
}

#endif // CWGLRENDERER_H
