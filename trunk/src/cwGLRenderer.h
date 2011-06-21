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
    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwGLLinePlot* linePlot READ linePlot)

public:
    explicit cwGLRenderer(QDeclarativeItem *parent = 0);

    QVector3D unProject(QPoint point);

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

signals:
    void glWidgetChanged();
    void cavingRegionChanged();

public slots:
    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    void setGLWidget(QGLWidget* widget);
    QGLWidget* glWidget();

    cwGLLinePlot* linePlot();

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

    /**
      \brief This is used to render text labels correctly

      This will transform the points in a multi thread manor.
      */
    class TransformPoint {
    public:
        typedef QVector3D result_type;

        TransformPoint(QMatrix4x4 modelViewProjection, QRect viewport) {
            ModelViewProjection = modelViewProjection;
            Viewport = viewport;
        }

        /**
          \brief Transforms the point
          */
        QVector3D operator()(QWeakPointer<cwStation> station);

    private:
        QMatrix4x4 ModelViewProjection;
        QRect Viewport;
    };

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
    cwGLLinePlot* LinePlot;

    //Shaders for testing
    cwShaderDebugger* ShaderDebugger;

    //For rendering label
    cwCavingRegion* Region;
    cwCollisionRectKdTree LabelKdTree;

    float sampleDepthBuffer(QPoint point);

    void renderStationLabels(QPainter* painter);
    void renderStationLabels(QPainter* painter, cwCave* cave);
};

inline QGLWidget* cwGLRenderer::glWidget() { return GLWidget; }

/**
  \brief Returns the object that renderes the line plot
  */
inline cwGLLinePlot* cwGLRenderer::linePlot() { return LinePlot; }

/**
  \brief Returns the caving region that's owned by the renderer
  */
inline cwCavingRegion* cwGLRenderer::cavingRegion() const {
    return Region;
}

#endif // CWGLRENDERER_H
