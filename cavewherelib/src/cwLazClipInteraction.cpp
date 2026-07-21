/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazClipInteraction.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QVector3D>
#include <QtMath>

//QMath3d
#include <qray3d.h>

//Our includes
#include "cwCamera.h"
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwFuture.h"
#include "cwFutureManagerToken.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwProject.h"

namespace {
constexpr int kClipNameDigits = 3;
} // namespace

cwLazClipInteraction::cwLazClipInteraction(QQuickItem* parent) :
    cwInteraction(parent)
{
    connect(this, &cwInteraction::deactivated,
            this, &cwLazClipInteraction::onDeactivated);
}

cwLazClipInteraction::~cwLazClipInteraction()
{
    // The .context(this, ...) callback is auto-disconnected on destroy, but
    // the worker keeps consuming file handles and writing the output until
    // it sees promise.isCanceled() — propagate so it actually stops.
    m_currentClip.cancel();
}

void cwLazClipInteraction::setCamera(cwCamera* camera)
{
    if (m_camera == camera) {
        return;
    }
    m_camera = camera;
    emit cameraChanged();
}

void cwLazClipInteraction::setRegion(cwCavingRegion* region)
{
    if (m_region == region) {
        return;
    }
    m_region = region;
    emit regionChanged();
}

void cwLazClipInteraction::setLazLayersSceneNode(cwLazLayersSceneNode* node)
{
    if (m_sceneNode == node) {
        return;
    }
    m_sceneNode = node;
    emit lazLayersSceneNodeChanged();
}

QVariantList cwLazClipInteraction::polygonPointsWorld() const
{
    QVariantList result;
    result.reserve(m_polygonWorldXYZ.size());
    for (const QVector3D& p : m_polygonWorldXYZ) {
        result.append(QVariant::fromValue(p));
    }
    return result;
}

void cwLazClipInteraction::addPoint(QPointF screenPos)
{
    if (m_state == State::Processing || m_state == State::Closed) {
        return;
    }

    // Auto-close when clicking near the first vertex (>=3 points already).
    if (isNearFirstPoint(screenPos)) {
        closePolygon();
        return;
    }

    QVector3D worldXYZ;
    if (!screenToWorldXYZ(screenPos, worldXYZ)) {
        setErrorMessage(QStringLiteral("No camera available to unproject the click."));
        return;
    }
    addWorldPoint(worldXYZ);
}

void cwLazClipInteraction::addWorldPoint(QVector3D worldXYZ)
{
    if (m_state == State::Processing || m_state == State::Closed) {
        return;
    }
    m_polygonWorldXYZ.append(worldXYZ);
    setErrorMessage(QString());
    if (m_state == State::Idle) {
        setState(State::Drawing);
    }
    emit polygonChanged();
}

void cwLazClipInteraction::closePolygon()
{
    if (m_state != State::Drawing) {
        return;
    }
    if (m_polygonWorldXYZ.size() < 3) {
        setErrorMessage(QStringLiteral("Polygon needs at least 3 vertices to close."));
        return;
    }
    setErrorMessage(QString());
    setState(State::Closed);
}

bool cwLazClipInteraction::isNearFirstPoint(QPointF screenPos, double pixelRadius) const
{
    if (m_polygonWorldXYZ.size() < 3) {
        return false;
    }
    if (m_state != State::Drawing) {
        return false;
    }
    const QPointF firstScreen = worldToScreen(m_polygonWorldXYZ.first());
    const QPointF d = firstScreen - screenPos;
    return (d.x() * d.x() + d.y() * d.y()) <= (pixelRadius * pixelRadius);
}

QPointF cwLazClipInteraction::worldToScreen(QVector3D worldXYZ) const
{
    if (m_camera.isNull()) {
        return QPointF();
    }
    return m_camera->project(worldXYZ);
}

void cwLazClipInteraction::commit(Mode mode)
{
    if (m_state != State::Closed) {
        return;
    }
    if (m_region.isNull()) {
        reportFailure(QStringLiteral("No region available."));
        return;
    }
    cwLazLayerModel* model = lazLayerModel();
    if (model == nullptr) {
        reportFailure(QStringLiteral("No LAZ layer model available."));
        return;
    }
    if (m_sceneNode.isNull()) {
        reportFailure(QStringLiteral("LAZ scene node not bound — cannot resolve visible layers."));
        return;
    }
    const QList<cwLazLayer*> visible = m_sceneNode->visibleLayers();
    if (visible.isEmpty()) {
        reportFailure(QStringLiteral("No visible LAZ layers to clip."));
        return;
    }

    // Snapshot the participating layers as QPointers so the success callback
    // can flip enabled=false after the rescan. cwLazLayerModel::rescan() is a
    // diff against (size, mtime), and the clip writes only a new clip_NNN.laz
    // — the sources are untouched on disk, so their cwLazLayer instances
    // survive the rescan. QPointer is belt-and-braces in case some other path
    // does delete one before the callback runs.
    QList<QPointer<cwLazLayer>> sourceLayers;
    sourceLayers.reserve(visible.size());
    for (cwLazLayer* layer : visible) {
        if (layer != nullptr) {
            sourceLayers.append(QPointer<cwLazLayer>(layer));
        }
    }

    const QString outPath = nextOutputPath();
    if (outPath.isEmpty()) {
        reportFailure(QStringLiteral("Could not determine output path in GIS Layers."));
        return;
    }

    cwLazClipOperation::Request req;
    req.sources.reserve(visible.size());
    for (cwLazLayer* layer : visible) {
        if (layer == nullptr) {
            continue;
        }
        // Pass the on-disk path, not the in-memory geometry: the worker
        // re-streams each file so a multi-GB cloud never has a second full
        // copy on the heap, and every source attribute survives the clip.
        req.sources.append({ layer->sourcePath(), layer->sourceCSOverride() });
    }
    req.polygonWorldXYZ = m_polygonWorldXYZ;
    // Freeze the view at commit time — pan between commit and worker run
    // shifts polygon and points by the same amount in the eye-XY test.
    if (!m_camera.isNull()) {
        req.viewMatrix = m_camera->viewMatrix();
    }
    req.worldOrigin = m_region->geoReference()->worldOrigin();
    req.outputWktCS = m_region->geoReference()->globalCoordinateSystem();
    // Two parallel Mode enums (QML-facing + operation-facing). Switch with
    // no default: so adding a new enumerator fails to compile here until
    // both sides are updated. Asserts also catch silent reordering.
    static_assert(int(cwLazClipOperation::Mode::Keep) == int(Mode::Keep));
    static_assert(int(cwLazClipOperation::Mode::Remove) == int(Mode::Remove));
    switch (mode) {
    case Mode::Keep:
        req.mode = cwLazClipOperation::Mode::Keep;
        break;
    case Mode::Remove:
        req.mode = cwLazClipOperation::Mode::Remove;
        break;
    }
    req.outputPath = outPath;

    setState(State::Processing);

    QPointer<cwLazLayerModel> modelGuard(model);
    m_currentClip = cwLazClipOperation::run(req);

    cwFutureManagerToken token = model->futureManagerToken();
    if (token.isValid()) {
        const QString jobName = (mode == Mode::Keep)
                                    ? tr("Cropping %1 LAZ layer(s)").arg(visible.size())
                                    : tr("Erasing from %1 LAZ layer(s)").arg(visible.size());
        token.addJob(m_currentClip, jobName);
    }

    AsyncFuture::observe(m_currentClip).context(this,
        [this, modelGuard, sourceLayers](cwLazClipOperation::Result result) {
            if (!result.hasError()) {
                if (modelGuard) {
                    modelGuard->rescan();
                }
                // Disable each contributing source layer so only the newly
                // written clip stays drawn. setEnabled(false) flips the
                // persisted bit through the paired .cwlaz on the next save,
                // so the disable survives a project reopen — symmetric with
                // the user explicitly toggling the row off in the UI.
                for (const QPointer<cwLazLayer>& layer : sourceLayers) {
                    if (!layer.isNull()) {
                        layer->setEnabled(false);
                    }
                }
                resetPolygonToIdle();
                emit clipSucceeded(result.value().outputPath);
            } else if (m_currentClip.isCanceled()) {
                // User-initiated cancel: drop the polygon and go straight to
                // Idle instead of surfacing the worker's "Clip cancelled."
                // string as an error.
                resetPolygonToIdle();
            } else {
                resetPolygonToIdle();
                // The tool deactivated as soon as the user pressed Crop /
                // Erase, so there's no in-tool banner to read this. Push
                // the failure through the project-wide error model.
                reportFailure(result.errorMessage());
            }
        });
}

void cwLazClipInteraction::cancel()
{
    if (m_state == State::Processing) {
        // Propagate cancel to the worker. It checks promise.isCanceled()
        // between chunks, exits the loop, removes the partial output, and
        // returns a "Clip cancelled." result that the observe.context
        // callback handles via the error branch.
        m_currentClip.cancel();
        return;
    }
    resetPolygonToIdle();
}

void cwLazClipInteraction::onDeactivated()
{
    // Processing means commit() already queued the worker; deactivation
    // is the normal Crop/Erase exit, not an abort.
    if (m_state == State::Processing) {
        return;
    }
    cancel();
}

void cwLazClipInteraction::resetPolygonToIdle()
{
    m_polygonWorldXYZ.clear();
    emit polygonChanged();
    setErrorMessage(QString());
    setState(State::Idle);
}

void cwLazClipInteraction::reportFailure(const QString& message)
{
    emit clipFailed(message);

    cwErrorListModel* errors = projectErrorModel();
    if (errors != nullptr) {
        errors->append(cwError(message, cwError::Fatal));
    }
}

cwErrorListModel* cwLazClipInteraction::projectErrorModel() const
{
    if (m_region.isNull()) {
        return nullptr;
    }
    cwProject* project = m_region->parentProject();
    if (project == nullptr) {
        return nullptr;
    }
    return project->errorModel();
}

void cwLazClipInteraction::setState(State newState)
{
    if (m_state == newState) {
        return;
    }
    m_state = newState;
    emit stateChanged();
}

void cwLazClipInteraction::setErrorMessage(const QString& message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

bool cwLazClipInteraction::screenToWorldXYZ(QPointF screenPos, QVector3D& outWorldXYZ) const
{
    if (m_camera.isNull()) {
        return false;
    }
    // Near-plane unprojection: vertices land on a single plane
    // perpendicular to the view direction, so the polygon is coplanar.
    const QRay3D ray = m_camera->rayFromQtViewport(screenPos);
    outWorldXYZ = ray.origin();
    return true;
}

cwLazLayerModel* cwLazClipInteraction::lazLayerModel() const
{
    if (m_region.isNull()) {
        return nullptr;
    }
    return m_region->lazLayers();
}

QString cwLazClipInteraction::nextOutputPath() const
{
    cwLazLayerModel* model = lazLayerModel();
    if (model == nullptr) {
        return QString();
    }
    const QDir dir = model->gisLayersDir();
    if (dir.absolutePath().isEmpty() || !dir.exists()) {
        return QString();
    }

    // Match clip_<digits>.{laz,las} so we pick up either extension if the
    // user previously imported a hand-named one.
    static const QRegularExpression rx(
        QStringLiteral("^clip_(\\d+)\\.(?:laz|las)$"),
        QRegularExpression::CaseInsensitiveOption);

    int maxIndex = 0;
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString& f : files) {
        const auto m = rx.match(f);
        if (m.hasMatch()) {
            maxIndex = (std::max)(maxIndex, m.captured(1).toInt());
        }
    }
    const QString name = QStringLiteral("clip_%1.laz")
                             .arg(maxIndex + 1, kClipNameDigits, 10, QLatin1Char('0'));
    return dir.filePath(name);
}
