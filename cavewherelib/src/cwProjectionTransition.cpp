/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProjectionTransition.h"
#include "cwCamera.h"
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"
#include "cwBaseTurnTableInteraction.h"
#include "cwMatrix4x4Animation.h"
#include "cwGlobals.h"

//Std
#include <algorithm>

namespace {
    // Length of the eased blend timeline the interpolator scrubs over. Only the
    // ratio matters (progress * duration), so the value is arbitrary; it mirrors
    // the duration the old QML Matrix4x4Animation used.
    constexpr int kTransitionDurationMs = 1000;
}

cwProjectionTransition::cwProjectionTransition(QObject* parent) :
    QObject(parent),
    m_interpolator(new cwMatrix4x4Animation(this))
{
    m_interpolator->setDuration(kTransitionDurationMs);
}

void cwProjectionTransition::setCamera(cwCamera* camera) {
    if(m_camera == camera) { return; }
    m_camera = camera;
    emit cameraChanged();
}

void cwProjectionTransition::setOrthoProjection(cwOrthogonalProjection* projection) {
    if(m_orthoProjection == projection) { return; }
    m_orthoProjection = projection;
    emit orthoProjectionChanged();
}

void cwProjectionTransition::setPerspectiveProjection(cwPerspectiveProjection* projection) {
    if(m_perspectiveProjection == projection) { return; }
    m_perspectiveProjection = projection;
    emit perspectiveProjectionChanged();
}

void cwProjectionTransition::setInteraction(cwBaseTurnTableInteraction* interaction) {
    if(m_interaction == interaction) { return; }
    m_interaction = interaction;
    emit interactionChanged();
}

void cwProjectionTransition::setProgress(double progress) {
    progress = std::clamp(progress, 0.0, 1.0);
    if(m_progress == progress) { return; }

    const double previous = m_progress;
    m_progress = progress;

    const bool leavingEndpoint = (previous <= 0.0 || previous >= 1.0)
            && progress > 0.0 && progress < 1.0;
    if(leavingEndpoint && !m_transitioning) {
        beginTransition(/*toPerspective=*/previous <= 0.0);
    }

    if(progress <= 0.0) {
        settle(/*perspective=*/false);
    } else if(progress >= 1.0) {
        settle(/*perspective=*/true);
    } else if(m_transitioning) {
        blend(progress);
    }

    emit progressChanged();
}

void cwProjectionTransition::beginTransition(bool toPerspective) {
    if(m_camera.isNull() || m_orthoProjection.isNull() || m_perspectiveProjection.isNull()) {
        return;
    }

    if(!m_interaction.isNull()) {
        const double fovRadians =
                m_perspectiveProjection->fieldOfView() * cwGlobals::degreesToRadians();
        m_interaction->reconcileZoomForProjection(toPerspective, fovRadians);
    }

    // Snapshot AFTER the reconcile: perspective->ortho changes zoomScale, which
    // the ortho matrix depends on, so the ortho provider must recompute first.
    m_orthoProjection->refresh();
    m_perspectiveProjection->refresh();

    m_interpolator->setStartValue(m_orthoProjection->matrix());
    m_interpolator->setEndValue(m_perspectiveProjection->matrix());

    m_transitioning = true;
}

void cwProjectionTransition::blend(double progress) {
    if(m_camera.isNull()) { return; }
    m_interpolator->setCurrentTime(qRound(progress * m_interpolator->duration()));
    m_camera->setCustomProjection(m_interpolator->currentValue().value<QMatrix4x4>());
}

void cwProjectionTransition::settle(bool perspective) {
    m_transitioning = false;
    if(m_orthoProjection.isNull() || m_perspectiveProjection.isNull()) { return; }

    // Enabling a provider pushes its typed projection into the camera, replacing
    // the custom blended matrix left over from scrubbing.
    m_orthoProjection->setEnabled(!perspective);
    m_perspectiveProjection->setEnabled(perspective);
}
