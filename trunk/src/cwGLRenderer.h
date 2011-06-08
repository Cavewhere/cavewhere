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

private slots:
    //Interaction events
    void startPanning(QMouseEvent* event);
    void pan(QMouseEvent* event);

    void startRotating(QMouseEvent* event);
    void rotate(QMouseEvent* event);
    void zoom(QWheelEvent* event);

    void resetView();

    void resizeGL();
protected:
    void initializeGL();

    void paintGL();

private:
    QGLWidget* GLWidget; //This is so we make current when setting up the object

    QGLFramebufferObject* MultiSampleFramebuffer;
    QGLFramebufferObject* TextureFramebuffer;
    //GLuint Texture;

    //The main camera for the viewer
    cwCamera* Camera;

    //Interaction statemachine
    QStateMachine* InteractionMachine;
    QState* DefaultInteractionState;

    //For interaction
    QVector3D LastMouseGlobalPosition; //For panning
    QPoint LastMousePosition; //For rotation
    QQuaternion CurrentRotation;
    float Pitch;
    float Azimuth;

    //The terrain that's rendered
    cwGLTerrain* Terrain;

    //Shaders for testing
    cwShaderDebugger* ShaderDebugger;

    float sampleDepthBuffer(QPoint point);

    void setupCamera();
    void setupInteractionStateMachine();
    void setupPan();
    void setupRotate();
    void setupZoom();

};

inline QGLWidget* cwGLRenderer::glWidget() { return GLWidget; }

#endif // CWGLRENDERER_H
