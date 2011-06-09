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
#include <cwRegularTile.h>
#include <cwEdgeTile.h>

class cwGLRenderer : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(QGLWidget* glWidget READ glWidget WRITE setGLWidget NOTIFY glWidgetChanged)

public:
    explicit cwGLRenderer(QDeclarativeItem *parent = 0);

    QVector3D unProject(QPoint point);

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

signals:
    void glWidgetChanged();

public slots:
    void setGLWidget(QGLWidget* widget);
    QGLWidget* glWidget();

    void startPanning(QPoint currentMousePos);
    void pan(QPoint currentMousePos);

    void startRotating(QPoint currentMousePos);
    void rotate(QPoint currentMousePos);
private slots:
    //Interaction events
    void resetView();

    void resizeGL();
protected:
    void initializeGL();

    void paintGL();

    virtual void wheelEvent(QGraphicsSceneWheelEvent *event);
    void zoom(QGraphicsSceneWheelEvent* event);

private:
    QGLWidget* GLWidget; //This is so we make current when setting up the object

    //Framebuffer for renderering
    QGLFramebufferObject* MultiSampleFramebuffer;
    GLuint TextureFramebuffer;

    GLuint ColorTexture;
    GLuint DepthTexture;

    //The main camera for the viewer
    cwCamera* Camera;

    //For interaction
    QVector3D LastMouseGlobalPosition; //For panning
    QPointF LastMousePosition; //For rotation
    QQuaternion CurrentRotation;
    float Pitch;
    float Azimuth;

    //The terrain that's rendered
    cwGLTerrain* Terrain;

    //Shaders for testing
    cwShaderDebugger* ShaderDebugger;

    float sampleDepthBuffer(QPoint point);
};

inline QGLWidget* cwGLRenderer::glWidget() { return GLWidget; }

#endif // CWGLRENDERER_H
