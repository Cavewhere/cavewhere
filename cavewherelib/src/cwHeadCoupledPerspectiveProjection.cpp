//Our includes
#include "cwHeadCoupledPerspectiveProjection.h"
#include "cwAbstractHeadTracker.h"
#include "cwViewMatrixComposer.h"
#include "cw3dRegionViewer.h"
#include "cwGlobals.h"

//Qt includes
#include <QtMath>

cwHeadCoupledPerspectiveProjection::cwHeadCoupledPerspectiveProjection(QObject* parent)
    : cwAbstractProjection(parent)
{
    updateCachedDefaultEz();
}

void cwHeadCoupledPerspectiveProjection::updateCachedDefaultEz()
{
    double sh = m_screenHeightMeters / 2.0;
    m_cachedDefaultEz = sh / qTan(qDegreesToRadians(m_fieldOfView) / 2.0);
}

void cwHeadCoupledPerspectiveProjection::setTracker(cwAbstractHeadTracker* tracker)
{
    if (m_tracker == tracker)
    {
        return;
    }

    if (m_tracker)
    {
        disconnect(m_tracker, &cwAbstractHeadTracker::eyePositionChanged,
                   this, &cwHeadCoupledPerspectiveProjection::onEyePositionChanged);
    }

    m_tracker = tracker;

    if (m_tracker)
    {
        connect(m_tracker, &cwAbstractHeadTracker::eyePositionChanged,
                this, &cwHeadCoupledPerspectiveProjection::onEyePositionChanged);
    }

    emit trackerChanged();
}

void cwHeadCoupledPerspectiveProjection::setFieldOfView(double fov)
{
    fov = qBound(1.0, fov, 179.0);
    if (!qFuzzyCompare(m_fieldOfView, fov))
    {
        m_fieldOfView = fov;
        updateCachedDefaultEz();
        updateProjection();
        emit fieldOfViewChanged();
    }
}

void cwHeadCoupledPerspectiveProjection::setScreenWidthMeters(double width)
{
    if (!qFuzzyCompare(m_screenWidthMeters, width) && width > 0.0)
    {
        m_screenWidthMeters = width;
        updateProjection();
        emit screenWidthMetersChanged();
    }
}

void cwHeadCoupledPerspectiveProjection::setScreenHeightMeters(double height)
{
    if (!qFuzzyCompare(m_screenHeightMeters, height) && height > 0.0)
    {
        m_screenHeightMeters = height;
        updateCachedDefaultEz();
        updateProjection();
        emit screenHeightMetersChanged();
    }
}

void cwHeadCoupledPerspectiveProjection::setSensitivity(double sensitivity)
{
    sensitivity = qBound(0.1, sensitivity, 3.0);
    if (!qFuzzyCompare(m_sensitivity, sensitivity))
    {
        m_sensitivity = sensitivity;
        updateProjection();
        emit sensitivityChanged();
    }
}

void cwHeadCoupledPerspectiveProjection::setViewMatrixComposer(cwViewMatrixComposer* composer)
{
    m_viewMatrixComposer = composer;
}

void cwHeadCoupledPerspectiveProjection::onEyePositionChanged(QVector3D eyePos)
{
    m_lastEyePos = eyePos;
    updateProjection();
}

cwProjection cwHeadCoupledPerspectiveProjection::calculateProjection()
{
    cwProjection proj;

    if (viewer() == nullptr)
    {
        return proj;
    }

    // Eye position: sensitivity scales lateral offset only (not distance)
    double ex = m_lastEyePos.x() * m_sensitivity;
    double ey = m_lastEyePos.y() * m_sensitivity;
    double ez = m_lastEyePos.z();

    double sh = m_screenHeightMeters / 2.0;
    double sw = m_screenWidthMeters / 2.0;

    // If no tracker data yet (eye at origin), use the cached default distance
    if (qFuzzyIsNull(ez))
    {
        ez = m_cachedDefaultEz;
    }

    // Guard against zero/negative distance
    if (ez < 0.01)
    {
        ez = 0.01;
    }

    double np = nearPlane();
    double fp = farPlane();

    // Use defaults from camera if not yet set
    if (qFuzzyIsNull(np) || qFuzzyIsNull(fp))
    {
        cwProjection defaultProj = viewer()->camera()->perspectiveProjectionDefault();
        np = defaultProj.near();
        fp = defaultProj.far();
        setPrivateNearPlane(np);
        setPrivateFarPlane(fp);
    }

    // Off-axis frustum computation
    double scale = np / ez;
    double left   = (-sw - ex) * scale;
    double right  = ( sw - ex) * scale;
    double bottom = (-sh - ey) * scale;
    double top    = ( sh - ey) * scale;

    proj.setFrustum(left, right, bottom, top, np, fp);

    // Derive and store FOV and aspect ratio so zoomTo() works
    double verticalFov = 2.0 * qAtan((top - bottom) / (2.0 * np)) * (180.0 / M_PI);
    double aspectRatio = (right - left) / (top - bottom);
    proj.setFieldOfView(verticalFov);
    proj.setAspectRatio(aspectRatio);

    // Update head offset on the view matrix composer
    if (m_viewMatrixComposer)
    {
        QMatrix4x4 headOffset;
        headOffset.translate(static_cast<float>(-ex),
                             static_cast<float>(-ey),
                             0.0f);
        m_viewMatrixComposer->setHeadOffset(headOffset);
    }

    return proj;
}
