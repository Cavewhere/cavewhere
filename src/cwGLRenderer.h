#ifndef CWGLRENDERER_H
#define CWGLRENDERER_H

//GLEW includes
//#include <GL/glew.h>

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

public:
    explicit cwGLRenderer(QQuickItem *parent = 0);
    ~cwGLRenderer();

//    QVector3D unProject(QPoint point);

    cwCamera* camera() const;

signals:
    void glWidgetChanged();
    void cameraChanged();


//public slots:
//    void setGLWidget(QGLWidget* widget);
//    QGLWidget* glWidget();


protected:
    virtual void initializeGL() {}

//    QGLWidget* GLWidget; //This is so we make current when setting up the object

    //Shaders for testing
    cwShaderDebugger* ShaderDebugger;

    //The main camera for the viewer
    cwCamera* Camera;

    bool Initialized;

    //For querying the depth buffer in the renderer
    float sampleDepthBuffer(QPoint point);

    virtual QSGNode * updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *data);

//    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);

protected slots:
    virtual void resizeGL() {}
    void updateRenderer();

private slots:
    void privateResizeGL();

};

inline cwCamera* cwGLRenderer::camera() const { return Camera; }
inline void cwGLRenderer::updateRenderer() { update(); }

#endif // CWGLRENDERER_H
