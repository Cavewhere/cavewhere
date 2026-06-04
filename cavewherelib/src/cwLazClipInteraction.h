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
#include <QList>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVector3D>

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
 * Polygon clipping for LAZ point clouds. Vertices live in
 * worldOrigin-relative XYZ; on commit, visible layers are unioned and
 * clipped into one clip_NNN.laz under the project's GIS Layers dir.
 *
 * Clicks unproject to the camera's near plane so vertices stay
 * coplanar. Worker projects both polygon and points through the
 * frozen view matrix to eye XY — pan and ortho zoom cancel out, since
 * translation shifts both sides equally and projection isn't consulted.
 * Rotation is locked QML-side while the tool is active.
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
    int pointCount() const { return m_polygonWorldXYZ.size(); }
    QVariantList polygonPointsWorld() const;

    /// Const-ref accessor for the renderer's per-frame sync — skips the
    /// per-frame QVariantList allocation the Q_PROPERTY accessor incurs.
    const QList<QVector3D>& polygonWorldXYZ() const { return m_polygonWorldXYZ; }

    /// Unproject @a screenPos to a world vertex on the camera's near
    /// plane and append it. Auto-closes if near the first vertex with
    /// >=3 points already. No-op while Processing.
    Q_INVOKABLE void addPoint(QPointF screenPos);

    /// Test entry — appends a world vertex without unprojection so the
    /// state machine can be driven without a camera.
    Q_INVOKABLE void addWorldPoint(QVector3D worldXYZ);

    /// Closes the polygon explicitly (requires >=3 vertices). Used by the
    /// "double-click to close" affordance in QML.
    Q_INVOKABLE void closePolygon();

    /// Is the next click close enough to the first vertex (and do we have
    /// enough points) that addPoint() would auto-close? Drives the snap
    /// indicator in the canvas.
    Q_INVOKABLE bool isNearFirstPoint(QPointF screenPos, double pixelRadius = 12.0) const;

    /// Project a world XYZ (worldOrigin-relative) point through the
    /// camera to screen pixels. Used by the canvas to render the polygon.
    QPointF worldToScreen(QVector3D worldXYZ) const;

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

    /// Near-plane unprojection. Returns false only when the camera is null.
    bool screenToWorldXYZ(QPointF screenPos, QVector3D& outWorldXYZ) const;

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

    QList<QVector3D> m_polygonWorldXYZ;
    State m_state = State::Idle;
    QString m_errorMessage;

    // Retain the in-flight clip so the destructor can cancel it — the
    // .context(this, ...) callback is auto-disconnected on destroy, but
    // the worker keeps reading/writing LAZ files until told to stop.
    QFuture<cwLazClipOperation::Result> m_currentClip;
};

#endif // CWLAZCLIPINTERACTION_H
