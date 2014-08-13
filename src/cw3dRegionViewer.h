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
    Q_PROPERTY(cwOrthogonalProjection* orthoProjection READ orthoProjection CONSTANT)
    Q_PROPERTY(cwPerspectiveProjection* perspectiveProjection READ perspectiveProjection CONSTANT)

public:

    enum ProjectionType {
        Prespective,
        Orthoganal,
        Unknown
    };

    cw3dRegionViewer(QQuickItem* parent = 0);

    cwOrthogonalProjection* orthoProjection() const;
    cwPerspectiveProjection* perspectiveProjection() const;

signals:
    void resized();

private slots:
    virtual void resizeGL();

protected:
    QVector3D unProject(QPoint point);

private:
    cwOrthogonalProjection* OrthognalProjection; //!<
    cwPerspectiveProjection* PerspectiveProjection; //!<

};

Q_DECLARE_METATYPE(cw3dRegionViewer*)


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
