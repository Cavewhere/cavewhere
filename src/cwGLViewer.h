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
#include <QPointer>
class QGLWidget;

//Our includes
#include "cwCamera.h"
class cwMouseEventTransition;
class cwGLShader;
class cwShaderDebugger;
class cwCavingRegion;
class cwCave;
class cwScene;
#include <cwStation.h>
#include <cwRegularTile.h>
#include <cwEdgeTile.h>
#include <cwCollisionRectKdTree.h>


class cwGLViewer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(cwCamera* camera READ camera NOTIFY cameraChanged)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)

public:
    explicit cwGLViewer(QQuickItem *parent = 0);
    ~cwGLViewer();

//    cwGeometryItersecter* geometryItersecter() const;
    cwCamera* camera() const;

    cwScene* scene() const;
    void setScene(cwScene* scene);

    void paint(QPainter *painter);
signals:
    void glWidgetChanged();
    void cameraChanged();
    void sceneChanged();

protected:
//    virtual void initializeGL() {}

    //The main camera for the viewer
    cwCamera* Camera;

    //For Painting
    QPointer<cwScene> Scene;

//    bool Initialized;

//    virtual QSGNode * updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *data);

protected slots:
    virtual void resizeGL() {}
    void updateRenderer();

private slots:
    void privateResizeGL();

};

inline cwCamera* cwGLViewer::camera() const { return Camera; }
inline void cwGLViewer::updateRenderer() { update(); }

//inline cwGeometryItersecter *cwGLRenderer::geometryItersecter() const
//{
//    return GeometryItersecter;
//}



#endif // CWGLRENDERER_H
