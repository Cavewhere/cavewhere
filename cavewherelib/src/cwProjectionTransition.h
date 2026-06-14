/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROJECTIONTRANSITION_H
#define CWPROJECTIONTRANSITION_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QMatrix4x4>
#include <QQmlEngine>

//Our includes
class cwCamera;
class cwOrthogonalProjection;
class cwPerspectiveProjection;
class cwBaseTurnTableInteraction;
class cwMatrix4x4Animation;

/**
 * Owns the camera's projection mode and the scrubbable, scale-preserving
 * transition between orthographic and perspective (issue #513).
 *
 * The two projection providers each only know their own matrix; the camera
 * holds just the active one; and the eye-distance used for scale matching only
 * has meaning in the turn-table's view-state model. A smooth transition needs
 * all three at once, so it gets its own owner here.
 *
 * progress runs 0 (ortho) -> 1 (perspective). Driven continuously by the
 * ProjectionSlider. When progress leaves a settled endpoint the controller
 * reconciles the inactive zoom channel once (so both endpoints frame the same
 * on-screen scale), snapshots the two endpoint matrices, then blends them into
 * the camera as a custom projection while scrubbing. Reaching an endpoint
 * settles the camera onto that provider's typed projection.
 */
class cwProjectionTransition : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ProjectionTransition)

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(cwOrthogonalProjection* orthoProjection READ orthoProjection WRITE setOrthoProjection NOTIFY orthoProjectionChanged)
    Q_PROPERTY(cwPerspectiveProjection* perspectiveProjection READ perspectiveProjection WRITE setPerspectiveProjection NOTIFY perspectiveProjectionChanged)
    Q_PROPERTY(cwBaseTurnTableInteraction* interaction READ interaction WRITE setInteraction NOTIFY interactionChanged)
    Q_PROPERTY(double progress READ progress WRITE setProgress NOTIFY progressChanged)

public:
    explicit cwProjectionTransition(QObject* parent = nullptr);

    cwCamera* camera() const { return m_camera; }
    void setCamera(cwCamera* camera);

    cwOrthogonalProjection* orthoProjection() const { return m_orthoProjection; }
    void setOrthoProjection(cwOrthogonalProjection* projection);

    cwPerspectiveProjection* perspectiveProjection() const { return m_perspectiveProjection; }
    void setPerspectiveProjection(cwPerspectiveProjection* projection);

    cwBaseTurnTableInteraction* interaction() const { return m_interaction; }
    void setInteraction(cwBaseTurnTableInteraction* interaction);

    double progress() const { return m_progress; }
    void setProgress(double progress);

signals:
    void cameraChanged();
    void orthoProjectionChanged();
    void perspectiveProjectionChanged();
    void interactionChanged();
    void progressChanged();

private:
    // Sets up a transition leaving a settled endpoint: reconcile the inactive
    // zoom channel for the target direction, then snapshot both endpoint
    // matrices (after the reconcile, since perspective->ortho changes the
    // zoomScale the ortho matrix depends on).
    void beginTransition(bool toPerspective);

    // Pushes the blended endpoint matrices into the camera at the given scrub
    // position (only while strictly between the endpoints).
    void blend(double progress);

    // Settles the camera onto a typed projection by enabling that provider.
    void settle(bool perspective);

    QPointer<cwCamera> m_camera;
    QPointer<cwOrthogonalProjection> m_orthoProjection;
    QPointer<cwPerspectiveProjection> m_perspectiveProjection;
    QPointer<cwBaseTurnTableInteraction> m_interaction;

    double m_progress = 0.0;
    bool m_transitioning = false;

    // Reused purely as a matrix interpolator (with the existing InExpo easing);
    // scrubbed via setCurrentTime(), never started.
    cwMatrix4x4Animation* m_interpolator;
};

#endif // CWPROJECTIONTRANSITION_H
