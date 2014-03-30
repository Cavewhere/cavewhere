/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CW3DREGIONVIEWER_H
#define CW3DREGIONVIEWER_H

//Qt includes
#include <QVector3D>
#include <QMatrix4x4>
#include <QPlane3D>

//Our includes
#include "cwGLViewer.h"
//class cwGLTerrain;
//class cwGLLinePlot;
//class cwGLScraps;
//class cwGLGridPlane;
class cwGeometryItersecter;
class cwOrthogonalProjection;
class cwPerspectiveProjection;

class cw3dRegionViewer : public cwGLViewer
{
    Q_OBJECT
//    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
//    Q_PROPERTY(cwGLLinePlot* linePlot READ linePlot)
//    Q_PROPERTY(cwGLScraps* scraps READ scraps)
    Q_PROPERTY(QQuaternion rotation READ rotation NOTIFY rotationChanged)
    Q_PROPERTY(double azimuth READ azimuth WRITE setAzimuth NOTIFY azimuthChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(cwOrthogonalProjection* orthoProjection READ orthoProjection CONSTANT)
    Q_PROPERTY(cwPerspectiveProjection* perspectiveProjection READ perspectiveProjection CONSTANT)

public:

    enum ProjectionType {
        Prespective,
        Orthoganal,
        Unknown
    };

    cw3dRegionViewer(QQuickItem* parent = 0);

//    virtual void paint(QPainter* painter);
//    Q_INVOKABLE virtual void initializeGL();

    void setGridPlane(const QPlane3D& plan);

    cwProjection orthoProjectionDefault() const;
    cwProjection perspectiveProjectionDefault() const;

    QQuaternion rotation() const;

    cwOrthogonalProjection* orthoProjection() const;
    cwPerspectiveProjection* perspectiveProjection() const;

    double azimuth() const;
    void setAzimuth(double azimuth);

    double pitch() const;
    void setPitch(double pitch);

public slots:
//    cwGLLinePlot* linePlot();
//    cwGLScraps* scraps() const;

//    void setCavingRegion(cwCavingRegion* region);
//    cwCavingRegion* cavingRegion() const;

    void startPanning(QPoint currentMousePos);
    void pan(QPoint currentMousePos);

    void startRotating(QPoint currentMousePos);
    void rotate(QPoint currentMousePos);


    void zoom(QPoint position, int delta);

signals:
//    void cavingRegionChanged();
    void resized();

    void rotationChanged();
    void azimuthChanged();
    void pitchChanged();

private slots:
    //Interaction events
    void resetView();

    void rotateLastPosition();
    void zoomLastPosition();
    void translateLastPosition();

    virtual void resizeGL();

protected:

    QVector3D unProject(QPoint point);

//    virtual QSGNode * 	updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);


private:
    QVector3D LastMouseGlobalPosition; //For panning
    QPlane3D PanPlane;
    QPointF LastMousePosition; //For rotation
    QQuaternion CurrentRotation;
    float Pitch;
    float Azimuth;

    QPlane3D LastDitchRotationPlane;

    const static float DefaultPitch;
    const static float DefaultAzimuth;

//    //The terrain that's rendered
//    cwGLTerrain* Terrain;
//    cwGLLinePlot* LinePlot;
//    cwGLScraps* Scraps;
//    cwGLGridPlane* Plane;

    QTimer* RotationInteractionTimer;
    QPoint TimeoutRotationPosition;

    QTimer* ZoomInteractionTimer;
    QPoint ZoomPosition;
    int ZoomDelta;
    double ZoomLevel; //This is for orthoginal projections, This is in pixel / meter

    QTimer* TranslateTimer;
    QPoint TranslatePosition;

    cwOrthogonalProjection* OrthognalProjection; //!<
    cwPerspectiveProjection* PerspectiveProjection; //!<

//    //For rendering label
//    cwCavingRegion* Region;

    void zoomPerspective();
    void zoomOrtho();

    void setupInteractionTimers();

    void setCurrentRotation(QQuaternion rotation);
    QQuaternion defaultRotation() const;

    void updateRotationMatrix();

    double clampAzimuth(double azimuth) const;
    double clampPitch(double pitch) const;

};

Q_DECLARE_METATYPE(cw3dRegionViewer*)

///**
//  \brief Returns the object that renderes the line plot
//  */
//inline cwGLLinePlot* cw3dRegionViewer::linePlot() { return LinePlot; }

//inline cwGLScraps *cw3dRegionViewer::scraps() const
//{
//    return Scraps;
//}

///**
//  \brief Returns the caving region that's owned by the renderer
//  */
//inline cwCavingRegion* cw3dRegionViewer::cavingRegion() const {
//    return Region;
//}

/**
* @brief cw3dRegionViewer::azimuth
* @return
*/
inline double cw3dRegionViewer::azimuth() const {
    return Azimuth;
}

/**
* @brief cw3dRegionViewer::pitch
* @return
*/
inline double cw3dRegionViewer::pitch() const {
    return Pitch;
}



/**
* @brief cw3dRegionViewer::orthoProjectionObject
* @return
*/
inline cwOrthogonalProjection* cw3dRegionViewer::orthoProjection() const {
    return OrthognalProjection;
}

/**
* @brief cw3dRegionViewer::perspectiveProjectionObject
* @return
*/
inline cwPerspectiveProjection* cw3dRegionViewer::perspectiveProjection() const {
    return PerspectiveProjection;
}


#endif // CW3DREGIONVIEWER_H
