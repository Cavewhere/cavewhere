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
#include "cwRenderingSettings.h"
#include "cwScene.h"
#include "cwCamera.h"
#include "cwOffscreenRenderParameters.h"

//Qt includes
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
#include <QImage>
#include <QColor>
#include <QFileInfo>
#include <QDir>

#include <asyncfuture.h>

//const float cw3dRegionViewer::DefaultPitch = 90.0f;
//const float cw3dRegionViewer::DefaultAzimuth = 0.0f;

cw3dRegionViewer::cw3dRegionViewer(QQuickItem *parent) :
    cwRhiViewer(parent)
{
    cwRenderingSettings* renderingSettings = cwRenderingSettings::instance();
    Q_ASSERT(renderingSettings);
    renderingSettings->driveSampleCount(this);

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

void cw3dRegionViewer::renderToImage(const QString& filePath, QSize size)
{
    cwScene* sceneObject = scene();
    cwCamera* cameraObject = camera();
    if (!sceneObject || !cameraObject || filePath.isEmpty()) {
        qWarning() << "renderToImage: no scene/camera or empty path";
        return;
    }

    // Catch a bad destination here, on the GUI thread, so the caller gets an
    // immediate, specific failure instead of a misleading "failed to save" after
    // the async render completes.
    const QDir outputDir = QFileInfo(filePath).absoluteDir();
    if (!outputDir.exists()) {
        qWarning() << "renderToImage: output directory does not exist" << outputDir.absolutePath();
        return;
    }

    // Empty size means "render what's on screen": mirror the live framebuffer by
    // rendering at the physical (DPR-scaled) resolution and feeding the live DPR,
    // so DPR-dependent geometry — point-cloud point size scales by it in the
    // shader — matches the live view exactly. An explicit size is a
    // display-independent render (e.g. a fixed-resolution export or the sink
    // classifier), so it keeps DPR at 1 to stay consistent across machines.
    const double dpr = cameraObject->devicePixelRatio();
    QSize outputSize;
    float requestDevicePixelRatio;
    if (size.isEmpty()) {
        const QSize logical = cameraObject->viewport().size();
        outputSize = QSize(qRound(logical.width() * dpr), qRound(logical.height() * dpr));
        requestDevicePixelRatio = float(dpr);
    } else {
        outputSize = size;
        requestDevicePixelRatio = 1.0f;
    }
    if (outputSize.isEmpty()) {
        qWarning() << "renderToImage: empty output size";
        return;
    }

    // The offscreen path applies the RHI clip-space correction itself, so hand it
    // the camera's raw projection (matching how the live frame feeds the scene).
    cwOffscreenRenderParameters parameters;
    parameters.viewMatrix = cameraObject->viewMatrix();
    parameters.projectionMatrix = cameraObject->projectionMatrix();
    parameters.outputSize = outputSize;
    parameters.devicePixelRatio = requestDevicePixelRatio;
    parameters.backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 1.0);

    QFuture<QImage> future = sceneObject->renderOffscreen(parameters);

    AsyncFuture::observe(future).context(this, [this, filePath](QImage image) {
        if (image.isNull()) {
            qWarning() << "renderToImage: offscreen render produced no image" << filePath;
            return;
        }
        if (!image.save(filePath)) {
            qWarning() << "renderToImage: failed to write image to" << filePath
                       << "(check the extension and write permissions)";
            return;
        }
        qInfo() << "renderToImage: saved offscreen render to" << filePath;
        emit imageRendered(filePath);
    });
}



//}
