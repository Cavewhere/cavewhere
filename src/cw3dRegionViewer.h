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

class cw3dRegionViewer : public cwGLRenderer
{
    Q_OBJECT
    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwGLLinePlot* linePlot READ linePlot)
    Q_PROPERTY(cwGLScraps* scraps READ scraps)

public:
    cw3dRegionViewer(QQuickItem* parent = 0);

    virtual void paint(QPainter* painter);
    Q_INVOKABLE virtual void initializeGL();
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

    //The terrain that's rendered
    cwGLTerrain* Terrain;
    cwGLLinePlot* LinePlot;
    cwGLScraps* Scraps;
    cwGLGridPlane* Plane;

    QTimer* RotationInteractionTimer;
    QPoint TimeoutRotationPosition;

    QTimer* ZoomInteractionTimer;
    QPoint ZoomPosition;
    int ZoomDelta;

    QTimer* TranslateTimer;
    QPoint TranslatePosition;

    //For rendering label
    cwCavingRegion* Region;

    void renderStationLabels(QPainter* painter);
    void renderStationLabels(QPainter* painter, cwCave* cave);

    void setupInteractionTimers();

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
