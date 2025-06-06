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
#include "cwRhiViewer.h"
//class cwGLTerrain;
//class cwGLLinePlot;
//class cwGLScraps;
//class cwGLGridPlane;
class cwGeometryItersecter;
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"

class cw3dRegionViewer : public cwRhiViewer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionViewer)
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

// Q_DECLARE_METATYPE(cw3dRegionViewer*)




#endif // CW3DREGIONVIEWER_H
