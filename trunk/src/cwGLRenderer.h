#ifndef CWGLRENDERER_H
#define CWGLRENDERER_H

//GLEW includes
#include <GL/glew.h>

//Qt includes
#include <QDeclarativeItem>
#include <QGLBuffer>
#include <QQuaternion>
#include <QStateMachine>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGLShaderProgram>
#include <QGLFramebufferObject>
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


class cwGLRenderer : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(QGLWidget* glWidget READ glWidget WRITE setGLWidget NOTIFY glWidgetChanged)
    Q_PROPERTY(cwCamera* camera READ camera NOTIFY cameraChanged)

public:
    explicit cwGLRenderer(QDeclarativeItem *parent = 0);
    ~cwGLRenderer();

    QVector3D unProject(QPoint point);

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

    cwCamera* camera() const;

signals:
    void glWidgetChanged();
    void cameraChanged();


public slots:
    void setGLWidget(QGLWidget* widget);
    QGLWidget* glWidget();


protected:
    virtual void initializeGL() {}

    QGLWidget* GLWidget; //This is so we make current when setting up the object

    //Framebuffer for renderering
    QGLFramebufferObject* MultiSampleFramebuffer;

    //The framebuffer that the render buffer will be blit to
    GLuint TextureFramebuffer;
    GLuint ColorTexture;
    GLuint DepthTexture;
    bool HasBlit;

    //Shaders for testing
    cwShaderDebugger* ShaderDebugger;

    //The main camera for the viewer
    cwCamera* Camera;

    //For querying the depth buffer in the renderer
    float sampleDepthBuffer(QPoint point);

    /**
      \brief This is called by the paint in this functon

      The subclass should reimplement this function with opengl rendering commands.  All these
      rendering commands will be rendered to the framebuffer
      */
    virtual void paintFramebuffer() { qDebug() << "Bad paint"; }

protected slots:
    virtual void resizeGL() {}
    void updateRenderer();

private:
    void copyRenderFramebufferToTextures();
    void renderTextureFramebuffer();

    void privateInitializeGL();

private slots:
    void privateResizeGL();

};

inline QGLWidget* cwGLRenderer::glWidget() { return GLWidget; }
inline cwCamera* cwGLRenderer::camera() const { return Camera; }
inline void cwGLRenderer::updateRenderer() { update(); }

#endif // CWGLRENDERER_H
