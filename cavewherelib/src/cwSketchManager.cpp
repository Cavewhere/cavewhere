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
#include "cwSurveyNoteModelBase.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"

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
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(0);
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
    ctx.zoom     = zoom;
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
    if (!m_flushTimer.isActive()) {
        m_flushTimer.start();
    }
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
