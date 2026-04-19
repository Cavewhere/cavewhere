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
#include <QRectF>
#include <QUrl>

Q_LOGGING_CATEGORY(lcSketchManager, "cw.sketch.manager")

// Ours
#include "cwCacheImageProvider.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwPenStrokeModel.h"
#include "cwProject.h"
#include "cwRegionTreeModel.h"
#include "cwSketch.h"
#include "cwSketchDrawQPainter.h"
#include "cwSketchPainter.h"
#include "cwSketchPainterPathModel.h"
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
QRectF strokeBoundingRect(const cwSketch* sketch)
{
    QRectF bounds;
    bool hasBounds = false;
    if (sketch == nullptr) {
        return bounds;
    }

    for (const auto& stroke : sketch->strokes()) {
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

} // namespace

cwSketchManager::cwSketchManager(QObject* parent)
    : QObject(parent)
{
    // Trailing-edge debounce: each markDirty() restarts the timer, so the
    // icon only rasterises after the user pauses for this long. Pen-up via
    // cwSketch::strokeEnded bypasses the debounce for immediate feedback.
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(1000);
    connect(&m_flushTimer, &QTimer::timeout, this, &cwSketchManager::flushDirty);
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
    }

    m_regionModel = regionTreeModel;

    if (!m_regionModel.isNull()) {
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsInserted,
                this, &cwSketchManager::regionRowsInserted);
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                this, &cwSketchManager::regionRowsAboutToBeRemoved);
        handleRegionReset();
    }
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

QImage cwSketchManager::renderIcon(const cwSketch* sketch, int edgePixels)
{
    if (sketch == nullptr || sketch->strokes().isEmpty() || edgePixels <= 0) {
        return QImage();
    }

    QRectF world = strokeBoundingRect(sketch);
    if (!world.isValid() || world.isEmpty()) {
        // Single-point or zero-extent sketches still get a placeholder canvas
        // centred on the stroke so the thumbnail is non-empty.
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

    // Centre the stroke bbox inside the square image.
    const double offsetX = (edgePixels - world.width()  * zoom) * 0.5;
    const double offsetY = (edgePixels - world.height() * zoom) * 0.5;
    painter.translate(offsetX - world.left() * zoom,
                      offsetY - world.top()  * zoom);
    painter.scale(zoom, zoom);

    cwSketchPainterPathModel pathModel;
    pathModel.setStrokeModel(sketch->strokeModel());

    cwSketchDrawQPainter draw(&painter);
    cwSketchPainter::PaintContext ctx;
    ctx.viewport = world;
    ctx.mapScale = zoom;
    ctx.strokes  = &pathModel;
    cwSketchPainter::paint(&draw, ctx);

    painter.end();
    return image;
}

void cwSketchManager::rasteriseNow(cwSketch* sketch)
{
    writeIcon(sketch);
}

QDir cwSketchManager::cacheDir() const
{
    return m_project ? m_project->dataRootDir() : QDir();
}

void cwSketchManager::writeIcon(cwSketch* sketch)
{
    if (sketch == nullptr) {
        return;
    }

    const QImage icon = renderIcon(sketch);
    if (icon.isNull()) {
        sketch->setIconImagePath(QString());
        return;
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    if (!icon.save(&buffer, "PNG")) {
        qCWarning(lcSketchManager) << "writeIcon: QImage::save(PNG) failed";
        return;
    }

    const QDir dir = cacheDir();
    const auto key = cacheKey(sketch);

    cwDiskCacher cacher(dir);
    cacher.insert(key, pngData);

    // Version the URL by cache-file mtime so QML's Image cache invalidates
    // whenever the icon is rewritten. Without this, setIconImagePath() would
    // early-return on the unchanged URL and QML would keep showing stale
    // (or initial blank) pixels even though the on-disk bytes changed.
    const QFileInfo info(cacher.filePath(key));
    const QString version = QString::number(info.lastModified().toMSecsSinceEpoch());
    sketch->setIconImagePath(cacheUrl(key, version));
}

void cwSketchManager::updateIconFromCache(cwSketch* sketch)
{
    if (sketch == nullptr) {
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

void cwSketchManager::markDirty(cwSketch* sketch)
{
    if (sketch == nullptr) {
        return;
    }
    m_dirtySketches.insert(sketch);
    m_flushTimer.start();
}

void cwSketchManager::flushDirty()
{
    const auto sketches = m_dirtySketches;
    m_dirtySketches.clear();
    for (cwSketch* sketch : sketches) {
        if (sketch != nullptr) {
            writeIcon(sketch);
        }
    }
}

// ---------------------- Region tree wiring ----------------------

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

// ---------------------- Trip wiring ----------------------

void cwSketchManager::connectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    auto* model = trip->notesSketch();
    if (model == nullptr) {
        return;
    }

    if (!m_connectionChecker.add(model)) {
        return;
    }

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

    m_connectionChecker.remove(model);
    disconnect(model, &QAbstractItemModel::rowsInserted,
               this, &cwSketchManager::sketchRowsInserted);
    disconnect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
               this, &cwSketchManager::sketchRowsAboutToBeRemoved);

    const int rows = model->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx = model->index(row, 0);
        QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
        if (auto* sketch = qobject_cast<cwSketch*>(obj)) {
            disconnectSketch(sketch);
        }
    }
}

// ---------------------- Sketch model wiring ----------------------

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
    if (sketch == nullptr || !m_connectionChecker.add(sketch)) {
        return;
    }

    connect(sketch, &QObject::destroyed, this, &cwSketchManager::sketchDestroyed);
    connect(sketch, &cwSketch::strokesReset, this, [this, sketch]() {
        markDirty(sketch);
    });
    connect(sketch, &cwSketch::strokeEnded, this, [this, sketch]() {
        // Pen-up: flush immediately so the thumbnail updates at the natural
        // stroke boundary rather than waiting out the debounce.
        m_dirtySketches.remove(sketch);
        if (m_dirtySketches.isEmpty()) {
            m_flushTimer.stop();
        }
        writeIcon(sketch);
    });

    if (auto* model = sketch->strokeModel()) {
        connect(model, &QAbstractItemModel::dataChanged, this,
                [this, sketch]() { markDirty(sketch); });
        connect(model, &QAbstractItemModel::rowsInserted, this,
                [this, sketch]() { markDirty(sketch); });
        connect(model, &QAbstractItemModel::rowsRemoved, this,
                [this, sketch]() { markDirty(sketch); });
    } else {
        qCWarning(lcSketchManager) << "connectSketch: sketch has no strokeModel" << sketch;
    }

    updateIconFromCache(sketch);
}

void cwSketchManager::disconnectSketch(cwSketch* sketch)
{
    if (sketch == nullptr) {
        return;
    }
    m_connectionChecker.remove(sketch);
    disconnect(sketch, nullptr, this, nullptr);
    if (auto* model = sketch->strokeModel()) {
        disconnect(model, nullptr, this, nullptr);
    }
    m_dirtySketches.remove(sketch);
}

void cwSketchManager::sketchDestroyed(QObject* sketchObj)
{
    auto* sketch = static_cast<cwSketch*>(sketchObj);
    m_dirtySketches.remove(sketch);
}

// ---------------------- Per-trip line plot pipeline ----------------------

void cwSketchManager::acquireLinePlot(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        auto pipeline = std::make_unique<TripPipeline>();
        pipeline->restarter = std::make_unique<Restarter>(this);

        // Re-look up the pipeline by trip pointer at callback time: the
        // entry may have been released between restart() and resolve.
        Restarter* capturedRestarter = pipeline->restarter.get();
        pipeline->restarter->onFutureChanged([this, capturedRestarter, trip]() {
            auto future = capturedRestarter->future();
            AsyncFuture::observe(future).context(this,
                [this, trip, future]() {
                    if (future.isCanceled() || future.resultCount() == 0) {
                        return;
                    }
                    auto entry = m_tripPipelines.find(trip);
                    if (entry == m_tripPipelines.end()) {
                        return;
                    }
                    entry->second->latest = future.result();
                    emit linePlotUpdated(trip);
                });
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
    // Any structural change to the trip's chunks rewires per-chunk signals
    // and triggers a rerun. Calibrations cover declination (stripped) but
    // also tape/compass corrections that DO affect geometry.
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
    // Disconnect all existing per-chunk connections; reconnect based on
    // current chunk list. Cheaper than bookkeeping inserts/removes.
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
    // The trip is gone; connections to it are already dead. Just drop the
    // hash entry. Do NOT touch pipeline.tripConnections here (QObject::destroyed
    // handlers are invoked with the target partially destroyed).
    auto it = m_tripPipelines.find(trip);
    if (it == m_tripPipelines.end()) {
        return;
    }
    it->second->tripConnections.clear();
    it->second->chunkConnections.clear();
    m_tripPipelines.erase(it);
}
