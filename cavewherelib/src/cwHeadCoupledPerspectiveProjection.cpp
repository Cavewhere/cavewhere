//Our includes
#include "cwHeadCoupledPerspectiveProjection.h"
#include "cwAbstractHeadTracker.h"
#include "cwViewMatrixComposer.h"
#include "cw3dRegionViewer.h"
#include "cwGlobals.h"

//Qt includes
#include <QtMath>

//Std includes
#include <cmath>

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

void cwHeadCoupledPerspectiveProjection::setParallaxStrength(double strength)
{
    strength = qBound(0.0, strength, 2.0);
    if (!qFuzzyCompare(m_parallaxStrength, strength))
    {
        m_parallaxStrength = strength;
        updateProjection();
        emit parallaxStrengthChanged();
    }
}

void cwHeadCoupledPerspectiveProjection::setTranslationScale(double scale)
{
    scale = qBound(0.0, scale, 3.0);
    if (!qFuzzyCompare(m_translationScale, scale))
    {
        m_translationScale = scale;
        updateProjection();
        emit translationScaleChanged();
    }
}

void cwHeadCoupledPerspectiveProjection::setViewMatrixOffsetEnabled(bool enabled)
{
    if (m_viewMatrixOffsetEnabled != enabled)
    {
        m_viewMatrixOffsetEnabled = enabled;
        updateProjection();
        emit viewMatrixOffsetEnabledChanged();
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

    // If no tracker data yet (eye at origin), use the cached default distance
    if (qFuzzyIsNull(ez))
    {
        ez = m_cachedDefaultEz;
        m_referenceEz = 0.0; // Reset so it recalibrates on next real data
    }
    else if (qFuzzyIsNull(m_referenceEz))
    {
        // Capture the first real distance as the reference for Z delta
        m_referenceEz = ez;
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

    // Build a standard symmetric perspective frustum from FOV and viewport aspect
    double halfH = np * qTan(qDegreesToRadians(m_fieldOfView) / 2.0);
    double aspect = static_cast<double>(viewer()->width()) / viewer()->height();
    double halfW = halfH * aspect;

    // Head angle: how far the head is offset from screen center (in radians-ish)
    // ex/ez ≈ tan(angle), so multiplying by np gives the shift at the near plane
    // parallaxStrength amplifies this to make the effect visible
    double shiftX = -(ex / ez) * np * m_parallaxStrength;
    double shiftY = -(ey / ez) * np * m_parallaxStrength;

    double left   = -halfW + shiftX;
    double right  =  halfW + shiftX;
    double bottom = -halfH + shiftY;
    double top    =  halfH + shiftY;

    proj.setFrustum(left, right, bottom, top, np, fp);

    // Store FOV and aspect ratio so zoomTo() works
    proj.setFieldOfView(m_fieldOfView);
    proj.setAspectRatio(aspect);

    // Update head offset on the view matrix composer
    if (m_viewMatrixComposer)
    {
        if (m_viewMatrixOffsetEnabled)
        {
            // Z delta: leaning forward (smaller ez) moves camera into scene
            double deltaZ = 0.0;
            if (!qFuzzyIsNull(m_referenceEz))
            {
                deltaZ = (m_referenceEz - ez) * m_sensitivity;
            }

            // Scale translation by camera distance so the effect is
            // proportional to the scene (farther = more amplification)
            float cameraDistance = std::abs(
                m_viewMatrixComposer->baseViewMatrix().column(3).z());
            float scale = cameraDistance * static_cast<float>(m_translationScale);

            QMatrix4x4 headOffset;
            headOffset.translate(static_cast<float>(-ex) * scale,
                                 static_cast<float>(-ey) * scale,
                                 static_cast<float>(deltaZ) * scale);
            m_viewMatrixComposer->setHeadOffset(headOffset);
        }
        else
        {
            m_viewMatrixComposer->setHeadOffset(QMatrix4x4());
        }
    }

    return proj;
}
