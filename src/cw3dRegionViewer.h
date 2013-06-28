#ifndef CW3DREGIONVIEWER_H
#define CW3DREGIONVIEWER_H

//Qt includes
#include <QVector3D>
#include <QMatrix4x4>
#include <QPlane3D>

//Our includes
#include "cwGLRenderer.h"
class cwGLTerrain;
class cwGLLinePlot;
class cwGLScraps;
class cwGLGridPlane;
class cwGeometryItersecter;
class cwGLCompass;

class cw3dRegionViewer : public cwGLRenderer
{
    Q_OBJECT
    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwGLLinePlot* linePlot READ linePlot)
    Q_PROPERTY(cwGLScraps* scraps READ scraps)
    Q_PROPERTY(QQuaternion rotation READ rotation NOTIFY rotationChanged)

public:

    enum ProjectionType {
        Prespective,
        Orthoganal,
        Unknown
    };

    cw3dRegionViewer(QQuickItem* parent = 0);

    virtual void paint(QPainter* painter);
    Q_INVOKABLE virtual void initializeGL();

    cwProjection orthoProjection() const;
    cwProjection perspectiveProjection() const;

    QQuaternion rotation() const;

public slots:
    cwGLLinePlot* linePlot();
    cwGLScraps* scraps() const;

    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    void startPanning(QPoint currentMousePos);
    void pan(QPoint currentMousePos);

    void startRotating(QPoint currentMousePos);
    void rotate(QPoint currentMousePos);

    void zoom(QPoint position, int delta);

signals:
    void cavingRegionChanged();
    void resized();

    void rotationChanged();

private slots:
    //Interaction events
    void resetView();

    void rotateLastPosition();
    void zoomLastPosition();
    void translateLastPosition();

    virtual void resizeGL();

protected:

    QVector3D unProject(QPoint point);

    virtual QSGNode * 	updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);


private:
    QVector3D LastMouseGlobalPosition; //For panning
    QPlane3D PanPlane;
    QPointF LastMousePosition; //For rotation
    QQuaternion CurrentRotation;
    float Pitch;
    float Azimuth;

    const static float DefaultPitch;
    const static float DefaultAzimuth;

    //The terrain that's rendered
    cwGLTerrain* Terrain;
    cwGLLinePlot* LinePlot;
    cwGLScraps* Scraps;
    cwGLGridPlane* Plane;
    cwGLCompass* Compass;

    QTimer* RotationInteractionTimer;
    QPoint TimeoutRotationPosition;

    QTimer* ZoomInteractionTimer;
    QPoint ZoomPosition;
    int ZoomDelta;
    double ZoomLevel; //This is for orthoginal projections, This is in pixel / meter

    QTimer* TranslateTimer;
    QPoint TranslatePosition;

    //For rendering label
    cwCavingRegion* Region;

    void renderStationLabels(QPainter* painter);
    void renderStationLabels(QPainter* painter, cwCave* cave);

    void zoomPerspective();
    void zoomOrtho();

//    cwProjection orthoProjection() const;
//    cwProjection perspectiveProjection() const;

    void setupInteractionTimers();

    void setCurrentRotation(QQuaternion rotation);
    QQuaternion defaultRotation() const;

};

Q_DECLARE_METATYPE(cw3dRegionViewer*)

/**
  \brief Returns the object that renderes the line plot
  */
inline cwGLLinePlot* cw3dRegionViewer::linePlot() { return LinePlot; }

inline cwGLScraps *cw3dRegionViewer::scraps() const
{
    return Scraps;
}

/**
  \brief Returns the caving region that's owned by the renderer
  */
inline cwCavingRegion* cw3dRegionViewer::cavingRegion() const {
    return Region;
}



#endif // CW3DREGIONVIEWER_H
