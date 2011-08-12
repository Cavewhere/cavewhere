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
class cwCamera;
class cwMouseEventTransition;
class cwGLShader;
class cwShaderDebugger;
class cwGLTerrain;
class cwGLLinePlot;
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

public:
    explicit cwGLRenderer(QDeclarativeItem *parent = 0);
    ~cwGLRenderer();

    QVector3D unProject(QPoint point);

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

signals:
    void glWidgetChanged();


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
    virtual void paintFramebuffer() {}

protected slots:
    virtual void resizeGL() {}

private:
    void copyRenderFramebufferToTextures();
    void renderTextureFramebuffer();

    void privateInitializeGL();

private slots:
    void privateResizeGL();

};

inline QGLWidget* cwGLRenderer::glWidget() { return GLWidget; }


#endif // CWGLRENDERER_H
