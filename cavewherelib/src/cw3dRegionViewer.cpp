/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGLViewer.h"
#include "cw3dRegionViewer.h"
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"

//Qt includes
#include <QPainter>
#include <QString>
#include <QVector3D>
#include <QRect>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QtConcurrentMap>
#include <QFontMetrics>
#include <QRay3D>
#include <QOpenGLFunctions>

//const float cw3dRegionViewer::DefaultPitch = 90.0f;
//const float cw3dRegionViewer::DefaultAzimuth = 0.0f;

cw3dRegionViewer::cw3dRegionViewer(QQuickItem *parent) :
    cwGLViewer(parent)
{
    //TODO: Not all Mac and Windows support multi-sampling
//#ifndef Q_OS_WIN
//    setAntialiasing(true);
//#endif

    OrthognalProjection = new cwOrthogonalProjection(this);
    OrthognalProjection->setViewer(this);
    OrthognalProjection->setEnabled(true);

    PerspectiveProjection = new cwPerspectiveProjection(this);
    PerspectiveProjection->setViewer(this);
    PerspectiveProjection->setEnabled(false);
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



//}
