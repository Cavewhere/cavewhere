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
#include "cwFuture.h"
#include "cwFutureManagerToken.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"

namespace {
constexpr float kGroundParallelEpsilon = 1e-6f;
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
    result.reserve(m_polygonLocalXY.size());
    for (const QPointF& p : m_polygonLocalXY) {
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

    QPointF worldXY;
    if (!screenToWorldXY(screenPos, worldXY)) {
        setErrorMessage(QStringLiteral("Could not project click to ground plane."));
        return;
    }
    addWorldPoint(worldXY);
}

void cwLazClipInteraction::addWorldPoint(QPointF worldXY)
{
    if (m_state == State::Processing || m_state == State::Closed) {
        return;
    }
    m_polygonLocalXY.append(worldXY);
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
    if (m_polygonLocalXY.size() < 3) {
        setErrorMessage(QStringLiteral("Polygon needs at least 3 vertices to close."));
        return;
    }
    setErrorMessage(QString());
    setState(State::Closed);
}

bool cwLazClipInteraction::isNearFirstPoint(QPointF screenPos, double pixelRadius) const
{
    if (m_polygonLocalXY.size() < 3) {
        return false;
    }
    if (m_state != State::Drawing) {
        return false;
    }
    const QPointF firstScreen = worldToScreen(m_polygonLocalXY.first());
    const QPointF d = firstScreen - screenPos;
    return (d.x() * d.x() + d.y() * d.y()) <= (pixelRadius * pixelRadius);
}

QPointF cwLazClipInteraction::worldToScreen(QPointF worldXY) const
{
    if (m_camera.isNull()) {
        return QPointF();
    }
    return m_camera->project(QVector3D(float(worldXY.x()), float(worldXY.y()), 0.0f));
}

void cwLazClipInteraction::commit(Mode mode)
{
    if (m_state != State::Closed) {
        return;
    }
    if (m_region.isNull()) {
        emit clipFailed(QStringLiteral("No region available."));
        return;
    }
    cwLazLayerModel* model = lazLayerModel();
    if (model == nullptr) {
        emit clipFailed(QStringLiteral("No LAZ layer model available."));
        return;
    }
    if (m_sceneNode.isNull()) {
        emit clipFailed(QStringLiteral("LAZ scene node not bound — cannot resolve visible layers."));
        return;
    }
    const QList<cwLazLayer*> visible = m_sceneNode->visibleLayers();
    if (visible.isEmpty()) {
        emit clipFailed(QStringLiteral("No visible LAZ layers to clip."));
        return;
    }

    const QString outPath = nextOutputPath();
    if (outPath.isEmpty()) {
        emit clipFailed(QStringLiteral("Could not determine output path in GIS Layers."));
        return;
    }

    cwLazClipOperation::Request req;
    req.sources.reserve(visible.size());
    for (cwLazLayer* layer : visible) {
        if (layer == nullptr) {
            continue;
        }
        // cwGeometry is implicitly shared — this is a header copy + refcount
        // bump, not a buffer clone. Main-thread mutation of layer->geometry()
        // after this point would detach into a new buffer.
        req.sources.append(layer->geometry());
    }
    req.polygonLocalXY = m_polygonLocalXY;
    req.worldOrigin = m_region->worldOrigin();
    req.outputWktCS = m_region->globalCoordinateSystem();
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
        token.addJob(cwFuture(QFuture<void>(m_currentClip), jobName));
    }

    AsyncFuture::observe(m_currentClip).context(this,
        [this, modelGuard](cwLazClipOperation::Result result) {
            if (!result.hasError()) {
                if (modelGuard) {
                    modelGuard->rescan();
                }
                m_polygonLocalXY.clear();
                emit polygonChanged();
                setErrorMessage(QString());
                setState(State::Idle);
                emit clipSucceeded(result.value().outputPath);
                // Drop out of clip mode so InteractionManager's
                // activeInteraction clears and any QML view bound to
                // it (e.g. the toolbar's NeutralIconButton.selected)
                // releases. The user re-enters the tool from the
                // toolbar if they want another clip.
                deactivate();
            } else if (m_currentClip.isCanceled()) {
                // User-initiated cancel: drop the polygon and go straight to
                // Idle instead of surfacing the worker's "Clip cancelled."
                // string as an error.
                m_polygonLocalXY.clear();
                emit polygonChanged();
                setErrorMessage(QString());
                setState(State::Idle);
            } else {
                setErrorMessage(result.errorMessage());
                // Drop back to Closed so the user can retry or cancel.
                setState(State::Closed);
                emit clipFailed(result.errorMessage());
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
    m_polygonLocalXY.clear();
    emit polygonChanged();
    setErrorMessage(QString());
    setState(State::Idle);
}

void cwLazClipInteraction::onDeactivated()
{
    cancel();
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

bool cwLazClipInteraction::screenToWorldXY(QPointF screenPos, QPointF& outWorldXY) const
{
    if (m_camera.isNull()) {
        return false;
    }
    const QRay3D ray = m_camera->rayFromQtViewport(screenPos);
    const QVector3D origin = ray.origin();
    const QVector3D direction = ray.direction();

    if (std::abs(direction.z()) < kGroundParallelEpsilon) {
        // Ray runs along the ground plane — no unique intersection.
        return false;
    }
    const float t = -origin.z() / direction.z();
    const QVector3D hit = origin + t * direction;
    outWorldXY = QPointF(double(hit.x()), double(hit.y()));
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
