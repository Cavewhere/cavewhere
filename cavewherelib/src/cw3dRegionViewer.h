/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CW3DREGIONVIEWER_H
#define CW3DREGIONVIEWER_H

//Our includes
#include "cwRhiViewer.h"
class cwGeometryItersecter;
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"
class cwHeadCoupledPerspectiveProjection;
class cwAbstractHeadTracker;
class cwViewMatrixComposer;

class cw3dRegionViewer : public cwRhiViewer
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionViewer)
    Q_PROPERTY(cwOrthogonalProjection* orthoProjection READ orthoProjection CONSTANT)
    Q_PROPERTY(cwPerspectiveProjection* perspectiveProjection READ perspectiveProjection CONSTANT)
    Q_PROPERTY(cwHeadCoupledPerspectiveProjection* headCoupledProjection READ headCoupledProjection CONSTANT)
    Q_PROPERTY(cwAbstractHeadTracker* headTracker READ headTracker NOTIFY headTrackerChanged)
    Q_PROPERTY(cwViewMatrixComposer* viewMatrixComposer READ viewMatrixComposer CONSTANT)

public:

    enum ProjectionType {
        Prespective,
        Orthoganal,
        Unknown
    };

    cw3dRegionViewer(QQuickItem* parent = 0);

    cwOrthogonalProjection* orthoProjection() const;
    cwPerspectiveProjection* perspectiveProjection() const;
    cwHeadCoupledPerspectiveProjection* headCoupledProjection() const;
    cwAbstractHeadTracker* headTracker() const;
    cwViewMatrixComposer* viewMatrixComposer() const;

signals:
    void resized();
    void headTrackerChanged();

private slots:
    virtual void resizeGL();

protected:
    QVector3D unProject(QPoint point);

private:
    cwOrthogonalProjection* OrthognalProjection; //!<
    cwPerspectiveProjection* PerspectiveProjection; //!<
    cwHeadCoupledPerspectiveProjection* HeadCoupledProjection; //!<
    cwAbstractHeadTracker* HeadTracker; //!<
    cwViewMatrixComposer* ViewMatrixComposer; //!<

};

// Include full type definitions after class declaration (needed by moc for Q_PROPERTY).
// Forward declarations above break the circular dependency during class definition.
#include "cwHeadCoupledPerspectiveProjection.h"
#include "cwAbstractHeadTracker.h"
#include "cwViewMatrixComposer.h"

#endif // CW3DREGIONVIEWER_H
