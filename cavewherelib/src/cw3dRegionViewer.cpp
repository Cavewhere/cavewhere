/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRhiViewer.h"
#include "cw3dRegionViewer.h"
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"
#include "cwHeadCoupledPerspectiveProjection.h"
#include "cwViewMatrixComposer.h"
#include "cwAbstractHeadTracker.h"
#include "cwDlibHeadTracker.h"
#include "cwCamera.h"

//Qt includes
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QString>
#include <QVector3D>
#include <QRect>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QFontMetrics>
#include <QRay3D>
#include <QOpenGLFunctions>

//const float cw3dRegionViewer::DefaultPitch = 90.0f;
//const float cw3dRegionViewer::DefaultAzimuth = 0.0f;

cw3dRegionViewer::cw3dRegionViewer(QQuickItem *parent) :
    cwRhiViewer(parent)
{
    //This doesn't work on macos
    setSampleCount(4); //MSAA sample count

    OrthognalProjection = new cwOrthogonalProjection(this);
    OrthognalProjection->setViewer(this);
    OrthognalProjection->setEnabled(true);

    PerspectiveProjection = new cwPerspectiveProjection(this);
    PerspectiveProjection->setViewer(this);
    PerspectiveProjection->setEnabled(false);

    HeadTracker = nullptr; // Created lazily on first enable to avoid loading dlib model at startup

    ViewMatrixComposer = new cwViewMatrixComposer(this);
    ViewMatrixComposer->setCamera(camera());

    HeadCoupledProjection = new cwHeadCoupledPerspectiveProjection(this);
    HeadCoupledProjection->setViewer(this);
    HeadCoupledProjection->setEnabled(false);
    HeadCoupledProjection->setViewMatrixComposer(ViewMatrixComposer);

    connect(HeadCoupledProjection, &cwAbstractProjection::enabledChanged, this, [this]() {
        if (HeadCoupledProjection->enabled())
        {
            // Create the tracker lazily to avoid loading dlib model at startup
            if (!HeadTracker)
            {
                HeadTracker = new cwDlibHeadTracker(this);
                HeadCoupledProjection->setTracker(HeadTracker);
            }

            // Copy FOV/near/far from the perspective projection
            HeadCoupledProjection->setFieldOfView(PerspectiveProjection->fieldOfView());
            HeadCoupledProjection->setNearPlane(PerspectiveProjection->nearPlane());
            HeadCoupledProjection->setFarPlane(PerspectiveProjection->farPlane());

            // Disable other projections
            OrthognalProjection->setEnabled(false);
            PerspectiveProjection->setEnabled(false);

            HeadTracker->start();
        }
        else
        {
            if (HeadTracker)
            {
                HeadTracker->stop();
            }

            // Reset head offset to identity
            ViewMatrixComposer->setHeadOffset(QMatrix4x4());

            // Re-enable perspective projection
            PerspectiveProjection->setFieldOfView(HeadCoupledProjection->fieldOfView());
            PerspectiveProjection->setEnabled(true);
        }
    });

    // Auto-detect screen dimensions
    auto* screen = QGuiApplication::primaryScreen();
    if (screen)
    {
        QSizeF physicalSize = screen->physicalSize(); // millimeters
        if (physicalSize.width() > 0 && physicalSize.height() > 0)
        {
            HeadCoupledProjection->setScreenWidthMeters(physicalSize.width() / 1000.0);
            HeadCoupledProjection->setScreenHeightMeters(physicalSize.height() / 1000.0);
        }
    }
}

/**
  \brief Called when the viewer's size changes

  This updates the projection matrix for the view
  */
void cw3dRegionViewer::resizeGL() {
    emit resized();
}

/**
* @brief cw3dRegionViewer::orthoProjectionObject
* @return
*/
 cwOrthogonalProjection* cw3dRegionViewer::orthoProjection() const {
    return OrthognalProjection;
}

/**
* @brief cw3dRegionViewer::perspectiveProjectionObject
* @return
*/
 cwPerspectiveProjection* cw3dRegionViewer::perspectiveProjection() const {
    return PerspectiveProjection;
}

cwHeadCoupledPerspectiveProjection* cw3dRegionViewer::headCoupledProjection() const {
    return HeadCoupledProjection;
}

cwAbstractHeadTracker* cw3dRegionViewer::headTracker() const {
    return HeadTracker;
}

cwViewMatrixComposer* cw3dRegionViewer::viewMatrixComposer() const {
    return ViewMatrixComposer;
}
