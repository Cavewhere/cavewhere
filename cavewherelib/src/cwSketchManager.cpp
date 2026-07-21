/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchManager.h"

// Qt
#include <QAbstractItemModel>
#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPromise>
#include <QRectF>
#include <QTransform>
#include <QUrl>

Q_LOGGING_CATEGORY(lcSketchManager, "cw.sketch.manager")

// Ours
#include "cwCacheImageProvider.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwConcurrent.h"
#include "cwPenStrokeModel.h"
#include "cwProject.h"
#include "cwRegionTreeModel.h"
#include "cwSketch.h"
#include "cwSketchStrokeGeometry.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModelBase.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwTripLinePlotTask.h"

namespace {

// Strokes are stored in world units. Compute their bbox so we can fit the
// sketch into the thumbnail with a small margin. A plain `hasBounds` flag
// is used rather than QRectF::isNull(), because isNull() returns true for
// any zero-size rect (including one seeded by the first point) — which
// previously caused the accumulator to overwrite bounds on every iteration
// and collapse to just the last point.
QRectF strokeBoundingRect(const QVector<cwPenStroke>& strokes)
{
    QRectF bounds;
    bool hasBounds = false;
    for (const auto& stroke : strokes) {
        for (const auto& p : stroke.points) {
            if (!hasBounds) {
                bounds = QRectF(p.position, QSizeF(0.0, 0.0));
                hasBounds = true;
            } else {
                if (p.position.x() < bounds.left())   bounds.setLeft(p.position.x());
                if (p.position.x() > bounds.right())  bounds.setRight(p.position.x());
                if (p.position.y() < bounds.top())    bounds.setTop(p.position.y());
                if (p.position.y() > bounds.bottom()) bounds.setBottom(p.position.y());
            }
        }
    }
    return bounds;
}

// PNG-encode a rendered QImage. Returns an empty QByteArray on failure.
QByteArray encodePng(const QImage& image)
{
    if (image.isNull()) {
        return QByteArray();
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return QByteArray();
    }
    return png;
}

} // namespace

// --------------------------------------------------------------------------
// renderIcon / renderIconFromSnapshot
// --------------------------------------------------------------------------

QImage cwSketchManager::renderIconFromSnapshot(const QVector<cwPenStroke>& strokes,
                                               int edgePixels)
{
    if (strokes.isEmpty() || edgePixels <= 0) {
        return QImage();
    }

    QRectF world = strokeBoundingRect(strokes);
    if (!world.isValid() && world.width() == 0.0 && world.height() == 0.0
        && !std::isfinite(world.x())) {
        return QImage();
    }

    if (world.width() == 0.0 && world.height() == 0.0) {
        // Single-point only.
        const QPointF centre = world.topLeft();
        world = QRectF(centre - QPointF(1.0, 1.0), QSizeF(2.0, 2.0));
    }

    // Pad so thick strokes at the edge aren't clipped.
    constexpr double marginFrac = 0.1;
    const double pad = std::max(world.width(), world.height()) * marginFrac;
    world.adjust(-pad, -pad, pad, pad);

    const double fitSide = std::max(world.width(), world.height());
    if (fitSide <= 0.0) {
        return QImage();
    }
    const double zoom = double(edgePixels) / fitSide;

    QImage image(edgePixels, edgePixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    // Centre the bbox and Y-flip so world north (largest Y) lands at image top.
    const double offsetX = (edgePixels - world.width()  * zoom) * 0.5;
    const double offsetY = (edgePixels - world.height() * zoom) * 0.5;
    QTransform worldToItem;
    worldToItem.translate(offsetX - world.left()   * zoom,
                          offsetY + world.bottom() * zoom);
    worldToItem.scale(zoom, -zoom);
    painter.setTransform(worldToItem);

    // Pen widths are in screen pixels; cancel the world-to-pixel scale so they
    // render at their intended thickness.
    const double penScale = zoom > 0.0 ? 1.0 / zoom : 1.0;

    for (const auto& stroke : strokes) {
        QPainterPath path;
        cwSketchStrokeGeometry::buildPath(path, stroke.points, stroke.width);
        if (path.isEmpty()) {
            continue;
        }
        const QColor color = stroke.color.isValid() ? stroke.color : QColor(Qt::black);
        if (stroke.width > 0.0) {
            QPen pen(color, stroke.width * penScale, Qt::SolidLine,
                     Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawPath(path);
        }
    }

    painter.end();
    return image;
}

QImage cwSketchManager::renderIcon(const cwSketch* sketch, int edgePixels)
{
    if (sketch == nullptr) {
        return QImage();
    }
    return renderIconFromSnapshot(sketch->strokes(), edgePixels);
}

// --------------------------------------------------------------------------
// cwSketchManager
// --------------------------------------------------------------------------

cwSketchManager::cwSketchManager(QObject* parent)
    : QObject(parent),
      m_connectionRegistry(this)
{
}

cwSketchManager::~cwSketchManager() = default;

void cwSketchManager::setProject(cwProject* project)
{
    if (m_project == project) {
        return;
    }
    m_project = project;
}

void cwSketchManager::setRegionTreeModel(cwRegionTreeModel* regionTreeModel)
{
    if (m_regionModel == regionTreeModel) {
        return;
    }

    if (!m_regionModel.isNull()) {
        disconnect(m_regionModel.data(), &cwRegionTreeModel::rowsInserted,
                   this, &cwSketchManager::regionRowsInserted);
        disconnect(m_regionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                   this, &cwSketchManager::regionRowsAboutToBeRemoved);
        disconnect(m_regionModel.data(), &cwRegionTreeModel::modelReset,
                   this, &cwSketchManager::handleRegionReset);
    }

    m_regionModel = regionTreeModel;

    if (!m_regionModel.isNull()) {
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsInserted,
                this, &cwSketchManager::regionRowsInserted);
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                this, &cwSketchManager::regionRowsAboutToBeRemoved);
        // A model reset (project load / git checkout) replaces every row without
        // per-row signals, so re-walk the reloaded trips. Mirrors cwScrapManager.
        connect(m_regionModel.data(), &cwRegionTreeModel::modelReset,
                this, &cwSketchManager::handleRegionReset);
        handleRegionReset();
    }
}

void cwSketchManager::setAutoIconUpdates(bool enabled)
{
    if (m_autoIconUpdates == enabled) {
        return;
    }
    m_autoIconUpdates = enabled;
    emit autoIconUpdatesChanged();

    if (!enabled) {
        // Stop pending idle timers; in-flight renders complete naturally.
        for (auto& entry : m_sketchPipelines) {
            if (entry.second->idleTimer) {
                entry.second->idleTimer->stop();
            }
        }
    }
}

void cwSketchManager::setIdleIntervalMs(int ms)
{
    if (ms < 0) {
        ms = 0;
    }
    if (m_idleIntervalMs == ms) {
        return;
    }
    m_idleIntervalMs = ms;
    emit idleIntervalMsChanged();
}

cwDiskCacher::Key cwSketchManager::cacheKey(const cwSketch* sketch)
{
    cwDiskCacher::Key key;
    if (sketch == nullptr) {
        return key;
    }
    key.path = QDir(QStringLiteral("sketch-icons"));
    key.id = sketch->id().toString(QUuid::WithoutBraces);
    return key;
}

QString cwSketchManager::cacheUrl(const cwDiskCacher::Key& key, const QString& version)
{
    const QString encoded = cwCacheImageProvider::encodeKey(key);
    const QByteArray escaped = QUrl::toPercentEncoding(encoded);
    QString url = QStringLiteral("image://%1/%2")
        .arg(cwCacheImageProvider::name(), QString::fromLatin1(escaped));
    if (!version.isEmpty()) {
        url += QStringLiteral("?v=") + version;
    }
    return url;
}

void cwSketchManager::rasteriseNow(cwSketch* sketch)
{
    writeIconSync(sketch);
}

void cwSketchManager::flushIconIfDirty(QObject* sketchObject)
{
    auto* sketch = qobject_cast<cwSketch*>(sketchObject);
    if (sketch == nullptr) {
        return;
    }
    auto* pipeline = pipelineFor(sketch);
    if (pipeline == nullptr) {
        return;
    }
    if (pipeline->activeDrawing) {
        return;
    }
    if (pipeline->dirtyEpoch == pipeline->completedEpoch) {
        return;
    }
    if (pipeline->idleTimer) {
        pipeline->idleTimer->stop();
    }
    submitRender(sketch);
}

QDir cwSketchManager::cacheDir() const
{
    return m_project ? m_project->dataRootDir() : QDir();
}

void cwSketchManager::writeIconSync(cwSketch* sketch)
{
    if (sketch == nullptr || m_project == nullptr) {
        return;
    }

    const QImage icon = renderIcon(sketch);
    if (icon.isNull()) {
        sketch->setIconImagePath(QString());
        return;
    }

    const QByteArray pngData = encodePng(icon);
    if (pngData.isEmpty()) {
        qCWarning(lcSketchManager) << "writeIconSync: PNG encode failed";
        return;
    }

    const QDir dir = cacheDir();
    const auto key = cacheKey(sketch);

    cwDiskCacher cacher(dir);
    cacher.insert(key, pngData);

    const QFileInfo info(cacher.filePath(key));
    const QString version = QString::number(info.lastModified().toMSecsSinceEpoch());
    sketch->setIconImagePath(cacheUrl(key, version));

    if (auto* p = pipelineFor(sketch)) {
        p->completedEpoch = p->dirtyEpoch;
        p->submittedEpoch = p->dirtyEpoch;
    }
}

void cwSketchManager::updateIconFromCache(cwSketch* sketch)
{
    if (sketch == nullptr || m_project == nullptr) {
        return;
    }

    cwDiskCacher cacher(cacheDir());
    const auto key = cacheKey(sketch);
    if (cacher.hasEntry(key)) {
        const QFileInfo info(cacher.filePath(key));
        const QString version = QString::number(info.lastModified().toMSecsSinceEpoch());
        sketch->setIconImagePath(cacheUrl(key, version));
    } else {
        sketch->setIconImagePath(QString());
    }
}

// --------------------------------------------------------------------------
// Pipeline helpers
// --------------------------------------------------------------------------

cwSketchManager::SketchPipeline* cwSketchManager::pipelineFor(cwSketch* sketch)
{
    auto it = m_sketchPipelines.find(sketch);
    if (it == m_sketchPipelines.end()) {
        return nullptr;
    }
    return it->second.get();
}

void cwSketchManager::markDirty(cwSketch* sketch)
{
    if (sketch == nullptr) {
        return;
    }
    auto* pipeline = pipelineFor(sketch);
    if (pipeline == nullptr) {
        return;
    }
    ++pipeline->dirtyEpoch;

    if (!m_autoIconUpdates) {
        return; // manual mode: wait for flushIconIfDirty.
    }
    if (pipeline->activeDrawing) {
        return; // never arm the timer while drawing.
    }
    if (pipeline->idleTimer) {
        pipeline->idleTimer->start(m_idleIntervalMs);
    }
}

void cwSketchManager::onActiveDrawingChanged(cwSketch* sketch, bool active)
{
    auto* pipeline = pipelineFor(sketch);
    if (pipeline == nullptr) {
        return;
    }
    pipeline->activeDrawing = active;

    if (active) {
        if (pipeline->idleTimer) {
            pipeline->idleTimer->stop();
        }
        // Drop any in-flight render before its disk write lands.
        if (pipeline->cancelToken) {
            pipeline->cancelToken->store(true);
        }
    } else {
        if (m_autoIconUpdates
            && pipeline->dirtyEpoch > pipeline->completedEpoch
            && pipeline->idleTimer) {
            pipeline->idleTimer->start(m_idleIntervalMs);
        }
    }
}

void cwSketchManager::onIdleTimeout(cwSketch* sketch)
{
    auto* pipeline = pipelineFor(sketch);
    if (pipeline == nullptr) {
        return;
    }
    if (pipeline->activeDrawing) {
        return;
    }
    if (pipeline->dirtyEpoch == pipeline->completedEpoch) {
        return; // battery guard
    }
    submitRender(sketch);
}

void cwSketchManager::submitRender(cwSketch* sketch)
{
    if (sketch == nullptr || m_project == nullptr) {
        return;
    }
    auto* pipeline = pipelineFor(sketch);
    if (pipeline == nullptr || !pipeline->restarter) {
        return;
    }

    // Snapshot GUI-thread state into the worker closure.
    const QVector<cwPenStroke> snapshot = sketch->strokes();
    const cwDiskCacher::Key key = cacheKey(sketch);
    const QDir dir = cacheDir();
    const int edgePixels = 256;
    pipeline->submittedEpoch = pipeline->dirtyEpoch;
    pipeline->cancelToken = std::make_shared<std::atomic<bool>>(false);
    auto cancelToken = pipeline->cancelToken;

    pipeline->restarter->restart(
        [snapshot, key, dir, edgePixels, cancelToken]() -> QFuture<QByteArray> {
            return cwConcurrent::run(
                [snapshot, key, dir, edgePixels, cancelToken]
                (QPromise<QByteArray>& promise) {
                    if (promise.isCanceled() || cancelToken->load()) return;
                    const QImage img = renderIconFromSnapshot(snapshot, edgePixels);
                    if (img.isNull()) {
                        promise.addResult(QByteArray());
                        return;
                    }

                    if (promise.isCanceled() || cancelToken->load()) return;
                    const QByteArray png = encodePng(img);
                    if (png.isEmpty()) return;

                    if (promise.isCanceled() || cancelToken->load()) return;
                    cwDiskCacher(dir).insert(key, png);
                    promise.addResult(png);
                });
        });
}

// --------------------------------------------------------------------------
// Region / trip / sketch wiring
// --------------------------------------------------------------------------

void cwSketchManager::handleRegionReset()
{
    if (m_regionModel.isNull() || m_regionModel->cavingRegion() == nullptr) {
        return;
    }
    for (cwCave* cave : m_regionModel->cavingRegion()->caves()) {
        for (cwTrip* trip : cave->trips()) {
            connectTrip(trip);
        }
    }
}

void cwSketchManager::regionRowsInserted(const QModelIndex& parent, int begin, int end)
{
    if (m_regionModel.isNull()) {
        return;
    }
    for (int i = begin; i <= end; i++) {
        const QModelIndex idx = m_regionModel->index(i, 0, parent);
        if (m_regionModel->isTrip(idx)) {
            if (auto* trip = m_regionModel->trip(idx)) {
                connectTrip(trip);
            }
        }
    }
}

void cwSketchManager::regionRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end)
{
    if (m_regionModel.isNull()) {
        return;
    }
    for (int i = begin; i <= end; i++) {
        const QModelIndex idx = m_regionModel->index(i, 0, parent);
        if (m_regionModel->isTrip(idx)) {
            if (auto* trip = m_regionModel->trip(idx)) {
                disconnectTrip(trip);
            }
        }
    }
}

void cwSketchManager::connectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    auto* model = trip->notesSketch();
    if (model == nullptr) {
        return;
    }

    m_connectionRegistry.add(model, [this, model]{
        connect(model, &QAbstractItemModel::rowsInserted,
                this, &cwSketchManager::sketchRowsInserted);
        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
                this, &cwSketchManager::sketchRowsAboutToBeRemoved);

        const int rows = model->rowCount();
        for (int row = 0; row < rows; ++row) {
            const QModelIndex idx = model->index(row, 0);
            QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
            if (auto* sketch = qobject_cast<cwSketch*>(obj)) {
                connectSketch(sketch);
            }
        }
    });
}

void cwSketchManager::disconnectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    auto* model = trip->notesSketch();
    if (model == nullptr) {
        return;
    }

    // remove() tears down the model↔this row connections wholesale (equivalent to the
    // two specific disconnects this replaced).
    m_connectionRegistry.remove(model);

    const int rows = model->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx = model->index(row, 0);
        QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
        if (auto* sketch = qobject_cast<cwSketch*>(obj)) {
            disconnectSketch(sketch);
        }
    }
}

void cwSketchManager::attachSketch(cwSketch* sketch)
{
    connectSketch(sketch);
}

void cwSketchManager::detachSketch(cwSketch* sketch)
{
    disconnectSketch(sketch);
}

void cwSketchManager::sketchRowsInserted(const QModelIndex& parent, int begin, int end)
{
    Q_UNUSED(parent);

    auto* model = qobject_cast<cwSurveyNoteSketchModel*>(sender());
    if (model == nullptr) {
        return;
    }

    for (int row = begin; row <= end; ++row) {
        const QModelIndex idx = model->index(row, 0);
        QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
        if (auto* sketch = qobject_cast<cwSketch*>(obj)) {
            connectSketch(sketch);
        }
    }
}

void cwSketchManager::sketchRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end)
{
    Q_UNUSED(parent);

    auto* model = qobject_cast<cwSurveyNoteSketchModel*>(sender());
    if (model == nullptr) {
        return;
    }

    for (int row = begin; row <= end; ++row) {
        const QModelIndex idx = model->index(row, 0);
        QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
        if (auto* sketch = qobject_cast<cwSketch*>(obj)) {
            disconnectSketch(sketch);
        }
    }
}

void cwSketchManager::connectSketch(cwSketch* sketch)
{
    if (sketch == nullptr) {
        return;
    }

    if (!m_connectionRegistry.add(sketch, [this, sketch]{ wireSketch(sketch); })) {
        return;
    }

    updateIconFromCache(sketch);
}

/**
 * @brief Creates the per-sketch pipeline and wires the sketch's signals to this manager.
 */
void cwSketchManager::wireSketch(cwSketch* sketch)
{
    // Create the per-sketch pipeline BEFORE wiring signals so slot callbacks
    // can assume it exists.
    auto pipeline = std::make_unique<SketchPipeline>();
    pipeline->restarter = std::make_unique<IconRestarter>(this);
    pipeline->idleTimer = std::make_unique<QTimer>();
    pipeline->idleTimer->setSingleShot(true);

    IconRestarter* restarterPtr = pipeline->restarter.get();

    // Runs on the GUI thread after a submitted render settles. onResult()
    // supplies the cancel/empty guard; the result body handles the remaining
    // sketch-specific drops.
    restarterPtr->onResult(this, [this, sketch](const QByteArray& renderedIcon) {
        auto* p = pipelineFor(sketch);
        if (p == nullptr) {
            return; // sketch went away
        }
        if (renderedIcon.isEmpty()) {
            return; // worker cancelled mid-flight; no disk write happened
        }
        if (p->cancelToken && p->cancelToken->load()) {
            return; // superseded by activeDrawing; drop stale result
        }
        if (m_project == nullptr) {
            return;
        }
        const auto key = cacheKey(sketch);
        const QDir dir = cacheDir();
        const QFileInfo info(cwDiskCacher(dir).filePath(key));
        const QString version = QString::number(
            info.lastModified().toMSecsSinceEpoch());
        sketch->setIconImagePath(cacheUrl(key, version));
        p->completedEpoch = p->submittedEpoch;

        if (m_autoIconUpdates
            && p->dirtyEpoch > p->completedEpoch
            && !p->activeDrawing
            && p->idleTimer) {
            p->idleTimer->start(m_idleIntervalMs);
        }
    });

    QTimer* timerPtr = pipeline->idleTimer.get();
    connect(timerPtr, &QTimer::timeout, this,
            [this, sketch]() { onIdleTimeout(sketch); });

    m_sketchPipelines.emplace(sketch, std::move(pipeline));

    connect(sketch, &QObject::destroyed, this, &cwSketchManager::sketchDestroyed);
    connect(sketch, &cwSketch::strokesReset, this, [this, sketch]() {
        markDirty(sketch);
    });
    connect(sketch, &cwSketch::activeDrawingChanged, this,
            [this, sketch](bool active) { onActiveDrawingChanged(sketch, active); });

    if (auto* model = sketch->strokeModel()) {
        connect(model, &QAbstractItemModel::dataChanged, this,
                [this, sketch]() { markDirty(sketch); });
        connect(model, &QAbstractItemModel::rowsInserted, this,
                [this, sketch]() { markDirty(sketch); });
        connect(model, &QAbstractItemModel::rowsRemoved, this,
                [this, sketch]() { markDirty(sketch); });
    } else {
        qCWarning(lcSketchManager) << "wireSketch: sketch has no strokeModel" << sketch;
    }
}

void cwSketchManager::disconnectSketch(cwSketch* sketch)
{
    if (sketch == nullptr) {
        return;
    }
    // remove() unrecords the sketch and tears down every sketch↔this connection.
    m_connectionRegistry.remove(sketch);
    if (auto* model = sketch->strokeModel()) {
        // strokeModel is a second source object, not covered by the sketch's own
        // wholesale disconnect above.
        disconnect(model, nullptr, this, nullptr);
    }
    m_sketchPipelines.erase(sketch); // restarter cancels in-flight in its dtor
}

void cwSketchManager::sketchDestroyed(QObject* sketchObj)
{
    auto* sketch = static_cast<cwSketch*>(sketchObj);
    m_sketchPipelines.erase(sketch);
}

// --------------------------------------------------------------------------
// Per-trip line plot pipeline (unchanged)
// --------------------------------------------------------------------------

void cwSketchManager::acquireLinePlot(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        auto pipeline = std::make_unique<TripPipeline>();
        pipeline->restarter = std::make_unique<Restarter>(this);

        pipeline->restarter->onResult(this,
            [this, trip](const QList<cwTripLinePlotTask::TripComponent>& result) {
                auto entry = m_tripPipelines.find(trip);
                if (entry == m_tripPipelines.end()) {
                    return;
                }
                entry->second->latest = result;
                emit linePlotUpdated(trip);
            });

        auto [inserted, ok] = m_tripPipelines.emplace(trip, std::move(pipeline));
        Q_UNUSED(ok);
        it = inserted;

        connectLinePlotTripSignals(trip, *it->second);
        connectLinePlotChunkSignals(trip, *it->second);

        it->second->destroyedConnection = connect(trip, &QObject::destroyed,
            this, [this, trip]() { onTripDestroyed(trip); });

        it->second->refCount = 1;
        requestLinePlotRerun(trip);
        return;
    }

    it->second->refCount += 1;
}

void cwSketchManager::releaseLinePlot(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }
    auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        return;
    }
    it->second->refCount -= 1;
    if (it->second->refCount > 0) {
        return;
    }

    disconnectLinePlotSignals(*it->second);
    QObject::disconnect(it->second->destroyedConnection);
    m_tripPipelines.erase(it);
}

QList<cwTripLinePlotTask::TripComponent>
cwSketchManager::latestLinePlot(cwTrip* trip) const
{
    const auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        return {};
    }
    return it->second->latest;
}

void cwSketchManager::connectLinePlotTripSignals(cwTrip* trip, TripPipeline& pipeline)
{
    pipeline.tripConnections.append(
        connect(trip, &cwTrip::chunksInserted, this, [this, trip]() {
            rebuildChunkSignals(trip);
            requestLinePlotRerun(trip);
        }));
    pipeline.tripConnections.append(
        connect(trip, &cwTrip::chunksRemoved, this, [this, trip]() {
            rebuildChunkSignals(trip);
            requestLinePlotRerun(trip);
        }));
    if (auto* calib = trip->calibrations()) {
        pipeline.tripConnections.append(
            connect(calib, &cwTripCalibration::calibrationsChanged,
                    this, [this, trip]() { requestLinePlotRerun(trip); }));
    }
}

void cwSketchManager::connectLinePlotChunkSignals(cwTrip* trip, TripPipeline& pipeline)
{
    for (auto* chunk : trip->chunks()) {
        if (chunk == nullptr) {
            continue;
        }
        pipeline.chunkConnections.append(
            connect(chunk, &cwSurveyChunk::dataChanged,
                    this, [this, trip]() { requestLinePlotRerun(trip); }));
        pipeline.chunkConnections.append(
            connect(chunk, &cwSurveyChunk::stationsAdded,
                    this, [this, trip]() { requestLinePlotRerun(trip); }));
        pipeline.chunkConnections.append(
            connect(chunk, &cwSurveyChunk::stationsRemoved,
                    this, [this, trip]() { requestLinePlotRerun(trip); }));
        pipeline.chunkConnections.append(
            connect(chunk, &cwSurveyChunk::shotsAdded,
                    this, [this, trip]() { requestLinePlotRerun(trip); }));
        pipeline.chunkConnections.append(
            connect(chunk, &cwSurveyChunk::shotsRemoved,
                    this, [this, trip]() { requestLinePlotRerun(trip); }));
    }
}

void cwSketchManager::disconnectLinePlotSignals(TripPipeline& pipeline)
{
    for (const auto& c : pipeline.tripConnections) {
        QObject::disconnect(c);
    }
    pipeline.tripConnections.clear();
    for (const auto& c : pipeline.chunkConnections) {
        QObject::disconnect(c);
    }
    pipeline.chunkConnections.clear();
}

void cwSketchManager::rebuildChunkSignals(cwTrip* trip)
{
    auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        return;
    }
    for (const auto& c : it->second->chunkConnections) {
        QObject::disconnect(c);
    }
    it->second->chunkConnections.clear();
    connectLinePlotChunkSignals(trip, *it->second);
}

void cwSketchManager::requestLinePlotRerun(cwTrip* trip)
{
    auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        return;
    }
    auto input = cwTripLinePlotTask::buildInput(trip);
    it->second->restarter->restart([input = std::move(input)]() mutable {
        return cwTripLinePlotTask::run(std::move(input));
    });
}

void cwSketchManager::onTripDestroyed(cwTrip* trip)
{
    auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        return;
    }
    it->second->tripConnections.clear();
    it->second->chunkConnections.clear();
    m_tripPipelines.erase(it);
}
