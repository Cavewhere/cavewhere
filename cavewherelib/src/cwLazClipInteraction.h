/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZCLIPINTERACTION_H
#define CWLAZCLIPINTERACTION_H

//Qt includes
#include <QFuture>
#include <QPointF>
#include <QPointer>
#include <QPolygonF>
#include <QString>
#include <QVariantList>

//AsyncFuture
#include <asyncfuture.h>

//Our includes
#include "cwCamera.h"
#include "cwCavingRegion.h"
#include "cwInteraction.h"
#include "cwLazClipOperation.h"
#include "cwLazLayersSceneNode.h"

class cwErrorListModel;
class cwLazLayer;
class cwLazLayerModel;

/**
 * Interaction for screen-space polygon clipping of LAZ point clouds.
 *
 * Vertices live in worldOrigin-relative world XY — the same space the
 * loaded LAZ geometry sits in. On commit, every currently-visible LAZ
 * layer is unioned and clipped by the polygon, producing one clip_NNN.laz
 * in the project's GIS Layers directory.
 *
 * Screen → world unprojection casts each click as a ray through the
 * camera and intersects it with the ground plane at z = 0. The polygon
 * is therefore the camera-projection of the user's screen-drawn shape
 * onto z = 0, and works at any camera angle — only a ray exactly
 * parallel to the ground plane (e.g. perspective view on the horizon)
 * is rejected.
 */
class CAVEWHERE_LIB_EXPORT cwLazClipInteraction : public cwInteraction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LazClipInteraction)

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(cwLazLayersSceneNode* lazLayersSceneNode READ lazLayersSceneNode
                   WRITE setLazLayersSceneNode NOTIFY lazLayersSceneNodeChanged)

    Q_PROPERTY(QVariantList polygonPointsWorld READ polygonPointsWorld NOTIFY polygonChanged)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY polygonChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool canCommit READ canCommit NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    enum class State {
        Idle,        //!< No polygon in progress
        Drawing,     //!< Adding vertices; polygon is open
        Closed,      //!< Polygon closed; awaiting commit (Crop / Erase)
        Processing   //!< Async clip operation running
    };
    Q_ENUM(State)

    enum class Mode : int {
        Keep,
        Remove
    };
    Q_ENUM(Mode)

    explicit cwLazClipInteraction(QQuickItem* parent = nullptr);
    ~cwLazClipInteraction() override;

    cwCamera* camera() const { return m_camera; }
    void setCamera(cwCamera* camera);

    cwCavingRegion* region() const { return m_region; }
    void setRegion(cwCavingRegion* region);

    cwLazLayersSceneNode* lazLayersSceneNode() const { return m_sceneNode; }
    void setLazLayersSceneNode(cwLazLayersSceneNode* node);

    State state() const { return m_state; }
    bool canCommit() const { return m_state == State::Closed; }
    QString errorMessage() const { return m_errorMessage; }
    int pointCount() const { return m_polygonLocalXY.size(); }
    QVariantList polygonPointsWorld() const;

    /// C++-only accessor for the polygon vertices in worldOrigin-relative
    /// XY. Used by cwLazClipPolygonRenderer on every frame sync — avoids
    /// the per-frame QVariantList allocation that the Q_PROPERTY accessor
    /// above incurs.
    const QPolygonF& polygonLocalXY() const { return m_polygonLocalXY; }

    /// Adds a polygon vertex by unprojecting @a screenPos to world XY at
    /// z = 0 (worldOrigin-relative). If the click is within snap range of
    /// the first vertex and the polygon already has >=3 points, closes it
    /// instead. No-op while Processing.
    Q_INVOKABLE void addPoint(QPointF screenPos);

    /// Adds a polygon vertex directly in world XY (worldOrigin-relative).
    /// Used by tests so the state machine can be exercised without
    /// standing up a real camera + viewport; production callers go
    /// through addPoint(QPointF) instead.
    Q_INVOKABLE void addWorldPoint(QPointF worldXY);

    /// Closes the polygon explicitly (requires >=3 vertices). Used by the
    /// "double-click to close" affordance in QML.
    Q_INVOKABLE void closePolygon();

    /// Is the next click close enough to the first vertex (and do we have
    /// enough points) that addPoint() would auto-close? Drives the snap
    /// indicator in the canvas.
    Q_INVOKABLE bool isNearFirstPoint(QPointF screenPos, double pixelRadius = 12.0) const;

    /// Project a world XY (worldOrigin-relative, z = 0) point through the
    /// camera to screen pixels. Used by the canvas to render the polygon.
    QPointF worldToScreen(QPointF worldXY) const;

    /// Runs the clip with @a mode (Crop / Erase). Requires state == Closed
    /// and at least one visible LAZ layer. On success, the output file is
    /// written into the GIS Layers directory and the model is rescanned.
    Q_INVOKABLE void commit(Mode mode);

    /// Drop in-progress polygon, reset to Idle.
    Q_INVOKABLE void cancel();

signals:
    void cameraChanged();
    void regionChanged();
    void lazLayersSceneNodeChanged();
    void polygonChanged();
    void stateChanged();
    void errorMessageChanged();
    void clipSucceeded(const QString& outputPath);
    void clipFailed(const QString& message);

private slots:
    void onDeactivated();

private:
    void setState(State newState);
    void setErrorMessage(const QString& message);

    /// World XY at z = 0 (worldOrigin-relative) for the screen point.
    /// Returns false only when the camera ray is parallel to (or
    /// vanishingly close to parallel with) the ground plane — at any
    /// other angle the ray-plane intersection gives a valid hit.
    bool screenToWorldXY(QPointF screenPos, QPointF& outWorldXY) const;

    cwLazLayerModel* lazLayerModel() const;
    QString nextOutputPath() const;
    cwErrorListModel* projectErrorModel() const;

    /// Emits clipFailed and pushes a Fatal cwError onto the project's
    /// error model so the failure shows up in the global errors UI even
    /// when the clip view has already deactivated.
    void reportFailure(const QString& message);

    /// Drop the polygon, clear errors, return to Idle. Shared cleanup
    /// for the cancel path and all three terminal branches of commit()'s
    /// async callback.
    void resetPolygonToIdle();

    QPointer<cwCamera> m_camera;
    QPointer<cwCavingRegion> m_region;
    QPointer<cwLazLayersSceneNode> m_sceneNode;

    QPolygonF m_polygonLocalXY;
    State m_state = State::Idle;
    QString m_errorMessage;

    // Retain the in-flight clip so the destructor can cancel it — the
    // .context(this, ...) callback is auto-disconnected on destroy, but
    // the worker keeps reading/writing LAZ files until told to stop.
    QFuture<cwLazClipOperation::Result> m_currentClip;
};

#endif // CWLAZCLIPINTERACTION_H
