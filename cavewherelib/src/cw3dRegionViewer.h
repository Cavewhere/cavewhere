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

    // Render the resident scene through the offscreen path using the current
    // camera and save the result to filePath as a PNG. When size is empty the
    // camera's viewport size is used. The seed for the image-export and
    // sink-render paths (and a debug/eyeball hook for the offscreen renderer).
    Q_INVOKABLE void renderToImage(const QString& filePath, QSize size = QSize());

signals:
    void resized();

    // Emitted once the offscreen render requested by renderToImage has been
    // written to filePath. Lets callers act on the finished file (the debug menu
    // opens it; the export/sink paths can chain off it).
    void imageRendered(const QString& filePath);

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
